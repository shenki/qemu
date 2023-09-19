/*
 * ASPEED SoC 2700 family
 *
 * Copyright (c) 2023, IBM Corporation.
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/misc/unimp.h"
#include "hw/arm/aspeed_soc.h"
#include "qemu/module.h"
#include "qemu/error-report.h"
#include "hw/i2c/aspeed_i2c.h"
#include "net/net.h"
#include "sysemu/sysemu.h"

#define ASPEED_SOC_IOMEM_SIZE       0x02c3e000

static const hwaddr aspeed_soc_ast2700_memmap[] = {
    [ASPEED_DEV_SRAM]    = 0x10000000, /* ECC SRAM Memory Buffer */
    [ASPEED_DEV_DPMCU]   = 0x11000000, /* DisplayPort Micro Controller Unit Memory */
    [ASPEED_DEV_AHB]     = 0x12000000, /* AHB Bus Controller */
    [ASPEED_DEV_USB3A]   = 0x12010000, /* USB Hub3A Controller */
    [ASPEED_DEV_USB1A]   = 0x12011000, /* USB PortA Hub1 Controller */
    [ASPEED_DEV_USB3B]   = 0x12020000, /* USB Hub3B Controller */
    [ASPEED_DEV_USB1B]   = 0x12021000, /* USB PortB Hub1 Controller */
    [ASPEED_DEV_XHCIA]   = 0x12030000, /* USB xHCI Host Controller A */
    [ASPEED_DEV_XHCIB]   = 0x12050000, /* USB xHCI Host Controller B */
    [ASPEED_DEV_HUB0A]   = 0x12060000, /* USB PortA Hub0 Controller */
    [ASPEED_DEV_ECHIA]   = 0x12061000, /* USB EHCI Host Controller A */
    [ASPEED_DEV_HUB0B]   = 0x12062000, /* USB PortB Hub0 Controller */
    [ASPEED_DEV_ECHIB]   = 0x12063000, /* USB EHCI Host Controller B */
    [ASPEED_DEV_HACE]    = 0x12070000, /* Hash & Crypto Engine (HACE) */
    [ASPEED_DEV_RSA]     = 0x12080000, /* RSA Engine (RSA) */
    [ASPEED_DEV_EMMC]    = 0x12090000, /* eMMC Controller */
    [ASPEED_DEV_VIDEO]   = 0x120A0000, /* Video Engine */
    [ASPEED_DEV_RVAS0]   = 0x120B8000, /* RVAS VGA Snoop #0 Controller */
    [ASPEED_DEV_RVAS1]   = 0x120BC000, /* RVAS VGA Snoop #1 Controller */
    [ASPEED_DEV_2D]      = 0x120D0000, /* 2D Graphics Engine */
    [ASPEED_DEV_H2X0]    = 0x120E0000, /* AHB to PCIe Bus Bridge (H2X0) */
    [ASPEED_DEV_IRQ]     = 0x12100000, /* Software Interrupt Controller */
    [ASPEED_DEV_PCI2BMC] = 0x12110000, /* PCIe Host to BMC Controller */
    [ASPEED_DEV_PRIV0]   = 0x12140000, /* Privilege Control #0 */
    [ASPEED_DEV_DRAMC]   = 0x12C00000, /* SDRAM Memory Controller (DRAMC) */
    [ASPEED_DEV_DRAB]    = 0x12C01000, /* SDRAM Memory Arbiter Controller (DARB) */
    [ASPEED_DEV_SCU0]    = 0x12C02000, /* System Control Unit #0 (SCU0) */
    [ASPEED_DEV_XDMA]    = 0x12C04000, /* X-DMA Controller (XDMA) */
    [ASPEED_DEV_MCTP]    = 0x12C06000, /* MCTP Controller (MCTP) */
    [ASPEED_DEV_UFS]     = 0x12C08000, /* UFS Controller (UFS) */
    [ASPEED_DEV_GFX]     = 0x12C09000, /* SOC Display Controller (GFX) */
    [ASPEED_DEV_DP]      = 0x12C0A000, /* DisplayPort Interface */
    [ASPEED_DEV_EMMCBOOT] = 0x12C0B000, /* eMMC Boot Controller */
    [ASPEED_DEV_MSI]     = 0x12C0C000, /* MSI Controller (MSI) */
    [ASPEED_DEV_SRAMREG] = 0x12C0E000, /* ECC SRAM Memory Register */
    [ASPEED_DEV_RTC]     = 0x12C0F000, /* Real Time Clock (RTC)  */
    [ASPEED_DEV_TIMER0]  = 0x12C10000, /* Timer Controller #0 */
    [ASPEED_DEV_TIMER1]  = 0x12C10040, /* Timer Controller #1 */
    [ASPEED_DEV_TIMER2]  = 0x12C10080, /* Timer Controller #2 */
    [ASPEED_DEV_TIMER3]  = 0x12C100C0, /* Timer Controller #3 */
    [ASPEED_DEV_TIMER4]  = 0x12C10100, /* Timer Controller #4 */
    [ASPEED_DEV_TIMER5]  = 0x12C10140, /* Timer Controller #5 */
    [ASPEED_DEV_TIMER6]  = 0x12C10180, /* Timer Controller #6 */
    [ASPEED_DEV_TIMER7]  = 0x12C101C0, /* Timer Controller #7 */
    [ASPEED_DEV_GPIO0]   = 0x12C11000, /* GPIO Controller #0 */
    [ASPEED_DEV_UARTDMA] = 0x12C12000, /* UART DMA #0 */
    [ASPEED_DEV_UART4]   = 0x12C13000, /* UART4 - debug */
    [ASPEED_DEV_APB2PCIRC] = 0x12C16000, /* APB to PCIe RC Bridge */
    [ASPEED_DEV_BRIDGE0] = 0x12C17000, /* Internal Bridge Controller #0 */
    [ASPEED_DEV_IPC0]    = 0x12C1C000, /* InterProcessor Controller #0 */
    [ASPEED_DEV_VGA0]    = 0x12C1D000, /* VGA Link Controller #0 */
    [ASPEED_DEV_ECDSA]   = 0x12C1E000, /* ECDSA Controller */
    [ASPEED_DEV_SEC]     = 0x12C1F000, /* Secure Extension Controller (SEC) */
    [ASPEED_DEV_JTAG]    = 0x12C20000, /* JTAG Master Controller #0 */
    [ASPEED_DEV_PCI2MBUS]= 0x12C21000, /* PCIe to MBus Bridge */
    [ASPEED_DEV_FMC]     = 0x14000000, /* Firmware SPI Memory Controller */
    [ASPEED_DEV_SPI0]    = 0x14010000, /* SPI0 Memory Controller */
    [ASPEED_DEV_SPI1]    = 0x14020000, /* SPI1 Memory Controller */
    [ASPEED_DEV_SPI2]    = 0x14030000, /* SPI2 Memory Controller */
    [ASPEED_DEV_MDIO]    = 0x14040000, /* Ethernet MDC/MDIO Bus Controller */
    [ASPEED_DEV_MAC0]    = 0x14050000, /* 10/100/1G Ethernet MAC Controller (MAC0) */
    [ASPEED_DEV_MAC1]    = 0x14060000, /* 10/100/1G Ethernet MAC Controller (MAC1) */
    [ASPEED_DEV_MAC2]    = 0x14070000, /* 10/100/1G Ethernet MAC Controller (MAC2) */
    [ASPEED_DEV_SD]      = 0x14080000, /* SD/SDIO Host Controller */
    [ASPEED_DEV_TACH]    = 0x140C0000, /* PWM & Fan Tacho Controller */
    [ASPEED_DEV_AHB2PCIRC2] = 0x140D0000, /* AHB to PCIe RC bridge controller #2 */
    [ASPEED_DEV_PCI2BMC2] = 0x140F0000, /* PCIe Host to BMC Controller #2 */
    [ASPEED_DEV_PRIV1]   = 0x14100000, /* Privilege Control #1 */
    [ASPEED_DEV_SRAM]    = 0x14BC0000, /* SRAM Memory Buffer */
    [ASPEED_DEV_ADC]     = 0x14C00000, /* ADC Controller */
    [ASPEED_DEV_SGMII]   = 0x14C01000, /* SGMII Controller */
    [ASPEED_DEV_SCU1]    = 0x14C02000, /* System Control Unit #1 (SCU1) */
    [ASPEED_DEV_BBSRAM]  = 0x14C04000, /* Battery Backed SRAM */
    [ASPEED_DEV_ESPI]    = 0x14C05000, /* eSPI Controller */
    [ASPEED_DEV_OTP]     = 0x14C07000, /* OTP Controller */
    [ASPEED_DEV_JTAG1]   = 0x14C09000, /* JTAG Master Controller #1 */
    [ASPEED_DEV_SRAMMC]  = 0x14C0A000, /* SRAM Memory Controller */
    [ASPEED_DEV_GPIO1]   = 0x14C0B000, /* GPIO Controller #1 */
    [ASPEED_DEV_SGPIO0]  = 0x14C0C000, /* SGPIO Master Controller #0 */
    [ASPEED_DEV_SGPIO1]  = 0x14C0D000, /* SGPIO Master Controller #1 */
    [ASPEED_DEV_SPGIOM0] = 0x14C0E000, /* SGPIO Monitor Controller #0 */
    [ASPEED_DEV_SGPIOM1] = 0x14C0E800, /* SGPIO Monitor Controller #1 */
    [ASPEED_DEV_I2C]     = 0x14C0F000, /* I2C/SMBus Controller */
    [ASPEED_DEV_UARTDMA1]= 0x14C12000, /* UART DMA #1 */
    [ASPEED_DEV_BMCUART] = 0x14C13000, /* BMC UART - debug */
    [ASPEED_DEV_SMBUSF]  = 0x14C14000, /* SMBus Filter Controller */
    [ASPEED_DEV_QSPIF0]  = 0x14C15000, /* QSPI Filter Controller #0 */
    [ASPEED_DEV_QSPIF1]  = 0x14C16000, /* QSPI Filter Controller #1 */
    [ASPEED_DEV_QSPIF2]  = 0x14C17000, /* QSPI Filter Controller #2 */
    [ASPEED_DEV_QSPIF3]  = 0x14C18000, /* QSPI Filter Controller #3 */
    [ASPEED_DEV_MCTP2]   = 0x14C1A000, /* MCTP Controller #2 (MCTP2) */
    [ASPEED_DEV_MSI2]    = 0x14C1B000, /* MSI Controller #2 (MSI2) */
    [ASPEED_DEV_APB2PCIRC2] = 0x14C1D800, /* APB to PCIe RC Bridge #2 */
    [ASPEED_DEV_BRIDGE1] = 0x14C1E000, /* Internal Bridge Controller #1 */
    [ASPEED_DEV_PECI]    = 0x14C1F000, /* PECI Controller */
    [ASPEED_DEV_I2C]     = 0x14C20000, /* I3C Controller */
    [ASPEED_DEV_VUART0]  = 0x14C30000, /* Virtual UART0 (VUART0) */
    [ASPEED_DEV_VUART1]  = 0x14C30100, /* Virtual UART1 (VUART1) */
    [ASPEED_DEV_VUART2]  = 0x14C30200, /* Virtual UART2 (VUART2) */
    [ASPEED_DEV_VUART3]  = 0x14C30300, /* Virtual UART3 (VUART3) */
    [ASPEED_DEV_LPC]     = 0x14C31000, /* LPC Controller */
    [ASPEED_DEV_UART0]   = 0x14C33000, /* UART Controller (UART0) */
    [ASPEED_DEV_UART1]   = 0x14C33100, /* UART Controller (UART1) */
    [ASPEED_DEV_UART2]   = 0x14C33200, /* UART Controller (UART2) */
    [ASPEED_DEV_UART3]   = 0x14C33300, /* UART Controller (UART3) */
    [ASPEED_DEV_UART5]   = 0x14C33400, /* UART Controller (UART5) */
    [ASPEED_DEV_UART6]   = 0x14C33500, /* UART Controller (UART6) */
    [ASPEED_DEV_UART7]   = 0x14C33600, /* UART Controller (UART7) */
    [ASPEED_DEV_UART8]   = 0x14C33700, /* UART Controller (UART8) */
    [ASPEED_DEV_UART9]   = 0x14C33800, /* UART Controller (UART9) */
    [ASPEED_DEV_UART10]  = 0x14C33900, /* UART Controller (UART10) */
    [ASPEED_DEV_UART11]  = 0x14C33A00, /* UART Controller (UART11) */
    [ASPEED_DEV_BUCUART] = 0x14C33B00, /* UART Controller (BMC UART) */
    [ASPEED_DEV_LTPI0]   = 0x14C34000, /* LTPI Controller #0 */
    [ASPEED_DEV_LTPI1]   = 0x14C35000, /* LTPI Controller #1 */
    [ASPEED_DEV_WDT]     = 0x14C37000, /* Watchdog Timer (WDT) */
    [ASPEED_DEV_EDAF]    = 0x14C38000, /* eDAF Bridge Controller */
    [ASPEED_DEV_IPC1]    = 0x14C39000, /* Inter-Processor Controller #1 */
    [ASPEED_DEV_VGA1]    = 0x14C3A000, /* VGA Link Controller #1 */
    [ASPEED_DEV_TRNG]    = 0x14C3B000, /* TRNG Controller  */
    [ASPEED_DEV_SPGIO]   = 0x14C3C000, /* SGPIO Slave Controller */
    [ASPEED_DEV_CANBUS]  = 0x14C3E000, /* CANBUS Controller */
    [ASPEED_DEV_FSIBUF]  = 0x20000000, /* FSI memory buffer */
    [ASPEED_DEV_FMCMAP]  = 0x100000000, /* BMC SPI Flash Memory */
    [ASPEED_DEV_SPI0MAP] = 0x180000000, /* SPI0 Flash Memory */
    [ASPEED_DEV_SPI1MAP] = 0x200000000, /* SPI1 Flash Memory */
    [ASPEED_DEV_SPI2MAP] = 0x280000000, /* SPI2 Flash Memory */
    [ASPEED_DEV_SDRAM]   = 0x400000000, /* SDRAM */
};

static const int aspeed_soc_ast2700_irqmap[] = {
};

static qemu_irq aspeed_soc_ast2700_get_irq(AspeedSoCState *s, int dev)
{
    AspeedSoCClass *sc = ASPEED_SOC_GET_CLASS(s);

    return qdev_get_gpio_in(DEVICE(&s->a7mpcore), sc->irqmap[dev]);
}

static void aspeed_soc_ast2700_init(Object *obj)
{
    AspeedSoCState *s = ASPEED_SOC(obj);
    AspeedSoCClass *sc = ASPEED_SOC_GET_CLASS(s);
    int i;
    char socname[8];
    char typename[64];

    if (sscanf(sc->name, "%7s", socname) != 1) {
        g_assert_not_reached();
    }

    for (i = 0; i < sc->num_cpus; i++) {
        object_initialize_child(obj, "cpu[*]", &s->cpu[i], sc->cpu_type);
    }

    object_initialize_child(obj, "iomem", &s->iomem, TYPE_UNIMPLEMENTED_DEVICE);
}


static void aspeed_soc_ast2700_realize(DeviceState *dev, Error **errp)
{
    int i;
    AspeedSoCState *s = ASPEED_SOC(dev);
    AspeedSoCClass *sc = ASPEED_SOC_GET_CLASS(s);

    /* IO space */
    aspeed_mmio_map_unimplemented(s, SYS_BUS_DEVICE(&s->iomem), "aspeed.io",
                                  sc->memmap[ASPEED_DEV_IOMEM],
                                  ASPEED_SOC_IOMEM_SIZE);
}

static void aspeed_soc_ast2700_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    AspeedSoCClass *sc = ASPEED_SOC_CLASS(oc);

    dc->realize      = aspeed_soc_ast2700_realize;

    sc->name         = "ast2700-a0";
    sc->cpu_type     = ARM_CPU_TYPE_NAME("cortex-a35");
    sc->silicon_rev  = AST2600_A3_SILICON_REV;
    sc->sram_size    = 0x16400;
    sc->spis_num     = 2;
    sc->ehcis_num    = 2;
    sc->wdts_num     = 4;
    sc->macs_num     = 4;
    sc->uarts_num    = 13;
    sc->irqmap       = aspeed_soc_ast2700_irqmap;
    sc->memmap       = aspeed_soc_ast2700_memmap;
    sc->num_cpus     = 2;
    sc->get_irq      = aspeed_soc_ast2700_get_irq;
}

static const TypeInfo aspeed_soc_ast2700_type_info = {
    .name           = "ast2700-a0",
    .parent         = TYPE_ASPEED_SOC,
    .instance_size  = sizeof(AspeedSoCState),
    .instance_init  = aspeed_soc_ast2700_init,
    .class_init     = aspeed_soc_ast2700_class_init,
    .class_size     = sizeof(AspeedSoCClass),
};

static void aspeed_soc_register_types(void)
{
    type_register_static(&aspeed_soc_ast2700_type_info);
};

type_init(aspeed_soc_register_types)
