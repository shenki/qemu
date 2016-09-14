/*
 * Silabs EFM32HG SoC
 *
 * Copyright 2016 Joel Stanley <joel@jms.id.au>
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "hw/arm/efm32hg_soc.h"

static void tomu_init(MachineState *machine)
{
    DeviceState *dev;

    dev = qdev_create(NULL, TYPE_EFM32HG_SOC);
    if (machine->kernel_filename) {
        qdev_prop_set_string(dev, "kernel-filename", machine->kernel_filename);
    }
    object_property_set_bool(OBJECT(dev), true, "realized", &error_fatal);
}

static void tomu_machine_init(MachineClass *mc)
{
    mc->desc = "Tomu";
    mc->init = tomu_init;
}
DEFINE_MACHINE("tomu", tomu_machine_init);
