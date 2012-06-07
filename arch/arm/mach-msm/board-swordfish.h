/* arch/arm/mach-msm/board-swordfish.h
 *
 * Copyright (C) 2009 Google Inc.
 * Author: Dima Zavin <dima@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

#ifndef __ARCH_ARM_MACH_MSM_BOARD_SWORDFISH_H
#define __ARCH_ARM_MACH_MSM_BOARD_SWORDFISH_H

#include <mach/board.h>

#define MSM_SMI_BASE		0x02B00000
#define MSM_SMI_SIZE		0x01500000

#define MSM_PMEM_MDP_BASE	0x03000000
#define MSM_PMEM_MDP_SIZE	0x01000000

#define MSM_EBI1_BASE		0x20000000
#define MSM_EBI1_SIZE		0x0E000000

#define MSM_PMEM_ADSP_BASE      0x2A300000
#define MSM_PMEM_ADSP_SIZE      0x02000000

#define MSM_PMEM_GPU1_BASE	0x2C300000
#define MSM_PMEM_GPU1_SIZE	0x01400000

#define MSM_PMEM_GPU0_BASE	0x2D700000
#define MSM_PMEM_GPU0_SIZE	0x00400000

#define MSM_GPU_MEM_BASE	0x2DB00000
#define MSM_GPU_MEM_SIZE	0x00200000

#define MSM_RAM_CONSOLE_BASE	0x2DD00000
#define MSM_RAM_CONSOLE_SIZE	0x00040000

#define MSM_FB_BASE		0x2DE00000
#define MSM_FB_SIZE		0x00200000

#endif /* __ARCH_ARM_MACH_MSM_BOARD_SWORDFISH_H */
