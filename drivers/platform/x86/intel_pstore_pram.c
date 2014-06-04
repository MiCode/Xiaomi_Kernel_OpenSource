/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/acpi.h>
#include <linux/platform_device.h>
#include <linux/pstore_ram.h>

#define SZ_4K 0x00001000
#define SZ_2M 0x00200000

/* PRAM stands for 'Persisted RAM' from BIOS point of view */
#define ACPI_SIG_PRAM "PRAM"

/*
 * Following parameters match to those defined in fs/pstore/ram.c in
 * order to keep campatibility between driver intefaces, please refer
 * to it for implementaion details.
 */
static ulong pram_record_size = SZ_4K;
module_param_named(record_size, pram_record_size, ulong, 0400);
MODULE_PARM_DESC(record_size, "size of each dump done on oops/panic");

static ulong pram_console_size = SZ_2M;
module_param_named(console_size, pram_console_size, ulong, 0400);
MODULE_PARM_DESC(console_size, "size of kernel console log");

static ulong pram_ftrace_size = 2*SZ_4K;
module_param_named(ftrace_size, pram_ftrace_size, ulong, 0400);
MODULE_PARM_DESC(ftrace_size, "size of ftrace log");

static int pram_dump_oops = 1;
module_param_named(dump_oops, pram_dump_oops, int, 0600);
MODULE_PARM_DESC(dump_oops,
		 "set to 1 to dump oopses, 0 to only dump panics (default 1)");

static int pram_ecc;
module_param_named(ecc, pram_ecc, int, 0600);
MODULE_PARM_DESC(ecc,
		 "if non-zero, the option enables SW ECC support, provided by"
		 "fs/pstore/ram_core.c, and specifies ECC buffer size in bytes"
		 "(1 is a special value, means 16 bytes ECC)");

static struct ramoops_platform_data *pram_data;
static struct platform_device *pram_dev;

struct acpi_table_pram {
	struct acpi_table_header header;
	u64 addr;
	u32 size;
} __packed;

static int register_pram_dev(unsigned long mem_address,
			     unsigned long mem_size)
{
	pram_data = kzalloc(sizeof(*pram_data), GFP_KERNEL);
	if (!pram_data) {
		pr_err("could not allocate pram_data\n");
		return -ENOMEM;
	}

	pram_data->mem_address = mem_address;
	pram_data->mem_size = mem_size;
	pram_data->record_size = pram_record_size;
	pram_data->console_size = pram_console_size;
	pram_data->ftrace_size = pram_ftrace_size;
	pram_data->dump_oops = pram_dump_oops;
	/*
	 * For backwards compatibility with previous
	 * fs/pstore/ram_core.c implementation,
	 * intel_pstore_pram.ecc=1 means 16 bytes ECC.
	 */
	pram_data->ecc_info.ecc_size = pram_ecc == 1 ? 16 : pram_ecc;

	pram_dev = platform_device_register_data(NULL, "ramoops", -1,
			pram_data, sizeof(struct ramoops_platform_data));
	if (IS_ERR(pram_dev)) {
		pr_err("could not create platform device: %ld\n",
		       PTR_ERR(pram_dev));
		kfree(pram_data);
		return PTR_ERR(pram_dev);
	}

	pr_info("registered pram device, addr=0x%lx, size=0x%lx\n",
		pram_data->mem_address, pram_data->mem_size);

	return 0;
}

static int __init intel_pram_init(void)
{
	acpi_status status;
	struct acpi_table_pram *pramt;

	status = acpi_get_table(ACPI_SIG_PRAM, 0,
				(struct acpi_table_header **)&pramt);
	if (status == AE_NOT_FOUND) {
		pr_debug("PRAM table not found\n");
		return -ENODEV;
	} else if (ACPI_FAILURE(status)) {
		const char *msg = acpi_format_exception(status);
		pr_err("Failed to get PRAM table: %s\n", msg);
		return -EINVAL;
	}

	if (!pramt->addr || !pramt->size) {
		pr_debug("PRAM: bad address (0x%llx) or size (0x%lx)\n",
			 (unsigned long long)pramt->addr,
			 (unsigned long)pramt->size);
		return -ENODEV;
	}

	return register_pram_dev(pramt->addr, pramt->size);
}
device_initcall(intel_pram_init);

static void __exit intel_pram_exit(void)
{
	platform_device_unregister(pram_dev);
	kfree(pram_data);
}
module_exit(intel_pram_exit);
