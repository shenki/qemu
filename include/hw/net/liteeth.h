/*
 * LiteX Liteeth Ethernet controller
 *
 * Copyright (c) 2021, IBM Corporation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LITEETH_H
#define LITEETH_H

#include "net/net.h"
#include "hw/sysbus.h"

struct LiteEthState {
    SysBusDevice parent;

    MemoryRegion mmio;
    MemoryRegion buf;
    qemu_irq irq;
    NICState *nic;
    NICConf conf;

    /* Hardware state */
    uint32_t tx_slot;
    uint32_t rx_slot;

    /*
     * Hardware configuration
     *
     * n slots, slot_size bytes each.
     *
     *  -----   <- rx_base
     * |  1  |
     * |  2  |
     * |  .  |
     * |  n  |
     * |-----|  <- tx_base
     * |  1  |
     * |  2  |
     * |  .  |
     * |  n  |
     *  -----
     */
    hwaddr rx_base;
    hwaddr tx_base;
    uint32_t tx_slots;
    uint32_t rx_slots;
    uint32_t slot_size;

    uint32_t regs[0x100/4];
};

#define TYPE_LITEETH "liteeth"
OBJECT_DECLARE_SIMPLE_TYPE(LiteEthState, LITEETH)

#endif
