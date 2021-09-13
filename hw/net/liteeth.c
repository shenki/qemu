/*
 * LiteX Liteeth Ethernet controller
 *
 * Copyright (c) 2021, IBM Corporation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"


#include "qemu-common.h"
#include "qapi/error.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/net/liteeth.h"
#include "net/eth.h"
#include "qemu/bitops.h"
#include "qemu/error-report.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/units.h"
#include "sysemu/dma.h"
#include "trace.h"

#define LITEETH_WRITER_SLOT       (0x00 / 4)
#define LITEETH_WRITER_LENGTH     (0x04 / 4)
#define LITEETH_WRITER_ERRORS     (0x08 / 4)
#define LITEETH_WRITER_EV_STATUS  (0x0C / 4)
#define LITEETH_WRITER_EV_PENDING (0x10 / 4)
#define LITEETH_WRITER_EV_ENABLE  (0x14 / 4)
#define LITEETH_READER_START      (0x18 / 4)
#define LITEETH_READER_READY      (0x1C / 4)
#define LITEETH_READER_LEVEL      (0x20 / 4)
#define LITEETH_READER_SLOT       (0x24 / 4)
#define LITEETH_READER_LENGTH     (0x28 / 4)
#define LITEETH_READER_EV_STATUS  (0x2C / 4)
#define LITEETH_READER_EV_PENDING (0x30 / 4)
#define LITEETH_READER_EV_ENABLE  (0x34 / 4)
#define LITEETH_PREAMBLE_CRC      (0x38 / 4)
#define LITEETH_PREAMBLE_ERRORS   (0x3C / 4)
#define LITEETH_CRC_ERRORS        (0x40 / 4)

static ssize_t liteeth_xmit(LiteEthState *s)
{
    g_autofree uint8_t *buf;
    uint16_t len;
    uint8_t slot;
    hwaddr addr;

    slot = s->regs[LITEETH_READER_SLOT];
    len = s->regs[LITEETH_READER_LENGTH];

    trace_liteeth_xmit(len, slot);

    if (len > s->slot_size) {
        /* Limit size of guest controlled operation */
        len = s->slot_size;
    }

    addr = s->tx_base + slot * s->slot_size;
    buf = g_malloc0(len);

    dma_memory_read(&address_space_memory, addr, buf, len);

    qemu_send_packet(qemu_get_queue(s->nic), buf, len);

    if (s->regs[LITEETH_READER_EV_ENABLE]) {
        s->regs[LITEETH_READER_EV_PENDING] = 1;
        qemu_set_irq(s->rx_irq, 1);
    }

    return 0;
}

static bool liteeth_can_recieve(NetClientState *nc)
{
    return true;
}

static ssize_t liteeth_receive(NetClientState *nc, const uint8_t *buf, size_t len1)
{
    LiteEthState *s = LITEETH(qemu_get_nic_opaque(nc));
    uint32_t len = len1;
    hwaddr buf_addr;

    trace_liteeth_receive(len);

    /* Get free buffer address */
    buf_addr = s->rx_base + s->rx_slot * s->slot_size;

    /* Copy data into memory */
    dma_memory_write(&address_space_memory, buf_addr, buf, len);

    /* Update registers */
    s->regs[LITEETH_READER_SLOT] = s->rx_slot;
    s->regs[LITEETH_READER_READY] = 1;
    s->regs[LITEETH_READER_LENGTH] = len;

    if (s->regs[LITEETH_READER_EV_ENABLE]) {
        s->regs[LITEETH_READER_EV_PENDING] = 1;
        qemu_set_irq(s->rx_irq, 1);
    }

    s->rx_slot = (s->rx_slot + 1) % s->rx_slots;

    return 0;
};

static void liteeth_reset(DeviceState *dev)
{
    LiteEthState *s = LITEETH(dev);

    memset(s->regs, 0, sizeof(s->regs));
    s->tx_slot = 0;
    s->rx_slot = 0;

    qemu_set_irq(s->tx_irq, 0);
    qemu_set_irq(s->rx_irq, 0);
}

static uint64_t liteeth_read(void *opaque, hwaddr addr, unsigned width)
{
    LiteEthState *s = LITEETH(opaque);
    uint32_t reg = addr >> 2;
    uint64_t val;

    switch (reg) {
    case LITEETH_READER_READY:
        val = 1;
        break;
    default:
        val = s->regs[reg];
    }

    trace_liteeth_read(addr, val);

    return val;
}

static void liteeth_write(void *opaque, hwaddr addr, uint64_t val,
                         unsigned width)
{
    LiteEthState *s = LITEETH(opaque);
    uint32_t reg = addr >> 2;

    trace_liteeth_write(addr, val);
    s->regs[reg] = val;

    switch (reg) {
    case LITEETH_READER_START:
        liteeth_xmit(s);
        break;
    case LITEETH_READER_EV_PENDING:
        qemu_set_irq(s->rx_irq, 0);
        break;
    case LITEETH_WRITER_EV_PENDING:
        qemu_set_irq(s->tx_irq, 0);
        break;
    }
}

static const MemoryRegionOps liteeth_ops = {
    .read = liteeth_read,
    .write = liteeth_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

struct NetClientInfo net_liteeth_info = {
    .type = NET_CLIENT_DRIVER_NIC,
    .size = sizeof(NICState),
    .can_receive = liteeth_can_recieve,
    .receive = liteeth_receive,
};

static void liteeth_realize(DeviceState *dev, Error **errp)
{
    LiteEthState *s = LITEETH(dev);
    Error *err = NULL;
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    uint64_t buf_size = (s->tx_slots + s->rx_slots) * s->slot_size;

    sysbus_init_irq(sbd, &s->tx_irq);
    sysbus_init_irq(sbd, &s->rx_irq);

    /* MAC registers */
    memory_region_init_io(&s->mmio, OBJECT(s), &liteeth_ops, s,
                          TYPE_LITEETH, 0x44);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mmio);

    /* Packet buffers */
    memory_region_init_ram(&s->buf, OBJECT(s), "liteeth.buf", buf_size, &err);
    if (err) {
        error_propagate(errp, err);
    }
    memory_region_add_subregion(get_system_memory(), s->rx_base, &s->buf);

    qemu_macaddr_default_if_unset(&s->conf.macaddr);
    s->nic = qemu_new_nic(&net_liteeth_info, &s->conf,
                          object_get_typename(OBJECT(dev)), dev->id, s);
    qemu_format_nic_info_str(qemu_get_queue(s->nic), s->conf.macaddr.a);
}

static Property liteeth_properties[] = {
    DEFINE_PROP_UINT64("rx-base", struct LiteEthState, rx_base, 0xc8030000),
    DEFINE_PROP_UINT64("tx-base", struct LiteEthState, tx_base, 0xc8031000),
    DEFINE_PROP_UINT32("tx-slots", struct LiteEthState, tx_slots, 2),
    DEFINE_PROP_UINT32("rx-slots", struct LiteEthState, rx_slots, 2),
    DEFINE_PROP_UINT32("slot-size", struct LiteEthState, slot_size, 0x800),
    DEFINE_NIC_PROPERTIES(struct LiteEthState, conf),
    DEFINE_PROP_END_OF_LIST(),
};

static void liteeth_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->desc    = "LiteX Ethernet";
    dc->realize = liteeth_realize;
    dc->reset = liteeth_reset;
    device_class_set_props(dc, liteeth_properties);
    set_bit(DEVICE_CATEGORY_NETWORK, dc->categories);
}

static const TypeInfo liteeth_info = {
    .name = TYPE_LITEETH,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(struct LiteEthState),
    .class_init = liteeth_class_init,
};

static void liteeth_register_types(void)
{
    type_register_static(&liteeth_info);
}

type_init(liteeth_register_types);
