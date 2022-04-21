// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/kobject.h>
#include <linux/atomic.h>

#include "ccci_config.h"
#include "ccci_common_config.h"
#include "ccci_core.h"
#include "ccci_bm.h"
#include "modem_sys.h"
#include "ccci_hif.h"
#include "md_sys1_platform.h"

#if IS_ENABLED(CONFIG_OF)
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif
#include "mt-plat/mtk_ccci_common.h"

#define TAG "md"

#define DBM_S (CCCI_SMEM_SIZE_DBM + CCCI_SMEM_SIZE_DBM_GUARD * 2)
#define CCB_CACHE_MIN_SIZE    (2 * 1024 * 1024)

static struct ccci_smem_region md1_6297_noncacheable_fat[] = {
	{SMEM_USER_RAW_DFD,	        0,	0,		 0, },
	{SMEM_USER_RAW_UDC_DATA,	0,	0,		 0, },
	{SMEM_USER_MD_WIFI_PROXY,	0,	0,		 0,},
#ifdef CCCI_SUPPORT_AP_MD_SECURE_FEATURE
	{SMEM_USER_SECURITY_SMEM,	0,	0,
		SMF_NCLR_FIRST, },
#endif
	{SMEM_USER_RAW_AMMS_POS,	0,	0,
		SMF_NCLR_FIRST, },

	{SMEM_USER_RAW_MDCCCI_DBG,	0,	2*1024,	 0, },
	{SMEM_USER_RAW_MDSS_DBG,	0,	14*1024, 0, },
	{SMEM_USER_RAW_RESERVED,	0,	42*1024, 0, },
	{SMEM_USER_RAW_RUNTIME_DATA,	0,	4*1024,	 0, },
	{SMEM_USER_RAW_FORCE_ASSERT,	0,	1*1024,	 0, },
	{SMEM_USER_LOW_POWER,		0,	512,	 0, },
	{SMEM_USER_RAW_DBM,		0,	512,	 0, },
	{SMEM_USER_CCISM_SCP,		0,	32*1024, 0, },
	{SMEM_USER_RAW_CCB_CTRL,	0,	4*1024,
		SMF_NCLR_FIRST, },
	{SMEM_USER_RAW_NETD,		0,	8*1024,	 0, },
	{SMEM_USER_RAW_USB,	        0,	4*1024,	 0, },
	{SMEM_USER_RAW_AUDIO,		0,	52*1024,
		SMF_NCLR_FIRST, },
	{SMEM_USER_CCISM_MCU,	0, (720+1)*1024,	SMF_NCLR_FIRST, },
	{SMEM_USER_CCISM_MCU_EXP, 0, (120+1)*1024,	SMF_NCLR_FIRST, },
	{SMEM_USER_RESERVED, 0, 18*1024,	 0, },
	{SMEM_USER_MD_DRDI, 0, BANK4_DRDI_SMEM_SIZE, SMF_NCLR_FIRST, },
	{SMEM_USER_MAX, }, /* tail guard */
};

static struct ccci_smem_region md1_6297_cacheable[] = {
/*
 * all CCB user should be put together, and the total size is set
 * in the first one, all reset CCB users' address, offset and size
 * will be re-calculated during port initialization. and please be
 * aware of that CCB user's size will be aligned to 4KB.
 */
{SMEM_USER_RAW_MD_CONSYS,	0,	0, (SMF_NCLR_FIRST | SMF_NO_REMAP), },
{SMEM_USER_MD_NVRAM_CACHE,	0,	0, 0, },
{SMEM_USER_CCB_DHL,		0,	0, 0, },
{SMEM_USER_CCB_MD_MONITOR,	0,	0, 0, },
{SMEM_USER_CCB_META,		0,	0, 0, },
{SMEM_USER_RAW_DHL,		0,	0, 0, },
{SMEM_USER_RAW_MDM,		0,	0, 0, },
{SMEM_USER_RAW_UDC_DESCTAB,	0,	0, 0, },
{SMEM_USER_RAW_USIP,		0,	0, SMF_NCLR_FIRST, },
{SMEM_USER_MAX, },
};

static struct ccci_smem_region md1_6293_noncacheable_fat[] = {
{SMEM_USER_RAW_MDCCCI_DBG,	0,		2*1024,		0, },
{SMEM_USER_RAW_MDSS_DBG,	2*1024,		10*1024,	0, },
{SMEM_USER_RAW_RESERVED,	12*1024,	46*1024,	0, },
{SMEM_USER_RAW_RUNTIME_DATA,	58*1024,	4*1024,		0, },
{SMEM_USER_RAW_FORCE_ASSERT,	62*1024,	1*1024,		0, },
{SMEM_USER_RAW_DBM,		64*1024-DBM_S,	DBM_S,		0, },
{SMEM_USER_CCISM_SCP,		64*1024,	32*1024,	0, },
{SMEM_USER_RAW_CCB_CTRL,	96*1024,	4*1024,
	SMF_NCLR_FIRST, },
{SMEM_USER_RAW_NETD,		100*1024,	4*1024,		0, },
{SMEM_USER_RAW_USB,		104*1024,	4*1024,		0, },
{SMEM_USER_RAW_AUDIO,		108*1024,	52*1024,
	SMF_NCLR_FIRST, },
{SMEM_USER_CCISM_MCU,		160*1024, (720+1)*1024, SMF_NCLR_FIRST, },
{SMEM_USER_CCISM_MCU_EXP,	881*1024, (120+1)*1024, SMF_NCLR_FIRST, },
{SMEM_USER_RAW_DFD,		1*1024*1024,	448*1024,	0, },
{SMEM_USER_RAW_UDC_DATA, (1*1024+448)*1024, 0*1024*1024,	0, },
{SMEM_USER_RAW_AMMS_POS,	(1*1024 + 448)*1024,	0,
				SMF_NCLR_FIRST, },
{SMEM_USER_RAW_ALIGN_PADDING,	(1*1024 + 448)*1024,	0,
				SMF_NCLR_FIRST, },

/* for SIB */
{SMEM_USER_RAW_LWA,		(1*1024+448)*1024,	0*1024*1024,	0, },
{SMEM_USER_RAW_PHY_CAP,	(1*1024+448)*1024, 0*1024*1024, SMF_NCLR_FIRST, },
{SMEM_USER_MAX, }, /* tail guard */
};
#define CCB_CACHE_MIN_SIZE    (2 * 1024 * 1024)
static struct ccci_smem_region md1_6293_cacheable[] = {
/*
 * all CCB user should be put together, and the total size is set
 * in the first one, all reset CCB users' address, offset and size
 * will be re-calculated during port initialization. and please be
 * aware of that CCB user's size will be aligned to 4KB.
 */
{SMEM_USER_CCB_DHL,		0*1024*1024,	CCB_CACHE_MIN_SIZE,	0, },
{SMEM_USER_CCB_MD_MONITOR,	0*1024*1024,	CCB_CACHE_MIN_SIZE,	0, },
{SMEM_USER_CCB_META,		0*1024*1024,	CCB_CACHE_MIN_SIZE,	0, },
{SMEM_USER_RAW_DHL,		CCB_CACHE_MIN_SIZE,	20*1024*1024,	0, },
{SMEM_USER_RAW_MDM,		CCB_CACHE_MIN_SIZE,	20*1024*1024,	0, },
{SMEM_USER_RAW_UDC_DESCTAB,	0*1024*1024,	0*1024*1024,	0, },
{SMEM_USER_RAW_MD_CONSYS,	0*1024*1024,	0*1024*1024, SMF_NCLR_FIRST, },
{SMEM_USER_RAW_USIP,		0*1024*1024,	0*1024, SMF_NCLR_FIRST, },
{SMEM_USER_MAX, },
};

struct ccci_smem_region *get_smem_by_user_id(
	struct ccci_smem_region *regions, enum SMEM_USER_ID user_id)
{
	int i;

	for (i = 0; ; i++) {
		if (!regions || regions[i].id == SMEM_USER_MAX)
			return NULL;

		if (regions[i].id == user_id)
			return regions + i;
	}
	return NULL;
}

static void init_smem_regions(struct ccci_smem_region *regions,
	phys_addr_t base_ap_view_phy,
	void __iomem *base_ap_view_vir,
	phys_addr_t base_md_view_phy)
{
	int i;

	for (i = 0; ; i++) {
		if (!regions || regions[i].id == SMEM_USER_MAX)
			break;

		regions[i].base_ap_view_phy =
			base_ap_view_phy + regions[i].offset;
		/* 1. mapping one region; 2. no mapping; 3. mapping together. */
		if (!base_ap_view_vir && !(regions[i].flag & SMF_NO_REMAP))
			regions[i].base_ap_view_vir = (!regions[i].size)?NULL:ccci_map_phy_addr(
					regions[i].base_ap_view_phy, regions[i].size);
		else if (regions[i].flag & SMF_NO_REMAP)
			regions[i].base_ap_view_vir = NULL;
		else
			regions[i].base_ap_view_vir =
				base_ap_view_vir + regions[i].offset;
		regions[i].base_md_view_phy =
			base_md_view_phy + regions[i].offset;
		CCCI_BOOTUP_LOG(-1, TAG,
			"%s: reg[%d]<%d>(%lx %lx %lx)[%x]\n", __func__,
			i, regions[i].id,
			(unsigned long)regions[i].base_ap_view_phy,
			(unsigned long)regions[i].base_ap_view_vir,
			(unsigned long)regions[i].base_md_view_phy,
			regions[i].size);

		if (regions[i].id == SMEM_USER_RAW_MDSS_DBG)
			mrdump_mini_add_extra_file((unsigned long)regions[i].base_ap_view_vir,
					(unsigned long)regions[i].base_ap_view_phy, regions[i].size,
					"EXTRA_MDSS");
	}
}

static void clear_smem_region(struct ccci_smem_region *regions, int first_boot)
{
	int i;

	for (i = 0; ; i++) {
		if (!regions || regions[i].id == SMEM_USER_MAX)
			break;

		if (first_boot) {
			if (!(regions[i].flag & SMF_NCLR_FIRST)) {
				if (regions[i].size) {
					memset_io(regions[i].base_ap_view_vir,
						0, regions[i].size);
				}
			}
		} else {
			if (regions[i].flag & SMF_CLR_RESET && regions[i].size)
				memset_io(regions[i].base_ap_view_vir,
					0, regions[i].size);
		}
	}
}

int __weak amms_cma_free(phys_addr_t addr, unsigned long size)
{
	return 0;
}

#if (MD_GENERATION >= 6297)
static inline int update_smem_region(struct ccci_smem_region *region)
{
	unsigned int offset, size;
	int ret = 0;

	if (get_nc_smem_region_info(region->id, &offset, NULL, &size)) {
		region->offset = offset;
		region->size = size;
		ret = 1;
		CCCI_BOOTUP_LOG(MD_SYS1, TAG, "Update <%d>:0x%x 0x%x\n",
			region->id, region->offset, region->size);
	}
	return ret;
}

static void ccci_6297_md_smem_layout_config(struct ccci_mem_layout *mm_str)
{
	//struct ccci_mem_layout *mm_str = &md->mem_layout;
	unsigned int md_resv_mem_offset = 0, ccb_offset = 0;
	unsigned int md_resv_mem_size = 0, ccb_size = 0;
	unsigned int i;
	phys_addr_t md_resv_smem_addr = 0, smem_amms_pos_addr = 0;
	int size;

	/* non-cacheable start */
	get_md_resv_mem_info(NULL, NULL, &md_resv_smem_addr, NULL);

	for (i = 0; i < ARRAY_SIZE(md1_6297_noncacheable_fat); i++) {
		update_smem_region(&md1_6297_noncacheable_fat[i]);
		if (i == 0)
			continue;
		if (md1_6297_noncacheable_fat[i].offset == 0)
			/* update offset */
			md1_6297_noncacheable_fat[i].offset =
				md1_6297_noncacheable_fat[i-1].offset
				+ md1_6297_noncacheable_fat[i-1].size;

		/* Special case */
		switch (md1_6297_noncacheable_fat[i].id) {
		case SMEM_USER_RAW_AMMS_POS:
			size = get_smem_amms_pos_size();
			if (size >= 0) {
				/* free AMMS POS smem*/
				smem_amms_pos_addr = md_resv_smem_addr
					+ md1_6297_noncacheable_fat[i].offset;
				amms_cma_free(smem_amms_pos_addr, size);
			}
			CCCI_BOOTUP_LOG(0, TAG,
			"smem amms pos size:%d\n",
			md1_6297_noncacheable_fat[i].size);
			break;
		default:
			break;
		}
	}

	mm_str->md_bank4_noncacheable = md1_6297_noncacheable_fat;
	get_md_resv_csmem_info(
		&mm_str->md_bank4_cacheable_total.base_ap_view_phy,
		&mm_str->md_bank4_cacheable_total.size);
	/* cacheable start */
	mm_str->md_bank4_cacheable_total.base_md_view_phy = 0x40000000
		+ get_md_smem_cachable_offset()
		+ mm_str->md_bank4_cacheable_total.base_ap_view_phy -
		round_down(mm_str->md_bank4_cacheable_total.base_ap_view_phy,
			0x00100000);

	/* specially, CCB size. */
	/* get_md_resv_ccb_info(&ccb_offset, &ccb_size); */
	get_md_cache_region_info(SMEM_USER_CCB_START,
				&ccb_offset,
				&ccb_size);
	CCCI_BOOTUP_LOG(0, TAG,
			"ccb totoal :offset = 0x%x, size = 0x%x\n",
			ccb_offset, ccb_size);
	for (i = 0; i < (sizeof(md1_6297_cacheable)/
		sizeof(struct ccci_smem_region)); i++) {

		switch (md1_6297_cacheable[i].id) {
		case SMEM_USER_CCB_DHL:
		case SMEM_USER_CCB_MD_MONITOR:
		case SMEM_USER_CCB_META:
			md1_6297_cacheable[i].size =
				(ccb_size > CCB_CACHE_MIN_SIZE) ?
				CCB_CACHE_MIN_SIZE:0;
			md1_6297_cacheable[i].offset =  ccb_offset;
			break;
		case SMEM_USER_RAW_DHL:
		case SMEM_USER_RAW_MDM:
			md1_6297_cacheable[i].size =
				(ccb_size > CCB_CACHE_MIN_SIZE) ?
				(ccb_size - CCB_CACHE_MIN_SIZE):0;
			md1_6297_cacheable[i].offset =
				(ccb_size > CCB_CACHE_MIN_SIZE) ?
				(ccb_offset + CCB_CACHE_MIN_SIZE):ccb_offset;
			CCCI_BOOTUP_LOG(0, TAG,
				"[%d]RAW size:%d\n", md1_6297_cacheable[i].id,
				md1_6297_cacheable[i].size);
			break;
		case SMEM_USER_RAW_MD_CONSYS: /* go through */
		case SMEM_USER_MD_NVRAM_CACHE: /* go through */
		case SMEM_USER_RAW_UDC_DESCTAB: /* go through */
		case SMEM_USER_RAW_USIP:
			get_md_cache_region_info(md1_6297_cacheable[i].id,
				&md_resv_mem_offset,
				&md_resv_mem_size);

			md1_6297_cacheable[i].size = md_resv_mem_size;
			if (md_resv_mem_offset || md_resv_mem_size)
				md1_6297_cacheable[i].offset =
					md_resv_mem_offset; /* LK config */
			else if (i == 0)
				md1_6297_cacheable[i].offset = 0;
			else
				md1_6297_cacheable[i].offset =
					md1_6297_cacheable[i - 1].offset +
					md1_6297_cacheable[i - 1].size;
			break;
		default:
			md1_6297_cacheable[i].size = 0;
			md1_6297_cacheable[i].offset = 0;
			break;
		}
	}

	mm_str->md_bank4_cacheable = md1_6297_cacheable;
}
#endif

void ccci_md_config_layout_6293(struct ccci_mem_layout *mem_layout)
{
	int dfd_size;
	phys_addr_t smem_amms_pos_addr = 0;
	unsigned int offset_adjust_flag = 0;
	unsigned int udc_noncache_size = 0;
	unsigned int udc_cache_size = 0;
	int size;
	unsigned int md_bank4_cacheable_total_size = 0;
	phys_addr_t smem_align_padding_addr = 0;
	phys_addr_t md_resv_smem_addr = 0;
	unsigned int i;

	/* Get udc cache&noncache size */
	get_md_resv_udc_info(
			&udc_noncache_size, &udc_cache_size);

	md_bank4_cacheable_total_size
		= mem_layout->md_bank4_cacheable_total.size;

	mem_layout->md_bank4_noncacheable
		= md1_6293_noncacheable_fat;
	mem_layout->md_bank4_cacheable
		= md1_6293_cacheable;
	/* Runtime adjust md_phy_capture and udc noncache size */
	for (i = 0; i < (sizeof(md1_6293_noncacheable_fat)/
		sizeof(struct ccci_smem_region)); i++) {
		if (offset_adjust_flag == 1)
			md1_6293_noncacheable_fat[i].offset =
			md1_6293_noncacheable_fat[i-1].offset
			+ md1_6293_noncacheable_fat[i-1].size;
		if (md1_6293_noncacheable_fat[i].id ==
			SMEM_USER_RAW_PHY_CAP) {
			md1_6293_noncacheable_fat[i].size =
				get_md_resv_phy_cap_size();
			CCCI_BOOTUP_LOG(0, TAG,
			"PHY size:%d\n",
			md1_6293_noncacheable_fat[i].size);
		}
		if (md1_6293_noncacheable_fat[i].id ==
			SMEM_USER_RAW_UDC_DATA) {
			md1_6293_noncacheable_fat[i].size =
				udc_noncache_size;
			offset_adjust_flag = 1;
		}
		if (md1_6293_noncacheable_fat[i].id ==
			SMEM_USER_RAW_DFD) {
			dfd_size = get_md_smem_dfd_size();
			if (dfd_size >= 0 && dfd_size !=
				md1_6293_noncacheable_fat[i].size) {
				md1_6293_noncacheable_fat[i].size =
				dfd_size;
				offset_adjust_flag = 1;
			}
			CCCI_BOOTUP_LOG(0, TAG,
			"dfd size:%d\n",
			md1_6293_noncacheable_fat[i].size);
		}
		if (md1_6293_noncacheable_fat[i].id ==
			SMEM_USER_RAW_AMMS_POS) {
			size = get_smem_amms_pos_size();
			if (size >= 0 && size !=
			md1_6293_noncacheable_fat[i].size) {
				md1_6293_noncacheable_fat[i].size =
				size;
				offset_adjust_flag = 1;
				/* free AMMS POS smem*/
				smem_amms_pos_addr = md_resv_smem_addr
				+ md1_6293_noncacheable_fat[i].offset;
				amms_cma_free(smem_amms_pos_addr, size);
			}
			CCCI_BOOTUP_LOG(0, TAG,
			"smem amms pos size:%d\n",
			md1_6293_noncacheable_fat[i].size);
		}
		if (md1_6293_noncacheable_fat[i].id ==
			SMEM_USER_RAW_ALIGN_PADDING) {
			size = get_smem_align_padding_size();
			if (size >= 0 && size !=
			md1_6293_noncacheable_fat[i].size) {
				md1_6293_noncacheable_fat[i].size =
				size;
				offset_adjust_flag = 1;
				/* free POS padding smem*/
				smem_align_padding_addr =
				md_resv_smem_addr
				+ md1_6293_noncacheable_fat[i].offset;
				amms_cma_free(
				smem_align_padding_addr,
				size);
			}
			CCCI_BOOTUP_LOG(0, TAG,
			"smem align padding size:%d\n",
			md1_6293_noncacheable_fat[i].size);
		}

	}
	if (md_bank4_cacheable_total_size
		>= CCB_CACHE_MIN_SIZE) {
		/*
		 * 2M is control part size,
		 * md1_6293_cacheable[0].size
		 * initial value but changed by collect_ccb_info
		 */
		for (i = (SMEM_USER_CCB_END -
			SMEM_USER_CCB_START + 1);
			i < (sizeof(md1_6293_cacheable)/
			sizeof(struct ccci_smem_region)); i++) {
			if (md1_6293_cacheable[i].id >
					SMEM_USER_CCB_END) {
				/* for rumtime udc offset */
				if (md1_6293_cacheable[i].id ==
					SMEM_USER_RAW_UDC_DESCTAB) {
					md1_6293_cacheable[i].offset =
					md1_6293_cacheable[i-1].offset +
					md1_6293_cacheable[i-1].size;
					CCCI_BOOTUP_LOG(0, TAG,
					"UDC offset:%d\n",
					md1_6293_cacheable[i].offset);
					md1_6293_cacheable[i].size
					= udc_cache_size;
					continue;
				}
				md1_6293_cacheable[i].size =
				md_bank4_cacheable_total_size -
				udc_cache_size -
				CCB_CACHE_MIN_SIZE;

			CCCI_BOOTUP_LOG(0, TAG,
				"RAW size:%d\n",
				md1_6293_cacheable[i].size);
			}
		}
	} else if (udc_cache_size) {
		for (i = 0; i < (sizeof(md1_6293_cacheable)/
			sizeof(struct ccci_smem_region)); i++) {
			if (md1_6293_cacheable[i].id ==
				SMEM_USER_RAW_UDC_DESCTAB) {
				md1_6293_cacheable[i].offset = 0;
				CCCI_BOOTUP_LOG(0, TAG,
				"UDC offset:%d\n",
				md1_6293_cacheable[i].offset);
				md1_6293_cacheable[i].size
					= udc_cache_size;
				continue;
			}
			md1_6293_cacheable[i].offset = 0;
			md1_6293_cacheable[i].size = 0;
		}
	} else
		mem_layout->md_bank4_cacheable = NULL;
}

void ccci_md_smem_layout_config(struct ccci_mem_layout *mm_str)
{
	//struct ccci_mem_layout *mm_str = &md->mem_layout;
	unsigned int md_resv_mem_offset = 0, ccb_offset = 0;
	unsigned int md_resv_mem_size = 0, ccb_size = 0;
	unsigned int offset_adjust_flag = 0;
	unsigned int udc_noncache_size = 0, udc_cache_size = 0;
	unsigned int i;
	phys_addr_t md_resv_smem_addr = 0, smem_amms_pos_addr = 0,
		smem_align_padding_addr = 0;
	int size;

	/* non-cacheable start */
	get_md_resv_mem_info(NULL, NULL, &md_resv_smem_addr, NULL);
	for (i = 0; i < (sizeof(md1_6293_noncacheable_fat)/
		sizeof(struct ccci_smem_region)); i++) {
		/* update offset */
		if (offset_adjust_flag == 1)
			md1_6293_noncacheable_fat[i].offset =
				md1_6293_noncacheable_fat[i-1].offset
				+ md1_6293_noncacheable_fat[i-1].size;
		switch (md1_6293_noncacheable_fat[i].id) {
		case SMEM_USER_RAW_PHY_CAP:
			md1_6293_noncacheable_fat[i].size =
				get_md_resv_phy_cap_size();
			CCCI_BOOTUP_LOG(0, TAG,
			"PHY size:%d\n",
			md1_6293_noncacheable_fat[i].size);
			break;
		case SMEM_USER_RAW_DFD:
			size = get_md_smem_dfd_size();
			if (size >= 0 && size !=
			md1_6293_noncacheable_fat[i].size) {
				md1_6293_noncacheable_fat[i].size = size;
				offset_adjust_flag = 1;
			}
			CCCI_BOOTUP_LOG(0, TAG,
			"dfd size:%d\n",
			md1_6293_noncacheable_fat[i].size);
			break;
		case SMEM_USER_RAW_AMMS_POS:
			size = get_smem_amms_pos_size();
			if (size >= 0 && size !=
			md1_6293_noncacheable_fat[i].size) {
				md1_6293_noncacheable_fat[i].size = size;
				offset_adjust_flag = 1;
				/* free AMMS POS smem*/
				smem_amms_pos_addr = md_resv_smem_addr
					+ md1_6293_noncacheable_fat[i].offset;
				amms_cma_free(smem_amms_pos_addr, size);
			}
			CCCI_BOOTUP_LOG(0, TAG,
			"smem amms pos size:%d\n",
			md1_6293_noncacheable_fat[i].size);
			break;
		case SMEM_USER_RAW_ALIGN_PADDING:
			size = get_smem_align_padding_size();
			if (size >= 0 && size !=
			md1_6293_noncacheable_fat[i].size) {
				md1_6293_noncacheable_fat[i].size = size;
				offset_adjust_flag = 1;
				/* free POS padding smem*/
				smem_align_padding_addr = md_resv_smem_addr
					+ md1_6293_noncacheable_fat[i].offset;
				amms_cma_free(smem_align_padding_addr, size);
			}
			CCCI_BOOTUP_LOG(0, TAG,
			"smem align padding size:%d\n",
			md1_6293_noncacheable_fat[i].size);
			break;
		case SMEM_USER_RAW_UDC_DATA:
			get_md_resv_udc_info(
				&udc_noncache_size, &udc_cache_size);
			if (udc_noncache_size > 0 && udc_noncache_size !=
				md1_6293_noncacheable_fat[i].size) {
				md1_6293_noncacheable_fat[i].size =
					udc_noncache_size;
				offset_adjust_flag = 1;
			}
			break;
		default:
			break;
		}
	}

	mm_str->md_bank4_noncacheable = md1_6293_noncacheable_fat;
	get_md_resv_csmem_info(
		&mm_str->md_bank4_cacheable_total.base_ap_view_phy,
		&mm_str->md_bank4_cacheable_total.size);
	/* cacheable start */
	if (mm_str->md_bank4_cacheable_total.base_ap_view_phy &&
		mm_str->md_bank4_cacheable_total.size)
		mm_str->md_bank4_cacheable_total.base_ap_view_vir =
			ccci_map_phy_addr(
			mm_str->md_bank4_cacheable_total.base_ap_view_phy,
			mm_str->md_bank4_cacheable_total.size);
	else
		CCCI_ERROR_LOG(0, TAG,
			"get cacheable info base:%lx size:%x\n",
			(unsigned long)
			mm_str->md_bank4_cacheable_total.base_ap_view_phy,
			mm_str->md_bank4_cacheable_total.size);

	mm_str->md_bank4_cacheable_total.base_md_view_phy = 0x40000000
		+ get_md_smem_cachable_offset()
		+ mm_str->md_bank4_cacheable_total.base_ap_view_phy -
		round_down(mm_str->md_bank4_cacheable_total.base_ap_view_phy,
			0x00100000);

	/* specially, CCB size. */
	/* get_md_resv_ccb_info(&ccb_offset, &ccb_size); */
	get_md_cache_region_info(SMEM_USER_CCB_START,
				&ccb_offset,
				&ccb_size);
	CCCI_BOOTUP_LOG(0, TAG,
			"ccb totoal :offset = 0x%x, size = 0x%x\n",
			ccb_offset, ccb_size);
	for (i = 0; i < (sizeof(md1_6293_cacheable)/
		sizeof(struct ccci_smem_region)); i++) {

		switch (md1_6293_cacheable[i].id) {
		case SMEM_USER_CCB_DHL:
		case SMEM_USER_CCB_MD_MONITOR:
		case SMEM_USER_CCB_META:
			md1_6293_cacheable[i].size =
				(ccb_size > CCB_CACHE_MIN_SIZE) ?
				CCB_CACHE_MIN_SIZE:0;
			md1_6293_cacheable[i].offset =  ccb_offset;
			break;
		case SMEM_USER_RAW_DHL:
		case SMEM_USER_RAW_MDM:
			md1_6293_cacheable[i].size =
				(ccb_size > CCB_CACHE_MIN_SIZE) ?
				(ccb_size - CCB_CACHE_MIN_SIZE):0;
			md1_6293_cacheable[i].offset =
				(ccb_size > CCB_CACHE_MIN_SIZE) ?
				(ccb_offset + CCB_CACHE_MIN_SIZE):ccb_offset;
			CCCI_BOOTUP_LOG(0, TAG,
				"[%d]RAW size:%d\n", md1_6293_cacheable[i].id,
				md1_6293_cacheable[i].size);
			break;
		case SMEM_USER_RAW_MD_CONSYS:
		case SMEM_USER_RAW_USIP:
			get_md_cache_region_info(md1_6293_cacheable[i].id,
				&md_resv_mem_offset,
				&md_resv_mem_size);
			md1_6293_cacheable[i].size = md_resv_mem_size;

			if (md_resv_mem_offset || md_resv_mem_size)
				md1_6293_cacheable[i].offset =
					md_resv_mem_offset; /* LK config */
			else if (i == 0)
				md1_6293_cacheable[i].offset = 0;
			else
				md1_6293_cacheable[i].offset =
					md1_6293_cacheable[i - 1].offset +
					md1_6293_cacheable[i - 1].size;
			break;
		case SMEM_USER_RAW_UDC_DESCTAB:
			get_md_cache_region_info(md1_6293_cacheable[i].id,
				&md_resv_mem_offset,
				&md_resv_mem_size);
			md1_6293_cacheable[i].size = md_resv_mem_size;

			if (md_resv_mem_offset || md_resv_mem_size)
				md1_6293_cacheable[i].offset =
					md_resv_mem_offset; /* LK config */
			else if (i == 0)
				md1_6293_cacheable[i].offset = 0;
			else
				md1_6293_cacheable[i].offset =
					md1_6293_cacheable[i - 1].offset +
					md1_6293_cacheable[i - 1].size;
			break;
		default:
			md1_6293_cacheable[i].size = 0;
			md1_6293_cacheable[i].offset = 0;
			break;
		}
	}

	mm_str->md_bank4_cacheable = md1_6293_cacheable;
	/* md_smem_layout_parsing(md); */
}

void ap_md_mem_init(struct ccci_mem_layout *mem_layout)
{
	phys_addr_t md_resv_mem_addr = 0, md_resv_smem_addr = 0;

	unsigned int md_resv_mem_size = 0, md_resv_smem_size = 0;

	/* Get memory info */
	get_md_resv_mem_info(&md_resv_mem_addr,
		&md_resv_mem_size, &md_resv_smem_addr, &md_resv_smem_size);

	/* setup memory layout */
	/* MD image */
	mem_layout->md_bank0.base_ap_view_phy = md_resv_mem_addr;
	mem_layout->md_bank0.size = md_resv_mem_size;
	/* do not remap whole region, consume too much vmalloc space */
	/* Share memory */
	/*
	 * MD bank4 is remap to nearest 32M aligned address
	 * assume share memoy layout is:
	 * |---AP/MD1--| <--MD1 bank4 0x0 (non-cacheable)
	 * |--MD1/MD3--| <--MD3 bank4 0x0 (non-cacheable)
	 * |---AP/MD3--|
	 * |--non-used_-|
	 * |--cacheable--| <-- MD1 bank4 0x8000000 (for 6292)
	 * this should align with LK's remap setting
	 */
	/* non-cacheable region */

	mem_layout->md_bank4_noncacheable_total.base_ap_view_phy
		= md_resv_smem_addr;

	mem_layout->md_bank4_noncacheable_total.size
		= md_resv_smem_size;

	mem_layout->md_bank4_noncacheable_total.base_ap_view_vir =
		ccci_map_phy_addr(
		mem_layout->md_bank4_noncacheable_total.base_ap_view_phy,
			mem_layout->md_bank4_noncacheable_total.size);
	mem_layout->md_bank4_noncacheable_total.base_md_view_phy =
		0x40000000 +
		mem_layout->md_bank4_noncacheable_total.base_ap_view_phy -
		round_down(
		mem_layout->md_bank4_noncacheable_total.base_ap_view_phy,
		0x02000000);
#if (MD_GENERATION >= 6297)
	ccci_6297_md_smem_layout_config(mem_layout);
#else
	if (md->hw_info->plat_val->md_gen >= 6295)
		ccci_md_smem_layout_config(md);
	else {
		/* cacheable region */
		get_md_resv_ccb_info(
		&mem_layout->md_bank4_cacheable_total.base_ap_view_phy,
			&mem_layout->md_bank4_cacheable_total.size);
		if (mem_layout->md_bank4_cacheable_total.base_ap_view_phy
			&& mem_layout->md_bank4_cacheable_total.size)
			mem_layout->md_bank4_cacheable_total.base_ap_view_vir
				= ccci_map_phy_addr(
			mem_layout->md_bank4_cacheable_total.base_ap_view_phy
			, mem_layout->md_bank4_cacheable_total.size);
		else
			CCCI_ERROR_LOG(, TAG,
				"get ccb info base:%lx size:%x\n",
			(unsigned long)
			mem_layout->md_bank4_cacheable_total.base_ap_view_phy
			, mem_layout->md_bank4_cacheable_total.size);

		mem_layout->md_bank4_cacheable_total.base_md_view_phy
			= 0x40000000 + (224 * 1024 * 1024) +
		mem_layout->md_bank4_cacheable_total.base_ap_view_phy
			- round_down(
		mem_layout->md_bank4_cacheable_total.base_ap_view_phy
			, 0x00100000);

		if (md->hw_info->plat_val->md_gen == 6293)
			ccci_md_config_layout_6293(md);
	}

#endif
	CCCI_BOOTUP_LOG(0, TAG,
		"smem info: (%lx %lx %lx %d) (%lx %lx %lx %d)\n",
		(unsigned long)
		mem_layout->md_bank4_noncacheable_total.base_ap_view_phy,
		(unsigned long)
		mem_layout->md_bank4_noncacheable_total.base_md_view_phy,
		(unsigned long)
		mem_layout->md_bank4_noncacheable_total.base_ap_view_vir,
		mem_layout->md_bank4_noncacheable_total.size,
		(unsigned long)
		mem_layout->md_bank4_cacheable_total.base_ap_view_phy,
		(unsigned long)
		mem_layout->md_bank4_cacheable_total.base_md_view_phy,
		(unsigned long)
		mem_layout->md_bank4_cacheable_total.base_ap_view_vir,
		mem_layout->md_bank4_cacheable_total.size);

	init_smem_regions(mem_layout->md_bank4_noncacheable,
		mem_layout->md_bank4_noncacheable_total.base_ap_view_phy,
		mem_layout->md_bank4_noncacheable_total.base_ap_view_vir,
		mem_layout->md_bank4_noncacheable_total.base_md_view_phy);
	init_smem_regions(mem_layout->md_bank4_cacheable,
		mem_layout->md_bank4_cacheable_total.base_ap_view_phy,
		mem_layout->md_bank4_cacheable_total.base_ap_view_vir,
		mem_layout->md_bank4_cacheable_total.base_md_view_phy);
}

struct ccci_mem_layout *ccci_md_get_mem(void)
{
	return &modem_sys->mem_layout;
}

struct ccci_smem_region *ccci_md_get_smem_by_user_id(enum SMEM_USER_ID user_id)
{
	struct ccci_smem_region *curr = NULL;

	if (modem_sys == NULL) {
		CCCI_ERROR_LOG(0, TAG,
			"md not enable/ before driver int, return NULL\n");
		return NULL;
	}

	curr = get_smem_by_user_id(
		modem_sys->mem_layout.md_bank4_noncacheable, user_id);
	if (curr)
		return curr;
	curr = get_smem_by_user_id(
		modem_sys->mem_layout.md_bank4_cacheable, user_id);
	return curr;
}
EXPORT_SYMBOL(ccci_md_get_smem_by_user_id);

void ccci_md_clear_smem(int first_boot)
{
	struct ccci_smem_region *region = NULL;
	unsigned int size;

	/* MD will clear share memory itself after the first boot */
	clear_smem_region(modem_sys->mem_layout.md_bank4_noncacheable,
		first_boot);
	clear_smem_region(modem_sys->mem_layout.md_bank4_cacheable,
		first_boot);
	if (!first_boot) {
		CCCI_NORMAL_LOG(-1, TAG, "clear buffer ! first_boot\n");
		region = ccci_md_get_smem_by_user_id(SMEM_USER_CCB_DHL);
		if (region && region->size) {
			/*clear ccb data smem*/
			memset_io(region->base_ap_view_vir, 0, region->size);
		}
		region = ccci_md_get_smem_by_user_id(SMEM_USER_RAW_DHL);
		if (region && region->size) {
			/* clear first 1k bytes in dsp log buffer */
			size = (region->size > (128 * sizeof(long long))) ?
			(128 * sizeof(long long))
			: region->size;
			memset_io(region->base_ap_view_vir, 0, size);
			CCCI_NORMAL_LOG(-1, TAG,
			"clear buffer user_id = SMEM_USER_RAW_DHL, szie = 0x%x\n",
			size);
		}
	}
}

/* maybe we can change it to hook function of MD FSM */
int smem_md_state_notification(unsigned char state)
{
	static int init = 1; /*default is 0*/
	int ret = 0;

	switch (state) {
	case BOOT_WAITING_FOR_HS1:
		ccci_md_clear_smem(init);
		if (init)
			init = 0;
		break;
	case READY:
		break;
	case RESET:
		break;
	case EXCEPTION:
	case WAITING_TO_STOP:
		break;
	case GATED:
		break;
	default:
		break;
	}
	return ret;
}

