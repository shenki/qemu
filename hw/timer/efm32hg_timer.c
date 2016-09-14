/*
 * Silabs EFM32HG USART
 *
 * Copyright 2016 Joel Stanley <joel@jms.id.au>
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "hw/timer/efm32hg_timer.h"
#include "qemu/log.h"

static inline int64_t efm32hg_ns_to_ticks(EFM32HGTimerState *s, int64_t t)
{
    return muldiv64(t, s->freq_hz, 1000000000ULL);
}

static void efm32hg_timer_set_alarm(EFM32HGTimerState *s, int64_t now)
{
    uint64_t ticks;
    int64_t now_ticks;

    /* TODO: Add topb support */

    now_ticks = efm32hg_ns_to_ticks(s, now);
    ticks = s->top - (now_ticks - s->tick_offset);

    s->hit_time = muldiv64((ticks + (uint64_t) now_ticks),
                               1000000000ULL, s->freq_hz);

    timer_mod(s->timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + s->hit_time);
}

static void efm32hg_timer_interrupt(void *opaque)
{
    EFM32HGTimerState *s = opaque;

	qemu_irq_pulse(s->irq);
	efm32hg_timer_set_alarm(s, s->hit_time);
}

static void efm32hg_timer_reset(DeviceState *dev)
{
    EFM32HGTimerState *s = EFM32HG_TIMER(dev);
    int64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);

    s->ctrl = 0;
    s->cmd = 0;
    s->status = 0;
    s->ien = 0;
    s->cnt = 0;
	s->top = 0xFFFF;
	s->top_buffer = 0;

    s->tick_offset = efm32hg_ns_to_ticks(s, now);
}

static uint64_t efm32hg_timer_read(void *opaque, hwaddr offset, unsigned size)
{
    EFM32HGTimerState *s = opaque;

    switch (offset) {
	case TIMER_CTRL:
		return s->ctrl;
	case TIMER_CMD:
		return s->cmd;
	case TIMER_STATUS:
		return s->status;
	case TIMER_IEN:
		return s->ien;
    case TIMER_CNT:
        return efm32hg_ns_to_ticks(s, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL)) -
               s->tick_offset;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: offset 0x%"HWADDR_PRIx" unsupported\n", __func__,
                      offset);
    }

    return 0;
}

static void efm32hg_timer_write(void *opaque, hwaddr offset, uint64_t val64,
                                unsigned size)
{
    EFM32HGTimerState *s = opaque;
    uint32_t value = val64;
    int64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);

    switch (offset) {
    case TIMER_CMD:
		if (value & BIT(0)) {
            /* start timer */
        }
        if (value & BIT(1)) {
            /* Stop timer */
        }
        return;
	case TIMER_CNT:
		s->tick_offset = efm32hg_ns_to_ticks(s, now) - value;
		efm32hg_timer_set_alarm(s, now);
        break;
	case TIMER_TOP:
		s->top = (uint16_t)value;
		return;
	case TIMER_TOPB:
		s->top_buffer = value;
		return;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: offset 0x%"HWADDR_PRIx" unsupported\n", __func__,
                      offset);
    }

}

static const MemoryRegionOps efm32hg_timer_ops = {
    .read = efm32hg_timer_read,
    .write = efm32hg_timer_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static Property efm32hg_timer_properties[] = {
    DEFINE_PROP_UINT64("clock-frequency", EFM32HGTimerState,
                       freq_hz, 1000000000),
    DEFINE_PROP_END_OF_LIST(),
};

static void efm32hg_timer_init(Object *obj)
{
    EFM32HGTimerState *s = EFM32HG_TIMER(obj);

    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);

    memory_region_init_io(&s->iomem, obj, &efm32hg_timer_ops, s,
                          "efm32hg_timer", 0x400);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->iomem);

    s->timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, efm32hg_timer_interrupt, s);
}

static void efm32hg_timer_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = efm32hg_timer_reset;
    dc->props = efm32hg_timer_properties;
}

static const TypeInfo efm32hg_timer_info = {
    .name          = TYPE_EFM32HG_TIMER,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(EFM32HGTimerState),
    .instance_init = efm32hg_timer_init,
    .class_init    = efm32hg_timer_class_init,
};

static void efm32hg_timer_register_types(void)
{
    type_register_static(&efm32hg_timer_info);
}
type_init(efm32hg_timer_register_types)
