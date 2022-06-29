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

#define CCB_CACHE_MIN_SIZE    (2 * 1024 * 1024)

static struct ccci_smem_region md1_noncacheable_tbl[] = {
	{SMEM_USER_RAW_DFD,		0,	0,	0},
	{SMEM_USER_RAW_UDC_DATA,	0,	0,	0},
	{SMEM_USER_MD_WIFI_PROXY,	0,	0,	0},
	{SMEM_USER_SECURITY_SMEM,	0,	0,	SMF_NCLR_FIRST,},
	{SMEM_USER_RAW_MDCCCI_DBG,	0,	0,	0},
	{SMEM_USER_RAW_MDSS_DBG,	0,	0,	0},
	{SMEM_USER_RAW_RESERVED,	0,	0,	0},
	{SMEM_USER_RAW_RUNTIME_DATA,	0,	0,	0},
	{SMEM_USER_RAW_FORCE_ASSERT,	0,	0,	0},
	{SMEM_USER_LOW_POWER,		0,	0,	0},
	{SMEM_USER_RAW_DBM,		0,	0,	0},
	{SMEM_USER_CCISM_SCP,		0,	0,	0},
	{SMEM_USER_RAW_CCB_CTRL,	0,	0,	0},
	{SMEM_USER_RAW_NETD,		0,	0,	0},
	{SMEM_USER_RAW_USB,		0,	0,	0},
	{SMEM_USER_RAW_AUDIO,		0,	0,	0},
	{SMEM_USER_CCISM_MCU,		0,	0,	SMF_NCLR_FIRST,},
	{SMEM_USER_CCISM_MCU_EXP,	0,	0,	SMF_NCLR_FIRST,},
	{SMEM_USER_RESERVED,		0,	0,	0},
	{SMEM_USER_MD_DRDI,		0,	0,	SMF_NCLR_FIRST,},
	{SMEM_USER_RAW_LWA,		0,	0,	0},
	{SMEM_USER_RAW_PHY_CAP,		0,	0,	SMF_NCLR_FIRST,},
	{SMEM_USER_RAW_AMMS_POS,	0,	0,	SMF_NCLR_FIRST,},
	{SMEM_USER_RAW_ALIGN_PADDING,	0,	0,	0},
	{SMEM_USER_MAX, }, /* tail guard */
};

static struct ccci_smem_region md1_cacheable_tbl[] = {
/*
 * all CCB user should be put together, and the total size is set
 * in the first one, all reset CCB users' address, offset and size
 * will be re-calculated during port initialization. and please be
 * aware of that CCB user's size will be aligned to 4KB.
 */
/*
 * Note: CCB ID should keep sequence(DHL,MDM,META, RAW_DHL, RAW_MDM.
 *       And DHL should be the first of CCB
 */
	{SMEM_USER_RAW_MD_CONSYS,	0,	0,	(SMF_NCLR_FIRST | SMF_NO_REMAP), },
	{SMEM_USER_MD_NVRAM_CACHE,	0,	0,	0},
	{SMEM_USER_CCB_DHL,		0,	0,	0},
	{SMEM_USER_CCB_MD_MONITOR,	0,	0,	0},
	{SMEM_USER_CCB_META,		0,	0,	0},
	{SMEM_USER_RAW_DHL,		0,	0,	0},
	{SMEM_USER_RAW_MDM,		0,	0,	0},
	{SMEM_USER_RAW_UDC_DESCTAB,	0,	0,	0},
	{SMEM_USER_RAW_USIP,		0,	0,	SMF_NCLR_FIRST, },
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


static void init_smem_regions(struct ccci_smem_region *regions, phys_addr_t md_view_base_phy)
{
	int i;
	unsigned int md_phy = 0;

	for (i = 0; ; i++) {
		if (!regions || regions[i].id == SMEM_USER_MAX)
			break;

		regions[i].size = mtk_ccci_get_smem_by_id(regions[i].id,
				&regions[i].base_ap_view_vir,
				&regions[i].base_ap_view_phy, &md_phy);
		regions[i].base_md_view_phy = (phys_addr_t)md_phy;
		regions[i].offset = (unsigned int)(regions[i].base_md_view_phy - md_view_base_phy);

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


static void post_cfg_for_ccb(void)
{
	unsigned int ccb_size = 0, section_a_size = 0, section_b_size = 0;
	unsigned int i = 0;
	void __iomem *ccb_ap_vir = NULL;
	phys_addr_t ccb_ap_phy = 0, ccb_md_phy = 0;
	unsigned int md_offset = 0;

	for (i = 0; i < ARRAY_SIZE(md1_cacheable_tbl); i++) {
		switch (md1_cacheable_tbl[i].id) {
		case SMEM_USER_CCB_DHL:
			ccb_size = md1_cacheable_tbl[i].size;
			if (ccb_size > CCB_CACHE_MIN_SIZE) {
				section_a_size = CCB_CACHE_MIN_SIZE;
				section_b_size = ccb_size - CCB_CACHE_MIN_SIZE;
			} else {
				section_a_size = 0;
				section_b_size = 0;
			}
			md1_cacheable_tbl[i].size = section_a_size;
			ccb_ap_vir = md1_cacheable_tbl[i].base_ap_view_vir;
			ccb_ap_phy = md1_cacheable_tbl[i].base_ap_view_phy;
			ccb_md_phy = md1_cacheable_tbl[i].base_md_view_phy;
			md_offset = md1_cacheable_tbl[i].offset;
			CCCI_BOOTUP_LOG(0, TAG,
				"ccb totoal :offset = 0x%x, size = 0x%x\n", md_offset, ccb_size);
			break;

		case SMEM_USER_CCB_MD_MONITOR:
		case SMEM_USER_CCB_META:
			md1_cacheable_tbl[i].size = section_a_size;
			md1_cacheable_tbl[i].offset = md_offset;
			md1_cacheable_tbl[i].base_ap_view_vir = ccb_ap_vir;
			md1_cacheable_tbl[i].base_ap_view_phy = ccb_ap_phy;
			md1_cacheable_tbl[i].base_md_view_phy = ccb_md_phy;
			break;
		case SMEM_USER_RAW_DHL:
		case SMEM_USER_RAW_MDM:
			md1_cacheable_tbl[i].size = section_b_size;
			if (section_b_size) {
				md1_cacheable_tbl[i].offset = md_offset + section_a_size;
				md1_cacheable_tbl[i].base_ap_view_vir
						= ccb_ap_vir + section_a_size;
				md1_cacheable_tbl[i].base_ap_view_phy
						= ccb_ap_phy + section_a_size;
				md1_cacheable_tbl[i].base_md_view_phy
						= ccb_md_phy + section_a_size;
			} else {
				md1_cacheable_tbl[i].offset = md_offset;
				md1_cacheable_tbl[i].base_ap_view_vir = ccb_ap_vir;
				md1_cacheable_tbl[i].base_ap_view_phy = ccb_ap_phy;
				md1_cacheable_tbl[i].base_md_view_phy = ccb_md_phy;
			}
			CCCI_BOOTUP_LOG(0, TAG,
				"[%d]RAW size:%d\n", md1_cacheable_tbl[i].id,
				md1_cacheable_tbl[i].size);
			break;
		default:
			break;
		}
	}
}


void ap_md_mem_init(struct ccci_mem_layout *mem_layout)
{
	phys_addr_t md_resv_mem_addr = 0;
	unsigned int md_resv_mem_size = 0, md_phy = 0;
	unsigned int i;

	/* Get memory info */
	get_md_resv_mem_info(&md_resv_mem_addr, &md_resv_mem_size, NULL, NULL);

	/* setup memory layout */
	/* MD image */
	mem_layout->md_bank0.base_ap_view_phy = md_resv_mem_addr;
	mem_layout->md_bank0.size = md_resv_mem_size;

	/* MD  non-cache share memory */
	mem_layout->md_bank4_noncacheable_total.size
		= mtk_ccci_get_md_nc_smem_inf(
			&mem_layout->md_bank4_noncacheable_total.base_ap_view_vir,
			&mem_layout->md_bank4_noncacheable_total.base_ap_view_phy,
			&md_phy);
	mem_layout->md_bank4_noncacheable_total.base_md_view_phy = (phys_addr_t)md_phy;
	mem_layout->md_bank4_noncacheable = md1_noncacheable_tbl;
	init_smem_regions(mem_layout->md_bank4_noncacheable, (phys_addr_t)md_phy);

	/* MD  cache share memory */
	mem_layout->md_bank4_cacheable_total.size = mtk_ccci_get_md_c_smem_inf(
				&mem_layout->md_bank4_cacheable_total.base_ap_view_vir,
				&mem_layout->md_bank4_cacheable_total.base_ap_view_phy,
				&md_phy);
	mem_layout->md_bank4_cacheable_total.base_md_view_phy = (phys_addr_t)md_phy;
	mem_layout->md_bank4_cacheable = md1_cacheable_tbl;
	init_smem_regions(mem_layout->md_bank4_cacheable, (phys_addr_t)md_phy);

	post_cfg_for_ccb();

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

	for (i = 0; i < ARRAY_SIZE(md1_noncacheable_tbl); i++)
		CCCI_BOOTUP_LOG(-1, TAG,
			"%s: reg[%d]<%d>(%lx %lx %lx)[%x]\n", __func__,
			i, md1_noncacheable_tbl[i].id,
			(unsigned long)md1_noncacheable_tbl[i].base_ap_view_phy,
			(unsigned long)md1_noncacheable_tbl[i].base_ap_view_vir,
			(unsigned long)md1_noncacheable_tbl[i].base_md_view_phy,
			md1_noncacheable_tbl[i].size);
	for (i = 0; i < ARRAY_SIZE(md1_cacheable_tbl); i++)
		CCCI_BOOTUP_LOG(-1, TAG,
			"%s: reg[%d]<%d>(%lx %lx %lx)[%x]\n", __func__,
			i, md1_cacheable_tbl[i].id,
			(unsigned long)md1_cacheable_tbl[i].base_ap_view_phy,
			(unsigned long)md1_cacheable_tbl[i].base_ap_view_vir,
			(unsigned long)md1_cacheable_tbl[i].base_md_view_phy,
			md1_cacheable_tbl[i].size);
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

