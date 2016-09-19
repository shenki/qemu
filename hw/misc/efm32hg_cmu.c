/*
 * Silabs EFM32HG CMU
 *
 * Copyright 2016 Joel Stanley <joel@jms.id.au>
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "hw/misc/efm32hg_cmu.h"
#include "qemu/log.h"
#include "trace.h"


static uint64_t efm32hg_cmu_read(void *p, hwaddr offset, unsigned size)
{
	EFM32HGCmuState *s = p;

	switch (offset) {
	case CMU_CTRL:
		break;
	case CMU_HFCORECLKDIV:
		break;
	case CMU_HFRCOCTRL:
		break;
	case CMU_STATUS:
		break;
	case CMU_HFPERCLKEN0:
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

static void efm32hg_cmu_write(void *p, hwaddr offset, uint64_t value,
		unsigned size)
{
	EFM32HGCmuState *s = p;

	switch (offset) {
	case CMU_HFPERCLKEN0:
		/* GPIO clock enable */
		if (value & BIT(8)) {
		}
		break;
	case CMU_HFCORECLKEN0:
		/* if LFXO is turned on, set LFXOENS and LFXORDY */
		if (value & BIT(2))
			s->reg[CMU_STATUS] |= BIT(9) | BIT(8);
		break;
	case CMU_HFCORECLKDIV:
		/* Bit 8 is enable if (value & BIT(8)) */
		break;
	case CMU_CMD:
		break;
	case CMU_OSCENCMD:
		break;
	default:
		qemu_log_mask(LOG_UNIMP, "UNIMP %s: 0x%" HWADDR_PRIx " <- 0x%" PRIx64 " [%u]\n", 
			__func__, offset, value, size);
		return;
	};

	s->reg[offset] = value;
	qemu_log_mask(LOG_TRACE, "%s: 0x%" HWADDR_PRIx " <- 0x%" PRIx64 " [%u]\n",
			__func__, offset, value, size);

}

static void efm32hg_cmu_reset(DeviceState *dev)
{
	EFM32HGCmuState *s = EFM32HG_CMU(dev);

	memset(s->reg, 0, sizeof(s->reg));

	s->reg[CMU_CTRL] = 0x000c262c;
	s->reg[CMU_CMD] = 0x0;
	s->reg[CMU_STATUS] = 0x0;
	s->reg[CMU_HFCORECLKEN0]= 0x0;
	s->reg[CMU_HFPERCLKEN0]= 0x0;
	s->reg[CMU_HFCORECLKDIV] = 0x0;
}

static const MemoryRegionOps efm32hg_cmu_ops = {
	.read = efm32hg_cmu_read,
	.write = efm32hg_cmu_write,
	.valid.max_access_size = 4,
	.endianness = DEVICE_LITTLE_ENDIAN,
	.valid.unaligned = false,
};

#define TYPE_EFM32HG_CMU "efm32hg-cmu"
#define EFM32HG_CMU(obj) \
    OBJECT_CHECK(EFM32HGCmuState, (obj), TYPE_EFM32HG_CMU)

static void efm32hg_cmu_realize(DeviceState *dev, Error **errp)
{
    EFM32HGCmuState *s = EFM32HG_CMU(dev);

    memory_region_init_io(&s->mmio, OBJECT(s), &efm32hg_cmu_ops, s,
                          TYPE_EFM32HG_CMU, 0x400);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mmio);
}

static void efm32hg_cmu_class_init(ObjectClass *klass, void *data)
{
	DeviceClass *dc = DEVICE_CLASS(klass);

	dc->reset = efm32hg_cmu_reset;
	dc->realize = efm32hg_cmu_realize;
}

static const TypeInfo efm32hg_cmu_info = {
    .name          = TYPE_EFM32HG_CMU,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(EFM32HGCmuState),
    .class_init	   = efm32hg_cmu_class_init,
};

static void efm32hg_cmu_register_types(void)
{
    type_register_static(&efm32hg_cmu_info);
}
type_init(efm32hg_cmu_register_types)
