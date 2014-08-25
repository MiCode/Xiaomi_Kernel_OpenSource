/* Copyright (c) 2010, 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/scm-boot.h>

/*
 * Set the cold/warm boot address for one of the CPU cores.
 */
int scm_set_boot_addr(phys_addr_t addr, unsigned int flags)
{
	struct {
		u32 flags;
		u32 addr;
	} cmd;

	cmd.addr = addr;
	cmd.flags = flags;
	return scm_call(SCM_SVC_BOOT, SCM_BOOT_ADDR,
			&cmd, sizeof(cmd), NULL, 0);
}
EXPORT_SYMBOL(scm_set_boot_addr);

/**
 *	scm_set_boot_addr_mc - Set entry physical address for cpus
 *	@addr:	32bit physical address
 *	@aff0:	Collective bitmask of the affinity-level-0 of the mpidr
 *		1<<aff0_CPU0| 1<<aff0_CPU1....... | 1<<aff0_CPU32
 *		Supports maximum 32 cpus under any affinity level.
 *	@aff1:	Collective bitmask of the affinity-level-1 of the mpidr
 *	@aff2:	Collective bitmask of the affinity-level-2 of the mpidr
 *	@flags:	Flag to differentiate between coldboot vs warmboot
 */
int scm_set_boot_addr_mc(phys_addr_t addr, u32 aff0,
		u32 aff1, u32 aff2, u32 flags)
{
	struct {
		u32 addr;
		u32 aff0;
		u32 aff1;
		u32 aff2;
		u32 reserved;
		u32 flags;
	} cmd;
	struct scm_desc desc = {0};

	if (!is_scm_armv8()) {
		cmd.addr = addr;
		cmd.aff0 = aff0;
		cmd.aff1 = aff1;
		cmd.aff2 = aff2;
		/*
		 * Reserved for future chips with affinity level 3 effectively
		 * 1 << 0
		 */
		cmd.reserved = ~0U;
		cmd.flags = flags | SCM_FLAG_HLOS;
		return scm_call(SCM_SVC_BOOT, SCM_BOOT_ADDR_MC,
				&cmd, sizeof(cmd), NULL, 0);
	}

	flags = flags | SCM_FLAG_HLOS;
	desc.args[0] = addr;
	desc.args[1] = aff0;
	desc.args[2] = aff1;
	desc.args[3] = aff2;
	desc.args[4] = ~0ULL;
	desc.args[5] = flags;
	desc.arginfo = SCM_ARGS(6);

	return scm_call2(SCM_SIP_FNID(SCM_SVC_BOOT, SCM_BOOT_ADDR_MC), &desc);
}
EXPORT_SYMBOL(scm_set_boot_addr_mc);

/**
 *	scm_set_warm_boot_addr_mc_for_all -
 *	Set entry physical address for __all__ possible cpus
 *	This API passes all_set mask to secure-os and relies
 *	on secure-os to appropriately
 *	set the boot-address on the current system.
 *	@addr:	32bit physical address
 */

int scm_set_warm_boot_addr_mc_for_all(phys_addr_t addr)
{
	return scm_set_boot_addr_mc(addr, ~0U, ~0U, ~0U,
			SCM_FLAG_WARMBOOT_MC);
}
EXPORT_SYMBOL(scm_set_warm_boot_addr_mc_for_all);

/**
 *	scm_is_mc_boot_available -
 *	Checks if TZ supports the boot API for multi-cluster configuration
 *	Returns true if available and false otherwise
 */
int scm_is_mc_boot_available(void)
{
	return scm_is_call_available(SCM_SVC_BOOT, SCM_BOOT_ADDR_MC);
}
EXPORT_SYMBOL(scm_is_mc_boot_available);
