/*
 * STM32 Ethernet MAC
 */

#ifndef HW_NET_STM32_ETHT_H
#define HW_NET_STM32_ETH_H

#include "qom/object.h"
#include "hw/sysbus.h"
#include "net/net.h"

#define TYPE_STM32_ETH "stm32-eth"
OBJECT_DECLARE_SIMPLE_TYPE(Stm32EthState, STM32_ETH)

struct Stm32EthState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    qemu_irq irq;

    MemoryRegion *dma_mr;
    AddressSpace dma_as;

    NICState *nic;
    NICConf conf;

    uint8_t mii_phy_addr;
};

#endif
