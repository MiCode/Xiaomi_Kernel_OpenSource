/*
 * arch/arm/mach-tegra/mcerr.h
 *
 * MC error interrupt handling header file. Various defines and declarations
 * across T20, T30, and T11x.
 *
 * Copyright (c) 2010-2012, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __MCERR_H
#define __MCERR_H

#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>

/* Pull in chip specific EMC header. */
#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
#include "tegra3_emc.h"
#define MC_LATENCY_ALLOWANCE_BASE	MC_LATENCY_ALLOWANCE_AFI
#elif defined(CONFIG_ARCH_TEGRA_11x_SOC)
#include "tegra11_emc.h"
#define MC_LATENCY_ALLOWANCE_BASE	MC_LATENCY_ALLOWANCE_AVPC_0
#endif

#define MAX_PRINTS			6

#define MC_INT_STATUS			0x0
#define MC_INT_MASK			0x4
#define MC_ERROR_STATUS			0x8
#define MC_ERROR_ADDRESS		0xC
#define MC_ERR_VPR_STATUS		0x654
#define MC_ERR_VPR_ADR			0x658
#define MC_ERR_SEC_STATUS		0x67c
#define MC_ERR_SEC_ADR			0x680

#define MC_INT_EXT_INTR_IN		(1<<1)
#define MC_INT_DECERR_EMEM		(1<<6)
#define MC_INT_SECURITY_VIOLATION	(1<<8)
#define MC_INT_ARBITRATION_EMEM		(1<<9)
#define MC_INT_INVALID_SMMU_PAGE	(1<<10)
#define MC_INT_DECERR_VPR		(1<<12)
#define MC_INT_SECERR_SEC		(1<<13)

/*
 * Number of unique interrupts we have for this chip.
 */
#ifdef CONFIG_ARCH_TEGRA_11x_SOC
#define INTR_COUNT	6
#else
#define INTR_COUNT	4
#endif

#define MC_ERR_DECERR_EMEM		(2)
#define MC_ERR_SECURITY_TRUSTZONE	(3)
#define MC_ERR_SECURITY_CARVEOUT	(4)
#define MC_ERR_INVALID_SMMU_PAGE	(6)

#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
#define _MC_INT_EN_MASK	(MC_INT_DECERR_EMEM |		\
			 MC_INT_SECURITY_VIOLATION |	\
			 MC_INT_INVALID_SMMU_PAGE)


#elif defined(CONFIG_ARCH_TEGRA_11x_SOC)
#define MC_DUAL_CHANNEL
#define _MC_INT_EN_MASK	(MC_INT_EXT_INTR_IN |		\
			 MC_INT_DECERR_EMEM |		\
			 MC_INT_SECURITY_VIOLATION |	\
			 MC_INT_INVALID_SMMU_PAGE |	\
			 MC_INT_DECERR_VPR |		\
			 MC_INT_SECERR_SEC)
#endif

#ifdef CONFIG_TEGRA_ARBITRATION_EMEM_INTR
#define MC_INT_EN_MASK	(_MC_INT_EN_MASK | MC_INT_ARBITRATION_EMEM)
)
#else
#define MC_INT_EN_MASK	(_MC_INT_EN_MASK)
#endif

extern void __iomem *mc;

struct mc_client {
	const char *name;
	unsigned int intr_counts[INTR_COUNT];
};

struct mcerr_chip_specific {

	const char	*(*mcerr_info)(u32 status);
	void		 (*mcerr_info_update)(struct mc_client *c, u32 status);
	const char	*(*mcerr_type)(u32 err);
	void		 (*mcerr_print)(const char *mc_err, u32 err, u32 addr,
					const struct mc_client *client,
					int is_secure, int is_write,
					const char *mc_err_info);
	int		 (*mcerr_debugfs_show)(struct seq_file *s, void *v);

	/* Numeric fields that must be set by the different architectures. */
	unsigned int	 nr_clients;
};

#define client(_name) { .name = _name }

/*
 * Error MMA tracking.
 */
#define MMA_HISTORY_SAMPLES 20
struct arb_emem_intr_info {
	int arb_intr_mma;
	u64 time;
	spinlock_t lock;
};

/*
 * Externs that get defined by the chip specific code. This way the generic
 * T3x/T11x can handle a much as possible.
 */
extern struct mc_client mc_clients[];
extern void mcerr_chip_specific_setup(struct mcerr_chip_specific *spec);

#endif /* __MCERR_H */
