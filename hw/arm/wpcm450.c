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

/*
 * This covers the whole MMIO space. We'll use this to catch any MMIO accesses
 * that aren't handled by any device.
 */
#define WPCM450_MMIO_BA         0x80000000
#define WPCM450_MMIO_SZ         0x7ffd0000

enum {
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


static const int wpcm450_irqmap[] = {
    [WPCM_IRQ_WDT]     = 1,
    [WPCM_IRQ_GPIO0]   = 2,
    [WPCM_IRQ_GPIO1]   = 3,
    [WPCM_IRQ_GPIO2]   = 4,
    [WPCM_IRQ_GPIO3]   = 5,
    [WPCM_IRQ_PECI]    = 6,
    [WPCM_IRQ_UART0]   = 7,
    [WPCM_IRQ_UART1]   = 8,
    [WPCM_IRQ_KCS]     = 9,
    [WPCM_IRQ_FIU]     = 10,
    [WPCM_IRQ_SHM]     = 11,
    [WPCM_IRQ_TIMER0]  = 12,
    [WPCM_IRQ_TIMER1]  = 13,
    [WPCM_IRQ_TIMER]   = 14,
    [WPCM_IRQ_MAC1_RX] = 15,
    [WPCM_IRQ_MAC1_TX] = 16,
    [WPCM_IRQ_MAC2_RX] = 17,
    [WPCM_IRQ_MAC2_TX] = 18,
    [WPCM_IRQ_EHCI]    = 20,
    [WPCM_IRQ_UHCI]    = 21,
    [WPCM_IRQ_VCD]     = 22,
    [WPCM_IRQ_I2C3]    = 23,
    [WPCM_IRQ_TACH0]   = 24,
    [WPCM_IRQ_TACH1]   = 25,
    [WPCM_IRQ_I2C]     = 26,
    [WPCM_IRQ_I2C4]    = 27,
    [WPCM_IRQ_PWM]     = 28,
    [WPCM_IRQ_I2C5]    = 29,
    [WPCM_IRQ_ADC]     = 31
};

static const hwaddr wpcm450_memmap[] = {
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

static struct arm_boot_info wpcm_board_binfo = {
    .board_id = -1, /* device-tree-only board */
};

struct WPCMMachineState {
    MachineState parent_obj;

    ARMCPU      cpu;
    MemoryRegion *dram;

    NPCM7xxCLKState     clk;
    NPCM7xxTimerCtrlState timer;
};

#define TYPE_WPCM450_MACHINE MACHINE_TYPE_NAME("wpcm450")
#define WPCM450_MACHINE(obj)                                            \
    OBJECT_CHECK(WPCM450Machine, (obj), TYPE_WPCM450_MACHINE)

#define TYPE_WPCM450    "wpcm450"
#define WPCM450(obj)    OBJECT_CHECK(WPCM450State, (obj), TYPE_WPCM450)

static qemu_irq wpcm_irq(NPCM7xxState *s, int n)
{
    return qdev_get_gpio_in(DEVICE(&s->cpu), n);
}

static void wpcm_init(Object *obj)
{
    WPCM450MachineState *s = WPCM450(obj);

    object_initialize_child(obj, "cpu", &s->cpu, ARM_CPU_TYPE_NAME("arm926"));

    SysBusDevice *sbd = SYS_BUS_DEVICE(&s->timer[i]);
    int j;

    /* Connect the timer clock. */
    qdev_connect_clock_in(DEVICE(&s->timer[i]), "clock", qdev_get_clock_out(
                          DEVICE(&s->clk), "timer-clock"));

    sysbus_realize(sbd, &error_abort);
    sysbus_mmio_map(sbd, 0, wpcm450_memmap[WPCM_DEV_TIMER]);

    for (j = 0; j < NPCM7XX_TIMERS_PER_CTRL; j++) {
        qemu_irq irq = npcm7xx_irq(s, wpcm450_irqmap[WPCM_DEV_TIMER0 + j]);
        sysbus_connect_irq(sbd, j, irq);
    }

    /* IRQ for watchdogs */
    sysbus_connect_irq(sbd, NPCM7XX_TIMERS_PER_CTRL,
                       npcm7xx_irq(s, wpcm450_irqmap[NPCM7XX_DEV_WDT]));
#if 0
    /* GPIO that connects clk module with watchdog */
    qdev_connect_gpio_out_named(DEVICE(&s->timer),
            NPCM7XX_WATCHDOG_RESET_GPIO_IN, 0,
            qdev_get_gpio_in_named(DEVICE(&s->clk), NPCM7XX_WATCHDOG_RESET_GPIO_IN, 0));
#endif
}
