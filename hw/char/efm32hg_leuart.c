/*
 * Silabs EFM32HG LEUART
 *
 * Copyright 2016 Joel Stanley <joel@jms.id.au>
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "hw/char/efm32hg_leuart.h"
#include "qemu/log.h"
#include "trace.h"

static void efm32hg_leuart_reset(DeviceState *dev)
{
    EFM32HGLeuartState *s = EFM32HG_LEUART(dev);

    memset(s->reg, 0, sizeof(s->reg));
    s->reg[LEUART_STATUS] = 0x10;

    qemu_set_irq(s->irq, 0);
}

static int efm32hg_leuart_can_receive(void *opaque)
{
    EFM32HGLeuartState *s = opaque;

    return !!(s->reg[LEUART_STATUS] & BIT(5));
}

static void efm32hg_leuart_receive(void *opaque, const uint8_t *buf, int size)
{
    EFM32HGLeuartState *s = opaque;

    if (s->reg[LEUART_STATUS] & BIT(5)) {
	    /* alrady have a bit, dropping it */
	    printf("alrady have a bit, dropping it\n");
	    return;
    }

    s->reg[LEUART_RXDATA] = *buf;

    /* set when bit is available */
    s->reg[LEUART_STATUS] |= BIT(5);
}

static uint64_t efm32hg_leuart_read(void *opaque, hwaddr offset, unsigned size)
{
    EFM32HGLeuartState *s = opaque;
    uint8_t c;

    switch (offset) {
    case LEUART_STATUS:
    case LEUART_CTRL:
    case LEUART_CMD:
        qemu_log_mask(LOG_TRACE, "%s: 0x%" HWADDR_PRIx " [%u] -> 0x%x\n",
			__func__, offset, size, s->reg[offset]);
	return s->reg[offset];
    case LEUART_RXDATA:
        c = s->reg[LEUART_RXDATA];
        s->reg[LEUART_RXDATA] = 0;
        if (s->chr) {
            qemu_chr_accept_input(s->chr);
        }
        return c;
    default:
	qemu_log_mask(LOG_UNIMP, "%s: 0x%" HWADDR_PRIx " [%u]\n",
			__func__, offset, size);
    };

    return 0;
}

static void efm32hg_leuart_write(void *opaque, hwaddr offset, uint64_t val64,
                                unsigned size)
{
    EFM32HGLeuartState *s = opaque;
    uint32_t value = val64;
    unsigned char c;

    switch (offset) {
    case LEUART_TXDATA:
        c = value;
	if (s->chr) {
		qemu_chr_fe_write_all(s->chr, &c, 1);
	}
	/*
	 * TXBL is set when empty.
	 * TODO: set a timer to clear this in the near future?
	 */
	s->reg[LEUART_STATUS] |= BIT(4);
        return;
    case LEUART_CMD:
	    /* enable/disable rx */
	    if (value & BIT(0))
		    s->reg[LEUART_STATUS] |= BIT(0);
	    if (value & BIT(1))
		    s->reg[LEUART_STATUS] &= ~BIT(0);
	    /* enable/disable tx */
	    if (value & BIT(2))
		    s->reg[LEUART_STATUS] |= BIT(1);
	    if (value & BIT(3))
		    s->reg[LEUART_STATUS] &= ~BIT(1);
    case LEUART_CTRL:
    case LEUART_STATUS:
    case LEUART_RXDATAX:
    case LEUART_RXDATA:
    case LEUART_RXDATAXP:
    case LEUART_IF:
	s->reg[offset] = value;
	qemu_log_mask(LOG_TRACE, "%s: 0x%" HWADDR_PRIx " <- 0x%x [%u]\n", 
			__func__, offset, value, size);
        return;
    default:
	qemu_log_mask(LOG_UNIMP, "%s: 0x%" HWADDR_PRIx " <- 0x%x [%u]\n", 
			__func__, offset, value, size);
        return;
    }
}

static const MemoryRegionOps efm32hg_leuart_ops = {
    .read = efm32hg_leuart_read,
    .write = efm32hg_leuart_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static Property efm32hg_leuart_properties[] = {
    DEFINE_PROP_CHR("chardev", EFM32HGLeuartState, chr),
    DEFINE_PROP_END_OF_LIST(),
};

static void efm32hg_leuart_init(Object *obj)
{
    EFM32HGLeuartState *s = EFM32HG_LEUART(obj);

    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);

    memory_region_init_io(&s->mmio, obj, &efm32hg_leuart_ops, s,
                          TYPE_EFM32HG_LEUART, 0x400);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);
}

static void efm32hg_leuart_realize(DeviceState *dev, Error **errp)
{
    EFM32HGLeuartState *s = EFM32HG_LEUART(dev);

    if (s->chr) {
        qemu_chr_add_handlers(s->chr, efm32hg_leuart_can_receive,
                              efm32hg_leuart_receive, NULL, s);
    }
}

static void efm32hg_leuart_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = efm32hg_leuart_reset;
    dc->props = efm32hg_leuart_properties;
    dc->realize = efm32hg_leuart_realize;
}

static const TypeInfo efm32hg_leuart_info = {
    .name          = TYPE_EFM32HG_LEUART,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(EFM32HGLeuartState),
    .instance_init = efm32hg_leuart_init,
    .class_init    = efm32hg_leuart_class_init,
};

static void efm32hg_leuart_register_types(void)
{
    type_register_static(&efm32hg_leuart_info);
}
type_init(efm32hg_leuart_register_types)
