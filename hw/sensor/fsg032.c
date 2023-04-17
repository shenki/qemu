/*
 * PMbus device for AcBel FSG032 power supply
 *
 * Copyright 2023 IBM Corp. Joel Stanley <joel@jms.id.au>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/i2c/pmbus_device.h"

#define TYPE_FSG032 "fsg032"
#define FSG032(obj) OBJECT_CHECK(FSG032State, (obj), TYPE_FSG032)

typedef struct FSG032State {
    PMBusDevice parent;
} FSG032State;

static void fsg032_exit_reset(Object *obj)
{
    PMBusDevice *pmdev = PMBUS_DEVICE(obj);

    pmdev->pages[0].mfr_id = "ACBEL";
    pmdev->pages[0].mfr_model = "FSG032";
}

static void fsg032_init(Object *obj)
{
    PMBusDevice *pmdev = PMBUS_DEVICE(obj);

    uint64_t flags = PB_HAS_VOUT_MODE | PB_HAS_VOUT | PB_HAS_VIN | PB_HAS_IOUT |
                     PB_HAS_PIN | PB_HAS_TEMPERATURE | PB_HAS_MFR_INFO;

    pmbus_page_config(pmdev, 0, flags);
}

static void fsg032_class_init(ObjectClass *klass, void *data)
{
    ResettableClass *rc = RESETTABLE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);
    PMBusDeviceClass *k = PMBUS_DEVICE_CLASS(klass);

    dc->desc = "AcBel Power Systems FSG032 Power Supply";
    k->device_num_pages = 1;

    rc->phases.exit = fsg032_exit_reset;
}

static const TypeInfo fsg032_info = {
    .name = TYPE_FSG032,
    .parent = TYPE_PMBUS_DEVICE,
    .instance_size = sizeof(FSG032State),
    .instance_init = fsg032_init,
    .class_init = fsg032_class_init,
};

static void fsg032_register_types(void)
{
    type_register_static(&fsg032_info);
}

type_init(fsg032_register_types);
