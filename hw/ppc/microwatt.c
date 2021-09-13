/*
 * QEMU PowerPC Microwatt
 *
 * Copyright (c) 2021, IBM Corporation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "qemu/datadir.h"
#include "qemu/units.h"
#include "qemu/cutils.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "elf.h"

#include "sysemu/device_tree.h"
#include "sysemu/sysemu.h"
#include "sysemu/reset.h"
#include "sysemu/runstate.h"

#include "hw/qdev-properties.h"
#include "hw/loader.h"
#include "hw/intc/intc.h"
#include "hw/misc/unimp.h"
#include "hw/irq.h"
#include "hw/ppc/ppc.h"
#include "hw/char/serial.h"
#include "hw/ppc/microwatt.h"

#include "trace.h"

#include <libfdt.h>

/*
 * Microwatt Sys config
 */

#define SYS_REG_SIGNATURE		0x00
#define SYS_REG_INFO			0x08
#define   SYS_REG_INFO_HAS_UART 		(1ull << 0)
#define   SYS_REG_INFO_HAS_DRAM 		(1ull << 1)
#define   SYS_REG_INFO_HAS_BRAM 		(1ull << 2)
#define   SYS_REG_INFO_HAS_SPI_FLASH 		(1ull << 3)
#define   SYS_REG_INFO_HAS_LITEETH 		(1ull << 4)
#define   SYS_REG_INFO_HAS_LARGE_SYSCON	        (1ull << 5)
#define   SYS_REG_INFO_HAS_UART1 		(1ull << 6)
#define   SYS_REG_INFO_HAS_ARTB                 (1ull << 7)
#define   SYS_REG_INFO_HAS_LITESDCARD 		(1ull << 8)
#define SYS_REG_BRAMINFO		0x10
#define   SYS_REG_BRAMINFO_SIZE_MASK		0xfffffffffffffull
#define SYS_REG_DRAMINFO		0x18
#define   SYS_REG_DRAMINFO_SIZE_MASK		0xfffffffffffffull
#define SYS_REG_CLKINFO			0x20
#define   SYS_REG_CLKINFO_FREQ_MASK		0xffffffffffull
#define SYS_REG_CTRL			0x28
#define   SYS_REG_CTRL_DRAM_AT_0		(1ull << 0)
#define   SYS_REG_CTRL_CORE_RESET		(1ull << 1)
#define   SYS_REG_CTRL_SOC_RESET		(1ull << 2)
#define SYS_REG_DRAMINITINFO		0x30
#define SYS_REG_SPI_INFO		0x38
#define   SYS_REG_SPI_INFO_FLASH_OFF_MASK	0xffffffff
#define SYS_REG_UART0_INFO		0x40
#define SYS_REG_UART1_INFO		0x48
#define   SYS_REG_UART_IS_16550			(1ull << 32)

static uint64_t mw_syscon_read(void *opaque, hwaddr addr, unsigned width)
{
    MwSysConState *s = MW_SYSCON(opaque);
    uint64_t val = -1;

    switch (addr) {
    case SYS_REG_SIGNATURE:
    case SYS_REG_INFO:
    case SYS_REG_BRAMINFO:
    case SYS_REG_DRAMINFO:
    case SYS_REG_CLKINFO:
    case SYS_REG_CTRL:
    case SYS_REG_DRAMINITINFO:
    case SYS_REG_SPI_INFO:
    case SYS_REG_UART0_INFO:
    case SYS_REG_UART1_INFO:
        val = s->regs[MW_SYSCON_REG(addr)];
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset 0x%" HWADDR_PRIx "\n",
                      __func__, addr);
        break;
    }

    trace_mw_syscon_read(addr, val);

    return val;
}

static void mw_syscon_write(void *opaque, hwaddr addr, uint64_t val,
                         unsigned width)
{
    trace_mw_syscon_write(addr, val);

    switch (addr) {
    case SYS_REG_CTRL:
        if (val & SYS_REG_CTRL_SOC_RESET) {
            qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
        }
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset 0x%" HWADDR_PRIx "\n",
                      __func__, addr);
        break;
    }
}

static const MemoryRegionOps mw_syscon_ops = {
    .read = mw_syscon_read,
    .write = mw_syscon_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 8,
        .max_access_size = 8,
    },
};

static void mw_syscon_reset(DeviceState *dev)
{
    MwSysConState *s = MW_SYSCON(dev);

    memset(s->regs, 0, sizeof s->regs);

    s->regs[MW_SYSCON_REG(SYS_REG_INFO)] =
        SYS_REG_INFO_HAS_UART |
        SYS_REG_INFO_HAS_DRAM |
        SYS_REG_INFO_HAS_LITEETH |
        SYS_REG_INFO_HAS_LARGE_SYSCON;
    s->regs[MW_SYSCON_REG(SYS_REG_SPI_INFO)] =
        MW_SOC_FLASH_BASE & SYS_REG_SPI_INFO_FLASH_OFF_MASK;
    s->regs[MW_SYSCON_REG(SYS_REG_UART0_INFO)] = SYS_REG_UART_IS_16550;
}

static void mw_syscon_realize(DeviceState *dev, Error **errp)
{
    MwSysConState *s = MW_SYSCON(dev);

    if (!s->soc) {
        error_setg(errp, TYPE_MW_SYSCON ": 'soc' link not set");
        return;
    }

    memory_region_init_io(&s->mmio, OBJECT(s), &mw_syscon_ops, s,
                          TYPE_MW_SYSCON, 0x100);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mmio);
}

static Property mw_syscon_properties[] = {
    DEFINE_PROP_LINK("soc", MwSysConState, soc, TYPE_MW_SOC, MwSoCState *),
    DEFINE_PROP_END_OF_LIST(),
};

static void mw_syscon_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->desc    = "MicroWatt Sys Config";
    dc->reset = mw_syscon_reset;
    dc->realize = mw_syscon_realize;
    dc->user_creatable = false;
    device_class_set_props(dc, mw_syscon_properties);
}

/*
 * Microwatt ICS sources
 */
static uint64_t mw_ics_read(void *opaque, hwaddr addr, unsigned width)
{
    MwICSState *s = MW_ICS(opaque);
    ICSState *ics = &s->ics;
    uint16_t srcno = addr >> 2;
    uint64_t val;

    val = (ics->irqs[srcno].server << 8) | ics->irqs[srcno].priority;

    trace_mw_ics_read(addr, val);
    return val;
}

static void mw_ics_write(void *opaque, hwaddr addr, uint64_t val,
                         unsigned width)
{
    MwICSState *s = MW_ICS(opaque);
    ICSState *ics = &s->ics;
    uint16_t srcno = addr >> 2;
    uint16_t server;
    uint8_t prio;

    trace_mw_ics_write(addr, val);

    server = (val >> 8) & 0xfff;
    prio   = val & 0xff;

    /* Update the underlying ICSState caching the configuration */
    ics_write_xive(ics, srcno, server, prio, prio);
}

static const MemoryRegionOps mw_ics_ops = {
    .read = mw_ics_read,
    .write = mw_ics_write,
    .endianness = DEVICE_BIG_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void mw_ics_set_irq(void *opaque, int srcno, int level)
{
    MwICSState *s = MW_ICS(opaque);

    ics_set_irq(&s->ics, srcno, level);
}

#define MW_ICS_IRQ_BASE    0x10
#define MW_ICS_NR_IRQS     0x10
#define MW_ICS_XIVE_OFFSET 0x800

static void mw_ics_realize(DeviceState *dev, Error **errp)
{
    MwICSState *s = MW_ICS(dev);
    ICSState *ics = &s->ics;
    int i;

    if (!qdev_realize(DEVICE(ics), NULL, errp)) {
        return;
    }

    for (i = 0; i < ics->nr_irqs; i++) {
        ics_set_irq_type(ics, i, false);
    }

    qdev_init_gpio_in(dev, mw_ics_set_irq, ics->nr_irqs);

    /* Global window */
    memory_region_init(&s->mmio, OBJECT(s), TYPE_MW_ICS,
                       MW_ICS_XIVE_OFFSET + ics->nr_irqs * 4);

    /* XIVE entries at 2K offset */
    memory_region_init_io(&s->xive_mmio, OBJECT(s), &mw_ics_ops, s,
                          TYPE_MW_ICS ".xive", ics->nr_irqs * 4);
    memory_region_add_subregion(&s->mmio, MW_ICS_XIVE_OFFSET,
                                &s->xive_mmio);

    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mmio);
}

static void mw_ics_instance_init(Object *obj)
{
    MwICSState *s = MW_ICS(obj);
    ICSState *ics = &s->ics;

    object_initialize_child(obj, "ics", &s->ics, TYPE_ICS);
    object_property_add_alias(obj, "nr-irqs", OBJECT(&s->ics), "nr-irqs");
    object_property_add_alias(obj, "xics", OBJECT(&s->ics), "xics");

    /* HW IRQ base number. QEMU should generate the DT */
    ics->offset = MW_ICS_IRQ_BASE;
}

static void mw_ics_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->desc    = "MicroWatt ICS";
    dc->realize = mw_ics_realize;
    dc->user_creatable = false;
}

/*
 * Microwatt CPU Presenters
 */
static void mw_icp_realize(DeviceState *dev, Error **errp)
{
    MwICPState *s = MW_ICP(dev);
    MwSoCState *soc = s->soc;
    MwSoCClass *msc;
    int i, j;

    if (!s->soc) {
        error_setg(errp, TYPE_MW_ICP ": 'soc' link not set");
        return;
    }

    msc = MW_SOC_GET_CLASS(soc);

    memory_region_init(&s->mmio, OBJECT(s), TYPE_MW_ICP, 0x100);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->mmio);

    for (i = 0; i < msc->num_cpus; i++) {
        MwCore *mw_core = &soc->cores[i];
        int core_hwid = CPU_CORE(mw_core)->core_id;

        for (j = 0; j < CPU_CORE(mw_core)->nr_threads; j++) {
            uint32_t pir = (core_hwid << 2) | j; /* P9 style */
            PnvICPState *icp = PNV_ICP(xics_icp_get(soc->xics, pir));

            memory_region_add_subregion(&s->mmio, pir << 12, &icp->mmio);
        }
    }
}

static Property mw_icp_properties[] = {
    DEFINE_PROP_LINK("soc", MwICPState, soc, TYPE_MW_SOC, MwSoCState *),
    DEFINE_PROP_END_OF_LIST(),
};

static void mw_icp_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->desc    = "MicroWatt ICP";
    dc->realize = mw_icp_realize;
    dc->user_creatable = false;
    device_class_set_props(dc, mw_icp_properties);
}

/*
 * Microwatt SoC
 */

static const hwaddr mw_soc_memmap[] = {
    [MW_DEV_IOMEM]  = MW_SOC_IOMEM_BASE,
    [MW_DEV_SYSCON] = MW_SOC_IOMEM_BASE + 0x0,
    [MW_DEV_UART0]  = MW_SOC_IOMEM_BASE + 0x2000,
    [MW_DEV_ICP]    = MW_SOC_IOMEM_BASE + 0x4000,
    [MW_DEV_ICS]    = MW_SOC_IOMEM_BASE + 0x5000,
    [MW_DEV_SPI]    = MW_SOC_IOMEM_BASE + 0x6000,
    [MW_DEV_DRAM]   = MW_SOC_IOMEM_BASE + 0x8000000,
    [MW_DEV_ETH]    = MW_SOC_IOMEM_BASE + 0x8021000,
};

static const int mw_soc_irqmap[] = {
    [MW_DEV_UART0]  = MW_ICS_IRQ_BASE + 0x0,
    [MW_DEV_ETH]    = MW_ICS_IRQ_BASE + 0x1,
};

static qemu_irq mw_soc_get_irq(MwSoCState *s, int ctrl)
{
    int hwirq = mw_soc_irqmap[ctrl];
    ICSState *ics = &s->ics.ics;

    assert(ics_valid_irq(ics, hwirq));
    return qdev_get_gpio_in(DEVICE(&s->ics), hwirq - ics->offset);
}

static void mw_soc_realize(DeviceState *dev, Error **errp)
{
    MwSoCState *s = MW_SOC(dev);
    MwSoCClass *msc = MW_SOC_GET_CLASS(s);
    int i;

    /* XICS fabric points to the machine. Move to the SoC ? */
    if (!s->xics) {
        error_setg(errp, TYPE_MW_SOC ": 'xics' link not set");
        return;
    }

    /* IO space */
    create_unimplemented_device(TYPE_MW_SOC ".io", mw_soc_memmap[MW_DEV_IOMEM],
                                MW_SOC_IOMEM_SIZE);

    /* CPU */
    for (i = 0; i < msc->num_cpus; i++) {
        object_property_set_link(OBJECT(&s->cores[i]), "soc", OBJECT(s),
                                 &error_abort);
        if (!qdev_realize(DEVICE(&s->cores[i]), NULL, errp)) {
            return;
        }
    }

    /* Sys config */
    object_property_set_link(OBJECT(&s->syscon), "soc", OBJECT(s), &error_abort);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->syscon), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->syscon), 0, mw_soc_memmap[MW_DEV_SYSCON]);

    /* CPU IRQ Presenters */
    object_property_set_link(OBJECT(&s->icp), "soc", OBJECT(s), &error_abort);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->icp), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->icp), 0, mw_soc_memmap[MW_DEV_ICP]);

    /* HW IRQ Sources */
    if (!object_property_set_int(OBJECT(&s->ics), "nr-irqs",
                                 MW_ICS_NR_IRQS, errp)) {
        return;
    }
    object_property_set_link(OBJECT(&s->ics), "xics", OBJECT(s->xics),
                             &error_abort);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->ics), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->ics), 0, mw_soc_memmap[MW_DEV_ICS]);

    /* UART */
    serial_mm_init(get_system_memory(), mw_soc_memmap[MW_DEV_UART0], 2,
                   mw_soc_get_irq(s, MW_DEV_UART0), 115200,
                   serial_hd(0), DEVICE_LITTLE_ENDIAN);

    /* Network */
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->eth), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->eth), 0, mw_soc_memmap[MW_DEV_ETH]);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->eth), 0,
                       mw_soc_get_irq(s, MW_DEV_ETH));
}

static void mw_soc_instance_init(Object *obj)
{
    MwSoCState *s = MW_SOC(obj);
    MwSoCClass *msc = MW_SOC_GET_CLASS(s);
    int i;

    for (i = 0; i < msc->num_cpus; i++) {
        object_initialize_child(obj, "cpu[*]", &s->cores[i], msc->cpu_type);
    }

    object_initialize_child(obj, "mw-syscon", &s->syscon, TYPE_MW_SYSCON);
    object_initialize_child(obj, "mw-ics", &s->ics, TYPE_MW_ICS);
    object_initialize_child(obj, "mw-icp", &s->icp, TYPE_MW_ICP);
    object_initialize_child(obj, "eth", &s->eth, TYPE_LITEETH);
}

static void mw_soc_power9_intc_create(MwSoCState *soc, PowerPCCPU *cpu,
                                      Error **errp)
{
    Error *local_err = NULL;
    Object *obj;
    MwCPUState *mw_cpu = mw_cpu_state(cpu);

    /*
     * TODO: replace/rename PNV_ICP -> XICS_NATIVE_ICP ? it's not
     * machine dependent.
     */
    obj = icp_create(OBJECT(cpu), TYPE_PNV_ICP, soc->xics, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return;
    }

    mw_cpu->intc = obj;
}

static void mw_soc_power9_intc_reset(MwSoCState *soc, PowerPCCPU *cpu)
{
    MwCPUState *mw_cpu = mw_cpu_state(cpu);

    icp_reset(ICP(mw_cpu->intc));
}

static void mw_soc_power9_intc_destroy(MwSoCState *soc, PowerPCCPU *cpu)
{
    MwCPUState *mw_cpu = mw_cpu_state(cpu);

    icp_destroy(ICP(mw_cpu->intc));
    mw_cpu->intc = NULL;
}

static void mw_soc_power9_intc_print_info(MwSoCState *soc, PowerPCCPU *cpu,
                                          Monitor *mon)
{
    icp_pic_print_info(ICP(mw_cpu_state(cpu)->intc), mon);
}

static void mw_soc_power9_class_init(ObjectClass *oc, void *data)
{
    MwSoCClass *sc = MW_SOC_CLASS(oc);

    sc->cpu_type = MW_CORE_TYPE_NAME("power9mw_v1.0");
    sc->num_cpus = 1;
    sc->intc_create = mw_soc_power9_intc_create;
    sc->intc_reset = mw_soc_power9_intc_reset;
    sc->intc_destroy = mw_soc_power9_intc_destroy;
    sc->intc_print_info = mw_soc_power9_intc_print_info;
}

static Property mw_soc_properties[] = {
    DEFINE_PROP_LINK("xics", MwSoCState, xics, TYPE_XICS_FABRIC, XICSFabric *),
    DEFINE_PROP_END_OF_LIST(),
};

static void mw_soc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = mw_soc_realize;
    dc->user_creatable = false;
    device_class_set_props(dc, mw_soc_properties);
}

/*
 * Microwatt machines
 */

#define TYPE_MW_MACHINE       MACHINE_TYPE_NAME("microwatt")
typedef struct MwMachineClass MwMachineClass;
typedef struct MwMachineState MwMachineState;
DECLARE_OBJ_CHECKERS(MwMachineState, MwMachineClass,
                     MW_MACHINE, TYPE_MW_MACHINE)

struct MwMachineState {
    /*< private >*/
    MachineState parent_obj;

    MemoryRegion ram_alias;
    MwSoCState   soc;
};

struct MwMachineClass {
    /*< private >*/
    MachineClass parent_class;

    const char   *soc_name;
};

static void mw_machine_reset(MachineState *machine)
{
    /* TODO: reload or generate the DTB instead */
    qemu_devices_reset();
}

static void mw_machine_init(MachineState *machine)
{
    MwMachineState *mw = MW_MACHINE(machine);
    MwMachineClass *mwc = MW_MACHINE_GET_CLASS(machine);
    MachineClass *mc = MACHINE_GET_CLASS(machine);
    hwaddr kernel_base;
    int kernel_size = 0;
    hwaddr initrd_base = 0;
    int initrd_size = 0;
    hwaddr dt_base = 0;
    hwaddr boot_entry = 0;
    void *fdt = NULL;
    int size;
    NICInfo *nd = &nd_table[0];

    if (machine->ram_size < mc->default_ram_size) {
        g_autofree char *sz = size_to_str(mc->default_ram_size);
        error_report("Invalid RAM size, should be bigger than %s", sz);
        exit(EXIT_FAILURE);
    }

    qemu_check_nic_model(nd, TYPE_LITEETH);

    /* RAM mapping and alias */
    memory_region_add_subregion(get_system_memory(), MW_SOC_DRAM_BASE,
                                machine->ram);
    memory_region_init_alias(&mw->ram_alias, NULL, "mw.ram_alias",
                             machine->ram, 0x0, machine->ram_size);
    memory_region_add_subregion(get_system_memory(), MW_SOC_MEMORY_BASE,
                                &mw->ram_alias);

    /* SoC */
    object_initialize_child(OBJECT(machine), "soc", &mw->soc, mwc->soc_name);

    object_property_set_link(OBJECT(&mw->soc), "xics", OBJECT(mw),
                             &error_abort);

    qdev_set_nic_properties(DEVICE(&mw->soc.eth), nd);

    sysbus_realize(SYS_BUS_DEVICE(&mw->soc), &error_abort);

    /* load kernel */
    if (machine->kernel_filename) {
        kernel_size = load_elf(machine->kernel_filename, NULL, NULL, NULL,
                               &boot_entry, &kernel_base, NULL, NULL,
                               0 /* LE */, PPC_ELF_MACHINE, 0, 0);
        if (kernel_size < 0) {
            error_report("Could not load kernel '%s' : %s",
                         machine->kernel_filename, load_elf_strerror(kernel_size));
            exit(1);
        }
    }

    /* load initrd */
    if (machine->initrd_filename) {
        initrd_base = QEMU_ALIGN_UP(kernel_base + kernel_size, 0x10000);
        initrd_size = load_image_targphys(machine->initrd_filename,
                                          initrd_base, 16 * MiB /* Some value */);
        if (initrd_size < 0) {
            error_report("Could not load initial ram disk '%s'",
                         machine->initrd_filename);
            exit(1);
        }
    }

    /* load dtb */
    if (machine->dtb) {
        char *filename;
        int ret;

        filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, machine->dtb);
        if (!filename) {
            error_report("Couldn't find dtb file '%s'", machine->dtb);
            exit(1);
        }

        fdt = load_device_tree(filename, &size);
        if (!fdt) {
            error_report("Couldn't load dtb file '%s'", filename);
            g_free(filename);
            exit(1);
        }
        g_free(filename);

        ret = qemu_fdt_setprop_string(fdt, "/chosen", "bootargs",
                                      machine->kernel_cmdline);
        if (ret < 0) {
            error_report("couldn't set /chosen/bootargs");
            exit(1);
        }

        if (initrd_size) {
            ret = qemu_fdt_setprop_cell(fdt, "/chosen", "linux,initrd-start",
                                        initrd_base);
            if (ret < 0) {
                error_report("Couldn't set /chosen/linux,initrd-start");
                exit(1);
            }

            ret = qemu_fdt_setprop_cell(fdt, "/chosen", "linux,initrd-end",
                                        (initrd_base + initrd_size));
            if (ret < 0) {
                error_report("Couldn't set /chosen/linux,initrd-end");
                exit(1);
            }
        }

        qemu_fdt_dumpdtb(fdt, fdt_totalsize(fdt));

        if (initrd_size) {
            dt_base = QEMU_ALIGN_UP(initrd_base + initrd_size, 0x10000);
        } else {
            dt_base = QEMU_ALIGN_UP(kernel_base + kernel_size, 0x10000);
        }

        cpu_physical_memory_write(dt_base, fdt, fdt_totalsize(fdt));

        g_free(fdt);
   }

    /* This is not the QOM way */
    mw->soc.boot_info.entry = boot_entry;
    mw->soc.boot_info.dt_base = dt_base;
}

static ICSState *mw_ics_get(XICSFabric *xi, int irq)
{
    MwMachineState *mw = MW_MACHINE(xi);
    ICSState *ics = &mw->soc.ics.ics;

    return ics_valid_irq(ics, irq) ? ics : NULL;
}

static void mw_ics_resend(XICSFabric *xi)
{
    MwMachineState *mw = MW_MACHINE(xi);

    ics_resend(&mw->soc.ics.ics);
}

static ICPState *mw_icp_get(XICSFabric *xi, int pir)
{
    PowerPCCPU *cpu = ppc_get_vcpu_by_pir(pir);

    return cpu ? ICP(mw_cpu_state(cpu)->intc) : NULL;
}

static void mw_pic_print_info(InterruptStatsProvider *obj,
                               Monitor *mon)
{
    MwMachineState *mw = MW_MACHINE(obj);
    CPUState *cs;

    CPU_FOREACH(cs) {
        PowerPCCPU *cpu = POWERPC_CPU(cs);

        /* XXX: loop on each chip/core/thread instead of CPU_FOREACH() */
        MW_SOC_GET_CLASS(&mw->soc)->intc_print_info(&mw->soc, cpu, mon);
    }
    ics_pic_print_info(&mw->soc.ics.ics, mon);
}

static void mw_machine_power9_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    XICSFabricClass *xic = XICS_FABRIC_CLASS(oc);
    MwMachineClass *mwc = MW_MACHINE_CLASS(oc);

    mc->desc = "Microwatt POWER9";
    mc->alias = "microwatt";

    mwc->soc_name = TYPE_MW_SOC "-power9";

    xic->icp_get = mw_icp_get;
    xic->ics_get = mw_ics_get;
    xic->ics_resend = mw_ics_resend;
}

static void mw_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    InterruptStatsProviderClass *ispc = INTERRUPT_STATS_PROVIDER_CLASS(oc);

    mc->desc = "Microwatt Generic";
    mc->init = mw_machine_init;
    mc->reset = mw_machine_reset;
    mc->no_floppy = 1;
    mc->no_cdrom = 1;
    mc->no_parallel = 1;
    mc->max_cpus = 1;
    mc->default_ram_size = 1 * GiB;
    mc->default_ram_id = "mw.ram";

    ispc->print_info = mw_pic_print_info;
}

static const TypeInfo types[] = {
    {
        .name          = MACHINE_TYPE_NAME("microwatt9"),
        .parent        = TYPE_MW_MACHINE,
        .class_init    = mw_machine_power9_class_init,
        .interfaces    = (InterfaceInfo[]) {
            { TYPE_XICS_FABRIC },
            { },
        },
    },
    {
        .name          = TYPE_MW_MACHINE,
        .parent        = TYPE_MACHINE,
        .abstract      = true,
        .instance_size = sizeof(MwMachineState),
        .class_init    = mw_machine_class_init,
        .class_size    = sizeof(MwMachineClass),
        .interfaces    = (InterfaceInfo[]) {
            { TYPE_INTERRUPT_STATS_PROVIDER },
            { },
        },
    },

    {
        .name          = TYPE_MW_SOC "-power9",
        .parent        = TYPE_MW_SOC,
        .class_init    = mw_soc_power9_class_init,
    },
    {
        .name          = TYPE_MW_SOC,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_init = mw_soc_instance_init,
        .instance_size = sizeof(MwSoCState),
        .class_init    = mw_soc_class_init,
        .class_size    = sizeof(MwSoCClass),
        .abstract       = true,
    },

    {
        .name          = TYPE_MW_ICP,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(MwICPState),
        .class_init    = mw_icp_class_init,
    },
    {
        .name          = TYPE_MW_ICS,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_init = mw_ics_instance_init,
        .instance_size = sizeof(MwICSState),
        .class_init    = mw_ics_class_init,
    },
    {
        .name          = TYPE_MW_SYSCON,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(MwSysConState),
        .class_init    = mw_syscon_class_init,
    },
};

DEFINE_TYPES(types)
