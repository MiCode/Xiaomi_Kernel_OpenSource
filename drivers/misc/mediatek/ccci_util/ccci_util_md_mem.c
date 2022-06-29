// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
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
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include "mt-plat/mtk_ccci_common.h"
#include "ccci_util_lib_main.h"
#include "ccci_util_log.h"


#define SMEM_ATTR_MD_NC           (1 << 0)
#define SMEM_ATTR_MD_C            (1 << 1)
#define SMEM_ATTR_PADDING         (1 << 2)
#define SMEM_ATTR_NO_MAP          (1 << 3)
#define SMEM_ATTR_NO_INIT         (1 << 4)
#define SMEM_ATTR_CCCI_USE        (1 << 5)
#define SMEM_ATTR_DEDICATED_MPU   (1 << 6)
#define SMEM_CLR_RESET            (1 << 7)
#define SMEM_NO_CLR_FIRST         (1 << 8)

struct smem_region_lk_fmt {
	int          id;
	unsigned int ap_offset;
	unsigned int size;
	unsigned int align;
	unsigned int flags;
	unsigned int md_offset;
};

struct rt_smem_region_lk_fmt {
	unsigned long long ap_phy;
	unsigned long long ap_vir;
	struct smem_region_lk_fmt inf;
};


static struct rt_smem_region_lk_fmt *s_smem_hash_tbl[SMEM_USER_MAX];

/******************************************************************************
 * Legacy chip compatible code start
 ******************************************************************************/
#define BANK4_DRDI_SMEM_SIZE	(512 * 1024)
/* For legacy compatible: gen6297 */
static struct rt_smem_region_lk_fmt gen6297_noncacheable_tbl[] = {
/*
 * ap_p, ap_v,  id,                    ap_offset, size,  align,   flags, md_offset
 */
{0ULL, 0ULL, {SMEM_USER_RAW_DFD,	        0,	0,	0,	0,	0}},
{0ULL, 0ULL, {SMEM_USER_RAW_UDC_DATA,	0,	0,	0,	0,	0}},
{0ULL, 0ULL, {SMEM_USER_MD_WIFI_PROXY,	0,	0,	0,	0,	0}},
{0ULL, 0ULL, {SMEM_USER_SECURITY_SMEM,	0,	0,	0,	SMEM_NO_CLR_FIRST, 0}},
{0ULL, 0ULL, {SMEM_USER_RAW_AMMS_POS,	0,	0,	0,	SMEM_NO_CLR_FIRST, 0}},
{0ULL, 0ULL, {SMEM_USER_RAW_MDCCCI_DBG,	0,	2*1024,	 0,	0,	0}},
{0ULL, 0ULL, {SMEM_USER_RAW_MDSS_DBG,	0,	14*1024, 0,	0,	0}},
{0ULL, 0ULL, {SMEM_USER_RAW_RESERVED,	0,	42*1024, 0,	0,	0}},
{0ULL, 0ULL, {SMEM_USER_RAW_RUNTIME_DATA, 0,	4*1024,	 0,	0,	0}},
{0ULL, 0ULL, {SMEM_USER_RAW_FORCE_ASSERT, 0,	1*1024,	 0,	0,	0}},
{0ULL, 0ULL, {SMEM_USER_LOW_POWER,	0,	512,	 0,	0,	0}},
{0ULL, 0ULL, {SMEM_USER_RAW_DBM,		0,	512,	 0,	0,	0}},
{0ULL, 0ULL, {SMEM_USER_CCISM_SCP,	0,	32*1024, 0,	0,	0}},
{0ULL, 0ULL, {SMEM_USER_RAW_CCB_CTRL,	0,	4*1024,	 0,	SMEM_NO_CLR_FIRST, 0}},
{0ULL, 0ULL, {SMEM_USER_RAW_NETD,	0,	8*1024,	 0,	0,	0}},
{0ULL, 0ULL, {SMEM_USER_RAW_USB,	        0,	4*1024,	 0,	0,	0}},
{0ULL, 0ULL, {SMEM_USER_RAW_AUDIO,	0,	52*1024, 0,	SMEM_NO_CLR_FIRST, 0}},
{0ULL, 0ULL, {SMEM_USER_CCISM_MCU,	0, (720+1)*1024, 0,	SMEM_NO_CLR_FIRST, 0}},
{0ULL, 0ULL, {SMEM_USER_CCISM_MCU_EXP,   0, (120+1)*1024,	SMEM_NO_CLR_FIRST, 0}},
{0ULL, 0ULL, {SMEM_USER_RESERVED,        0, 18*1024,	 0,	0,	0}},
{0ULL, 0ULL, {SMEM_USER_MD_DRDI,         0, BANK4_DRDI_SMEM_SIZE, 0, SMEM_NO_CLR_FIRST, 0}},
};

static struct rt_smem_region_lk_fmt gen6297_cacheable_tbl[] = {
/*
 * all CCB user should be put together, and the total size is set
 * in the first one, all reset CCB users' address, offset and size
 * will be re-calculated during port initialization. and please be
 * aware of that CCB user's size will be aligned to 4KB.
 *
 * ap_p, ap_v,  id,                    ap_offset, size,  align,   flags, md_offset
 */
{0ULL, 0ULL, {SMEM_USER_RAW_MD_CONSYS,	0,	0, 0, (SMEM_NO_CLR_FIRST | SMEM_ATTR_NO_MAP), 0}},
{0ULL, 0ULL, {SMEM_USER_MD_NVRAM_CACHE,	0,	0, 0, 0, 0}},
{0ULL, 0ULL, {SMEM_USER_CCB_START,		0,	0, 0, 0, 0}},
{0ULL, 0ULL, {SMEM_USER_RAW_UDC_DESCTAB,	0,	0, 0, }},
{0ULL, 0ULL, {SMEM_USER_RAW_USIP,	0,	0, 0, SMEM_NO_CLR_FIRST, 0}},
};


/* For legacy compatible: gen6295 and gen6293 */
#define CCCI_SMEM_SIZE_DBM (160)
#define CCCI_SMEM_SIZE_DBM_GUARD (8)
#define DBM_S (CCCI_SMEM_SIZE_DBM + CCCI_SMEM_SIZE_DBM_GUARD * 2)


static struct rt_smem_region_lk_fmt gen6293_6295_noncacheable_tbl[] = {
/*
 * ap_p, ap_v,  id,                    ap_offset, size,  align,   flags, md_offset
 */
{0ULL, 0ULL, {SMEM_USER_RAW_MDCCCI_DBG,	0,	2*1024,	 0,	0, 0}},
{0ULL, 0ULL, {SMEM_USER_RAW_MDSS_DBG,	2*1024,	10*1024, 0,	0, 0}},
{0ULL, 0ULL, {SMEM_USER_RAW_RESERVED,	12*1024,	46*1024, 0, 0,	0}},
{0ULL, 0ULL, {SMEM_USER_RAW_RUNTIME_DATA, 58*1024,	4*1024,	 0, 0,	0}},
{0ULL, 0ULL, {SMEM_USER_RAW_FORCE_ASSERT, 62*1024,	1*1024,	 0, 0,	0}},
{0ULL, 0ULL, {SMEM_USER_RAW_DBM,		64*1024-DBM_S,	DBM_S,	 0, 0,	0}},
{0ULL, 0ULL, {SMEM_USER_CCISM_SCP,	64*1024,	32*1024, 0, 0,	0}},
{0ULL, 0ULL, {SMEM_USER_RAW_CCB_CTRL,	96*1024,	4*1024,	 0, SMEM_NO_CLR_FIRST, 0}},
{0ULL, 0ULL, {SMEM_USER_RAW_NETD,	100*1024,	4*1024,	 0,	0,	0}},
{0ULL, 0ULL, {SMEM_USER_RAW_USB,	        104*1024,	4*1024,	 0,	0,	0}},
{0ULL, 0ULL, {SMEM_USER_RAW_AUDIO,	108*1024,	52*1024, 0, SMEM_NO_CLR_FIRST, 0}},
{0ULL, 0ULL, {SMEM_USER_CCISM_MCU,	160*1024, (720+1)*1024, 0, SMEM_NO_CLR_FIRST, 0}},
{0ULL, 0ULL, {SMEM_USER_CCISM_MCU_EXP,   881*1024, (120+1)*1024,	SMEM_NO_CLR_FIRST, 0}},
{0ULL, 0ULL, {SMEM_USER_RAW_DFD,	        1*1024*1024,	448*1024, 0,	0,	0}},
{0ULL, 0ULL, {SMEM_USER_RAW_UDC_DATA,	(1*1024+448)*1024,	0, 0,	0,	0}},
{0ULL, 0ULL, {SMEM_USER_RAW_AMMS_POS,	(1*1024 + 448)*1024,	0, 0,	SMEM_NO_CLR_FIRST, 0}},
{0ULL, 0ULL, {SMEM_USER_RAW_ALIGN_PADDING, (1*1024 + 448)*1024,	0,  0,
									SMEM_NO_CLR_FIRST, 0}},
{0ULL, 0ULL, {SMEM_USER_RAW_LWA,        (1*1024+448)*1024, 0,	 0, 0,	0}},
{0ULL, 0ULL, {SMEM_USER_RAW_PHY_CAP,	(1*1024+448)*1024, 0, 0, SMEM_NO_CLR_FIRST, 0}},
};

static struct rt_smem_region_lk_fmt gen6293_6295_cacheable_tbl[] = {
/*
 * all CCB user should be put together, and the total size is set
 * in the first one, all reset CCB users' address, offset and size
 * will be re-calculated during port initialization. and please be
 * aware of that CCB user's size will be aligned to 4KB.
 *
 * ap_p, ap_v,  id,                    ap_offset, size,  align,   flags, md_offset
 */
{0ULL, 0ULL, {SMEM_USER_CCB_START,		0,	0, 0, 0, 0}},
{0ULL, 0ULL, {SMEM_USER_RAW_UDC_DESCTAB,	0,	0, 0, }},
{0ULL, 0ULL, {SMEM_USER_RAW_MD_CONSYS,	0,	0, 0, (SMEM_NO_CLR_FIRST | SMEM_ATTR_NO_MAP), 0}},
{0ULL, 0ULL, {SMEM_USER_RAW_USIP,	0,	0, 0, SMEM_NO_CLR_FIRST, 0}},
};


static unsigned int get_phy_cap_size(void)
{
	unsigned int phy_cap_size = 0;

	if (mtk_ccci_find_args_val("md1_phy_cap",  (unsigned char *)&phy_cap_size,
		sizeof(phy_cap_size)) != sizeof(phy_cap_size))
		pr_info("ccci: %s using 0 as phy capture size\n", __func__);

	return phy_cap_size;
}

static int get_dfd_size(void)
{
	int dfd_size = -1;

	if (mtk_ccci_find_args_val("smem_dfd_size", (unsigned char *)&dfd_size,
		sizeof(dfd_size)) != sizeof(dfd_size))
		pr_info("ccci: %s using -1 as dfd size\n", __func__);

	return dfd_size;
}

static int get_amms_pose_size(void)
{
	int amms_pos_size = -1;

	if (mtk_ccci_find_args_val("smem_amms_pos_size",
		(unsigned char *)&amms_pos_size, sizeof(amms_pos_size)) != sizeof(amms_pos_size))
		pr_info("ccci:  %s using -1 as amms pos size\n", __func__);

	return amms_pos_size;
}

static int get_raw_align_padding_size(void)
{
	int size = -1;

	if (mtk_ccci_find_args_val("smem_align_padding_size", (unsigned char *)&size,
		sizeof(size)) != sizeof(size))
		pr_info("ccci:  %s using -1 as align padding size\n", __func__);

	return size;
}

struct udc_info {
	unsigned int noncache_size;
	unsigned int cache_size;
};

static unsigned int get_udc_nc_size(void)
{
	struct udc_info udc;

	memset(&udc, 0, sizeof(struct udc_info));
	if (mtk_ccci_find_args_val("udc_layout", (unsigned char *)&udc,
		sizeof(struct udc_info)) != sizeof(struct udc_info))
		pr_info("ccci:  %s using 0 as udc size\n", __func__);

	return udc.noncache_size;
}

struct _smem_layout {
	unsigned long long base_addr;
	unsigned int ap_md1_smem_offset;
	unsigned int ap_md1_smem_size;
	unsigned int ap_md3_smem_offset;
	unsigned int ap_md3_smem_size;
	unsigned int md1_md3_smem_offset;
	unsigned int md1_md3_smem_size;
	unsigned int total_smem_size;
};

static int cmpt_nc_smem_layout_init(struct rt_smem_region_lk_fmt tbl[], unsigned int num)
{
	unsigned int i;
	unsigned long long ap_phy_base = 0ULL;
	unsigned int adjust_en = 0;
	int tmp_sz;
	unsigned int tmp_u_sz;
	struct _smem_layout nc_layout;

	if (!tbl) {
		pr_info("ccci: %s get NULL table\n", __func__);
		return -1;
	}

	if (mtk_ccci_find_args_val("smem_layout", (unsigned char *)&nc_layout,
			sizeof(struct _smem_layout))  != sizeof(struct _smem_layout)) {
		pr_info("ccci: %s nc layout find fail\n", __func__);
		return -1;
	}
	ap_phy_base = nc_layout.base_addr;
	for (i = 0; i < num; i++) {
		if (i == 0) {
			tbl[i].ap_phy = ap_phy_base;
			tbl[i].inf.ap_offset = 0;
			tbl[i].inf.md_offset = 0;
		} else if (adjust_en) {
			tbl[i].ap_phy = tbl[i - 1].ap_phy + tbl[i - 1].inf.size;
			tbl[i].inf.ap_offset = tbl[i - 1].inf.ap_offset + tbl[i - 1].inf.size;
			tbl[i].inf.md_offset = tbl[i - 1].inf.md_offset + tbl[i - 1].inf.size;
		} else {
			tbl[i].ap_phy = ap_phy_base + tbl[i].inf.ap_offset;
			tbl[i].inf.ap_offset = tbl[i - 1].inf.ap_offset + tbl[i - 1].inf.size;
			tbl[i].inf.md_offset = tbl[i].inf.ap_offset;
		}

		switch (tbl[i].inf.id) {
		case SMEM_USER_RAW_PHY_CAP:
			tbl[i].inf.size = get_phy_cap_size();
			pr_info("ccci: PHY size:%d\n", tbl[i].inf.size);
			break;

		case SMEM_USER_RAW_DFD:
			tmp_sz = get_dfd_size();
			if (tmp_sz >= 0 && tmp_sz != tbl[i].inf.size) {
				tbl[i].inf.size = tmp_sz;
				adjust_en = 1;
			}
			pr_info("ccci: dfd size:%d\n", tbl[i].inf.size);
			break;

		case SMEM_USER_RAW_AMMS_POS:
			tmp_sz = get_amms_pose_size();
			if (tmp_sz >= 0 && tmp_sz != tbl[i].inf.size) {
				tbl[i].inf.size = (unsigned int)tmp_sz;
				adjust_en = 1;
				/* free AMMS POS smem chao -- Fix me*/
				//amms_cma_free((void *)(ap_vir_base + tbl[i].inf.ap_offset),
				//tmp_sz);
			}
			pr_info("ccci: smem amms pos size:%d\n", tbl[i].inf.size);
			break;

		case SMEM_USER_RAW_ALIGN_PADDING:
			tmp_sz = get_raw_align_padding_size();
			if (tmp_sz >= 0 && tmp_sz != tbl[i].inf.size) {
				tbl[i].inf.size = tmp_sz;
				adjust_en = 1;
				/* free POS padding smem chao -- Fix me*/
				//amms_cma_free((void *)(ap_vir_base + tbl[i].inf.ap_offset),
				//tmp_sz);
			}
			pr_info("ccci: smem align padding size:%d\n", tmp_sz);
			break;

		case SMEM_USER_RAW_UDC_DATA:
			tmp_u_sz = get_udc_nc_size();
			if (tmp_u_sz > 0 && tmp_u_sz != tbl[i].inf.size) {
				tbl[i].inf.size = tmp_u_sz;
				adjust_en = 1;
			}
			break;
		default:
			break;
		}
	}

	return 0;
}

struct ccb_layout {
	unsigned long long addr;
	unsigned int size;
};
static struct ccb_layout ccb_info;

struct _csmem_item {
	unsigned long long csmem_buffer_addr;
	unsigned int md_offset;
	unsigned int csmem_buffer_size;
	unsigned int item_cnt;
};

#define CSMEM_LAYOUT_NUM	10
static struct _csmem_item csmem_info;
static struct _csmem_item csmem_layout[CSMEM_LAYOUT_NUM];
static unsigned int s_md_cache_offset;

static void c_smem_info_parsing(void)
{
	unsigned int size;

	/* Get ccb memory layout */
	memset(&ccb_info, 0, sizeof(struct ccb_layout));
	if (mtk_ccci_find_args_val("ccb_info", (unsigned char *)&ccb_info,
					sizeof(struct ccb_layout)) != sizeof(struct ccb_layout))
		pr_info("ccci: %s using default ccb size\n", __func__);
	else
		pr_info("ccci: %s ccb: data:%llx data_size:%d\n", __func__,
			ccb_info.addr, ccb_info.size);

	memset(&csmem_info, 0, sizeof(struct _csmem_item));
	if (mtk_ccci_find_args_val("md1_bank4_cache_info", (unsigned char *)&csmem_info,
		sizeof(struct _csmem_item)) != sizeof(struct _csmem_item))
		pr_info("ccci: %s get csmem_info fail\n", __func__);

	size = csmem_info.item_cnt * sizeof(struct _csmem_item);
	memset(csmem_layout, 0, sizeof(csmem_layout));
	if (mtk_ccci_find_args_val("md1_bank4_cache_layout", (unsigned char *)csmem_layout,
					size) != size)
		pr_info("ccci: %s csmem_layout: get csmem_layout fail\n", __func__);

	/* Get smem cachable offset  */
	s_md_cache_offset = 0;
	if (mtk_ccci_find_args_val("md1_smem_cahce_offset", (unsigned char *)&s_md_cache_offset,
		sizeof(s_md_cache_offset)) != sizeof(s_md_cache_offset))
		/* Using 128MB offset as default */
		s_md_cache_offset = 0x8000000;
	pr_info("ccci: %s smem cachable offset 0x%X\n", __func__, s_md_cache_offset);
}

static int get_cache_smem_regin_info(int id, unsigned int *offset, unsigned int *size)
{
	unsigned int i, num;

	num = csmem_info.item_cnt > CSMEM_LAYOUT_NUM ? CSMEM_LAYOUT_NUM : csmem_info.item_cnt;
	for (i = 0; i < num; i++) {
		if (csmem_layout[i].item_cnt == id) {
			if (offset)
				*offset = csmem_layout[i].md_offset;
			if (size)
				*size = csmem_layout[i].csmem_buffer_size;
			return 0;
		}
	}
	return -1;
}

static int cmpt_c_smem_layout_init(struct rt_smem_region_lk_fmt tbl[], unsigned int num)
{
	unsigned int i;
	unsigned long long ap_phy_base = 0ULL;
	unsigned int offset, size;

	if (!tbl) {
		pr_info("ccci: %s get NULL table\n", __func__);
		return -1;
	}

	c_smem_info_parsing();
	if (csmem_info.csmem_buffer_addr)
		ap_phy_base = csmem_info.csmem_buffer_addr;
	else
		ap_phy_base = ccb_info.addr;

	for (i = 0; i < num; i++) {
		if (i == 0) {
			tbl[i].ap_phy = ap_phy_base;
			tbl[i].inf.md_offset = s_md_cache_offset;
			tbl[i].inf.ap_offset = 0;
		} else {
			tbl[i].ap_phy = tbl[i - 1].ap_phy + tbl[i - 1].inf.size;
			tbl[i].inf.ap_offset = tbl[i - 1].inf.ap_offset + tbl[i - 1].inf.size;
			tbl[i].inf.md_offset = tbl[i - 1].inf.md_offset + tbl[i - 1].inf.size;
		}

		switch (tbl[i].inf.id) {
		case SMEM_USER_CCB_START:
			if (!get_cache_smem_regin_info(SMEM_USER_CCB_START, &offset, &size))
				tbl[i].inf.size = size;
			else
				tbl[i].inf.size = ccb_info.size;
			break;

		case SMEM_USER_RAW_MD_CONSYS:
		case SMEM_USER_MD_NVRAM_CACHE: /* go through */
		case SMEM_USER_RAW_USIP:
		case SMEM_USER_RAW_UDC_DESCTAB:
			offset = 0;
			size = 0;
			if (!get_cache_smem_regin_info(tbl[i].inf.id, &offset, &size)) {
				tbl[i].inf.size = size;

				if (offset || size) {
					tbl[i].inf.ap_offset = offset;
					/* Using LK config */
					tbl[i].inf.md_offset = offset + s_md_cache_offset;
				}
			}
			break;

		default:
			tbl[i].inf.size = 0;
			tbl[i].inf.ap_offset = 0;
			tbl[i].inf.md_offset = 0;
			break;
		}
	}

	return 0;
}
/******************************************************************************
 * Legacy chip compatible code end
 ******************************************************************************/


/*SIB info */
struct md_sib {
	unsigned long long addr;
	unsigned int size;
};
static struct md_sib s_md_sib_inf;

static void get_sib_info_from_tag(void)
{
	if (mtk_ccci_find_args_val("md1_sib_info", (unsigned char *)&s_md_sib_inf,
			sizeof(s_md_sib_inf)) != sizeof(s_md_sib_inf)) {
		pr_info("ccci: %s: get sib info fail\n", __func__);
		return;
	}

	pr_info("ccci:%s: sib addr: 0x%llx size: %d\n",
		__func__, s_md_sib_inf.addr, s_md_sib_inf.size);
}


/* AP MD non-cachable memory information */
static struct rt_smem_region_lk_fmt *s_md_nc_smem_tbl;
static unsigned int s_md_nc_smem_item_num;
static unsigned int s_md_nc_smem_size;
static unsigned int s_md_nc_smem_mpu_size;

/* AP MD cachable memory information */
static struct rt_smem_region_lk_fmt *s_md_c_smem_tbl;
static unsigned int s_md_c_smem_item_num;
static unsigned int s_md_c_smem_size;
static unsigned int s_md_c_smem_mpu_size;

/* MD generation */
static unsigned int s_md_gen;

static void md_nc_smem_size_cal(struct rt_smem_region_lk_fmt tbl[], unsigned int num)
{
	unsigned int i;

	s_md_nc_smem_size = 0;
	for (i = 0; i < num; i++)
		s_md_nc_smem_size += tbl[i].inf.size;

	s_md_nc_smem_mpu_size = s_md_nc_smem_size;
}

static void md_c_smem_size_cal(struct rt_smem_region_lk_fmt tbl[], unsigned int num)
{
	unsigned int i;

	s_md_c_smem_size = 0;
	for (i = 0; i < num; i++)
		s_md_c_smem_size += tbl[i].inf.size;

	s_md_c_smem_mpu_size = s_md_c_smem_size;
}

static int get_md_smem_layout_tbl_from_lk_tag(void)
{
	int ret;
	unsigned long size;

	ret = mtk_ccci_find_args_val("nc_smem_layout_num",  (unsigned char *)&s_md_nc_smem_item_num,
			sizeof(s_md_nc_smem_item_num));
	if (ret != (int)sizeof(s_md_nc_smem_item_num)) {
		pr_info("ccci: %s: get nc_smem_num fail:%d\n", __func__, ret);
		return -1;
	}
	ret = mtk_ccci_find_args_val("c_smem_layout_num",  (unsigned char *)&s_md_c_smem_item_num,
			sizeof(s_md_c_smem_item_num));
	if (ret != (int)sizeof(s_md_c_smem_item_num)) {
		pr_info("ccci: %s: get c_smem_num fail:%d\n", __func__, ret);
		return -1;
	}

	size = sizeof(struct rt_smem_region_lk_fmt) * s_md_nc_smem_item_num;
	s_md_nc_smem_tbl = kmalloc(size, GFP_KERNEL);
	if (!s_md_nc_smem_tbl) {
		pr_info("ccci: %s: alloc nc_smem_tbl memory fail:%d\n", __func__);
		return -1;
	}
	ret = mtk_ccci_find_args_val("nc_smem_layout",  (unsigned char *)s_md_nc_smem_tbl, size);
	if (ret != (int)size) {
		pr_info("ccci: %s: get nc_smem_tbl fail:%d\n", __func__, ret);
		kfree(s_md_nc_smem_tbl);
		s_md_nc_smem_tbl = NULL;
		return -1;
	}

	size = sizeof(struct rt_smem_region_lk_fmt) * s_md_c_smem_item_num;
	s_md_c_smem_tbl = kmalloc(size, GFP_KERNEL);
	if (!s_md_c_smem_tbl) {
		pr_info("ccci: %s: alloc c_smem_tbl memory fail:%d\n", __func__);
		kfree(s_md_nc_smem_tbl);
		s_md_nc_smem_tbl = NULL;
		return -1;
	}
	ret = mtk_ccci_find_args_val("c_smem_layout",  (unsigned char *)s_md_c_smem_tbl, size);
	if (ret != (int)size) {
		pr_info("ccci: %s: get c_smem_tbl fail:%d\n", __func__, ret);
		kfree(s_md_nc_smem_tbl);
		s_md_nc_smem_tbl = NULL;
		kfree(s_md_c_smem_tbl);
		s_md_c_smem_tbl = NULL;
		return -1;
	}

	return 0;
}


static int get_md_smem_layout_tbl_from_lk_legacy_tag(void)
{
	if (s_md_gen < 6297) {
		s_md_nc_smem_tbl = gen6293_6295_noncacheable_tbl;
		s_md_c_smem_tbl = gen6293_6295_cacheable_tbl;
		s_md_nc_smem_item_num = ARRAY_SIZE(gen6293_6295_noncacheable_tbl);
		s_md_c_smem_item_num = ARRAY_SIZE(gen6293_6295_cacheable_tbl);
	} else {
		s_md_nc_smem_tbl = gen6297_noncacheable_tbl;
		s_md_c_smem_tbl = gen6297_cacheable_tbl;
		s_md_nc_smem_item_num = ARRAY_SIZE(gen6297_noncacheable_tbl);
		s_md_c_smem_item_num = ARRAY_SIZE(gen6297_cacheable_tbl);
	}

	if (cmpt_nc_smem_layout_init(s_md_nc_smem_tbl, s_md_nc_smem_item_num) != 0)
		return -1;

	if (cmpt_c_smem_layout_init(s_md_c_smem_tbl, s_md_c_smem_item_num) != 0)
		return -1;

	return 0;
}


/* Fix me */
static void __iomem *map_phy_addr(unsigned long long base, unsigned int size)
{
	/* Note： this is just a test function, the virtual address is dummy */
	return (void __iomem *)(base + 0xF000000000000000LL);
}


static int map_and_update_tbl(struct rt_smem_region_lk_fmt *tbl, u32 start, u32 end,
				u64 base, u32 size)
{
	void __iomem *vir_addr;
	u32 i;

	//vir_addr = ccci_map_phy_addr(base, size); /* Fix me for later */
	vir_addr = map_phy_addr(base, size);
	if (!vir_addr) {
		pr_info("ccci: %s for 0x%016llx[0x%08x] smem fail\n", __func__, base, size);
		return -1;
	}

	pr_info("ccci: %s for 0x%016llx[0x%08x] smem to 0x%016llx(%d:%d)\n", __func__,
				base, size, (u64)vir_addr, start, end);

	for (i = start; i <= end; i++)
		tbl[i].ap_vir = (u64)vir_addr + tbl[i].inf.ap_offset - tbl[start].inf.ap_offset;

	return 0;
}

static int map_phy_to_kernel(struct rt_smem_region_lk_fmt *tbl, u32 num)
{
	unsigned int size = 0, i, start_idx;
	unsigned long long start_addr;
	unsigned int state = 0; //0: find start, 1: find end
	int ret;

	for (i = 0; i < num; i++) {

		if (tbl[i].inf.flags & SMEM_ATTR_PADDING)
			continue;

		if (tbl[i].inf.id < SMEM_USER_MAX)
			s_smem_hash_tbl[tbl[i].inf.id] = &tbl[i];
		else
			pr_info("ccci: %s un-support id: %d\n", __func__, tbl[i].inf.id);

		if (state == 0) {
			if (tbl[i].inf.flags & SMEM_ATTR_NO_MAP)
				continue;

			start_addr = tbl[i].ap_phy;
			size = tbl[i].inf.size;
			start_idx = i;
			state = 1;
		} else {
			if (tbl[i].inf.flags & SMEM_ATTR_NO_MAP) {
				ret = map_and_update_tbl(tbl, start_idx, i - 1, start_addr, size);
				state = 0;
				if (ret < 0)
					return -1;
			} else
				size += tbl[i].inf.size;
		}
	}

	if (state)
		return map_and_update_tbl(tbl, start_idx, i - 1, start_addr, size);

	return 0;
}

static void smem_layout_dump(const char name[], struct rt_smem_region_lk_fmt *rt_tbl, u32 num)
{
	unsigned int i;

	CCCI_UTIL_INF_MSG("------- Dump %s datails -------------\n", name);
	for (i = 0; i < num; i++)
		CCCI_UTIL_INF_MSG("%03u-%03d-%016llX-%016llX-%08X-%08X-%08X-%08X-%u\n",
			i, rt_tbl[i].inf.id, rt_tbl[i].ap_phy, rt_tbl[i].ap_vir,
			rt_tbl[i].inf.md_offset + 0x40000000, rt_tbl[i].inf.size,
			rt_tbl[i].inf.ap_offset, rt_tbl[i].inf.flags,
			rt_tbl[i].inf.flags & SMEM_ATTR_PADDING);
}


/****************************************************************************
 * Export to external
 ****************************************************************************/

int mtk_ccci_md_smem_layout_init(void)
{
	int ret;

	s_md_gen = 6297;
	ret = mtk_ccci_find_args_val("md_generation",  (unsigned char *)&s_md_gen,
			sizeof(unsigned int));
	if (ret <= 0)
		pr_info("ccci: %s :get md generation fail(%d)\n", __func__, ret);

	/* Get share memory layout */
	ret = get_md_smem_layout_tbl_from_lk_tag();
	if (ret < 0) {
		pr_info("ccci: %s :get md smem layout from tag directly not support(%d)\n",
			__func__, ret);
		ret = get_md_smem_layout_tbl_from_lk_legacy_tag();
		if (ret < 0) {
			pr_info("ccci: %s :get md smem layout from tag legacy mode fail(%d)\n",
			__func__, ret);
			return -1;
		}
	}

	md_nc_smem_size_cal(s_md_nc_smem_tbl, s_md_nc_smem_item_num);

	md_c_smem_size_cal(s_md_c_smem_tbl, s_md_c_smem_item_num);

	/* Map share memory to kernel if need */
	ret = map_phy_to_kernel(s_md_nc_smem_tbl, s_md_nc_smem_item_num);
	if (ret < 0) {
		pr_info("ccci: %s :map md nc smem to kernel fail(%d)\n", __func__, ret);
		return -1;
	}

	ret = map_phy_to_kernel(s_md_c_smem_tbl, s_md_c_smem_item_num);
	if (ret < 0) {
		pr_info("ccci: %s :map md c smem to kernel fail(%d)\n", __func__, ret);
		return -1;
	}

	get_sib_info_from_tag();

	/* Show map result */
	smem_layout_dump("MD non-cache", s_md_nc_smem_tbl, s_md_nc_smem_item_num);
	smem_layout_dump("MD cache", s_md_c_smem_tbl, s_md_c_smem_item_num);

	return 0;
}


u32 mtk_ccci_get_smem_by_id(enum SMEM_USER_ID user_id,
				void __iomem **o_ap_vir, phys_addr_t *o_ap_phy, u32 *o_md_phy)
{
	if (user_id >= SMEM_USER_MAX)
		return 0;
	if (user_id < 0)
		return 0;

	if (!s_smem_hash_tbl[user_id]) {
		pr_info("ccci: %s hash table[%d] is NULL\n", __func__, user_id);
		return 0;
	}
	if (o_ap_vir)
		*o_ap_vir = (void __iomem *)s_smem_hash_tbl[user_id]->ap_vir;
	if (o_ap_phy)
		*o_ap_phy = (phys_addr_t)s_smem_hash_tbl[user_id]->ap_phy;
	if (o_md_phy)
		*o_md_phy = s_smem_hash_tbl[user_id]->inf.md_offset + 0x40000000;

	return s_smem_hash_tbl[user_id]->inf.size;
}
EXPORT_SYMBOL(mtk_ccci_get_smem_by_id);


void __iomem *mtk_ccci_get_smem_start_addr(enum SMEM_USER_ID user_id, int *size_o)
{
	void __iomem *addr = NULL;
	unsigned int size = 0;

	size = mtk_ccci_get_smem_by_id(user_id, &addr, NULL, NULL);

	if (size) {
		if (s_md_gen < 6297) {
			/* dbm addr returned to user should
			 * step over Guard pattern header
			 */
			if (user_id == SMEM_USER_RAW_DBM)
				addr += CCCI_SMEM_SIZE_DBM_GUARD;
		}

		if (size_o)
			*size_o = size;
	}
	return addr;
}
EXPORT_SYMBOL(mtk_ccci_get_smem_start_addr);


phys_addr_t mtk_ccci_get_smem_phy_start_addr(enum SMEM_USER_ID user_id, int *size_o)
{
	phys_addr_t addr = 0;
	unsigned int size = 0;

	size = mtk_ccci_get_smem_by_id(user_id, NULL, &addr, NULL);

	if (size) {
		CCCI_UTIL_INF_MSG("phy address: 0x%lx, ",
			(unsigned long)addr);
		if (size_o) {
			*size_o = size;
			CCCI_UTIL_INF_MSG("0x%x", *size_o);
		} else {
			CCCI_UTIL_INF_MSG("size_0 is NULL(invalid)");
		}
	}
	return addr;
}
EXPORT_SYMBOL(mtk_ccci_get_smem_phy_start_addr);


unsigned int mtk_ccci_get_md_nc_smem_inf(void __iomem **o_ap_vir, phys_addr_t *o_ap_phy,
						u32 *o_md_phy)
{
	if (s_md_nc_smem_tbl || s_md_nc_smem_item_num) {
		if (o_ap_vir)
			*o_ap_vir = (void __iomem *)s_md_nc_smem_tbl[0].ap_vir;
		if (o_ap_phy)
			*o_ap_phy = s_md_nc_smem_tbl[0].ap_phy;
		if (o_md_phy)
			*o_md_phy = s_md_nc_smem_tbl[0].inf.md_offset + 0x40000000;

		return s_md_nc_smem_size;
	}

	return 0;
}

unsigned int mtk_ccci_get_md_nc_smem_mpu_size(void)
{
	return s_md_nc_smem_mpu_size;
}

unsigned int mtk_ccci_get_md_c_smem_inf(void __iomem **o_ap_vir, phys_addr_t *o_ap_phy,
					u32 *o_md_phy)
{
	if (s_md_c_smem_tbl || s_md_c_smem_item_num) {
		if (o_ap_vir)
			*o_ap_vir = (void __iomem *)s_md_c_smem_tbl[0].ap_vir;
		if (o_ap_phy)
			*o_ap_phy = s_md_c_smem_tbl[0].ap_phy;
		if (o_md_phy)
			*o_md_phy = s_md_c_smem_tbl[0].inf.md_offset + 0x40000000;

		return s_md_c_smem_size;
	}

	return 0;
}

unsigned int mtk_ccci_get_md_c_smem_mpu_size(void)
{
	return s_md_c_smem_mpu_size;
}


struct md_mem_blk {
	unsigned int offset;
	unsigned int size;
	unsigned int inf_flag;
	unsigned int attr_flag;
	unsigned long long ap_phy;
};

void mtk_ccci_dump_md_mem_layout(void)
{
	int ret;
	unsigned int i, num;
	unsigned long long base;
	unsigned char *tmp_buf = NULL;
	struct md_mem_blk *desc = NULL;

	ret = mtk_ccci_find_args_val("md_bank0_base",  (unsigned char *)&base, sizeof(u64));
	if (ret <= 0) {
		pr_info("ccci: [%s] bank0 base not found\n", __func__);
		goto _show_na;
	}

	tmp_buf = kmalloc(1024, GFP_KERNEL);
	if (!tmp_buf)
		goto _show_na;

	ret = mtk_ccci_find_args_val("md_mem_layout",  tmp_buf, 1024);
	if (ret <= 0) {
		pr_info("ccci: [%s] get layout fail\n", __func__);
		goto _show_na;
	}

	num = (unsigned long)ret/sizeof(struct md_mem_blk);
	desc = (struct md_mem_blk *)tmp_buf;
	CCCI_UTIL_INF_MSG("------ Dump md layout(%u) ---------\n", num);
	for (i = 0; i < num; i++)
		CCCI_UTIL_INF_MSG("| 0x%016llx | 0x%08x | 0x%08x | 0x%08x | 0x%08x |\n",
				desc[i].ap_phy, desc[i].offset, desc[i].size,
				desc[i].inf_flag, desc[i].attr_flag);

	kfree(tmp_buf);
	return;

_show_na:
	CCCI_UTIL_INF_MSG("------ Dump md layout(NA) ---------\n");
	kfree(tmp_buf);
}

