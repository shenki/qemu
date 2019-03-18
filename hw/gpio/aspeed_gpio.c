/*
 *  ASPEED GPIO Controller
 *
 *  Copyright (C) 2017-2018 IBM Corp.
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/error-report.h"
#include "hw/gpio/aspeed_gpio.h"
#include "qapi/error.h"

static uint64_t aspeed_gpio_read(void *opaque, hwaddr addr,
                                     unsigned int size)
{
    AspeedGPIOState *s = ASPEED_GPIO(opaque);
    uint64_t val = 0;

    addr >>= 2;

    if (addr >= ASPEED_GPIO_NR_REGS) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Out-of-bounds read at offset 0x%" HWADDR_PRIx "\n",
                      __func__, addr << 2);
    } else {
        val = s->regs[addr];
    }

    return val;
}

static void aspeed_gpio_write(void *opaque, hwaddr addr, uint64_t data,
                              unsigned int size)
{
    AspeedGPIOState *s = ASPEED_GPIO(opaque);

    addr >>= 2;

    if (addr >= ASPEED_GPIO_NR_REGS) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Out-of-bounds write at offset 0x%" HWADDR_PRIx "\n",
                      __func__, addr << 2);
        return;
    }

    s->regs[addr] = data;
}

static const MemoryRegionOps aspeed_gpio_ops = {
    .read = aspeed_gpio_read,
    .write = aspeed_gpio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

static void aspeed_gpio_reset(DeviceState *dev)
{
    struct AspeedGPIOState *s = ASPEED_GPIO(dev);

    memset(s->regs, 0, sizeof(s->regs));
}

static void aspeed_gpio_realize(DeviceState *dev, Error **errp)
{
    AspeedGPIOState *s = ASPEED_GPIO(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    sysbus_init_irq(sbd, &s->irq);

    memory_region_init_io(&s->iomem, OBJECT(s), &aspeed_gpio_ops, s,
            TYPE_ASPEED_GPIO, 0x1000);

    sysbus_init_mmio(sbd, &s->iomem);
}

static const VMStateDescription vmstate_aspeed_gpio = {
    .name = TYPE_ASPEED_GPIO,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, AspeedGPIOState, ASPEED_GPIO_NR_REGS),
        VMSTATE_END_OF_LIST(),
    }
};

static void aspeed_gpio_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = aspeed_gpio_realize;
    dc->reset = aspeed_gpio_reset;
    dc->desc = "Aspeed GPIO Controller",
    dc->vmsd = &vmstate_aspeed_gpio;
}

static const TypeInfo aspeed_gpio_info = {
    .name = TYPE_ASPEED_GPIO,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AspeedGPIOState),
    .class_init = aspeed_gpio_class_init,
};

static void aspeed_gpio_register_types(void)
{
    type_register_static(&aspeed_gpio_info);
}

type_init(aspeed_gpio_register_types);
