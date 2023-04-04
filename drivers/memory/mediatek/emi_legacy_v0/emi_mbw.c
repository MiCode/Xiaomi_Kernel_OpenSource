/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/
#include <mt-plat/sync_write.h>
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#include <mt-plat/mboot_params.h>
#endif
#include <linux/of_address.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include <linux/module.h>

#include <soc/mediatek/emi_legacy_v0.h>
#include "emi_ctrl.h"
#include "emi_mbw.h"
#include "emi_elm.h"

#ifdef CONFIG_ARM64
#define IOMEM(a) ((void __force __iomem *)((a)))
#endif

#define EMI_BWCT0	(CEN_EMI_BASE + 0x5B0)
#define EMI_CONM	(CEN_EMI_BASE + 0x0060)

static void __iomem *CEN_EMI_BASE;
static void __iomem *CHA_EMI_BASE;
static void __iomem *INFRACFG_BASE;
static void __iomem *INFRA_AO_BASE;

int BM_GetEmiDcm(void)
{
	return readl(IOMEM(EMI_CONM)) >> 24;
}
EXPORT_SYMBOL(BM_GetEmiDcm);

int BM_SetEmiDcm(const unsigned int setting)
{
	unsigned int value;

	value = readl(IOMEM(EMI_CONM));
	mt_reg_sync_writel((value & 0x00FFFFFF) | (setting << 24), EMI_CONM);

	return 0;
}
EXPORT_SYMBOL(BM_SetEmiDcm);

int BM_SetBW(const unsigned int BW_config)
{
	mt_reg_sync_writel(BW_config, EMI_BWCT0);
	return 0;
}
EXPORT_SYMBOL(BM_SetBW);

unsigned int BM_GetBW(void)
{
	return readl(IOMEM(EMI_BWCT0));
}
EXPORT_SYMBOL(BM_GetBW);

void dump_last_bm(char *buf, unsigned int leng)
{
	elm_dump(buf, leng);
}
EXPORT_SYMBOL(dump_last_bm);

static inline void aee_simple_print(const char *msg, unsigned int val)
{
	char buf[128];
	int err;

	err = snprintf(buf, sizeof(buf), msg, val);
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	if (err > 0)
		aee_sram_fiq_log(buf);
#endif
}

#define EMI_DBG_SIMPLE_RWR(msg, addr, wval)	do {\
		aee_simple_print(msg, readl(addr));	\
		writel(wval, addr);			\
		aee_simple_print(msg, readl(addr));\
	} while (0)

#define EMI_DBG_SIMPLE_R(msg, addr)		\
		aee_simple_print(msg, readl(addr))

void dump_emi_outstanding(void)
{
	/* CEN_EMI_BASE: 0x10219000 */
	if (!CEN_EMI_BASE)
		return;

	/* CHA_EMI_BASE: 0x1022D000 */
	if (!CHA_EMI_BASE)
		return;

	/* INFRACFG_BASE: 0x1020E000 */
	if (!INFRACFG_BASE)
		return;

	EMI_DBG_SIMPLE_R("[EMI] 0x10001220 = 0x%x\n",
		(INFRA_AO_BASE + 0x220));
	EMI_DBG_SIMPLE_R("[EMI] 0x10001224 = 0x%x\n",
		(INFRA_AO_BASE + 0x224));
	EMI_DBG_SIMPLE_R("[EMI] 0x10001228 = 0x%x\n",
		(INFRA_AO_BASE + 0x228));
	EMI_DBG_SIMPLE_R("[EMI] 0x10001250 = 0x%x\n",
		(INFRA_AO_BASE + 0x250));
	EMI_DBG_SIMPLE_R("[EMI] 0x10001254 = 0x%x\n",
		(INFRA_AO_BASE + 0x254));
	EMI_DBG_SIMPLE_R("[EMI] 0x10001258 = 0x%x\n",
		(INFRA_AO_BASE + 0x258));

	EMI_DBG_SIMPLE_RWR("[EMI] 0x102190e8 = 0x%x\n",
		(CEN_EMI_BASE + 0x0e8), readl(CEN_EMI_BASE + 0x0e8) | 0x100);

	EMI_DBG_SIMPLE_RWR("[EMI] 0x1020e100 = 0x%x\n",
		(INFRACFG_BASE + 0x100), 0x00000104 >> 3);
	EMI_DBG_SIMPLE_RWR("[EMI] 0x1020e104 = 0x%x\n",
		(INFRACFG_BASE + 0x104), 0x00000204 >> 3);
	EMI_DBG_SIMPLE_RWR("[EMI] 0x1020e108 = 0x%x\n",
		(INFRACFG_BASE + 0x108), 0x00000304 >> 3);
	EMI_DBG_SIMPLE_RWR("[EMI] 0x1020e10c = 0x%x\n",
		(INFRACFG_BASE + 0x10c), 0x00000404 >> 3);
	EMI_DBG_SIMPLE_R("[EMI] 0x102197fc = 0x%x\n",
		(CEN_EMI_BASE + 0x7fc));
	EMI_DBG_SIMPLE_R("[EMI] 0x102197fc = 0x%x\n",
		(CEN_EMI_BASE + 0x7fc));

	EMI_DBG_SIMPLE_RWR("[EMI] 0x1020e100 = 0x%x\n",
		(INFRACFG_BASE + 0x100), 0x00000804 >> 3);
	EMI_DBG_SIMPLE_RWR("[EMI] 0x1020e104 = 0x%x\n",
		(INFRACFG_BASE + 0x104), 0x00000904 >> 3);
	EMI_DBG_SIMPLE_RWR("[EMI] 0x1020e108 = 0x%x\n",
		(INFRACFG_BASE + 0x108), 0x00001204 >> 3);
	EMI_DBG_SIMPLE_RWR("[EMI] 0x1020e10c = 0x%x\n",
		(INFRACFG_BASE + 0x10c), 0x00001104 >> 3);
	EMI_DBG_SIMPLE_R("[EMI] 0x102197fc = 0x%x\n",
		(CEN_EMI_BASE + 0x7fc));
	EMI_DBG_SIMPLE_R("[EMI] 0x102197fc = 0x%x\n",
		(CEN_EMI_BASE + 0x7fc));

	EMI_DBG_SIMPLE_RWR("[EMI] 0x1020e100 = 0x%x\n",
		(INFRACFG_BASE + 0x100), 0x00000504 >> 3);
	EMI_DBG_SIMPLE_RWR("[EMI] 0x1020e104 = 0x%x\n",
		(INFRACFG_BASE + 0x104), 0x00000604 >> 3);
	EMI_DBG_SIMPLE_RWR("[EMI] 0x1020e108 = 0x%x\n",
		(INFRACFG_BASE + 0x108), 0x00000a04 >> 3);
	EMI_DBG_SIMPLE_RWR("[EMI] 0x1020e10c = 0x%x\n",
		(INFRACFG_BASE + 0x10c), 0x00000b04 >> 3);
	EMI_DBG_SIMPLE_R("[EMI] 0x102197fc = 0x%x\n",
		(CEN_EMI_BASE + 0x7fc));
	EMI_DBG_SIMPLE_R("[EMI] 0x102197fc = 0x%x\n",
		(CEN_EMI_BASE + 0x7fc));

	EMI_DBG_SIMPLE_RWR("[EMI] 0x1020e100 = 0x%x\n",
		(INFRACFG_BASE + 0x100), 0x00001504 >> 3);
	EMI_DBG_SIMPLE_RWR("[EMI] 0x1020e104 = 0x%x\n",
		(INFRACFG_BASE + 0x104), 0x00001604 >> 3);
	EMI_DBG_SIMPLE_RWR("[EMI] 0x1020e108 = 0x%x\n",
		(INFRACFG_BASE + 0x108), 0x00001704 >> 3);
	EMI_DBG_SIMPLE_RWR("[EMI] 0x1020e10c = 0x%x\n",
		(INFRACFG_BASE + 0x10c), 0x00001804 >> 3);
	EMI_DBG_SIMPLE_R("[EMI] 0x102197fc = 0x%x\n",
		(CEN_EMI_BASE + 0x7fc));
	EMI_DBG_SIMPLE_R("[EMI] 0x102197fc = 0x%x\n",
		(CEN_EMI_BASE + 0x7fc));

	EMI_DBG_SIMPLE_RWR("[EMI] 0x1020e100 = 0x%x\n",
		(INFRACFG_BASE + 0x100), 0x00001904 >> 3);
	EMI_DBG_SIMPLE_RWR("[EMI] 0x1020e104 = 0x%x\n",
		(INFRACFG_BASE + 0x104), 0x00001a04 >> 3);
	EMI_DBG_SIMPLE_RWR("[EMI] 0x1020e108 = 0x%x\n",
		(INFRACFG_BASE + 0x108), 0x00001b04 >> 3);
	EMI_DBG_SIMPLE_RWR("[EMI] 0x1020e10c = 0x%x\n",
		(INFRACFG_BASE + 0x10c), 0x00001c04 >> 3);
	EMI_DBG_SIMPLE_R("[EMI] 0x102197fc = 0x%x\n",
		(CEN_EMI_BASE + 0x7fc));
	EMI_DBG_SIMPLE_R("[EMI] 0x102197fc = 0x%x\n",
		(CEN_EMI_BASE + 0x7fc));

	EMI_DBG_SIMPLE_RWR("[EMI] 0x1020e100 = 0x%x\n",
		(INFRACFG_BASE + 0x100), 0x00003600 >> 3);
	EMI_DBG_SIMPLE_RWR("[EMI] 0x1020e104 = 0x%x\n",
		(INFRACFG_BASE + 0x104), 0x00003700 >> 3);
	EMI_DBG_SIMPLE_RWR("[EMI] 0x1020e108 = 0x%x\n",
		(INFRACFG_BASE + 0x108), 0x00003800 >> 3);
	EMI_DBG_SIMPLE_RWR("[EMI] 0x1020e10c = 0x%x\n",
		(INFRACFG_BASE + 0x10c), 0x00003900 >> 3);
	EMI_DBG_SIMPLE_R("[EMI] 0x102197fc = 0x%x\n",
		(CEN_EMI_BASE + 0x7fc));
	EMI_DBG_SIMPLE_R("[EMI] 0x102197fc = 0x%x\n",
		(CEN_EMI_BASE + 0x7fc));

	EMI_DBG_SIMPLE_RWR("[EMI] 0x1022da80 = 0x%x\n",
		(CHA_EMI_BASE + 0xa80), 0x00000001);

	EMI_DBG_SIMPLE_RWR("[EMI] 0x1022da88 = 0x%x\n",
		(CHA_EMI_BASE + 0xa88), 0x00120000);
	EMI_DBG_SIMPLE_RWR("[EMI] 0x1022da8c = 0x%x\n",
		(CHA_EMI_BASE + 0xa8c), 0x00180017);
	EMI_DBG_SIMPLE_R("[EMI] 0x1022da84 = 0x%x\n",
		(CHA_EMI_BASE + 0xa84));
	EMI_DBG_SIMPLE_R("[EMI] 0x1022da84 = 0x%x\n",
		(CHA_EMI_BASE + 0xa84));

	EMI_DBG_SIMPLE_RWR("[EMI] 0x1022da88 = 0x%x\n",
		(CHA_EMI_BASE + 0xa88), 0x00160015);
	EMI_DBG_SIMPLE_RWR("[EMI] 0x1022da8c = 0x%x\n",
		(CHA_EMI_BASE + 0xa8c), 0x00180017);
	EMI_DBG_SIMPLE_R("[EMI] 0x1022da84 = 0x%x\n",
		(CHA_EMI_BASE + 0xa84));
	EMI_DBG_SIMPLE_R("[EMI] 0x1022da84 = 0x%x\n",
		(CHA_EMI_BASE + 0xa84));

	EMI_DBG_SIMPLE_RWR("[EMI] 0x1022da88 = 0x%x\n",
		(CHA_EMI_BASE + 0xa88), 0x001a0019);
	EMI_DBG_SIMPLE_RWR("[EMI] 0x1022da8c = 0x%x\n",
		(CHA_EMI_BASE + 0xa8c), 0x0001001b);
	EMI_DBG_SIMPLE_R("[EMI] 0x1022da84 = 0x%x\n",
		(CHA_EMI_BASE + 0xa84));
	EMI_DBG_SIMPLE_R("[EMI] 0x1022da84 = 0x%x\n",
		(CHA_EMI_BASE + 0xa84));

	EMI_DBG_SIMPLE_RWR("[EMI] 0x1022da88 = 0x%x\n",
		(CHA_EMI_BASE + 0xa88), 0x00370036);
	EMI_DBG_SIMPLE_RWR("[EMI] 0x1022da8c = 0x%x\n",
		(CHA_EMI_BASE + 0xa8c), 0x00390038);
	EMI_DBG_SIMPLE_R("[EMI] 0x1022da84 = 0x%x\n",
		(CHA_EMI_BASE + 0xa84));
	EMI_DBG_SIMPLE_R("[EMI] 0x1022da84 = 0x%x\n",
		(CHA_EMI_BASE + 0xa84));

	EMI_DBG_SIMPLE_RWR("[EMI] 0x1022da88 = 0x%x\n",
		(CHA_EMI_BASE + 0xa88), 0x00050004);
	EMI_DBG_SIMPLE_RWR("[EMI] 0x1022da8c = 0x%x\n",
		(CHA_EMI_BASE + 0xa8c), 0x00070006);
	EMI_DBG_SIMPLE_R("[EMI] 0x1022da84 = 0x%x\n",
		(CHA_EMI_BASE + 0xa84));
	EMI_DBG_SIMPLE_R("[EMI] 0x1022da84 = 0x%x\n",
		(CHA_EMI_BASE + 0xa84));

	EMI_DBG_SIMPLE_RWR("[EMI] 0x1022da88 = 0x%x\n",
		(CHA_EMI_BASE + 0xa88), 0x000e0008);
	EMI_DBG_SIMPLE_RWR("[EMI] 0x1022da8c = 0x%x\n",
		(CHA_EMI_BASE + 0xa8c), 0x0010000f);
	EMI_DBG_SIMPLE_R("[EMI] 0x1022da84 = 0x%x\n",
		(CHA_EMI_BASE + 0xa84));
	EMI_DBG_SIMPLE_R("[EMI] 0x1022da84 = 0x%x\n",
		(CHA_EMI_BASE + 0xa84));

	EMI_DBG_SIMPLE_RWR("[EMI] 0x1022da88 = 0x%x\n",
		(CHA_EMI_BASE + 0xa88), 0x000a0009);
	EMI_DBG_SIMPLE_RWR("[EMI] 0x1022da8c = 0x%x\n",
		(CHA_EMI_BASE + 0xa8c), 0x000c000b);
	EMI_DBG_SIMPLE_R("[EMI] 0x1022da84 = 0x%x\n",
		(CHA_EMI_BASE + 0xa84));
	EMI_DBG_SIMPLE_R("[EMI] 0x1022da84 = 0x%x\n",
		(CHA_EMI_BASE + 0xa84));
}
EXPORT_SYMBOL(dump_emi_outstanding);

void mbw_init(void)
{
	struct device_node *node;

	CEN_EMI_BASE = mt_cen_emi_base_get();
	CHA_EMI_BASE = mt_chn_emi_base_get();

	if (INFRACFG_BASE == NULL) {
		node = of_find_compatible_node(NULL, NULL, "mediatek,infracfg");
		if (node) {
			INFRACFG_BASE = of_iomap(node, 0);
			pr_debug("get INFRACFG_BASE@ %p\n", INFRACFG_BASE);
		} else
			pr_debug("can't find compatible node for INFRACFG_BASE\n");
	}

	if (INFRA_AO_BASE == NULL) {
		node = of_find_compatible_node(NULL, NULL, "mediatek,infracfg_ao");
		if (node) {
			INFRA_AO_BASE = of_iomap(node, 0);
			pr_info("get INFRA_AO_BASE@ %p\n", INFRACFG_BASE);
		} else
			pr_info("can't find compatible node for INFRA_AO_BASE\n");
	}
}

MODULE_DESCRIPTION("MediaTek EMI LEGACY V0 Driver");
MODULE_LICENSE("GPL v2");
