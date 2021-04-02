/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __SSMR_INTERNAL_H__
#define __SSMR_INTERNAL_H__

#include "memory_ssmr.h"

#define SVP_REGION_IOC_MAGIC 'S'

#define SVP_REGION_ACQUIRE _IOR(SVP_REGION_IOC_MAGIC, 6, int)
#define SVP_REGION_RELEASE _IOR(SVP_REGION_IOC_MAGIC, 8, int)

#define UPPER_LIMIT32 (1ULL << 32)
#define UPPER_LIMIT64 (1ULL << 63)

#define NAME_LEN 32
#define CMD_LEN 64

/* define scenario type */
#define SVP_FLAGS 0x01u
#define FACE_REGISTRATION_FLAGS 0x02u
#define FACE_PAYMENT_FLAGS 0x04u
#define FACE_UNLOCK_FLAGS 0x08u
#define TUI_FLAGS 0x10u

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
 *  enable :           show feature status
 */
struct SSMR_Feature {
	bool is_unmapping;
	bool use_cache_memory;
	char dt_prop_name[NAME_LEN];
	char feat_name[NAME_LEN];
	char cmd_online[CMD_LEN];
	char cmd_offline[CMD_LEN];
	char enable[CMD_LEN];
	const struct file_operations *proc_entry_fops;
	dma_addr_t phy_addr;
	size_t alloc_size;
	struct page *page;
	struct page *cache_page;
	unsigned int scheme_flag;
	unsigned int state;
	unsigned long alloc_pages;
	unsigned long count;
	u64 req_size;
	void *virt_addr;
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

enum ssmr_scheme_state {
	SSMR_SVP,
	SSMR_FACE_REGISTRATION,
	SSMR_FACE_PAYMENT,
	SSMR_FACE_UNLOCK,
	SSMR_TUI_SCHEME,
	__MAX_NR_SCHEME,
};

/* clang-format off */
const char *const ssmr_state_text[NR_STATES] = {
	[SSMR_STATE_DISABLED]   = "[DISABLED]",
	[SSMR_STATE_ONING_WAIT] = "[ONING_WAIT]",
	[SSMR_STATE_ONING]      = "[ONING]",
	[SSMR_STATE_ON]         = "[ON]",
	[SSMR_STATE_OFFING]     = "[OFFING]",
	[SSMR_STATE_OFF]        = "[OFF]",
};

struct page_change_data {
	pgprot_t set_mask;
	pgprot_t clear_mask;
};
/* clang-format on */

struct SSMR_Scheme {
	char name[NAME_LEN];
	u64 usable_size;
	unsigned int flags;
};

/* clang-format off */
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
	[SSMR_FEAT_SVP] = {
		.dt_prop_name = "svp-size",
		.feat_name = "svp",
		.cmd_online = "svp=on",
		.cmd_offline = "svp=off",
#if IS_ENABLED(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) || \
	IS_ENABLED(CONFIG_TRUSTONIC_TEE_SUPPORT) || \
	IS_ENABLED(CONFIG_MICROTRUST_TEE_SUPPORT) || \
	IS_ENABLED(CONFIG_MTK_SVP_ON_MTEE_SUPPORT)
		.enable = "on",
#else
		.enable = "off",
#endif
		.scheme_flag = SVP_FLAGS
	},
	[SSMR_FEAT_PROT_SHAREDMEM] = {
		.dt_prop_name = "prot-sharedmem-size",
		.feat_name = "prot-sharedmem",
		.cmd_online = "prot_sharedmem=on",
		.cmd_offline = "prot_sharedmem=off",
#if IS_ENABLED(CONFIG_MTK_PROT_MEM_SUPPORT)
		.enable = "on",
#else
		.enable = "off",
#endif
		.scheme_flag = FACE_REGISTRATION_FLAGS | FACE_PAYMENT_FLAGS |
				FACE_UNLOCK_FLAGS | SVP_FLAGS
	},
	[SSMR_FEAT_WFD] = {
		.dt_prop_name = "wfd-size",
		.feat_name = "wfd",
		.cmd_online = "wfd=on",
		.cmd_offline = "wfd=off",
#if IS_ENABLED(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
		.enable = "on",
#else
		.enable = "off",
#endif
		.scheme_flag = SVP_FLAGS
	},
	[SSMR_FEAT_TA_ELF] = {
		.dt_prop_name = "ta-elf-size",
		.feat_name = "ta-elf",
		.cmd_online = "ta_elf=on",
		.cmd_offline = "ta_elf=off",
#if IS_ENABLED(CONFIG_MTK_HAPP_MEM_SUPPORT)
		.enable = "on",
#else
		.enable = "off",
#endif
		.scheme_flag = FACE_REGISTRATION_FLAGS | FACE_PAYMENT_FLAGS |
				FACE_UNLOCK_FLAGS
	},
	[SSMR_FEAT_TA_STACK_HEAP] = {
		.dt_prop_name = "ta-stack-heap-size",
		.feat_name = "ta-stack-heap",
		.cmd_online = "ta_stack_heap=on",
		.cmd_offline = "ta_stack_heap=off",
#if IS_ENABLED(CONFIG_MTK_HAPP_MEM_SUPPORT)
		.enable = "on",
#else
		.enable = "off",
#endif
		.scheme_flag = FACE_REGISTRATION_FLAGS | FACE_PAYMENT_FLAGS |
				FACE_UNLOCK_FLAGS
	},
	[SSMR_FEAT_SDSP_TEE_SHAREDMEM] = {
		.dt_prop_name = "sdsp-tee-sharedmem-size",
		.feat_name = "sdsp-tee-sharedmem",
		.cmd_online = "sdsp_tee_sharedmem=on",
		.cmd_offline = "sdsp_tee_sharedmem=off",
#if IS_ENABLED(CONFIG_MTK_SDSP_SHARED_MEM_SUPPORT)
		.enable = "on",
#else
		.enable = "off",
#endif
		.scheme_flag = FACE_REGISTRATION_FLAGS | FACE_PAYMENT_FLAGS |
				FACE_UNLOCK_FLAGS
	},
	[SSMR_FEAT_SDSP_FIRMWARE] = {
		.dt_prop_name = "sdsp-firmware-size",
		.feat_name = "sdsp-firmware",
		.cmd_online = "sdsp_firmware=on",
		.cmd_offline = "sdsp_firmware=off",
#if IS_ENABLED(CONFIG_MTK_SDSP_MEM_SUPPORT)
		.enable = "on",
#else
		.enable = "off",
#endif
		.scheme_flag = FACE_UNLOCK_FLAGS
	},
	[SSMR_FEAT_2D_FR] = {
		.dt_prop_name = "2d_fr-size",
		.feat_name = "2d_fr",
		.cmd_online = "2d_fr=on",
		.cmd_offline = "2d_fr=off",
#if IS_ENABLED(CONFIG_MTK_CAM_SECURITY_SUPPORT)
		.enable = "on",
#else
		.enable = "off",
#endif
		.scheme_flag = FACE_REGISTRATION_FLAGS | FACE_PAYMENT_FLAGS |
				FACE_UNLOCK_FLAGS
	},
	[SSMR_FEAT_TUI] = {
		.dt_prop_name = "tui-size",
		.feat_name = "tui",
		.cmd_online = "tui=on",
		.cmd_offline = "tui=off",
#if IS_ENABLED(CONFIG_TRUSTONIC_TRUSTED_UI) ||\
	IS_ENABLED(CONFIG_BLOWFISH_TUI_SUPPORT)
		.enable = "on",
#else
		.enable = "off",
#endif
		.scheme_flag = TUI_FLAGS
	}
};
/* clang-format on */

extern void show_pte(struct mm_struct *mm, unsigned long addr);

struct SSMR_HEAP_INFO {
	unsigned int heap_id;
	char heap_name[NAME_SIZE];
};

struct SSMR_HEAP_INFO _ssmr_heap_info[__MAX_NR_SSMR_FEATURES];

#endif
