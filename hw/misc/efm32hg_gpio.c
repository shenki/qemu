/*
 * Silabs EFM32HG GPIO
 *
 * Copyright 2016 Joel Stanley <joel@jms.id.au>
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "hw/misc/efm32hg_gpio.h"
#include "qemu/log.h"
#include "trace.h"


static uint64_t efm32hg_gpio_read(void *p, hwaddr offset, unsigned size)
{
	EFM32HGGpioState *s = p;

	switch (offset) {
	case 0x10: /* GPIO_PA_DOUTSET */
		break;
	case 0x14: /* GPIO_PA_DOUTCLR */
		break;
	case 0x34: /* GPIO_PB_DOUTSET */
		break;
	case 0x38: /* GPIO_PB_DOUTCLR */
		break;
	default:
		qemu_log_mask(LOG_UNIMP, "UNIMP %s: 0x%" HWADDR_PRIx " [%u]\n",
				__func__, offset, size);
		return 0;
	}

	qemu_log_mask(LOG_TRACE, "%s: 0x%" HWADDR_PRIx " [%u] -> 0x%x\n",
			__func__, offset, size, s->reg[offset]);
	return s->reg[offset];
}

static void efm32hg_gpio_write(void *p, hwaddr offset, uint64_t value,
		unsigned size)
{
	EFM32HGGpioState *s = p;

	switch (offset) {
	default:
		qemu_log_mask(LOG_UNIMP, "UNIMP %s: 0x%" HWADDR_PRIx " <- 0x%" PRIx64 " [%u]\n", 
			__func__, offset, value, size);
		return;
	};

	s->reg[offset] = value;
	qemu_log_mask(LOG_TRACE, "%s: 0x%" HWADDR_PRIx " <- 0x%" PRIx64 " [%u]\n",
			__func__, offset, value, size);

}

static void efm32hg_gpio_reset(DeviceState *dev)
{
	EFM32HGGpioState *s = EFM32HG_GPIO(dev);

	memset(s->reg, 0, sizeof(s->reg));
}

static const MemoryRegionOps efm32hg_gpio_ops = {
	.read = efm32hg_gpio_read,
	.write = efm32hg_gpio_write,
	.valid.max_access_size = 4,
	.endianness = DEVICE_LITTLE_ENDIAN,
	.valid.unaligned = false,
};

#define TYPE_EFM32HG_GPIO "efm32hg-gpio"
#define EFM32HG_GPIO(obj) \
    OBJECT_CHECK(EFM32HGGpioState, (obj), TYPE_EFM32HG_GPIO)

static void efm32hg_gpio_realize(DeviceState *dev, Error **errp)
{
    EFM32HGGpioState *s = EFM32HG_GPIO(dev);

    memory_region_init_io(&s->mmio, OBJECT(s), &efm32hg_gpio_ops, s,
                          TYPE_EFM32HG_GPIO, 0x400);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mmio);
}

static void efm32hg_gpio_class_init(ObjectClass *klass, void *data)
{
	DeviceClass *dc = DEVICE_CLASS(klass);

	dc->reset = efm32hg_gpio_reset;
	dc->realize = efm32hg_gpio_realize;
}

static const TypeInfo efm32hg_gpio_info = {
    .name          = TYPE_EFM32HG_GPIO,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(EFM32HGGpioState),
    .class_init	   = efm32hg_gpio_class_init,
};

static void efm32hg_gpio_register_types(void)
{
    type_register_static(&efm32hg_gpio_info);
}
type_init(efm32hg_gpio_register_types)
