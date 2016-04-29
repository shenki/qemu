/*
 * OpenCompute Facebook Yosemite BMC
 *
 * Teddy Reed <reed@fb.com>
 *
 * Copyright 2016-Present, Facebook, Inc.
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "cpu.h"
#include "exec/address-spaces.h"
#include "hw/arm/arm.h"
#include "hw/arm/ast2400.h"
#include "hw/boards.h"
#include "hw/block/flash.h"

#define AST2400_FLASH0_BASE 0x20000000
#define AST2400_FLASH0_SIZE 0x02000000 /* Max is 64k, but set 32kB */
#define AST2400_FLASH1_BASE 0x24000000
#define AST2400_FLASH1_SIZE 0x02000000
#define FBYOSEMITE_TEXT_BASE 0x0

static struct arm_boot_info fbyosemite_bmc_binfo = {
    .loader_start = FBYOSEMITE_TEXT_BASE,
    .board_id = 0,
    .nb_cpus = 1,
};

typedef struct FBYosemiteBMCState {
    AST2400State soc;
    MemoryRegion ram;
    MemoryRegion flash0_alias;
} FBYosemiteBMCState;

static pflash_t* pflash_register(hwaddr base, hwaddr size, const char* name,
                            DriveInfo *info) {
    int sector_len = 128 * 1024;
    int be = 0;
    pflash_t *pf;

    pf = pflash_cfi01_register(base, NULL, name, size,
                               info ? blk_by_legacy_dinfo(info) : NULL,
                               sector_len, size / sector_len,
                               2, 0, 0, 0, 0, be);
    if (pf == NULL) {
        fprintf(stderr, "qemu: Error registering flash memory.\n");
        exit(1);
    }
    return pf;
}

static void fbyosemite_bmc_init(MachineState *machine)
{
    FBYosemiteBMCState *bmc;
    DriveInfo *dinfo;
    pflash_t *pflash0;
    MemoryRegion *pflash0mem;

    bmc = g_new0(FBYosemiteBMCState, 1);
    object_initialize(&bmc->soc, (sizeof(bmc->soc)), TYPE_AST2400);
    object_property_add_child(OBJECT(machine), "soc", OBJECT(&bmc->soc),
                              &error_abort);

    memory_region_allocate_system_memory(&bmc->ram, NULL, "ram", ram_size);
    memory_region_add_subregion(get_system_memory(), AST2400_SDRAM_BASE,
                                &bmc->ram);
    object_property_add_const_link(OBJECT(&bmc->soc), "ram", OBJECT(&bmc->ram),
                                   &error_abort);
    object_property_set_bool(OBJECT(&bmc->soc), true, "realized",
                             &error_abort);

    /* Connect flash0 */
    dinfo = drive_get_next(IF_PFLASH);
    pflash0 = pflash_register(AST2400_FLASH0_BASE, AST2400_FLASH0_SIZE,
                              "fbyosemite.flash0", dinfo);

    /* Map flash0 to FBYOSEMITE_TEXT_BASE */
    pflash0mem = sysbus_mmio_get_region(SYS_BUS_DEVICE(pflash0), 0);
    memory_region_set_readonly(pflash0mem, true);
    memory_region_init_alias(&bmc->flash0_alias, NULL,
                             "flash0.alias", pflash0mem, FBYOSEMITE_TEXT_BASE,
                             AST2400_FLASH0_SIZE);
    memory_region_add_subregion(get_system_memory(), FBYOSEMITE_TEXT_BASE,
                                &bmc->flash0_alias);
    memory_region_set_readonly(&bmc->flash0_alias, true);

    /* Connect flash1 */
    dinfo = drive_get_next(IF_PFLASH);
    pflash_register(AST2400_FLASH1_BASE, AST2400_FLASH1_SIZE,
                   "fbyosemite.flash1", dinfo);

    fbyosemite_bmc_binfo.kernel_filename = machine->kernel_filename;
    fbyosemite_bmc_binfo.initrd_filename = machine->initrd_filename;
    fbyosemite_bmc_binfo.kernel_cmdline = machine->kernel_cmdline;
    fbyosemite_bmc_binfo.ram_size = ram_size;
    arm_load_kernel(ARM_CPU(first_cpu), &fbyosemite_bmc_binfo);
}

static void fbyosemite_bmc_machine_init(MachineClass *mc)
{
    mc->desc = "OpenCompute Facebook Yosemite BMC";
    mc->init = fbyosemite_bmc_init;
    mc->max_cpus = 1;
    mc->no_sdcard = 1;
    mc->no_floppy = 1;
    mc->no_cdrom = 1;
    mc->no_parallel = 1;
}

DEFINE_MACHINE("fbyosemite-bmc", fbyosemite_bmc_machine_init);
