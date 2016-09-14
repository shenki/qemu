/*
 * Silabs EFM32HG LEUART
 *
 * Copyright 2016 Joel Stanley <joel@jms.id.au>
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#ifndef HW_CHAR_EFM32HG_LEUART_H
#define HW_CHAR_EFM32HG_LEUART_H

#include "hw/sysbus.h"
#include "sysemu/char.h"
#include "hw/hw.h"

#define TYPE_EFM32HG_LEUART "efm32hg-leuart"
#define EFM32HG_LEUART(obj) \
    OBJECT_CHECK(EFM32HGLeuartState, (obj), TYPE_EFM32HG_LEUART)

typedef struct {
    /* <private> */
    SysBusDevice parent_obj;

    /* <public> */
    MemoryRegion mmio;

    uint32_t reg[0x0AC];

    CharDriverState *chr;
    qemu_irq irq;

} EFM32HGLeuartState;

#define  LEUART_CTRL		0x000
#define  LEUART_CMD		0x004
#define  LEUART_STATUS		0x008
#define  LEUART_CLKDIV		0x00C
#define  LEUART_STARTFRAME	0x010
#define  LEUART_SIGFRAME	0x014
#define  LEUART_RXDATAX		0x018
#define  LEUART_RXDATA		0x01C
#define  LEUART_RXDATAXP	0x020
#define  LEUART_TXDATAX		0x024
#define  LEUART_TXDATA		0x028
#define  LEUART_IF		0x02C
#define  LEUART_IFS		0x030
#define  LEUART_IFC		0x034
#define  LEUART_IEN		0x038
#define  LEUART_PULSECTRL	0x03C
#define  LEUART_FREEZE		0x040
#define  LEUART_SYNCBUSY	0x044
#define  LEUART_ROUTE		0x054
#define  LEUART_INPUT		0x0AC

#endif
