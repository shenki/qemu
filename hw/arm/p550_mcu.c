/*
 * SiFive Premier P550 MCU model
 *
 * The Permier P550 is mATX system containing a RISC-V CPU with
 * a ST32F407 acting a board management contoller, called the 'MCU'
 * in SiFive's documetnation.
 *
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-clock.h"
#include "qemu/error-report.h"
#include "hw/arm/stm32f405_soc.h"
#include "hw/arm/boot.h"
#include "hw/arm/machines-qom.h"
#include "system/system.h"

static void p550_mcu_init(MachineState *machine)
{
    DeviceState *dev;
    Clock *sysclk;

    sysclk = clock_new(OBJECT(machine), "SYSCLK");
    clock_set_hz(sysclk, 168000000ULL);

    dev = qdev_new(TYPE_STM32F405_SOC);
    object_property_add_child(OBJECT(machine), "soc", OBJECT(dev));
    qdev_prop_set_chr(dev, "serial3", serial_hd(0));
    qdev_connect_clock_in(dev, "sysclk", sysclk);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    armv7m_load_kernel(STM32F405_SOC(dev)->armv7m.cpu,
                       machine->kernel_filename,
                       0, FLASH_SIZE);
}

static void p550_mcu_machine_init(MachineClass *mc)
{
    static const char * const valid_cpu_types[] = {
        ARM_CPU_TYPE_NAME("cortex-m4"),
        NULL
    };

    /* SRAM 128K */
    /* CCM 64K */
    /* Flash 512K */

    mc->desc = "P550 MCU (STM32F406)";
    mc->init = p550_mcu_init;
    mc->valid_cpu_types = valid_cpu_types;
}

DEFINE_MACHINE_ARM("p550-mcu", p550_mcu_machine_init)
