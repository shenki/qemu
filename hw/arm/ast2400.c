/*
 * Aspeed 2400
 *
 * Copyright (c) 2015 Jeremy Kerr
 */

#include "hw/arm/arm.h"
#include "hw/boards.h"
#include "hw/char/serial.h"
#include "exec/address-spaces.h"

static struct arm_boot_info ast2400_binfo = {
    .loader_start = 0x40000000,
    .board_id = 0,
};

/*
 * IO handler: simply catch any reads/writes to IO addresses that aren't
 * handled by a device mapping.
 */
static uint64_t ast2400_io_read(void *p, hwaddr offset, unsigned size)
{
    printf("%s: 0x%lx[%d]\n", __func__, offset, size);
    return 0;
}

static void ast2400_io_write(void *opaque, hwaddr offset, uint64_t value,
		unsigned size)
{
    printf("%s: 0x%lx[%d] <- %lx\n", __func__, offset, size, value);
}

static const MemoryRegionOps ast2400_io_ops = {
    .read = ast2400_io_read,
    .write = ast2400_io_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void ast2400_init(MachineState *machine)
{
    MemoryRegion *ram, *address_space, *iomem;
    ram_addr_t ram_size;
    ARMCPU *cpu;

    if (!machine->cpu_model)
        machine->cpu_model = "arm926";

    cpu = cpu_arm_init(machine->cpu_model);
    if (!cpu) {
        fprintf(stderr, "Unable to find CPU definition\n");
        exit(1);
    }

    address_space = get_system_memory();
    ram_size = machine->ram_size;

    /* system RAM */
    ram = g_new(MemoryRegion, 1);
    memory_region_allocate_system_memory(ram, NULL, "ast2400.ram", ram_size);
    memory_region_add_subregion(address_space, 0x40000000, ram);

    /* io space */
    iomem = g_new(MemoryRegion, 1);
    memory_region_init_io(iomem, NULL, &ast2400_io_ops, NULL, "ast2400.io",
		    0x00200000);
    memory_region_add_subregion(address_space, 0x1e600000, iomem);

    /* attach an 8250 to the IO space, as our UART0 */
    if (serial_hds[0])
	    serial_mm_init(iomem, 0x184000, 2, 0, 38400, serial_hds[0],
			    DEVICE_NATIVE_ENDIAN);

    ast2400_binfo.kernel_filename = machine->kernel_filename;
    ast2400_binfo.initrd_filename = machine->initrd_filename;
    ast2400_binfo.kernel_cmdline = machine->kernel_cmdline;
    ast2400_binfo.ram_size = ram_size;
    arm_load_kernel(cpu, &ast2400_binfo);
}

static QEMUMachine ast2400_machine = {
    .name = "ast2400",
    .desc = "ASpeed ast2400 BMC (ARM926EJ-S)",
    .init = ast2400_init,
};

static void ast2400_machine_init(void)
{
    qemu_register_machine(&ast2400_machine);
}

machine_init(ast2400_machine_init);
