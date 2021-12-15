/* Copyright (c) 2019, 2021, The Linux Foundation. All rights reserved.
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
#if !defined(_IPA_GCC_HWIO_DEF_H_)
#define _IPA_GCC_HWIO_DEF_H_

/* *****************************************************************************
 *
 * HWIO register definitions
 *
 * *****************************************************************************
 */
struct ipa_gcc_hwio_def_gcc_ipa_bcr_s {
	u32	blk_ares : 1;
	u32	reserved0 : 31;
};
union ipa_gcc_hwio_def_gcc_ipa_bcr_u {
	struct ipa_gcc_hwio_def_gcc_ipa_bcr_s	def;
	u32				value;
};
#endif
