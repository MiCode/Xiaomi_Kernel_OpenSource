/*
 * arch/arm/mach-tegra/tegra_simon_graders.h
 *
 * Copyright (c) 2014, NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MACH_TEGRA_SIMON_GRADERS_H_
#define _MACH_TEGRA_SIMON_GRADERS_H_

#if defined(CONFIG_ARCH_TEGRA_12x_SOC) && !defined(CONFIG_ARCH_TEGRA_13x_SOC)
int grade_gpu_simon_domain(int domain, int mv, int temperature);
int grade_cpu_simon_domain(int domain, int mv, int temperature);
#else
static inline int grade_gpu_simon_domain(int domain, int mv, int temperature)
{ return 0; }
static inline int grade_cpu_simon_domain(int domain, int mv, int temperature)
{ return 0; }
#endif

#endif
