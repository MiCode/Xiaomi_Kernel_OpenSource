/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef __SSMR_INTERNAL_H__
#define __SSMR_INTERNAL_H__

#if defined(CONFIG_TRUSTONIC_TRUSTED_UI) ||\
	defined(CONFIG_BLOWFISH_TUI_SUPPORT) ||\
	defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
#define SSMR_TUI_REGION_ENABLE
#else
#undef SSMR_TUI_REGION_ENABLE
#endif

#include "memory_ssmr.h"

#define SVP_REGION_IOC_MAGIC		'S'

#define SVP_REGION_ACQUIRE	_IOR(SVP_REGION_IOC_MAGIC, 6, int)
#define SVP_REGION_RELEASE	_IOR(SVP_REGION_IOC_MAGIC, 8, int)

#define UPPER_LIMIT32 (1ULL << 32)
#define UPPER_LIMIT64 (1ULL << 63)

#define NAME_LEN 32
#define CMD_LEN  64

/* define scenario type */
#define SVP_FLAGS		0x01u
#define FACE_REGISTRATION_FLAGS 0x02u
#define FACE_PAYMENT_FLAGS	0x04u
#define FACE_UNLOCK_FLAGS	0x08u
#define TUI_FLAGS		0x10u

#define SSMR_INVALID_FEATURE(f) (f >= __MAX_NR_SSMR_FEATURES)

/*
 *  req_size :         feature request size
 *  proc_entry_fops :  file operation fun pointer
 *  state :            region online/offline state
 *  count :            region max alloc size by feature
 *  alloc_pages :      current feature offline alloc size
 *  is_unmapping :     unmapping state
 *  use_cache_memory : when use reserved memory it will be true
 *  page :             zmc alloc page
 *  cache_page :       alloc page by reserved memory
 *  usable_size :      cma usage size
 *  scheme_flag :      show feaure support which schemes
 */
struct SSMR_Feature {
	char dt_prop_name[NAME_LEN];
	char feat_name[NAME_LEN];
	char cmd_online[CMD_LEN];
	char cmd_offline[CMD_LEN];
	bool is_unmapping;
	bool use_cache_memory;
	struct page *page;
	struct page *cache_page;
	u64 req_size;
	unsigned int scheme_flag;
	unsigned int state;
	unsigned long alloc_pages;
	unsigned long count;
	const struct file_operations *proc_entry_fops;
};

enum ssmr_state {
	SSMR_STATE_DISABLED,
	SSMR_STATE_ONING_WAIT,
	SSMR_STATE_ONING,
	SSMR_STATE_ON,
	SSMR_STATE_OFFING,
	SSMR_STATE_OFF,
	NR_STATES,
};

const char *const ssmr_state_text[NR_STATES] = {
	[SSMR_STATE_DISABLED]   = "[DISABLED]",
	[SSMR_STATE_ONING_WAIT] = "[ONING_WAIT]",
	[SSMR_STATE_ONING]      = "[ONING]",
	[SSMR_STATE_ON]         = "[ON]",
	[SSMR_STATE_OFFING]     = "[OFFING]",
	[SSMR_STATE_OFF]        = "[OFF]",
};

enum ssmr_scheme_state {
	SSMR_SVP,
	SSMR_FACE_REGISTRATION,
	SSMR_FACE_PAYMENT,
	SSMR_FACE_UNLOCK,
	SSMR_TUI_SCHEME,
	__MAX_NR_SCHEME,
};

struct SSMR_Scheme {
	char name[NAME_LEN];
	u64  usable_size;
	unsigned int flags;
};

static struct SSMR_Scheme _ssmrscheme[__MAX_NR_SCHEME] = {
	[SSMR_SVP] = {
		.name = "svp_scheme",
		.flags = SVP_FLAGS
	},
	[SSMR_FACE_REGISTRATION] = {
		.name = "face_registration_scheme",
		.flags = FACE_REGISTRATION_FLAGS
	},
	[SSMR_FACE_PAYMENT] = {
		.name = "face_payment_scheme",
		.flags = FACE_PAYMENT_FLAGS
	},
	[SSMR_FACE_UNLOCK] = {
		.name = "face_unlock_scheme",
		.flags = FACE_UNLOCK_FLAGS
	},
	[SSMR_TUI_SCHEME] = {
		.name = "tui_scheme",
		.flags = TUI_FLAGS
	}
};

static struct SSMR_Feature _ssmr_feats[__MAX_NR_SSMR_FEATURES] = {
#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) ||\
	defined(CONFIG_MTK_TEE_GP_SUPPORT)
	[SSMR_FEAT_SVP] = {
		.dt_prop_name = "svp-size",
		.feat_name = "svp",
		.cmd_online = "svp=on",
		.cmd_offline = "svp=off",
		.scheme_flag = SVP_FLAGS
	},
#endif
#ifdef CONFIG_MTK_CAM_SECURITY_SUPPORT
	[SSMR_FEAT_2D_FR] = {
		.dt_prop_name = "2d_fr-size",
		.feat_name = "2d_fr",
		.cmd_online = "2d_fr=on",
		.cmd_offline = "2d_fr=off",
		.scheme_flag = FACE_REGISTRATION_FLAGS | FACE_PAYMENT_FLAGS |
				FACE_UNLOCK_FLAGS
	},
#endif
#if defined(CONFIG_TRUSTONIC_TRUSTED_UI) ||\
	defined(CONFIG_BLOWFISH_TUI_SUPPORT)
	[SSMR_FEAT_TUI] = {
		.dt_prop_name = "tui-size",
		.feat_name = "tui",
		.cmd_online = "tui=on",
		.cmd_offline = "tui=off",
		.scheme_flag = TUI_FLAGS
	},
#endif
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	[SSMR_FEAT_WFD] = {
		.dt_prop_name = "wfd-size",
		.feat_name = "wfd",
		.cmd_online = "wfd=on",
		.cmd_offline = "wfd=off",
		.scheme_flag = SVP_FLAGS
	},
#endif
#ifdef CONFIG_MTK_PROT_MEM_SUPPORT
	[SSMR_FEAT_PROT_SHAREDMEM] = {
		.dt_prop_name = "prot-sharedmem-size",
		.feat_name = "prot-sharedmem",
		.cmd_online = "prot_sharedmem=on",
		.cmd_offline = "prot_sharedmem=off",
		.scheme_flag = FACE_REGISTRATION_FLAGS | FACE_PAYMENT_FLAGS |
				FACE_UNLOCK_FLAGS
	},
#endif
#ifdef CONFIG_MTK_HAPP_MEM_SUPPORT
	[SSMR_FEAT_TA_ELF] = {
		.dt_prop_name = "ta-elf-size",
		.feat_name = "ta-elf",
		.cmd_online = "ta_elf=on",
		.cmd_offline = "ta_elf=off",
		.scheme_flag = FACE_REGISTRATION_FLAGS | FACE_PAYMENT_FLAGS |
				FACE_UNLOCK_FLAGS
	},
	[SSMR_FEAT_TA_STACK_HEAP] = {
		.dt_prop_name = "ta-stack-heap-size",
		.feat_name = "ta-stack-heap",
		.cmd_online = "ta_stack_heap=on",
		.cmd_offline = "ta_stack_heap=off",
		.scheme_flag = FACE_REGISTRATION_FLAGS | FACE_PAYMENT_FLAGS |
				FACE_UNLOCK_FLAGS
	},
#endif
#ifdef CONFIG_MTK_SDSP_SHARED_MEM_SUPPORT
	[SSMR_FEAT_SDSP_TEE_SHAREDMEM] = {
		.dt_prop_name = "sdsp-tee-sharedmem-size",
		.feat_name = "sdsp-tee-sharedmem",
		.cmd_online = "sdsp_tee_sharedmem=on",
		.cmd_offline = "sdsp_tee_sharedmem=off",
		.scheme_flag = FACE_REGISTRATION_FLAGS | FACE_PAYMENT_FLAGS |
				FACE_UNLOCK_FLAGS
	},
#endif
#ifdef CONFIG_MTK_SDSP_MEM_SUPPORT
	[SSMR_FEAT_SDSP_FIRMWARE] = {
		.dt_prop_name = "sdsp-firmware-size",
		.feat_name = "sdsp-firmware",
		.cmd_online = "sdsp_firmware=on",
		.cmd_offline = "sdsp_firmware=off",
		.scheme_flag = FACE_UNLOCK_FLAGS
	}
#endif
};

extern void show_pte(struct mm_struct *mm, unsigned long addr);

#ifdef CONFIG_MTK_ION
extern void ion_sec_heap_dump_info(void);
#else
static inline void ion_sec_heap_dump_info(void)
{
	pr_info("%s is not supported\n", __func__);
}
#endif

#endif
