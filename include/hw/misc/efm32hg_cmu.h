/*
 * Silabs EFM32HG CMU
 *
 * Copyright 2016 Joel Stanley <joel@jms.id.au>
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#ifndef HW_CHAR_EFM32HG_CMU_H
#define HW_CHAR_EFM32HG_CMU_H

#include "hw/sysbus.h"
#include "sysemu/char.h"
#include "hw/hw.h"

typedef struct {
    /* <private> */
    SysBusDevice parent_obj;

    /* <public> */
    MemoryRegion mmio;

    uint32_t reg[0xDC];

} EFM32HGCmuState;

#define TYPE_EFM32HG_CMU "efm32hg-cmu"
#define EFM32HG_CMU(obj) \
    OBJECT_CHECK(EFM32HGCmuState, (obj), TYPE_EFM32HG_CMU)

#define CMU_CTRL		0x000
#define CMU_HFCORECLKDIV	0x004
#define CMU_HFPERCLKDIV		0x008
#define CMU_HFRCOCTRL		0x00C
#define CMU_LFRCOCTRL		0x010
#define CMU_AUXHFRCOCTRL	0x014
#define CMU_CALCTRL		0x018
#define CMU_CALCNT		0x01C
#define CMU_OSCENCMD		0x020
#define CMU_CMD			0x024
#define CMU_LFCLKSEL		0x028
#define CMU_STATUS		0x02C

#define CMU_HFCORECLKEN0	0x040
#define CMU_HFPERCLKEN0		0x044

#endif
