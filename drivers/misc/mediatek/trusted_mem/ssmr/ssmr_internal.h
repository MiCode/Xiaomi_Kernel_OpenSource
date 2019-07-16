/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __SSMR_INTERNAL_H__
#define __SSMR_INTERNAL_H__

#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)                                 \
	|| defined(CONFIG_TRUSTONIC_TEE_SUPPORT)                               \
	|| defined(CONFIG_MICROTRUST_TEE_SUPPORT)                              \
	|| defined(CONFIG_MTK_IRIS_SUPPORT)                                    \
	|| defined(CONFIG_MTK_CAM_SECURITY_SUPPORT)
#define SSMR_SECMEM_REGION_ENABLE
#else
#undef SSMR_SECMEM_REGION_ENABLE
#endif

#if defined(CONFIG_TRUSTONIC_TRUSTED_UI)                                       \
	|| defined(CONFIG_BLOWFISH_TUI_SUPPORT)                                \
	|| defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
#define SSMR_TUI_REGION_ENABLE
#else
#undef SSMR_TUI_REGION_ENABLE
#endif

#if defined(CONFIG_MTK_PROT_MEM_SUPPORT)
#define SSMR_PROT_SHAREDMEM_REGION_ENABLE
#else
#undef SSMR_PROT_SHAREDMEM_REGION_ENABLE
#endif

#include "memory_ssmr.h"

#define SVP_REGION_IOC_MAGIC 'S'

#define SVP_REGION_ACQUIRE _IOR(SVP_REGION_IOC_MAGIC, 6, int)
#define SVP_REGION_RELEASE _IOR(SVP_REGION_IOC_MAGIC, 8, int)

#define UPPER_LIMIT32 (1ULL << 32)
#define UPPER_LIMIT64 (1ULL << 63)

#define NAME_LEN 32
#define CMD_LEN 64

enum region_type {
	SSMR_SECMEM,
	SSMR_TUI,
	SSMR_PROT_SHAREDMEM,
	SSMR_TA_ELF,
	SSMR_TA_STACK_HEAP,
	SSMR_SDSP_TEE_SHAREDMEM,
	SSMR_SDSP_FIRMWARE,
	__MAX_NR_SSMRSUBS,
};

#define SSMR_INVALID_FEATURE(f) (f >= __MAX_NR_SSMR_FEATURES)
#define SSMR_INVALID_REGION(r) (r >= __MAX_NR_SSMRSUBS)

struct SSMR_Feature {
	char dt_prop_name[NAME_LEN];
	char feat_name[NAME_LEN];
	char cmd_online[CMD_LEN];
	char cmd_offline[CMD_LEN];
	u64 req_size;
	unsigned int region;
	char enable[CMD_LEN];
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

/* clang-format off */
const char *const ssmr_state_text[NR_STATES] = {
	[SSMR_STATE_DISABLED]   = "[DISABLED]",
	[SSMR_STATE_ONING_WAIT] = "[ONING_WAIT]",
	[SSMR_STATE_ONING]      = "[ONING]",
	[SSMR_STATE_ON]         = "[ON]",
	[SSMR_STATE_OFFING]     = "[OFFING]",
	[SSMR_STATE_OFF]        = "[OFF]",
};
/* clang-format on */

/*
 *  name :             region name
 *  proc_entry_fops :  file operation fun pointer
 *  state :            region online/offline state
 *  count :            region max alloc size by feature
 *  alloc_pages :      current feature offline alloc size
 *  is_unmapping :     unmapping state
 *  use_cache_memory : when use reserved memory it will be true
 *  page :             zmc alloc page
 *  cache_page :       alloc page by reserved memory
 *  usable_size :      cma usage size
 *  cur_feat :         current feature in use
 */
struct SSMR_Region {
	char name[NAME_LEN];
	const struct file_operations *proc_entry_fops;
	unsigned int state;
	unsigned long count;
	unsigned long alloc_pages;
	bool is_unmapping;
	bool use_cache_memory;
	struct page *page;
	struct page *cache_page;
	u64 usable_size;
	unsigned int cur_feat;
};

/* clang-format off */
static struct SSMR_Feature _ssmr_feats[__MAX_NR_SSMR_FEATURES] = {
	[SSMR_FEAT_SVP] = {
		.dt_prop_name = "svp-size",
		.feat_name = "svp",
		.cmd_online = "svp=on",
		.cmd_offline = "svp=off",
		.region = SSMR_SECMEM,
#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) ||\
	defined(CONFIG_TRUSTONIC_TEE_SUPPORT) ||\
	defined(CONFIG_MICROTRUST_TEE_SUPPORT)
		.enable = "on"
#else
		.enable = "off"
#endif
	},
	[SSMR_FEAT_PROT_SHAREDMEM] = {
		.dt_prop_name = "prot-sharedmem-size",
		.feat_name = "prot-sharedmem",
		.cmd_online = "prot_sharedmem=on",
		.cmd_offline = "prot_sharedmem=off",
		.region = SSMR_PROT_SHAREDMEM,
#ifdef CONFIG_MTK_PROT_MEM_SUPPORT
		.enable = "on"
#else
		.enable = "off"
#endif
	},
	[SSMR_FEAT_WFD] = {
		.dt_prop_name = "wfd-size",
		.feat_name = "wfd",
		.cmd_online = "wfd=on",
		.cmd_offline = "wfd=off",
		.region = SSMR_TUI,
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		.enable = "on"
#else
		.enable = "off"
#endif
	},
	[SSMR_FEAT_TA_ELF] = {
		.dt_prop_name = "ta-elf-size",
		.feat_name = "ta-elf",
		.cmd_online = "ta_elf=on",
		.cmd_offline = "ta_elf=off",
		.region = SSMR_TA_ELF,
#ifdef CONFIG_MTK_HAPP_MEM_SUPPORT
		.enable = "on"
#else
		.enable = "off"
#endif
	},
	[SSMR_FEAT_TA_STACK_HEAP] = {
		.dt_prop_name = "ta-stack-heap-size",
		.feat_name = "ta-stack-heap",
		.cmd_online = "ta_stack_heap=on",
		.cmd_offline = "ta_stack_heap=off",
		.region = SSMR_TA_STACK_HEAP,
#ifdef CONFIG_MTK_HAPP_MEM_SUPPORT
		.enable = "on"
#else
		.enable = "off"
#endif
	},
	[SSMR_FEAT_SDSP_TEE_SHAREDMEM] = {
		.dt_prop_name = "sdsp-tee-sharedmem-size",
		.feat_name = "sdsp-tee-sharedmem",
		.cmd_online = "sdsp_tee_sharedmem=on",
		.cmd_offline = "sdsp_tee_sharedmem=off",
		.region = SSMR_SDSP_TEE_SHAREDMEM,
#ifdef CONFIG_MTK_SDSP_SHARED_MEM_SUPPORT
		.enable = "on"
#else
		.enable = "off"
#endif
	},
	[SSMR_FEAT_SDSP_FIRMWARE] = {
		.dt_prop_name = "sdsp-firmware-size",
		.feat_name = "sdsp-firmware",
		.cmd_online = "sdsp_firmware=on",
		.cmd_offline = "sdsp_firmware=off",
		.region = SSMR_SDSP_FIRMWARE,
#ifdef CONFIG_MTK_SDSP_MEM_SUPPORT
		.enable = "on"
#else
		.enable = "off"
#endif
	},
	[SSMR_FEAT_2D_FR] = {
		.dt_prop_name = "2d_fr-size",
		.feat_name = "2d_fr",
		.cmd_online = "2d_fr=on",
		.cmd_offline = "2d_fr=off",
		.region = SSMR_SECMEM,
#ifdef CONFIG_MTK_CAM_SECURITY_SUPPORT
		.enable = "on"
#else
		.enable = "off"
#endif
	},
	[SSMR_FEAT_TUI] = {
		.dt_prop_name = "tui-size",
		.feat_name = "tui",
		.cmd_online = "tui=on",
		.cmd_offline = "tui=off",
		.region = SSMR_TUI,
#if defined(CONFIG_TRUSTONIC_TRUSTED_UI) ||\
	defined(CONFIG_BLOWFISH_TUI_SUPPORT)
		.enable = "on"
#else
		.enable = "off"
#endif
	},
	[SSMR_FEAT_IRIS] = {
		.dt_prop_name = "iris-recognition-size",
		.feat_name = "iris",
		.cmd_online = "iris=on",
		.cmd_offline = "iris=off",
		.region = SSMR_SECMEM,
#ifdef CONFIG_MTK_IRIS_SUPPORT
		.enable = "on"
#else
		.enable = "off"
#endif
	}
};

static struct SSMR_Region _ssmregs[__MAX_NR_SSMRSUBS] = {
	[SSMR_SECMEM] = {
		.name = "secmem_region",
		.cur_feat = __MAX_NR_SSMR_FEATURES
	},
	[SSMR_TUI] = {
		.name = "tui_region",
		.cur_feat = __MAX_NR_SSMR_FEATURES
	},
	[SSMR_PROT_SHAREDMEM] = {
		.name = "prot_sharedmem_region",
		.cur_feat = __MAX_NR_SSMR_FEATURES
	},
	[SSMR_TA_ELF] = {
		.name = "ta_elf_region",
		.cur_feat = __MAX_NR_SSMR_FEATURES
	},
	[SSMR_TA_STACK_HEAP] = {
		.name = "ta_stack_heap_region",
		.cur_feat = __MAX_NR_SSMR_FEATURES
	},
	[SSMR_SDSP_TEE_SHAREDMEM] = {
		.name = "sdsp_tee_sharedmem_region",
		.cur_feat = __MAX_NR_SSMR_FEATURES
	},
	[SSMR_SDSP_FIRMWARE] = {
		.name = "sdsp_firmware_region",
		.cur_feat = __MAX_NR_SSMR_FEATURES
	}
};
/* clang-format on */

extern void show_pte(struct mm_struct *mm, unsigned long addr);

#ifdef CONFIG_MTK_ION
extern void ion_sec_heap_dump_info(void);
#else
static inline void ion_sec_heap_dump_info(void)
{
	pr_info("%s is not supported\n", __func__);
}
#endif

struct SSMR_HEAP_INFO {
	unsigned int heap_id;
	char heap_name[NAME_SIZE];
};

struct SSMR_HEAP_INFO _ssmr_heap_info[__MAX_NR_SSMR_FEATURES];

#endif
