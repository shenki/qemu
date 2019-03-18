/*
 *  ASPEED GPIO Controller
 *
 *  Copyright (C) 2017-2018 IBM Corp.
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#ifndef ASPEED_GPIO_H
#define ASPEED_GPIO_H

#include "hw/sysbus.h"

#define TYPE_ASPEED_GPIO "aspeed.gpio"
#define ASPEED_GPIO(obj) OBJECT_CHECK(AspeedGPIOState, (obj), TYPE_ASPEED_GPIO)

#define ASPEED_GPIO_NR_REGS (0x3A4 >> 2)

typedef struct AspeedGPIOState {
    /* <private> */
    SysBusDevice parent;

    /*< public >*/
    MemoryRegion iomem;
    qemu_irq irq;

    uint32_t regs[ASPEED_GPIO_NR_REGS];
} AspeedGPIOState;

#endif /* _ASPEED_GPIO_H_ */
