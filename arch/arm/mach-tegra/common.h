/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __MACH_TEGRA_COMMON_H
#define __MACH_TEGRA_COMMON_H

extern struct smp_operations tegra_smp_ops;

extern phys_addr_t tegra_tsec_start;
extern phys_addr_t tegra_tsec_size;

#ifdef CONFIG_TEGRA_USE_SECURE_KERNEL
extern unsigned long tegra_tzram_start;
extern unsigned long tegra_tzram_size;
#endif

#ifdef CONFIG_CACHE_L2X0
void tegra_init_cache(bool init);
#else
static inline void tegra_init_cache(bool init) {}
#endif

extern void tegra_cpu_die(unsigned int cpu);
extern int tegra_cpu_kill(unsigned int cpu);
extern phys_addr_t tegra_avp_kernel_start;
extern phys_addr_t tegra_avp_kernel_size;
void ahb_gizmo_writel(unsigned long val, void __iomem *reg);

extern struct device tegra_generic_cma_dev;
extern struct device tegra_vpr_cma_dev;

u32 tegra_get_sku_id(void);
u32 tegra_get_chip_id(void);
u32 tegra_get_bct_strapping(void);
void __init display_tegra_dt_info(void);

#endif
