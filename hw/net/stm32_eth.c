/*
 * STM32 Ethernet MAC
 */

#include "qemu/osdep.h"

#include "hw/irq.h"
#include "hw/net/stm32_eth.h"
#include "hw/qdev-properties.h"
#include "net/checksum.h"
#include "net/eth.h"
#include "system/dma.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/units.h"

static uint64_t stm32_eth_read(void *opaque, hwaddr offset, unsigned size)
{
    Stm32EthState *s = STM32_ETH(opaque);
    uint32_t value = 0;

    switch (offset) {
    default:
        qemu_log_mask(LOG_UNIMP,
                      "%s: unimplmented read 0x"HWADDR_FMT_plx"\n",
                      __func__, offset);
    }

    return value;
}

static void stm32_eth_write(void *opaque, hwaddr offset,
                            uint64_t value, unsigned size)
{
    Stm32EthState *s = STM32_ETH(opaque);
    NetClientState *nc = qemu_get_queue(s->nic);

    switch (offset) {
    default:
        qemu_log_mask(LOG_UNIMP,
                      "%s: unimplmented write 0x"HWADDR_FMT_plx"\n",
                      __func__, offset);
    }
}

static bool stm32_eth_can_receive(NetClientState *nc)
{
    Stm32EthState *s = qemu_get_nic_opaque(nc);

    return true;
}

static ssize_t stm32_eth_receive(NetClientState *nc,
                                 const uint8_t *buf,
                                 size_t size)
{
    Stm32EthState *s = qemu_get_nic_opaque(nc);

    return size;
}

static void stm32_eth_transmit(Stm32EthState *s)
{
    NetClientState *nc = qemu_get_queue(s->nic);
}

static void stm32_eth_reset(DeviceState *dev)
{
    Stm32EthState *s = STM32_ETH(dev);
    NetClientState *nc = qemu_get_queue(s->nic);

}

static void stm32_eth_set_link(NetClientState *nc)
{
    Stm32EthState *s = qemu_get_nic_opaque(nc);

    //allwinner_sun8i_emac_mii_set_link(s, !nc->link_down);
}



static const MemoryRegionOps stm32_eth_mem_ops = {
    .read = stm32_eth_read,
    .write = stm32_eth_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .impl.min_access_size = 4,
};

static NetClientInfo net_stm32_eth_info = {
    .type = NET_CLIENT_DRIVER_NIC,
    .size = sizeof(NICState),
    .can_receive = stm32_eth_can_receive,
    .receive = stm32_eth_receive,
    .link_status_changed = stm32_eth_set_link,
};

static void stm32_eth_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    Stm32EthState *s = STM32_ETH(obj);

    memory_region_init_io(&s->iomem, OBJECT(s), &stm32_eth_mem_ops,
                           s, TYPE_STM32_ETH, 64 * KiB);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
}

static void stm32_eth_realize(DeviceState *dev, Error **errp)
{
    Stm32EthState *s = STM32_ETH(dev);

    if (!s->dma_mr) {
        error_setg(errp, TYPE_STM32_ETH " 'dma-memory' link not set");
        return;
    }

    address_space_init(&s->dma_as, s->dma_mr, "emac-dma");

    qemu_macaddr_default_if_unset(&s->conf.macaddr);
    s->nic = qemu_new_nic(&net_stm32_eth_info, &s->conf,
                          object_get_typename(OBJECT(dev)), dev->id,
                          &dev->mem_reentrancy_guard, s);
    qemu_format_nic_info_str(qemu_get_queue(s->nic), s->conf.macaddr.a);
}

static const Property stm32_eth_properties[] = {
    DEFINE_NIC_PROPERTIES(Stm32EthState, conf),
    DEFINE_PROP_UINT8("phy-addr", Stm32EthState, mii_phy_addr, 0),
    DEFINE_PROP_LINK("dma-memory", Stm32EthState, dma_mr,
                     TYPE_MEMORY_REGION, MemoryRegion *),
};

static void stm32_eth_class_init(ObjectClass *klass,
                                            const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = stm32_eth_realize;
    device_class_set_legacy_reset(dc, stm32_eth_reset);
    device_class_set_props(dc, stm32_eth_properties);
}

static const TypeInfo stm32_eth_info = {
    .name = TYPE_STM32_ETH,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Stm32EthState),
    .instance_init = stm32_eth_init,
    .class_init = stm32_eth_class_init,
};

static void stm32_eth_register_types(void)
{
    type_register_static(&stm32_eth_info);
}
type_init(stm32_eth_register_types);
