/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2015 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef _SH_CSS_FIRMWARE_H_
#define _SH_CSS_FIRMWARE_H_

#include <system_types.h>

#include <ia_css_err.h>
#include <ia_css_acc_types.h>

/* This is for the firmware loaded from user space */
struct  sh_css_fw_bi_file_h {
	char version[64];		/* branch tag + week day + time */
	int binary_nr;			/* Number of binaries */
	unsigned int h_size;		/* sizeof(struct sh_css_fw_bi_file_h) */
};

extern struct ia_css_fw_info     sh_css_sp_fw;
#if defined(HAS_SEC_SP)
extern struct ia_css_fw_info     sh_css_sp1_fw;
#endif /* HAS_SEC_SP */
extern struct ia_css_blob_descr *sh_css_blob_info;
extern unsigned			 sh_css_num_binaries;

char
*sh_css_get_fw_version(void);

bool
sh_css_check_firmware_version(const char *fw_data);

enum ia_css_err
sh_css_load_firmware(const char *fw_data,
		     unsigned int fw_size);

void sh_css_unload_firmware(void);

hrt_vaddress sh_css_load_blob(const unsigned char *blob, unsigned size);

enum ia_css_err
sh_css_load_blob_info(const char *fw, const struct ia_css_fw_info *bi, struct ia_css_blob_descr *bd, unsigned int i);

#endif /* _SH_CSS_FIRMWARE_H_ */
