/*
 * Silabs EFM32HG TIMER
 *
 * Copyright 2016 Joel Stanley <joel@jms.id.au>
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#ifndef HW_CHAR_EFM32HG_TIMER_H
#define HW_CHAR_EFM32HG_TIMER_H

#include "hw/sysbus.h"
#include "sysemu/char.h"
#include "hw/hw.h"

#define TYPE_EFM32HG_TIMER "efm32hg-timer"
#define EFM32HG_TIMER(obj) \
    OBJECT_CHECK(EFM32HGTimerState, (obj), TYPE_EFM32HG_TIMER)

typedef struct {
    /* <private> */
    SysBusDevice parent;

    /* <public> */
    MemoryRegion iomem;
    QEMUTimer *timer;
    qemu_irq irq;

    uint64_t freq_hz;
    uint64_t hit_time;
    int64_t tick_offset;

    uint32_t ctrl;
    uint32_t cmd;
    uint32_t status;
    uint32_t ien;
    uint16_t cnt;
    uint16_t top;
    uint16_t top_buffer;

} EFM32HGTimerState;

#define TIMER_CTRL		0x000
#define TIMER_CMD		0x004
#define TIMER_STATUS	0x008
#define TIMER_IEN		0x00C
#define TIMER_IF		0x010
#define TIMER_IFS		0x014
#define TIMER_IFC		0x018
#define TIMER_TOP		0x01C
#define TIMER_TOPB		0x020
#define TIMER_CNT		0x024
#define TIMER_ROUTE		0x028
#define TIMER_CC0_CTRL	0x030
#define TIMER_CC0_CCV	0x034
#define TIMER_CC0_CCVP	0x038
#define TIMER_CC0_CCVB	0x03C
#define TIMER_CC1_CTRL	0x040
#define TIMER_CC1_CCV	0x044
#define TIMER_CC1_CCVP	0x048
#define TIMER_CC1_CCVB	0x04C
#define TIMER_CC2_CTRL	0x050
#define TIMER_CC2_CCV	0x054
#define TIMER_CC2_CCVP	0x058
#define TIMER_CC2_CCVB	0x05C
#define TIMER_DTCTRL	0x070
#define TIMER_DTTIME	0x074
#define TIMER_DTFC		0x078
#define TIMER_DTOGEN	0x07C
#define TIMER_DTFAULT	0x080
#define TIMER_DTFAULTC	0x084
#define TIMER_DTLOCK	0x088

#endif
