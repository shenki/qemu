/*
 * Copyright 2021 Joel Stanley, IBM Corp.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "cpu.h"
#include "exec/address-spaces.h"
#include "hw/arm/boot.h"
#include "hw/boards.h"
#include "hw/qdev-properties.h"
#include "qemu/log.h"
#include "sysemu/block-backend.h"
#include "sysemu/sysemu.h"
#include "hw/loader.h"
#include "qemu/error-report.h"
#include "qemu/units.h"
#include "hw/qdev-clock.h"
#include "hw/char/serial.h"


#include "hw/arm/npcm7xx.h"


/*
 * This covers the whole MMIO space. We'll use this to catch any MMIO accesses
 * that aren't handled by any device.
 */
#define WPCM450_MMIO_BA         0x80000000
#define WPCM450_MMIO_SZ         0x7ffd0000

enum {
    WPCM_DEV_DRAM,
    WPCM_DEV_GCR,
    WPCM_DEV_MC,
    WPCM_DEV_MAC1,
    WPCM_DEV_MAC2,
    WPCM_DEV_USB2,
    WPCM_DEV_USB1,
    WPCM_DEV_SDHCI,
    WPCM_DEV_VCD,
    WPCM_DEV_UART0,
    WPCM_DEV_UART1,
    WPCM_DEV_PECI,
    WPCM_DEV_GFXI,
    WPCM_DEV_TIMER,
    WPCM_DEV_AIC,
    WPCM_DEV_GPIO,
    WPCM_DEV_TACH0,
    WPCM_DEV_TACH1,
    WPCM_DEV_I2C0,
    WPCM_DEV_I2C1,
    WPCM_DEV_I2C2,
    WPCM_DEV_I2C3,
    WPCM_DEV_I2C4,
    WPCM_DEV_I2C5,
    WPCM_DEV_PWM,
    WPCM_DEV_KCS,
    WPCM_DEV_ADC,
    WPCM_DEV_AES,
    WPCM_DEV_SRAM,
    WPCM_DEV_FIU
};

enum {
    WPCM_IRQ_WDT = 1,
    WPCM_IRQ_GPIO0,
    WPCM_IRQ_GPIO1,
    WPCM_IRQ_GPIO2,
    WPCM_IRQ_GPIO3,
    WPCM_IRQ_PECI,
    WPCM_IRQ_UART0,
    WPCM_IRQ_UART1,
    WPCM_IRQ_KCS,
    WPCM_IRQ_FIU,
    WPCM_IRQ_SHM,
    WPCM_IRQ_TIMER0,
    WPCM_IRQ_TIMER1,
    WPCM_IRQ_TIMER,
    WPCM_IRQ_MAC1_RX,
    WPCM_IRQ_MAC1_TX,
    WPCM_IRQ_MAC2_RX,
    WPCM_IRQ_MAC2_TX,
    WPCM_IRQ_EHCI,
    WPCM_IRQ_UHCI,
    WPCM_IRQ_VCD,
    WPCM_IRQ_I2C3,
    WPCM_IRQ_TACH0,
    WPCM_IRQ_TACH1,
    WPCM_IRQ_I2C,
    WPCM_IRQ_I2C4,
    WPCM_IRQ_PWM,
    WPCM_IRQ_I2C5,
    WPCM_IRQ_ADC = 31
};

static const hwaddr wpcm450_memmap[] = {
    [WPCM_DEV_DRAM]     = 0x00000000, /* Alias at 0x80000000 */
    [WPCM_DEV_GCR]      = 0xb0000000,
    [WPCM_DEV_MC]       = 0xb0001000,
    [WPCM_DEV_MAC1]     = 0xb0002000,
    [WPCM_DEV_MAC2]     = 0xb0003000,
    [WPCM_DEV_USB2]     = 0xb0005000,
    [WPCM_DEV_USB1]     = 0xb0006000,
    [WPCM_DEV_SDHCI]    = 0xb0007000,
    [WPCM_DEV_VCD]      = 0xb0018000,
    [WPCM_DEV_UART0]    = 0xb8000000,
    [WPCM_DEV_UART1]    = 0xb8000100,
    [WPCM_DEV_PECI]     = 0xb8000200,
    [WPCM_DEV_GFXI]     = 0xb8000300,
    [WPCM_DEV_TIMER]    = 0xb8001000,
    [WPCM_DEV_AIC]      = 0xb8002000,
    [WPCM_DEV_GPIO]     = 0xb8003000,
    [WPCM_DEV_TACH0]    = 0xb8004000,
    [WPCM_DEV_TACH1]    = 0xb8005000,
    [WPCM_DEV_I2C0]     = 0xb8006000,
    [WPCM_DEV_I2C1]     = 0xb8006100,
    [WPCM_DEV_I2C2]     = 0xb8006200,
    [WPCM_DEV_I2C3]     = 0xb8006300,
    [WPCM_DEV_I2C4]     = 0xb8006400,
    [WPCM_DEV_I2C5]     = 0xb8006500,
    [WPCM_DEV_PWM]      = 0xb8007000,
    [WPCM_DEV_KCS]      = 0xb8008000,
    [WPCM_DEV_ADC]      = 0xb8009000,
    [WPCM_DEV_AES]      = 0xb800b000,
    [WPCM_DEV_SRAM]     = 0xc6000000,
    [WPCM_DEV_FIU]      = 0xc8000000,
};

static struct arm_boot_info wpcm450_board_binfo = {
    .board_id = -1, /* device-tree-only board */
    .nb_cpus = 1,
    .loader_start = wpcm450_memmap[WPCM_DEV_DRAM],
};

typedef struct WPCM450MachineState {
    MachineState parent;

    ARMCPU      cpu;
    MemoryRegion *dram;

    NPCM7xxCLKState     clk;
    NPCM7xxTimerCtrlState timer;
} WPCM450MachineState;

#define TYPE_MICROBIT_MACHINE MACHINE_TYPE_NAME("microbit")

OBJECT_DECLARE_SIMPLE_TYPE(MicrobitMachineState, MICROBIT_MACHINE)


#define TYPE_WPCM450_MACHINE MACHINE_TYPE_NAME("wpcm450")
OBJECT_DECLARE_SIMPLE_TYPE(WPCM450MachineState, WPCM450_MACHINE)

static qemu_irq wpcm450_irq(WPCM450MachineState *s, int n)
{
    return qdev_get_gpio_in(DEVICE(&s->cpu), n);
}

static void wpcm450_init(MachineState *m)
{
    WPCM450MachineState *s = WPCM450_MACHINE(m);

    object_initialize_child(OBJECT(m), "cpu", &s->cpu, ARM_CPU_TYPE_NAME("arm926"));
    object_initialize_child(OBJECT(m), "timer", &s->timer, TYPE_NPCM7XX_TIMER);
    object_initialize_child(OBJECT(m), "clk", &s->clk, TYPE_NPCM7XX_CLK);

    SysBusDevice *sbd = SYS_BUS_DEVICE(&s->timer);
    sysbus_realize(sbd, &error_abort);
    sysbus_mmio_map(sbd, 0, wpcm450_memmap[WPCM_DEV_TIMER]);

    sysbus_connect_irq(sbd, 0, wpcm450_irq(s, WPCM_IRQ_TIMER0));
    sysbus_connect_irq(sbd, 1, wpcm450_irq(s, WPCM_IRQ_TIMER1));
    sysbus_connect_irq(sbd, 2, wpcm450_irq(s, WPCM_IRQ_TIMER));
    sysbus_connect_irq(sbd, 3, wpcm450_irq(s, WPCM_IRQ_TIMER));
    sysbus_connect_irq(sbd, 4, wpcm450_irq(s, WPCM_IRQ_TIMER));

    /* IRQ for watchdogs */
    sysbus_connect_irq(sbd, NPCM7XX_TIMERS_PER_CTRL, wpcm450_irq(s, WPCM_IRQ_WDT));

    /* UARTs (16550 compatible) */
    serial_mm_init(get_system_memory(), wpcm450_memmap[WPCM_DEV_UART0], 2,
            wpcm450_irq(s, WPCM_IRQ_UART0), 115200,
            serial_hd(0), DEVICE_LITTLE_ENDIAN);

    serial_mm_init(get_system_memory(), wpcm450_memmap[WPCM_DEV_UART1], 2,
            wpcm450_irq(s, WPCM_IRQ_UART1), 115200,
            serial_hd(1), DEVICE_LITTLE_ENDIAN);


    arm_load_kernel(ARM_CPU(first_cpu), m, &wpcm450_board_binfo);
}

static void wpcm450_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "WPCM450 (arm926)";
    mc->init = wpcm450_init;
    mc->max_cpus = 1;
}

static const TypeInfo wpcm450_info = {
    .name = TYPE_WPCM450_MACHINE,
    .parent = TYPE_MACHINE,
    .instance_size = sizeof(WPCM450MachineState),
    .class_init = wpcm450_machine_class_init,
};

static void wpcm450_machine_init(void)
{
    type_register_static(&wpcm450_info);
}
type_init(wpcm450_machine_init);
