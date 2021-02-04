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
#include "ccci_platform.h"
#include "ccci_core.h"
#include "ccci_bm.h"
#include "modem_sys.h"
#include "ccci_hif.h"

#include <mt-plat/mtk_meminfo.h>
#include <mt-plat/mtk_ccci_common.h>
#include <mt-plat/mtk_boot_common.h>
#if defined(ENABLE_32K_CLK_LESS)
#include <mt-plat/mtk_rtc.h>
#endif

#define TAG "md"

struct ccci_modem *modem_sys[MAX_MD_NUM];

/* flag for MD1_MD3_SMEM clear.
 * if it is been cleared by md1 bootup flow, set it to 1.
 * then it will not be cleared by md1 bootup flow
 */
static atomic_t md1_md3_smem_clear = ATOMIC_INIT(0);

#define DBM_S (CCCI_SMEM_SIZE_DBM + CCCI_SMEM_SIZE_DBM_GUARD * 2)

struct ccci_smem_region md1_6293_noncacheable_fat[] = {
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
{SMEM_USER_CCISM_MCU,		160*1024,	(720+1)*1024,	0, },
{SMEM_USER_CCISM_MCU_EXP,	881*1024,	(120+1)*1024,	0, },
{SMEM_USER_RAW_DFD,		1*1024*1024,	448*1024,	0, },
{SMEM_USER_RAW_UDC_DATA,	1*1024*1024,	0*1024*1024,	0, },
/* for SIB */
{SMEM_USER_RAW_LWA,		(1*1024+448)*1024,	0*1024*1024,	0, },
{SMEM_USER_RAW_PHY_CAP,	(1*1024+448)*1024, 0*1024*1024, SMF_NCLR_FIRST, },
{SMEM_USER_MAX, }, /* tail guard */
};
#define CCB_CACHE_MIN_SIZE    (2 * 1024 * 1024)
struct ccci_smem_region md1_6293_cacheable[] = {
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

struct ccci_smem_region md1_6292_noncacheable_fat[] = {
{SMEM_USER_RAW_MDCCCI_DBG,	0,		2*1024,		0, },
{SMEM_USER_RAW_MDSS_DBG,	2*1024,		10*1024,	0, },
{SMEM_USER_RAW_RESERVED,	12*1024,	46*1024,	0, },
{SMEM_USER_RAW_RUNTIME_DATA,	58*1024,	4*1024,		0, },
{SMEM_USER_RAW_FORCE_ASSERT,	62*1024,	1*1024,		0, },
{SMEM_USER_RAW_DBM,		64*1024-DBM_S,	DBM_S,		0, },
{SMEM_USER_CCISM_SCP,		64*1024,	32*1024,	0, },
/* {SMEM_USER_RAW_CCB_CTRL,	96*1024,	4*1024,
 * SMF_NCLR_FIRST, },
 */
{SMEM_USER_RAW_NETD,		100*1024,	4*1024,		0, },
{SMEM_USER_RAW_USB,		104*1024,	4*1024,		0, },
{SMEM_USER_RAW_AUDIO,		108*1024,	20*1024,	0, },
#if defined(CONFIG_MTK_MD3_SUPPORT) && \
	(CONFIG_MTK_MD3_SUPPORT > 0)
{SMEM_USER_RAW_MD2MD,		2*1024*1024,	2*1024*1024,
	SMF_MD3_RELATED, },
#endif
{SMEM_USER_MAX, }, /* tail guard */
};

struct ccci_smem_region md1_6292_cacheable[] = {
/*
 * all CCB user should be put together, and the total size is set
 * in the first one, all reset CCB users' address, offset and size
 * will be re-calculated during port initialization. and please be
 * aware of that CCB user's size will be aligned to 4KB.
 */
 #if 0
{SMEM_USER_CCB_DHL,		0*1024*1024,	16*1024*1024,	0, },
{SMEM_USER_CCB_MD_MONITOR,	0*1024*1024,	16*1024*1024,	0, },
{SMEM_USER_CCB_META,		0*1024*1024,	16*1024*1024,	0, },
{SMEM_USER_RAW_DHL,		16*1024*1024,	16*1024*1024,	0, },
#endif
{SMEM_USER_RAW_LWA,		32*1024*1024,	0*1024*1024,	0, },
{SMEM_USER_MAX, },
};

struct ccci_smem_region md3_6292_noncacheable_fat[] = {
{SMEM_USER_RAW_MD2MD,	0,		2*1024*1024,	0, },
{SMEM_USER_RAW_MDCCCI_DBG,	2*1024*1024,	2*1024,		0, },
{SMEM_USER_RAW_MDSS_DBG, (2*1024 + 2)*1024,	2*1024,		0, },
{SMEM_USER_RAW_RESERVED, (2*1024 + 4)*1024,	54*1024,	0, },
{SMEM_USER_RAW_RUNTIME_DATA, (2*1024 + 58)*1024, 4*1024, 0, },
{SMEM_USER_RAW_FORCE_ASSERT, (2*1024 + 62)*1024, 1*1024, 0, },
{SMEM_USER_RAW_DBM,	(2*1024 + 64)*1024-DBM_S, DBM_S, 0, },
{SMEM_USER_CCISM_SCP,	(2*1024 + 64)*1024,	32*1024,	0, },
{SMEM_USER_RAW_AUDIO,	(2*1024 + 96)*1024,	20*1024,	0, },
{SMEM_USER_CCISM_MCU,	(2*1024 + 116)*1024, 1*1024*1024, 0, },
{SMEM_USER_MAX, },
};

struct ccci_smem_region md1_6291_noncacheable_fat[] = {
{SMEM_USER_RAW_MDCCCI_DBG,	0,		2*1024,		0, },
{SMEM_USER_RAW_MDSS_DBG,	2*1024,		10*1024,	0, },
{SMEM_USER_RAW_RESERVED,	12*1024,	50*1024,	0, },
{SMEM_USER_RAW_FORCE_ASSERT,	62*1024,	1*1024,		0, },
{SMEM_USER_RAW_DBM,		64*1024-DBM_S,	DBM_S,		0, },
{SMEM_USER_RAW_RUNTIME_DATA,	64*1024,	4*1024,		0, },
{SMEM_USER_CCISM_SCP,		68*1024,	32*1024,	0, },
{SMEM_USER_RAW_NETD,		100*1024,	4*1024,		0, },
{SMEM_USER_RAW_USB,		104*1024,	4*1024,		0, },
#if defined(CONFIG_MTK_MD3_SUPPORT) &&  (CONFIG_MTK_MD3_SUPPORT > 0)
{SMEM_USER_RAW_MD2MD, 2*1024*1024, 2*1024*1024, SMF_MD3_RELATED, },
#endif
{SMEM_USER_MAX, },
};

struct ccci_smem_region md3_6291_noncacheable_fat[] = {
{SMEM_USER_RAW_MD2MD,	0,	2*1024*1024, 0, },
{SMEM_USER_RAW_MDCCCI_DBG,	2*1024*1024, 2*1024, 0, },
{SMEM_USER_RAW_MDSS_DBG,	(2*1024 + 2)*1024,	2*1024,	0, },
{SMEM_USER_RAW_RESERVED,	(2*1024 + 4)*1024,	58*1024, 0, },
{SMEM_USER_RAW_FORCE_ASSERT, (2*1024 + 62)*1024,	1*1024,	0, },
{SMEM_USER_RAW_DBM,		(2*1024 + 64)*1024-DBM_S, DBM_S, 0, },
{SMEM_USER_RAW_RUNTIME_DATA,	(2*1024 + 64)*1024,	4*1024,	0, },
{SMEM_USER_CCISM_SCP,		(2*1024 + 68)*1024,	32*1024,	0, },
{SMEM_USER_CCISM_MCU,		(2*1024 + 100)*1024, 1*1024*1024, 0, },
{SMEM_USER_MAX, },
};

static struct ccci_smem_region *get_smem_by_user_id(
	struct ccci_smem_region *regions, enum SMEM_USER_ID user_id)
{
	int i;

	for (i = 0; ; i++) {
		if (!regions || regions[i].id == SMEM_USER_MAX)
			return NULL;

		if (regions[i].id == user_id) {
			if (!get_modem_is_enabled(MD_SYS3) &&
				(regions[i].flag & SMF_MD3_RELATED))
				return NULL;
			else
				return regions + i;
		}
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

		if (!get_modem_is_enabled(MD_SYS3) &&
			(regions[i].flag & SMF_MD3_RELATED))
			continue;

		regions[i].base_ap_view_phy =
			base_ap_view_phy + regions[i].offset;
		regions[i].base_ap_view_vir =
			base_ap_view_vir + regions[i].offset;
		regions[i].base_md_view_phy =
			base_md_view_phy + regions[i].offset;
		CCCI_BOOTUP_LOG(-1, TAG,
			"init_smem_regions: reg[%d](%lx %p %lx)\n",
			i, (unsigned long)regions[i].base_ap_view_phy,
			regions[i].base_ap_view_vir,
			(unsigned long)regions[i].base_md_view_phy);
	}
}

static void clear_smem_region(struct ccci_smem_region *regions, int first_boot)
{
	int i;

	for (i = 0; ; i++) {
		if (!regions || regions[i].id == SMEM_USER_MAX)
			break;

		if (!get_modem_is_enabled(MD_SYS3) &&
			(regions[i].flag & SMF_MD3_RELATED))
			continue;
		if (first_boot) {
			if (!(regions[i].flag & SMF_NCLR_FIRST)) {
				if (regions[i].id == SMEM_USER_RAW_MD2MD) {
					if (atomic_add_unless(
						&md1_md3_smem_clear, 1, 1))
						memset_io(
						regions[i].base_ap_view_vir,
							0, regions[i].size);
				} else if (regions[i].size) {
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

/* setup function is only for data structure initialization */
struct ccci_modem *ccci_md_alloc(int private_size)
{
	struct ccci_modem *md = kzalloc(sizeof(struct ccci_modem), GFP_KERNEL);

	if (!md) {
		CCCI_ERROR_LOG(-1, TAG,
			"fail to allocate memory for modem structure\n");
		goto out;
	}
	if (private_size > 0)
		md->private_data = kzalloc(private_size, GFP_KERNEL);
	else
		md->private_data = NULL;
	md->per_md_data.config.setting |= MD_SETTING_FIRST_BOOT;
	md->per_md_data.is_in_ee_dump = 0;
	md->is_force_asserted = 0;
	md->per_md_data.md_dbg_dump_flag = MD_DBG_DUMP_AP_REG;
	md->needforcestop = 0;
 out:
	return md;
}

static inline int log2_remain(unsigned int value)
{
	int x = 0;
	int y;

	if (value < 32)
		return -1;

	/* value = (2^x)*y */
	while (!(value & (1 << x)))
		x++;
	y = (value >> x);
	if ((1 << x) * y != value)
		WARN_ON(1);

	return y;
}

void ccci_md_smem_layout_config(struct ccci_modem *md)
{
	struct ccci_mem_layout *mm_str = &md->mem_layout;
	unsigned int md_resv_mem_offset = 0, ccb_offset = 0;
	unsigned int md_resv_mem_size = 0, ccb_size = 0;
	int dfd_size;
	unsigned int offset_adjust_flag = 0;
	unsigned int udc_noncache_size = 0, udc_cache_size = 0;
	unsigned int i;

	/* non-cacheable start */
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
				get_md_resv_phy_cap_size(MD_SYS1);
			CCCI_BOOTUP_LOG(md->index, TAG,
			"PHY size:%d\n",
			md1_6293_noncacheable_fat[i].size);
			break;
		case SMEM_USER_RAW_DFD:
			dfd_size = get_md_smem_dfd_size(MD_SYS1);
			if (dfd_size >= 0 && dfd_size !=
			md1_6293_noncacheable_fat[i].size) {
				md1_6293_noncacheable_fat[i].size = dfd_size;
				offset_adjust_flag = 1;
			}
			CCCI_BOOTUP_LOG(md->index, TAG,
			"dfd size:%d\n",
			md1_6293_noncacheable_fat[i].size);
			break;
		case SMEM_USER_RAW_UDC_DATA:
			get_md_resv_udc_info(md->index,
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
	get_md_resv_csmem_info(md->index,
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
		CCCI_ERROR_LOG(md->index, TAG,
			"get cacheable info base:%lx size:%x\n",
			(unsigned long)
			mm_str->md_bank4_cacheable_total.base_ap_view_phy,
			mm_str->md_bank4_cacheable_total.size);

	mm_str->md_bank4_cacheable_total.base_md_view_phy = 0x40000000
		+ get_md_smem_cachable_offset(MD_SYS1)
		+ mm_str->md_bank4_cacheable_total.base_ap_view_phy -
		round_down(mm_str->md_bank4_cacheable_total.base_ap_view_phy,
			0x00100000);

	/* specially, CCB size. */
	/* get_md_resv_ccb_info(md->index, &ccb_offset, &ccb_size); */
	get_md_cache_region_info(SMEM_USER_CCB_START,
				&ccb_offset,
				&ccb_size);
	CCCI_BOOTUP_LOG(md->index, TAG,
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
			CCCI_BOOTUP_LOG(md->index, TAG,
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

void ccci_md_config(struct ccci_modem *md)
{
	phys_addr_t md_resv_mem_addr = 0,
		md_resv_smem_addr = 0, md1_md3_smem_phy = 0;
	unsigned int md_resv_mem_size = 0,
		md_resv_smem_size = 0, md1_md3_smem_size = 0;
#if (MD_GENERATION == 6293)
	unsigned int md_bank4_cacheable_total_size = 0;
	unsigned int udc_noncache_size = 0, udc_cache_size = 0;
	int offset_adjust_flag = 0;
	struct ccci_smem_region *smem_region = NULL;
	int smem_dfd_size = -1;
	unsigned int i;
#endif
	/* setup config */
	md->per_md_data.config.load_type = get_md_img_type(md->index);
	if (get_modem_is_enabled(md->index))
		md->per_md_data.config.setting |= MD_SETTING_ENABLE;
	else
		md->per_md_data.config.setting &= ~MD_SETTING_ENABLE;

	/* Get memory info */
	get_md_resv_mem_info(md->index, &md_resv_mem_addr,
		&md_resv_mem_size, &md_resv_smem_addr, &md_resv_smem_size);
	get_md1_md3_resv_smem_info(md->index, &md1_md3_smem_phy,
		&md1_md3_smem_size);
	/* setup memory layout */
	/* MD image */
	md->mem_layout.md_bank0.base_ap_view_phy = md_resv_mem_addr;
	md->mem_layout.md_bank0.size = md_resv_mem_size;
	/* do not remap whole region, consume too much vmalloc space */
	md->mem_layout.md_bank0.base_ap_view_vir =
		ccci_map_phy_addr(
			md->mem_layout.md_bank0.base_ap_view_phy,
			MD_IMG_DUMP_SIZE);
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
	if (md->index == MD_SYS1)
		md->mem_layout.md_bank4_noncacheable_total.base_ap_view_phy
			= md_resv_smem_addr;
	else if (md->index == MD_SYS3)
		md->mem_layout.md_bank4_noncacheable_total.base_ap_view_phy
			= md1_md3_smem_phy;
	md->mem_layout.md_bank4_noncacheable_total.size
			= md_resv_smem_size + md1_md3_smem_size;
	md->mem_layout.md_bank4_noncacheable_total.base_ap_view_vir =
		ccci_map_phy_addr(
			md->mem_layout.md_bank4_noncacheable_total.base_ap_view_phy,
			md->mem_layout.md_bank4_noncacheable_total.size);
	md->mem_layout.md_bank4_noncacheable_total.base_md_view_phy =
		0x40000000 +
		md->mem_layout.md_bank4_noncacheable_total.base_ap_view_phy -
		round_down(
		md->mem_layout.md_bank4_noncacheable_total.base_ap_view_phy,
		0x02000000);
#if (MD_GENERATION >= 6295)
	ccci_md_smem_layout_config(md);
#else
	/* cacheable region */
#if (MD_GENERATION >= 6292)
	get_md_resv_ccb_info(md->index,
		&md->mem_layout.md_bank4_cacheable_total.base_ap_view_phy,
		&md->mem_layout.md_bank4_cacheable_total.size);
	if (md->mem_layout.md_bank4_cacheable_total.base_ap_view_phy &&
		md->mem_layout.md_bank4_cacheable_total.size)
		md->mem_layout.md_bank4_cacheable_total.base_ap_view_vir =
			ccci_map_phy_addr(
			md->mem_layout.md_bank4_cacheable_total.base_ap_view_phy,
			md->mem_layout.md_bank4_cacheable_total.size);
	else
		CCCI_ERROR_LOG(md->index, TAG,
			"get ccb info base:%lx size:%x\n",
			(unsigned long)md->mem_layout.md_bank4_cacheable_total.base_ap_view_phy,
			md->mem_layout.md_bank4_cacheable_total.size);
#if (MD_GENERATION >= 6293)
	if (md->index == MD_SYS1) {
		md->mem_layout.md_bank4_cacheable_total.base_md_view_phy = 0x40000000
			+ (224 * 1024 * 1024)
			+ md->mem_layout.md_bank4_cacheable_total.base_ap_view_phy -
			round_down(md->mem_layout.md_bank4_cacheable_total.base_ap_view_phy, 0x00100000);
	}
#else
	if (md->index == MD_SYS1) {
		md->mem_layout.md_bank4_cacheable_total.base_md_view_phy = 0x40000000
			+ get_md_smem_cachable_offset(MD_SYS1)
			+ md->mem_layout.md_bank4_cacheable_total.base_ap_view_phy -
			round_down(md->mem_layout.md_bank4_cacheable_total.base_ap_view_phy, 0x00100000);
	}
#endif
#endif
#endif
	CCCI_BOOTUP_LOG(md->index, TAG,
		"smem info: (%lx %lx %p %d) (%lx %lx %p %d)\n",
		(unsigned long)md->mem_layout.md_bank4_noncacheable_total.base_ap_view_phy,
		(unsigned long)md->mem_layout.md_bank4_noncacheable_total.base_md_view_phy,
		md->mem_layout.md_bank4_noncacheable_total.base_ap_view_vir,
		md->mem_layout.md_bank4_noncacheable_total.size,
		(unsigned long)md->mem_layout.md_bank4_cacheable_total.base_ap_view_phy,
		(unsigned long)md->mem_layout.md_bank4_cacheable_total.base_md_view_phy,
		md->mem_layout.md_bank4_cacheable_total.base_ap_view_vir,
		md->mem_layout.md_bank4_cacheable_total.size);

#if (MD_GENERATION == 6293)
	/* Get udc cache&noncache size */
	get_md_resv_udc_info(md->index,
			&udc_noncache_size, &udc_cache_size);

	md_bank4_cacheable_total_size
		= md->mem_layout.md_bank4_cacheable_total.size;
	if (md->index == MD_SYS1) {
		md->mem_layout.md_bank4_noncacheable
			= md1_6293_noncacheable_fat;
		md->mem_layout.md_bank4_cacheable
			= md1_6293_cacheable;

		/* Runtime adjust md_phy_capture size */
		smem_dfd_size = get_md_smem_dfd_size(MD_SYS1);
		for (i = 0; i < (sizeof(md1_6293_noncacheable_fat)/
			sizeof(struct ccci_smem_region)); i++) {

			if (offset_adjust_flag == 1)
				md1_6293_noncacheable_fat[i].offset =
				md1_6293_noncacheable_fat[i-1].offset
				+ md1_6293_noncacheable_fat[i-1].size;

			smem_region = &md1_6293_noncacheable_fat[i];
			if (smem_region->id == SMEM_USER_RAW_DFD &&
				smem_dfd_size > -1 &&
				smem_dfd_size != smem_region->size) {
				offset_adjust_flag = 1;
				smem_region->size = smem_dfd_size;

				CCCI_BOOTUP_LOG(md->index, TAG,
					"smem_region->size = %d;\n",
					smem_region->size);
			}

			if (md1_6293_noncacheable_fat[i].id ==
				SMEM_USER_RAW_PHY_CAP) {
				offset_adjust_flag = 1;
				md1_6293_noncacheable_fat[i].size =
					get_md_resv_phy_cap_size(MD_SYS1);
				CCCI_BOOTUP_LOG(md->index, TAG,
				"PHY size:%d\n",
				md1_6293_noncacheable_fat[i].size);
			}
			if (md1_6293_noncacheable_fat[i].id ==
				SMEM_USER_RAW_UDC_DATA) {
				md1_6293_noncacheable_fat[i].size =
					udc_noncache_size;
				offset_adjust_flag = 1;
			}
		}
		if (md_bank4_cacheable_total_size
			>= CCB_CACHE_MIN_SIZE) {
			/*
			 * 2M is control part size,
			 *md1_6293_cacheable[0].size
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
						CCCI_BOOTUP_LOG(md->index, TAG,
						"UDC offset:%d\n",
						md1_6293_cacheable[i].offset);
						md1_6293_cacheable[i].size
							= udc_cache_size;
						continue;
					}
					md1_6293_cacheable[i].size =
					md_bank4_cacheable_total_size -
					udc_cache_size - CCB_CACHE_MIN_SIZE;

					CCCI_BOOTUP_LOG(md->index, TAG,
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
					CCCI_BOOTUP_LOG(md->index, TAG,
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
			md->mem_layout.md_bank4_cacheable = NULL;
	} else {
		WARN_ON(1);
	}
#elif (MD_GENERATION == 6292)
	if (md->index == MD_SYS1) {
		md->mem_layout.md_bank4_noncacheable
			= md1_6292_noncacheable_fat;
		md->mem_layout.md_bank4_cacheable
			= md1_6292_cacheable;
	} else if (md->index == MD_SYS3) {
		md->mem_layout.md_bank4_noncacheable
			= md3_6292_noncacheable_fat;
	}
#elif (MD_GENERATION == 6291)
	if (md->index == MD_SYS1)
		md->mem_layout.md_bank4_noncacheable
			= md1_6291_noncacheable_fat;
	else if (md->index == MD_SYS3)
		md->mem_layout.md_bank4_noncacheable
			= md3_6291_noncacheable_fat;
#endif

	init_smem_regions(md->mem_layout.md_bank4_noncacheable,
		md->mem_layout.md_bank4_noncacheable_total.base_ap_view_phy,
		md->mem_layout.md_bank4_noncacheable_total.base_ap_view_vir,
		md->mem_layout.md_bank4_noncacheable_total.base_md_view_phy);
	init_smem_regions(md->mem_layout.md_bank4_cacheable,
		md->mem_layout.md_bank4_cacheable_total.base_ap_view_phy,
		md->mem_layout.md_bank4_cacheable_total.base_ap_view_vir,
		md->mem_layout.md_bank4_cacheable_total.base_md_view_phy);

	/* updae image info */
	md->per_md_data.img_info[IMG_MD].type = IMG_MD;
	md->per_md_data.img_info[IMG_MD].address =
		md->mem_layout.md_bank0.base_ap_view_phy;
	md->per_md_data.img_info[IMG_DSP].type = IMG_DSP;
	md->per_md_data.img_info[IMG_ARMV7].type = IMG_ARMV7;
}

int ccci_md_register(struct ccci_modem *md)
{
	int ret;

	/* init per-modem sub-system */
	CCCI_INIT_LOG(md->index, TAG, "register modem\n");
	/* init modem */
	ret = md->ops->init(md);
	if (ret < 0)
		return ret;
	ccci_md_config(md);

	modem_sys[md->index] = md;
	ccci_sysfs_add_md(md->index, (void *)&md->kobj);
	ccci_platform_init(md);
	ccci_fsm_init(md->index);
	ccci_port_init(md->index);
	return 0;
}

int ccci_md_set_boot_data(unsigned char md_id, unsigned int data[], int len)
{
	int ret = 0;
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	if (len < 0 || data == NULL)
		return -1;

	md->mdlg_mode = data[MD_CFG_MDLOG_MODE];
	md->sbp_code  = data[MD_CFG_SBP_CODE];
	md->per_md_data.md_dbg_dump_flag =
		data[MD_CFG_DUMP_FLAG] == MD_DBG_DUMP_INVALID ?
		md->per_md_data.md_dbg_dump_flag : data[MD_CFG_DUMP_FLAG];

	return ret;
}

struct ccci_mem_layout *ccci_md_get_mem(int md_id)
{
	if (md_id >= MAX_MD_NUM)
		return NULL;
	return &modem_sys[md_id]->mem_layout;
}

struct ccci_smem_region *ccci_md_get_smem_by_user_id(int md_id,
	enum SMEM_USER_ID user_id)
{
	struct ccci_smem_region *curr = NULL;

	if (md_id >= MAX_MD_NUM)
		return NULL;

	if (modem_sys[md_id] == NULL) {
		CCCI_ERROR_LOG(md_id, TAG,
			"md%d not enable/ before driver int, return NULL\n",
			md_id);
		return NULL;
	}

	curr = get_smem_by_user_id(
		modem_sys[md_id]->mem_layout.md_bank4_noncacheable, user_id);
	if (curr)
		return curr;
	curr = get_smem_by_user_id(
		modem_sys[md_id]->mem_layout.md_bank4_cacheable, user_id);
	return curr;
}

void ccci_md_clear_smem(int md_id, int first_boot)
{
	struct ccci_smem_region *region;
	unsigned int size;

	/* MD will clear share memory itself after the first boot */
	clear_smem_region(modem_sys[md_id]->mem_layout.md_bank4_noncacheable,
		first_boot);
	clear_smem_region(modem_sys[md_id]->mem_layout.md_bank4_cacheable,
		first_boot);
	if (!first_boot) {
		CCCI_NORMAL_LOG(-1, TAG, "clear buffer ! first_boot\n");
		region = ccci_md_get_smem_by_user_id(md_id, SMEM_USER_CCB_DHL);
		if (region && region->size) {
			/*clear ccb data smem*/
			memset_io(region->base_ap_view_vir, 0, region->size);
		}
		region = ccci_md_get_smem_by_user_id(md_id, SMEM_USER_RAW_DHL);
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

int ccci_md_pre_stop(unsigned char md_id, unsigned int stop_type)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	return md->ops->pre_stop(md, stop_type);
}

int ccci_md_stop(unsigned char md_id, unsigned int stop_type)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	return md->ops->stop(md, stop_type);
}

int __weak md_cd_vcore_config(unsigned int md_id, unsigned int hold_req)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return 0;
}

int ccci_md_pre_start(unsigned char md_id)
{
	return md_cd_vcore_config(md_id, 1);
}
int ccci_md_start(unsigned char md_id)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	return md->ops->start(md);
}
int ccci_md_post_start(unsigned char md_id)
{
	return md_cd_vcore_config(md_id, 0);
}
int ccci_md_soft_stop(unsigned char md_id, unsigned int sim_mode)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	if (md->ops->soft_stop)
		return md->ops->soft_stop(md, sim_mode);
	return -1;
}
int ccci_md_soft_start(unsigned char md_id, unsigned int sim_mode)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	if (md->ops->soft_start)
		return md->ops->soft_start(md, sim_mode);
	return -1;
}

int ccci_md_send_runtime_data(unsigned char md_id)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	return md->ops->send_runtime_data(md, CCCI_CONTROL_TX, 0, 0);
}

int ccci_md_reset_pccif(unsigned char md_id)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	if (md->ops->reset_pccif)
		return md->ops->reset_pccif(md);
	return -1;
}

void ccci_md_dump_info(unsigned char md_id, MODEM_DUMP_FLAG flag,
	void *buff, int length)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	if (md)
		md->ops->dump_info(md, flag, buff, length);
	else
		CCCI_ERROR_LOG(md_id, TAG, "invalid md_id %d!!\n", md_id);
}

void ccci_md_exception_handshake(unsigned char md_id, int timeout)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	md->ops->ee_handshake(md, timeout);
}

int ccci_md_send_ccb_tx_notify(unsigned char md_id, int core_id)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	return md->ops->send_ccb_tx_notify(md, core_id);
}

int ccci_md_force_assert(unsigned char md_id, MD_FORCE_ASSERT_TYPE type,
	char *param, int len)
{
	int ret = 0;
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);
	struct ccci_force_assert_shm_fmt *ccci_fa_smem_ptr = NULL;
	struct ccci_smem_region *force_assert =
		ccci_md_get_smem_by_user_id(md_id, SMEM_USER_RAW_FORCE_ASSERT);

	if (md->is_force_asserted != 0)
		return ret;
	mdee_set_ex_time_str(md_id, type, param);
	if (type == MD_FORCE_ASSERT_BY_AP_MPU) {
		ret = md->ops->force_assert(md, CCIF_MPU_INTR);
	} else {
		ccci_fa_smem_ptr = (struct ccci_force_assert_shm_fmt *)
			force_assert->base_ap_view_vir;
		if (ccci_fa_smem_ptr) {
			ccci_fa_smem_ptr->error_code = type;
			if (param != NULL && len > 0) {
				if (len > force_assert->size -
				sizeof(struct ccci_force_assert_shm_fmt))
					len = force_assert->size -
					sizeof(
					struct ccci_force_assert_shm_fmt);
				memcpy_toio(ccci_fa_smem_ptr->param,
					param, len);
			}
		}
		ret = md->ops->force_assert(md, CCIF_INTERRUPT);
	}
	md->is_force_asserted = 1;
	return ret;
}

static void append_runtime_feature(char **p_rt_data,
	struct ccci_runtime_feature *rt_feature, void *data)
{
	CCCI_DEBUG_LOG(-1, TAG,
		"append rt_data %p, feature %u len %u\n",
		*p_rt_data, rt_feature->feature_id,
		rt_feature->data_len);
	memcpy_toio(*p_rt_data, rt_feature,
		sizeof(struct ccci_runtime_feature));
	*p_rt_data += sizeof(struct ccci_runtime_feature);
	if (data != NULL) {
		memcpy_toio(*p_rt_data, data, rt_feature->data_len);
		*p_rt_data += rt_feature->data_len;
	}
}

static unsigned int get_booting_start_id(struct ccci_modem *md)
{
	LOGGING_MODE mdlog_flag = MODE_IDLE;
	u32 md_wait_time = 0;
	u32 booting_start_id;

	mdlog_flag = md->mdlg_mode & 0x0000ffff;
	md_wait_time = md->mdlg_mode >> 16;
	if (md->per_md_data.md_boot_mode != MD_BOOT_MODE_INVALID) {
		if (md->per_md_data.md_boot_mode == MD_BOOT_MODE_META)
			booting_start_id = ((char)mdlog_flag << 8
								| META_BOOT_ID);
		else if ((get_boot_mode() == FACTORY_BOOT ||
				get_boot_mode() == ATE_FACTORY_BOOT))
			booting_start_id = ((char)mdlog_flag << 8
							| FACTORY_BOOT_ID);
		else
			booting_start_id = ((char)mdlog_flag << 8
							| NORMAL_BOOT_ID);
	} else {
		if (is_meta_mode() || is_advanced_meta_mode())
			booting_start_id = ((char)mdlog_flag << 8
							| META_BOOT_ID);
		else if ((get_boot_mode() == FACTORY_BOOT ||
				get_boot_mode() == ATE_FACTORY_BOOT))
			booting_start_id = ((char)mdlog_flag << 8
							| FACTORY_BOOT_ID);
		else
			booting_start_id = ((char)mdlog_flag << 8
							| NORMAL_BOOT_ID);
	}
	booting_start_id |= md_wait_time << 16;

	CCCI_BOOTUP_LOG(md->index, TAG,
		"get_booting_start_id 0x%x\n", booting_start_id);
	return booting_start_id;
}

static void config_ap_side_feature(struct ccci_modem *md,
	struct md_query_ap_feature *ap_side_md_feature)
{
#if (MD_GENERATION >= 6293)
	unsigned int udc_noncache_size = 0, udc_cache_size = 0;
#endif
	md->runtime_version = AP_MD_HS_V2;
	ap_side_md_feature->feature_set[BOOT_INFO].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[EXCEPTION_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
#if (MD_GENERATION >= 6293)
	ap_side_md_feature->feature_set[CCIF_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
#else
	if (md->index == MD_SYS1)
		ap_side_md_feature->feature_set[CCIF_SHARE_MEMORY].support_mask
			= CCCI_FEATURE_NOT_SUPPORT;
	else
		ap_side_md_feature->feature_set[CCIF_SHARE_MEMORY].support_mask
			= CCCI_FEATURE_MUST_SUPPORT;
#endif

#ifdef FEATURE_SCP_CCCI_SUPPORT
	ap_side_md_feature->feature_set[CCISM_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
#else
	ap_side_md_feature->feature_set[CCISM_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
#endif

#if (MD_GENERATION >= 6293)
	ap_side_md_feature->feature_set[CCISM_SHARE_MEMORY_EXP].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	if ((md->index == MD_SYS1) && (get_md_resv_phy_cap_size(MD_SYS1) > 0))
		ap_side_md_feature->feature_set[MD_PHY_CAPTURE].support_mask
			= CCCI_FEATURE_MUST_SUPPORT;
	else
		ap_side_md_feature->feature_set[MD_PHY_CAPTURE].support_mask
			= CCCI_FEATURE_NOT_SUPPORT;
	ap_side_md_feature->feature_set[MD_CONSYS_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[MD1MD3_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;

	get_md_resv_udc_info(md->index, &udc_noncache_size, &udc_cache_size);
	if (udc_noncache_size > 0 && udc_cache_size > 0)
		ap_side_md_feature->feature_set[UDC_RAW_SHARE_MEMORY].
			support_mask = CCCI_FEATURE_MUST_SUPPORT;
	else
		ap_side_md_feature->feature_set[UDC_RAW_SHARE_MEMORY].
			support_mask = CCCI_FEATURE_NOT_SUPPORT;
#else
	ap_side_md_feature->feature_set[CCISM_SHARE_MEMORY_EXP].
	support_mask = CCCI_FEATURE_NOT_SUPPORT;
	ap_side_md_feature->feature_set[MD_PHY_CAPTURE].
	support_mask = CCCI_FEATURE_NOT_SUPPORT;
	ap_side_md_feature->feature_set[MD_CONSYS_SHARE_MEMORY].
	support_mask = CCCI_FEATURE_NOT_SUPPORT;
	if (get_modem_is_enabled(MD_SYS3))
		ap_side_md_feature->feature_set[MD1MD3_SHARE_MEMORY].
		support_mask = CCCI_FEATURE_MUST_SUPPORT;
	else
		ap_side_md_feature->feature_set[MD1MD3_SHARE_MEMORY].
		support_mask = CCCI_FEATURE_NOT_SUPPORT;

#endif

#if (MD_GENERATION >= 6293)
	/* notice: CCB_SHARE_MEMORY should be set to support
	 * when at least one CCB region exists
	 */
	ap_side_md_feature->feature_set[CCB_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[DHL_RAW_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[LWA_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[DT_NETD_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[DT_USB_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[AUDIO_RAW_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
#elif (MD_GENERATION == 6292)
	/* notice: CCB_SHARE_MEMORY should be set to support
	 * when at least one CCB region exists
	 */
	ap_side_md_feature->feature_set[CCB_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
	ap_side_md_feature->feature_set[DHL_RAW_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
	ap_side_md_feature->feature_set[LWA_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[DT_NETD_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[DT_USB_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[AUDIO_RAW_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
#else
	ap_side_md_feature->feature_set[CCB_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
	ap_side_md_feature->feature_set[DHL_RAW_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
	ap_side_md_feature->feature_set[LWA_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
	ap_side_md_feature->feature_set[DT_NETD_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
	ap_side_md_feature->feature_set[DT_USB_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
	ap_side_md_feature->feature_set[AUDIO_RAW_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
#endif
	ap_side_md_feature->feature_set[MISC_INFO_HIF_DMA_REMAP].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[MULTI_MD_MPU].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;

#ifdef ENABLE_32K_CLK_LESS
	if (crystal_exist_status()) {
		CCCI_DEBUG_LOG(md->index, TAG,
			"MISC_32K_LESS no support, crystal_exist_status 1\n");
		ap_side_md_feature->feature_set[MISC_INFO_RTC_32K_LESS].
		support_mask = CCCI_FEATURE_NOT_SUPPORT;
	} else {
		CCCI_DEBUG_LOG(md->index, TAG, "MISC_32K_LESS support\n");
		ap_side_md_feature->feature_set[MISC_INFO_RTC_32K_LESS].
		support_mask = CCCI_FEATURE_MUST_SUPPORT;
	}
#else
	CCCI_DEBUG_LOG(md->index, TAG, "ENABLE_32K_CLK_LESS disabled\n");
	ap_side_md_feature->feature_set[MISC_INFO_RTC_32K_LESS].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
#endif
	ap_side_md_feature->feature_set[MISC_INFO_RANDOM_SEED_NUM].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[MISC_INFO_GPS_COCLOCK].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[MISC_INFO_SBP_ID].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[MISC_INFO_CCCI].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[MISC_INFO_CLIB_TIME].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[MISC_INFO_C2K].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[MD_IMAGE_START_MEMORY].support_mask
		= CCCI_FEATURE_OPTIONAL_SUPPORT;
	ap_side_md_feature->feature_set[EE_AFTER_EPOF].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[AP_CCMNI_MTU].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;

#ifdef ENABLE_FAST_HEADER
	ap_side_md_feature->feature_set[CCCI_FAST_HEADER].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
#endif

	/* tire1 features */
#ifdef FEATURE_TC1_CUSTOMER_VAL
	ap_side_md_feature->feature_set[MISC_INFO_CUSTOMER_VAL].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
#else
	ap_side_md_feature->feature_set[MISC_INFO_CUSTOMER_VAL].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
#endif
#ifdef FEATURE_SYNC_C2K_MEID
	ap_side_md_feature->feature_set[MISC_INFO_C2K_MEID].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
#else
	ap_side_md_feature->feature_set[MISC_INFO_C2K_MEID].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
#endif
	ap_side_md_feature->feature_set[SMART_LOGGING_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;

#if (MD_GENERATION >= 6295)
	ap_side_md_feature->feature_set[MD_USIP_SHARE_MEMORY].support_mask =
		CCCI_FEATURE_OPTIONAL_SUPPORT;
#else
	ap_side_md_feature->feature_set[MD_USIP_SHARE_MEMORY].support_mask =
		CCCI_FEATURE_NOT_SUPPORT;
#endif
	ap_side_md_feature
	->feature_set[MD_MTEE_SHARE_MEMORY_ENABLE].support_mask
		= CCCI_FEATURE_OPTIONAL_SUPPORT;
}

unsigned int align_to_2_power(unsigned int n)
{
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n++;

	return n;
}

static void ccci_smem_region_set_runtime(unsigned char md_id, unsigned int id,
	struct ccci_runtime_feature *rt_feature,
	struct ccci_runtime_share_memory *rt_shm)
{
	struct ccci_smem_region *region = ccci_md_get_smem_by_user_id(md_id,
		id);

	if (region) {
		rt_feature->data_len =
			sizeof(struct ccci_runtime_share_memory);
		rt_shm->addr = region->base_md_view_phy;
		rt_shm->size = region->size;
	} else {
		rt_feature->data_len =
			sizeof(struct ccci_runtime_share_memory);
		rt_shm->addr = 0;
		rt_shm->size = 0;
	}
}

int ccci_md_prepare_runtime_data(unsigned char md_id, unsigned char *data,
	int length)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);
	u8 i = 0;
	u32 total_len;
	int j;
	/*runtime data buffer */
	struct ccci_smem_region *region;
	struct ccci_smem_region *rt_data_region =
		ccci_md_get_smem_by_user_id(md_id, SMEM_USER_RAW_RUNTIME_DATA);
	char *rt_data = (char *)rt_data_region->base_ap_view_vir;

	struct ccci_runtime_feature rt_feature;
	/*runtime feature type */
	struct ccci_runtime_share_memory rt_shm;
	struct ccci_misc_info_element rt_f_element;

	struct md_query_ap_feature *md_feature;
	struct md_query_ap_feature md_feature_ap;
	struct ccci_runtime_boot_info boot_info;
	unsigned int random_seed = 0;
	struct timeval t;
	unsigned int c2k_flags = 0;

	CCCI_BOOTUP_LOG(md->index, TAG,
		"prepare_runtime_data  AP total %u features\n",
		MD_RUNTIME_FEATURE_ID_MAX);

	memset(&md_feature_ap, 0, sizeof(struct md_query_ap_feature));
	config_ap_side_feature(md, &md_feature_ap);

	md_feature = (struct md_query_ap_feature *)(data +
				sizeof(struct ccci_header));

	if (md_feature->head_pattern != MD_FEATURE_QUERY_PATTERN ||
	    md_feature->tail_pattern != MD_FEATURE_QUERY_PATTERN) {
		CCCI_BOOTUP_LOG(md->index, TAG,
			"md_feature pattern is wrong: head 0x%x, tail 0x%x\n",
			md_feature->head_pattern, md_feature->tail_pattern);
		if (md->index == MD_SYS3)
			md->ops->dump_info(md, DUMP_FLAG_CCIF, NULL, 0);
		return -1;
	}

	for (i = BOOT_INFO; i < FEATURE_COUNT; i++) {
		memset(&rt_feature, 0, sizeof(struct ccci_runtime_feature));
		memset(&rt_shm, 0, sizeof(struct ccci_runtime_share_memory));
		memset(&rt_f_element, 0, sizeof(struct ccci_misc_info_element));
		rt_feature.feature_id = i;
		if (md_feature->feature_set[i].support_mask ==
			CCCI_FEATURE_MUST_SUPPORT &&
		    md_feature_ap.feature_set[i].support_mask <
			CCCI_FEATURE_MUST_SUPPORT) {
			CCCI_BOOTUP_LOG(md->index, TAG,
				"feature %u not support for AP\n",
				rt_feature.feature_id);
			return -1;
		}

		CCCI_DEBUG_LOG(md->index, TAG,
			"ftr %u mask %u, ver %u\n",
			rt_feature.feature_id,
			md_feature->feature_set[i].support_mask,
			md_feature->feature_set[i].version);

		if (md_feature->feature_set[i].support_mask ==
			CCCI_FEATURE_NOT_EXIST) {
			rt_feature.support_info =
				md_feature->feature_set[i];
		} else if (md_feature->feature_set[i].support_mask ==
			CCCI_FEATURE_MUST_SUPPORT) {
			rt_feature.support_info =
				md_feature->feature_set[i];
		} else if (md_feature->feature_set[i].support_mask ==
			CCCI_FEATURE_OPTIONAL_SUPPORT) {
			if (md_feature->feature_set[i].version ==
			md_feature_ap.feature_set[i].version &&
			md_feature_ap.feature_set[i].support_mask >=
			CCCI_FEATURE_MUST_SUPPORT) {
				rt_feature.support_info.support_mask =
					CCCI_FEATURE_MUST_SUPPORT;
				rt_feature.support_info.version =
					md_feature_ap.feature_set[i].version;
			} else {
				rt_feature.support_info.support_mask =
					CCCI_FEATURE_NOT_SUPPORT;
				rt_feature.support_info.version =
					md_feature_ap.feature_set[i].version;
			}
		} else if (md_feature->feature_set[i].support_mask ==
			CCCI_FEATURE_SUPPORT_BACKWARD_COMPAT) {
			if (md_feature->feature_set[i].version >=
				md_feature_ap.feature_set[i].version) {
				rt_feature.support_info.support_mask =
					CCCI_FEATURE_MUST_SUPPORT;
				rt_feature.support_info.version =
					md_feature_ap.feature_set[i].version;
			} else {
				rt_feature.support_info.support_mask =
					CCCI_FEATURE_NOT_SUPPORT;
				rt_feature.support_info.version =
					md_feature_ap.feature_set[i].version;
			}
		}

		if (rt_feature.support_info.support_mask ==
		CCCI_FEATURE_MUST_SUPPORT) {
			switch (rt_feature.feature_id) {
			case BOOT_INFO:
				memset(&boot_info, 0, sizeof(boot_info));
				rt_feature.data_len = sizeof(boot_info);
				boot_info.boot_channel = CCCI_CONTROL_RX;
				boot_info.booting_start_id =
					get_booting_start_id(md);
				append_runtime_feature(&rt_data,
					&rt_feature, &boot_info);
				break;
			case EXCEPTION_SHARE_MEMORY:
				region = ccci_md_get_smem_by_user_id(md_id,
					SMEM_USER_RAW_MDCCCI_DBG);
				rt_feature.data_len =
				sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr = region->base_md_view_phy;
				rt_shm.size = CCCI_EE_SMEM_TOTAL_SIZE;
				append_runtime_feature(&rt_data,
					&rt_feature, &rt_shm);
				break;
			case CCIF_SHARE_MEMORY:
				ccci_smem_region_set_runtime(md_id,
					SMEM_USER_CCISM_MCU,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data,
					&rt_feature, &rt_shm);
				break;
			case CCISM_SHARE_MEMORY:
				ccci_smem_region_set_runtime(md_id,
					SMEM_USER_CCISM_SCP,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data,
					&rt_feature, &rt_shm);
				break;
			case CCB_SHARE_MEMORY:
				/* notice: we should add up
				 * all CCB region size here
				 */
				/* ctrl control first */
				region = ccci_md_get_smem_by_user_id(md_id,
					SMEM_USER_RAW_CCB_CTRL);
				if (region) {
					rt_feature.data_len =
					sizeof(struct ccci_misc_info_element);
					rt_f_element.feature[0] =
					region->base_md_view_phy;
					rt_f_element.feature[1] =
					region->size;
				}
				/* ccb data second */
				for (j = SMEM_USER_CCB_START;
					j <= SMEM_USER_CCB_END; j++) {
					region = ccci_md_get_smem_by_user_id(
						md_id, j);
					if (j == SMEM_USER_CCB_START
						&& region) {
						rt_f_element.feature[2] =
						region->base_md_view_phy;
						rt_f_element.feature[3] = 0;
					} else if (j == SMEM_USER_CCB_START
							&& region == NULL)
						break;
					if (region)
						rt_f_element.feature[3] +=
						region->size;
				}
				CCCI_BOOTUP_LOG(md->index, TAG,
					"ccb data size (include dsp raw): %X\n",
					rt_f_element.feature[3]);

				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case DHL_RAW_SHARE_MEMORY:
				ccci_smem_region_set_runtime(md_id,
					SMEM_USER_RAW_DHL,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data,
					&rt_feature, &rt_shm);
				break;
			case LWA_SHARE_MEMORY:
				ccci_smem_region_set_runtime(md_id,
					SMEM_USER_RAW_LWA,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_shm);
				break;
			case MULTI_MD_MPU:
				CCCI_BOOTUP_LOG(md->index, TAG,
				"new version md use multi-MPU.\n");
				md->multi_md_mpu_support = 1;
				rt_feature.data_len = 0;
				append_runtime_feature(&rt_data,
				&rt_feature, NULL);
				break;
			case DT_NETD_SHARE_MEMORY:
				ccci_smem_region_set_runtime(md_id,
					SMEM_USER_RAW_NETD,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_shm);
				break;
			case DT_USB_SHARE_MEMORY:
				ccci_smem_region_set_runtime(md_id,
					SMEM_USER_RAW_USB,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_shm);
				break;
			case SMART_LOGGING_SHARE_MEMORY:
				ccci_smem_region_set_runtime(md_id,
					SMEM_USER_SMART_LOGGING,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_shm);
				break;
			case MD1MD3_SHARE_MEMORY:
				ccci_smem_region_set_runtime(md_id,
					SMEM_USER_RAW_MD2MD,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_shm);
				break;

			case MISC_INFO_HIF_DMA_REMAP:
				rt_feature.data_len =
				sizeof(struct ccci_misc_info_element);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case MISC_INFO_RTC_32K_LESS:
				rt_feature.data_len =
				sizeof(struct ccci_misc_info_element);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case MISC_INFO_RANDOM_SEED_NUM:
				rt_feature.data_len =
				sizeof(struct ccci_misc_info_element);
				get_random_bytes(&random_seed, sizeof(int));
				rt_f_element.feature[0] = random_seed;
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case MISC_INFO_GPS_COCLOCK:
				rt_feature.data_len =
				sizeof(struct ccci_misc_info_element);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case MISC_INFO_SBP_ID:
				rt_feature.data_len =
				sizeof(struct ccci_misc_info_element);
				rt_f_element.feature[0] = md->sbp_code;
				if (md->per_md_data.config.load_type
					< modem_ultg)
					rt_f_element.feature[1] = 0;
				else
					rt_f_element.feature[1] =
					get_wm_bitmap_for_ubin();
				CCCI_BOOTUP_LOG(md->index, TAG,
					"sbp=0x%x,wmid[%d]\n",
					rt_f_element.feature[0],
					rt_f_element.feature[1]);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case MISC_INFO_CCCI:
				rt_feature.data_len =
				sizeof(struct ccci_misc_info_element);
				/* sequence check */
				rt_f_element.feature[0] |= (1 << 0);
				/* polling MD status */
				rt_f_element.feature[0] |= (1 << 1);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case MISC_INFO_CLIB_TIME:
				rt_feature.data_len =
				sizeof(struct ccci_misc_info_element);
				do_gettimeofday(&t);

				/*set seconds information */
				rt_f_element.feature[0] =
				((unsigned int *)&t.tv_sec)[0];
				rt_f_element.feature[1] =
				((unsigned int *)&t.tv_sec)[1];
				/*sys_tz.tz_minuteswest; */
				rt_f_element.feature[2] = current_time_zone;
				/*not used for now */
				rt_f_element.feature[3] = sys_tz.tz_dsttime;
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case MISC_INFO_C2K:
				rt_feature.data_len =
				sizeof(struct ccci_misc_info_element);
				c2k_flags = 0;
#if defined(CONFIG_MTK_MD3_SUPPORT) && (CONFIG_MTK_MD3_SUPPORT > 0)
				c2k_flags |= (1 << 0);
#endif
				/* SVLTE_MODE */
				if (ccci_get_opt_val("opt_c2k_lte_mode") == 1)
					c2k_flags |= (1 << 1);
				/* SRLTE_MODE */
				if (ccci_get_opt_val("opt_c2k_lte_mode") == 2)
					c2k_flags |= (1 << 2);
#ifdef CONFIG_MTK_C2K_OM_SOLUTION1
				c2k_flags |=  (1 << 3);
#endif
#ifdef CONFIG_CT6M_SUPPORT
				c2k_flags |= (1 << 4)
#endif
				rt_f_element.feature[0] = c2k_flags;
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case MD_IMAGE_START_MEMORY:
				rt_feature.data_len =
				sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr =
				md->per_md_data.img_info[IMG_MD].address;
				rt_shm.size =
				md->per_md_data.img_info[IMG_MD].size;
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_shm);
				break;
			case EE_AFTER_EPOF:
				rt_feature.data_len =
				sizeof(struct ccci_misc_info_element);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case AP_CCMNI_MTU:
				rt_feature.data_len =
				sizeof(unsigned int);
				random_seed =
				NET_RX_BUF - sizeof(struct ccci_header);
				append_runtime_feature(&rt_data,
				&rt_feature, &random_seed);
				break;
			case CCCI_FAST_HEADER:
				rt_feature.data_len = sizeof(unsigned int);
				random_seed = 1;
				append_runtime_feature(&rt_data,
				&rt_feature, &random_seed);
				break;
			case AUDIO_RAW_SHARE_MEMORY:
				ccci_smem_region_set_runtime(md_id,
					SMEM_USER_RAW_AUDIO,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_shm);
				break;
			case CCISM_SHARE_MEMORY_EXP:
				ccci_smem_region_set_runtime(md_id,
					SMEM_USER_CCISM_MCU_EXP,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data, &rt_feature,
				&rt_shm);
				break;
			case MD_PHY_CAPTURE:
				ccci_smem_region_set_runtime(md_id,
					SMEM_USER_RAW_PHY_CAP,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data, &rt_feature,
				&rt_shm);
				break;
			case UDC_RAW_SHARE_MEMORY:
				region = ccci_md_get_smem_by_user_id(md_id,
					SMEM_USER_RAW_UDC_DATA);
				if (region) {
					rt_feature.data_len = sizeof(
						struct ccci_misc_info_element);
					rt_f_element.feature[0] =
						region->base_md_view_phy;
					rt_f_element.feature[1] =
						region->size;
				} else {
					rt_feature.data_len = sizeof(
						struct ccci_misc_info_element);
					rt_f_element.feature[0] = 0;
					rt_f_element.feature[1] = 0;
				}
				region = ccci_md_get_smem_by_user_id(md_id,
					SMEM_USER_RAW_UDC_DESCTAB);
				if (region) {
					rt_feature.data_len = sizeof(
						struct ccci_misc_info_element);
					rt_f_element.feature[2] =
						region->base_md_view_phy;
					rt_f_element.feature[3] =
						region->size;
				} else {
					rt_feature.data_len = sizeof(
						struct ccci_misc_info_element);
					rt_f_element.feature[2] = 0;
					rt_f_element.feature[3] = 0;
				}
				append_runtime_feature(&rt_data,
					&rt_feature, &rt_f_element);
				break;
			case MD_CONSYS_SHARE_MEMORY:
				ccci_smem_region_set_runtime(md_id,
					SMEM_USER_RAW_MD_CONSYS,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data, &rt_feature,
				&rt_shm);
				break;
			case MD_USIP_SHARE_MEMORY:
				ccci_smem_region_set_runtime(md_id,
					SMEM_USER_RAW_USIP,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data, &rt_feature,
				&rt_shm);
				break;
			case MD_MTEE_SHARE_MEMORY_ENABLE:
				rt_feature.data_len = sizeof(unsigned int);
				/* use the random_seed as temp_u32 value */
				random_seed = get_mtee_is_enabled();
				append_runtime_feature(&rt_data, &rt_feature,
				&random_seed);
				break;
			default:
				break;
			};
		} else {
			rt_feature.data_len = 0;
			append_runtime_feature(&rt_data, &rt_feature, NULL);
		}

	}

	total_len = rt_data - (char *)rt_data_region->base_ap_view_vir;
	CCCI_BOOTUP_DUMP_LOG(md->index, TAG, "AP runtime data\n");
	ccci_util_mem_dump(md->index, CCCI_DUMP_BOOTUP,
		rt_data_region->base_ap_view_vir, total_len);

	return 0;
}

struct ccci_runtime_feature *ccci_md_get_rt_feature_by_id(unsigned char md_id,
	u8 feature_id, u8 ap_query_md)
{
	struct ccci_runtime_feature *rt_feature = NULL;
	u8 i = 0;
	u8 max_id = 0;
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);
	struct ccci_smem_region *rt_data_region =
		ccci_md_get_smem_by_user_id(md_id, SMEM_USER_RAW_RUNTIME_DATA);

	if (ap_query_md) {
		rt_feature = (struct ccci_runtime_feature *)
		(rt_data_region->base_ap_view_vir +
			CCCI_SMEM_SIZE_RUNTIME_AP);
		max_id = AP_RUNTIME_FEATURE_ID_MAX;
	} else {
		rt_feature = (struct ccci_runtime_feature *)
		(rt_data_region->base_ap_view_vir);
		max_id = MD_RUNTIME_FEATURE_ID_MAX;
	}
	while (i < max_id) {
		if (feature_id == rt_feature->feature_id)
			return rt_feature;
		if (rt_feature->data_len >
			sizeof(struct ccci_misc_info_element)) {
			CCCI_ERROR_LOG(md->index, TAG,
				"get invalid feature, id %u\n", i);
			return NULL;
		}
		rt_feature = (struct ccci_runtime_feature *)
		((char *)rt_feature->data + rt_feature->data_len);
		i++;
	}

	return NULL;
}

int ccci_md_parse_rt_feature(unsigned char md_id,
	struct ccci_runtime_feature *rt_feature, void *data, u32 data_len)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	if (unlikely(!rt_feature)) {
		CCCI_ERROR_LOG(md->index, TAG,
			"parse_md_rt_feature: rt_feature == NULL\n");
		return -EFAULT;
	}
	if (unlikely(rt_feature->data_len > data_len ||
		rt_feature->data_len == 0)) {
		CCCI_ERROR_LOG(md->index, TAG,
			"rt_feature %u data_len = %u, expected data_len %u\n",
			rt_feature->feature_id, rt_feature->data_len, data_len);
		return -EFAULT;
	}

	memcpy(data, (const void *)((char *)rt_feature->data),
		rt_feature->data_len);

	return 0;
}

struct ccci_per_md *ccci_get_per_md_data(unsigned char md_id)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	if (md)
		return &md->per_md_data;
	else
		return NULL;
}


