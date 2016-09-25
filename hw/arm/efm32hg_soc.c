/*
 * Silabs EFM32HG SoC
 *
 * Copyright 2016 Joel Stanley <joel@jms.id.au>
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "hw/arm/arm.h"
#include "exec/address-spaces.h"
#include "hw/arm/efm32hg_soc.h"
#include "sysemu/sysemu.h"
#include "qemu/log.h"

#define EFM32HG_SOC_IOMEM_BASE	0x40000000
#define EFM32HG_SOC_IOMEM_SIZE	0x1000000

static void efm32hg_soc_initfn(Object *obj)
{
    EFM32HGState *s = EFM32HG_SOC(obj);

    object_initialize(&s->leuart, sizeof(s->leuart), TYPE_EFM32HG_LEUART);
    qdev_set_parent_bus(DEVICE(&s->leuart), sysbus_get_default());

    object_initialize(&s->cmu, sizeof(s->cmu), TYPE_EFM32HG_CMU);
    qdev_set_parent_bus(DEVICE(&s->cmu), sysbus_get_default());
}

static uint64_t efm32hg_soc_io_read(void *p, hwaddr offset, unsigned size)
{
	qemu_log_mask(LOG_UNIMP, "%s: 0x%" HWADDR_PRIx " [%u]\n",
			__func__, offset, size);
	return 0;
}

static void efm32hg_soc_io_write(void *opaque, hwaddr offset, uint64_t value,
		unsigned size)
{
	qemu_log_mask(LOG_UNIMP, "%s: 0x%" HWADDR_PRIx " <- 0x%" PRIx64 " [%u]\n", 
			__func__, offset, value, size);
}

static const MemoryRegionOps efm32hg_soc_io_ops = {
	.read = efm32hg_soc_io_read,
	.write = efm32hg_soc_io_write,
	.endianness = DEVICE_LITTLE_ENDIAN,
};

static void efm32hg_soc_realize(DeviceState *dev_soc, Error **errp)
{
    EFM32HGState *s = EFM32HG_SOC(dev_soc);
    DeviceState *nvic;
    Error *err = NULL;

    /* HAX. See https://bugs.launchpad.net/qemu/+bug/696094 */
    system_clock_scale = 1000;

    memory_region_init_io(&s->iomem, NULL, &efm32hg_soc_io_ops, NULL,
		    "ef32hg_soc.io", EFM32HG_SOC_IOMEM_SIZE);
    memory_region_add_subregion_overlap(get_system_memory(),
		    EFM32HG_SOC_IOMEM_BASE, &s->iomem, -1);

    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *sram = g_new(MemoryRegion, 1);
    MemoryRegion *sram_alias = g_new(MemoryRegion, 1);
    MemoryRegion *flash = g_new(MemoryRegion, 1);

    memory_region_init_ram(flash, NULL, "EFM32HG.flash", FLASH_SIZE,
                           &error_fatal);

    vmstate_register_ram_global(flash);

    memory_region_set_readonly(flash, true);

    memory_region_add_subregion(system_memory, FLASH_BASE, flash);

    memory_region_init_ram(sram, NULL, "EFM32HG.sram", SRAM_SIZE,
                           &error_fatal);
    memory_region_init_alias(sram_alias, NULL, "EFM32HG.sram.alias",
                             sram, SRAM_BASE_ALIAS, SRAM_SIZE);
    vmstate_register_ram_global(sram);
    memory_region_add_subregion(system_memory, SRAM_BASE, sram);
    memory_region_add_subregion(system_memory, SRAM_BASE_ALIAS, sram_alias);

    /* TODO: implement a cortex m0 and update this */
    /* TODO: m0 has 20 irq lines */
    nvic = armv7m_init(get_system_memory(), FLASH_SIZE, 96,
                       s->kernel_filename, "cortex-m3");

    object_property_set_bool(OBJECT(&s->cmu), true, "realized", &err);
    if (err != NULL) {
	    error_propagate(errp, err);
	    return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->cmu), 0, CMU_BASE);

    qdev_prop_set_chr(DEVICE(&s->leuart), "chardev", serial_hds[0]);
    object_property_set_bool(OBJECT(&s->leuart), true, "realized", &err);
    if (err != NULL) {
	    error_propagate(errp, err);
	    return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->leuart), 0, LEUART0_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->leuart), 0, qdev_get_gpio_in(nvic, 10));
}

static Property efm32hg_soc_properties[] = {
    DEFINE_PROP_STRING("kernel-filename", EFM32HGState, kernel_filename),
    DEFINE_PROP_END_OF_LIST(),
};

static void efm32hg_soc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = efm32hg_soc_realize;
    dc->props = efm32hg_soc_properties;
}

static const TypeInfo efm32hg_soc_info = {
    .name          = TYPE_EFM32HG_SOC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(EFM32HGState),
    .instance_init = efm32hg_soc_initfn,
    .class_init    = efm32hg_soc_class_init,
};

static void efm32hg_soc_types(void)
{
    type_register_static(&efm32hg_soc_info);
}
type_init(efm32hg_soc_types)
