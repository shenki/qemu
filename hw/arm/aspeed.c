/*
 * Copyright 2015 IBM Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 */

#include "hw/boards.h"
#include "hw/arm/arm.h"
#include "qemu/error-report.h"
#include "exec/address-spaces.h"

#define ASPEED_RAM_DEFAULT_SIZE (512 * 1024 * 1024)

/*
 * Memory map for emulated AST2400
 * 0x4000000 - 0x5fffffff	RAM
 *
 */

#define ASPEED_AST2400_NUM_UARTS 1

typedef struct Ast2400State {
    /*< private >*/
    DeviceState parent_obj;

    /*< public >*/
    ARMCPU         cpu;
// TODO: These don't exist yet
//    ASTAVICState   avic;
//    ASTSerialState uart[ASPEED_AST2400_NUM_UARTS];
} Ast2400State;

typedef struct AST2400BMC {
	Ast2400State	soc;
	MemoryRegion	ram;
} AST2400BMC;

static struct arm_boot_info aspeed_binfo = {
    .loader_start = 0x0,
    .board_id = 0x20e,
};

static void aspeed_init(MachineState *machine)
{
    const char *cpu_model = machine->cpu_model;
    const char *kernel_filename = machine->kernel_filename;
    const char *kernel_cmdline = machine->kernel_cmdline;
    const char *initrd_filename = machine->initrd_filename;
    ARMCPU *cpu;
    MemoryRegion *address_space_mem = get_system_memory();
    MemoryRegion *ram = g_new(MemoryRegion, 1);

    if (!cpu_model) {
        cpu_model = "arm926";
    }
    cpu = cpu_arm_init(cpu_model);
    if (!cpu) {
        fprintf(stderr, "Unable to find CPU definition\n");
        exit(1);
    }

    memory_region_allocate_system_memory(ram, NULL, "aspeed.ram",
                                         ASPEED_RAM_DEFAULT_SIZE);
    memory_region_add_subregion(address_space_mem, 0, ram);

    aspeed_binfo.ram_size = ASPEED_RAM_DEFAULT_SIZE;
    aspeed_binfo.kernel_filename = kernel_filename;
    aspeed_binfo.kernel_cmdline = kernel_cmdline;
    aspeed_binfo.initrd_filename = initrd_filename;
    arm_load_kernel(cpu, &aspeed_binfo);
}

static void aspeed_machine_init(MachineClass *mc)
{
    mc->desc = "Aspeed AST2400 BMC";
    mc->init = aspeed_init;
}

DEFINE_MACHINE("aspeed", aspeed_machine_init)
