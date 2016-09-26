/*
 * Silabs EFM32HG GPIO
 *
 * Copyright 2016 Joel Stanley <joel@jms.id.au>
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#ifndef HW_CHAR_EFM32HG_GPIO_H
#define HW_CHAR_EFM32HG_GPIO_H

#include "hw/sysbus.h"
#include "sysemu/char.h"
#include "hw/hw.h"

typedef struct {
    /* <private> */
    SysBusDevice parent_obj;

    /* <public> */
    MemoryRegion mmio;

    uint32_t reg[0x140];

} EFM32HGGpioState;

#define TYPE_EFM32HG_GPIO "efm32hg-gpio"
#define EFM32HG_GPIO(obj) \
    OBJECT_CHECK(EFM32HGGpioState, (obj), TYPE_EFM32HG_GPIO)

#endif
