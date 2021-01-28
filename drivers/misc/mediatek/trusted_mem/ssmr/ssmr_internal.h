/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __SSMR_INTERNAL_H__
#define __SSMR_INTERNAL_H__

#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)                                 \
	|| IS_ENABLED(CONFIG_TRUSTONIC_TEE_SUPPORT)                            \
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
extern const char *const ssmr_state_text[NR_STATES];
/* clang-format on */

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
