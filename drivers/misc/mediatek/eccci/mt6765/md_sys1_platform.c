// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/module.h>
#include "ccci_config.h"
#include "ccci_common_config.h"
#include <linux/clk.h>
#include <mtk_pbm.h>
//#include <mtk_clkbuf_ctl.h>
#include <mt-plat/mtk-clkbuf-bridge.h>
#include <linux/pm_runtime.h>

#ifdef CONFIG_MTK_EMI_BWL
#include <emi_mbw.h>
#endif

#ifdef FEATURE_INFORM_NFC_VSIM_CHANGE
#include <mach/mt6605.h>
#endif

//#include "include/pmic_api_buck.h"
//#include <mt-plat/upmu_common.h>
//#include <mtk_spm_sleep.h>
//#include <linux/pm_qos.h>
#include <linux/soc/mediatek/mtk-pm-qos.h>

#include "ccci_core.h"
#include "ccci_platform.h"
#include <linux/regulator/consumer.h> /* for MD PMIC */


#include "md_sys1_platform.h"
#include "cldma_reg.h"
#include "modem_reg_base.h"

static struct regulator *reg_vmodem, *reg_vsram;

static struct ccci_clk_node clk_table[] = {
	{ NULL,	"scp-sys-md1-main"},

};
#if defined(CONFIG_PINCTRL_ELBRUS)
static struct pinctrl *mdcldma_pinctrl;
#endif

//volatile unsigned int devapc_check_flag = 1;

static void __iomem *md_sram_pd_psmcusys_base;


#define TAG "mcd"

#define ROr2W(a, b, c)  cldma_write32(a, b, (cldma_read32(a, b)|c))
#define RAnd2W(a, b, c)  cldma_write32(a, b, (cldma_read32(a, b)&c))
#define RabIsc(a, b, c) ((cldma_read32(a, b)&c) != c)


static int md_cd_io_remap_md_side_register(struct ccci_modem *md);
static void md_cd_dump_debug_register(struct ccci_modem *md);
static void md_cd_dump_md_bootup_status(struct ccci_modem *md);
static void md_cd_get_md_bootup_status(struct ccci_modem *md,
	unsigned int *buff, int length);
static void md_cd_check_emi_state(struct ccci_modem *md, int polling);
static int md_start_platform(struct ccci_modem *md);
static int md_cd_power_on(struct ccci_modem *md);
static int md_cd_power_off(struct ccci_modem *md, unsigned int timeout);
static int md_cd_soft_power_off(struct ccci_modem *md, unsigned int mode);
static int md_cd_soft_power_on(struct ccci_modem *md, unsigned int mode);
static int md_cd_let_md_go(struct ccci_modem *md);
static void md_cd_lock_cldma_clock_src(int locked);
static void md_cd_lock_modem_clock_src(int locked);

static int ccci_modem_remove(struct platform_device *dev);
static void ccci_modem_shutdown(struct platform_device *dev);
static int ccci_modem_suspend(struct platform_device *dev, pm_message_t state);
static int ccci_modem_resume(struct platform_device *dev);
static int ccci_modem_pm_suspend(struct device *device);
static int ccci_modem_pm_resume(struct device *device);
static int ccci_modem_pm_restore_noirq(struct device *device);
static int md_cd_vcore_config(unsigned int md_id, unsigned int hold_req);



struct ccci_plat_ops md_cd_plat_ptr = {
	.init = &ccci_platform_init_6765,
	//.cldma_hw_rst = &md_cldma_hw_reset,
	//.set_clk_cg = &ccci_set_clk_cg,
	.remap_md_reg = &md_cd_io_remap_md_side_register,
	.lock_cldma_clock_src = &md_cd_lock_cldma_clock_src,
	.lock_modem_clock_src = &md_cd_lock_modem_clock_src,
	.dump_md_bootup_status = &md_cd_dump_md_bootup_status,
	.get_md_bootup_status = &md_cd_get_md_bootup_status,
	.debug_reg = &md_cd_dump_debug_register,
	.check_emi_state = &md_cd_check_emi_state,
	.soft_power_off = &md_cd_soft_power_off,
	.soft_power_on = &md_cd_soft_power_on,
	.start_platform = md_start_platform,
	.power_on = &md_cd_power_on,
	.let_md_go = &md_cd_let_md_go,
	.power_off = &md_cd_power_off,
	.vcore_config = &md_cd_vcore_config,
};



int md_cd_get_modem_hw_info(struct platform_device *dev_ptr,
	struct ccci_dev_cfg *dev_cfg, struct md_hw_info *hw_info)
{
	struct device_node *node = NULL;
	struct device_node *node_infrao = NULL;
	int idx = 0;

	if (dev_ptr->dev.of_node == NULL) {
		CCCI_ERROR_LOG(0, TAG, "modem OF node NULL\n");
		return -1;
	}

	memset(dev_cfg, 0, sizeof(struct ccci_dev_cfg));
	of_property_read_u32(dev_ptr->dev.of_node,
		"mediatek,md_id", &dev_cfg->index);
	CCCI_DEBUG_LOG(dev_cfg->index, TAG,
		"modem hw info get idx:%d\n", dev_cfg->index);
	if (!get_modem_is_enabled(dev_cfg->index)) {
		CCCI_ERROR_LOG(dev_cfg->index, TAG,
			"modem %d not enable, exit\n", dev_cfg->index + 1);
		return -1;
	}

	memset(hw_info, 0, sizeof(struct md_hw_info));

	switch (dev_cfg->index) {
	case 0:		/* MD_SYS1 */
		dev_cfg->major = 0;
		dev_cfg->minor_base = 0;
		of_property_read_u32(dev_ptr->dev.of_node,
			"mediatek,cldma_capability", &dev_cfg->capability);

		hw_info->ap_ccif_base =
		 (unsigned long)of_iomap(dev_ptr->dev.of_node, 2);
		hw_info->md_ccif_base =
		 (unsigned long)of_iomap(dev_ptr->dev.of_node, 3);
		if (!(hw_info->ap_ccif_base && hw_info->md_ccif_base)) {
			CCCI_ERROR_LOG(-1, TAG, "%s: hw_info of_iomap failed\n",
				       dev_ptr->dev.of_node->full_name);
			return -1;
		}
		hw_info->ap_ccif_irq0_id =
		 irq_of_parse_and_map(dev_ptr->dev.of_node, 1);
		hw_info->ap_ccif_irq1_id =
		 irq_of_parse_and_map(dev_ptr->dev.of_node, 2);
		hw_info->md_wdt_irq_id =
		 irq_of_parse_and_map(dev_ptr->dev.of_node, 3);

		hw_info->md_pcore_pccif_base =
		 ioremap_nocache(MD_PCORE_PCCIF_BASE, 0x20);
		CCCI_BOOTUP_LOG(dev_cfg->index, TAG,
		 "pccif:%x\n", MD_PCORE_PCCIF_BASE);


		/* Device tree using none flag to register irq,
		 * sensitivity has set at "irq_of_parse_and_map"
		 */
		hw_info->ap_ccif_irq0_flags = IRQF_TRIGGER_NONE;
		hw_info->ap_ccif_irq1_flags = IRQF_TRIGGER_NONE;
		hw_info->md_wdt_irq_flags = IRQF_TRIGGER_NONE;
		hw_info->ap2md_bus_timeout_irq_flags = IRQF_TRIGGER_NONE;

		hw_info->sram_size = CCIF_SRAM_SIZE;
		hw_info->md_rgu_base = MD_RGU_BASE;
		hw_info->md_boot_slave_En = MD_BOOT_VECTOR_EN;
		of_property_read_u32(dev_ptr->dev.of_node,
			"mediatek,md_generation", &md_cd_plat_val_ptr.md_gen);
		node_infrao = of_find_compatible_node(NULL, NULL,
			"mediatek,mt6765-infracfg");
		md_cd_plat_val_ptr.infra_ao_base = of_iomap(node_infrao, 0);

		hw_info->plat_ptr = &md_cd_plat_ptr;
		hw_info->plat_val = &md_cd_plat_val_ptr;
		if ((hw_info->plat_ptr == NULL) || (hw_info->plat_val == NULL))
			return -1;
		hw_info->plat_val->offset_epof_md1 = 7*1024+0x234;
#if defined(CONFIG_PINCTRL_ELBRUS)
		mdcldma_pinctrl = devm_pinctrl_get(&dev_ptr->dev);
		if (IS_ERR(mdcldma_pinctrl)) {
			CCCI_ERROR_LOG(dev_cfg->index, TAG,
				"modem %d get mdcldma_pinctrl failed\n",
							dev_cfg->index + 1);
			return -1;
		}
#else
		CCCI_ERROR_LOG(dev_cfg->index, TAG,
			"gpio pinctrl is not ready yet, use workaround.\n");
#endif
		for (idx = 0; idx < ARRAY_SIZE(clk_table); idx++) {
			clk_table[idx].clk_ref = devm_clk_get(&dev_ptr->dev,
				clk_table[idx].clk_name);
			if (IS_ERR(clk_table[idx].clk_ref)) {
				CCCI_ERROR_LOG(dev_cfg->index, TAG,
					 "md%d get %s failed\n",
						dev_cfg->index + 1,
						clk_table[idx].clk_name);
				clk_table[idx].clk_ref = NULL;
			}
		}
		node = of_find_compatible_node(NULL, NULL,
			"mediatek,mt6765-apmixedsys");
		hw_info->ap_mixed_base = (unsigned long)of_iomap(node, 0);
		if (!hw_info->ap_mixed_base) {
			CCCI_ERROR_LOG(-1, TAG,
				"%s: hw_info->ap_mixed_base of_iomap failed\n",
				node->full_name);
			return -1;
		}
		node = of_find_compatible_node(NULL, NULL, "mediatek,topckgen");
		if (node)
			hw_info->ap_topclkgen_base = of_iomap(node, 0);
		else
			hw_info->ap_topclkgen_base =
				ioremap_nocache(0x10000000, 4);
		if (!hw_info->ap_topclkgen_base) {
			CCCI_ERROR_LOG(-1, TAG,
			"%s:ioremap topclkgen base address fail\n",
			__func__);
			return -1;
		}
		break;
	default:
		return -1;
	}

	if (hw_info->ap_ccif_base == 0 ||
		hw_info->md_ccif_base == 0) {
		CCCI_ERROR_LOG(dev_cfg->index, TAG,
			"ap_ccif_base:0x%p, md_ccif_base:0x%p\n",
			(void *)hw_info->ap_ccif_base,
			(void *)hw_info->md_ccif_base);
		return -1;
	}
	if (hw_info->ap_ccif_irq0_id == 0 ||
		hw_info->ap_ccif_irq1_id == 0 ||
		hw_info->md_wdt_irq_id == 0) {
		CCCI_ERROR_LOG(dev_cfg->index, TAG,
			"ccif_irq0:%d,ccif_irq0:%d,md_wdt_irq:%d\n",
			hw_info->ap_ccif_irq0_id,
			hw_info->ap_ccif_irq1_id, hw_info->md_wdt_irq_id);
		return -1;
	}

	CCCI_DEBUG_LOG(dev_cfg->index, TAG,
		"dev_major:%d,minor_base:%d,capability:%d\n",
		dev_cfg->major, dev_cfg->minor_base, dev_cfg->capability);

	CCCI_DEBUG_LOG(dev_cfg->index, TAG,
		"ap_ccif_base:0x%p, md_ccif_base:0x%p\n",
					(void *)hw_info->ap_ccif_base,
					(void *)hw_info->md_ccif_base);

	CCCI_DEBUG_LOG(dev_cfg->index, TAG,
		"ccif_irq0:%d,ccif_irq1:%d,md_wdt_irq:%d\n",
		hw_info->ap_ccif_irq0_id,
		hw_info->ap_ccif_irq1_id, hw_info->md_wdt_irq_id);

	return 0;
}

static int md_cd_io_remap_md_side_register(struct ccci_modem *md)
{
	struct md_pll_reg *md_reg;
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;

	md_info->md_boot_slave_En =
	 ioremap_nocache(md->hw_info->md_boot_slave_En, 0x4);
	md_info->md_rgu_base =
	 ioremap_nocache(md->hw_info->md_rgu_base, 0x300);
	md_info->l1_rgu_base =
	 ioremap_nocache(md->hw_info->l1_rgu_base, 0x40);
	md_info->md_global_con0 =
	 ioremap_nocache(MD_GLOBAL_CON0, 0x4);


	md_reg = kzalloc(sizeof(struct md_pll_reg), GFP_KERNEL);
	if (md_reg == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
		 "cldma_sw_init:alloc md reg map mem fail\n");
		return -1;
	}
	/*needed by bootup flow start*/
	md_reg->md_top_Pll =
		ioremap_nocache(MDTOP_PLLMIXED_BASE, MDTOP_PLLMIXED_LENGTH);
	md_reg->md_top_clkSW =
		ioremap_nocache(MDTOP_CLKSW_BASE, MDTOP_CLKSW_LENGTH);
	/*needed by bootup flow end*/

	md_reg->md_boot_stats0 = ioremap_nocache(MD1_CFG_BOOT_STATS0, 4);
	md_reg->md_boot_stats1 = ioremap_nocache(MD1_CFG_BOOT_STATS1, 4);
	/*just for dump end*/

	md_info->md_pll_base = md_reg;

	md_sram_pd_psmcusys_base =
		ioremap_nocache(MD_SRAM_PD_PSMCUSYS_SRAM_BASE,
			MD_SRAM_PD_PSMCUSYS_SRAM_LEN);

#ifdef MD_PEER_WAKEUP
	md_info->md_peer_wakeup = ioremap_nocache(MD_PEER_WAKEUP, 0x4);
#endif
	return 0;
}

static void md_cd_lock_cldma_clock_src(int locked)
{
	/* spm_ap_mdsrc_req(locked); */
}

static void md_cd_lock_modem_clock_src(int locked)
{
	//spm_ap_mdsrc_req(locked);
}

static void md_cd_dump_md_bootup_status(struct ccci_modem *md)
{
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;
	struct md_pll_reg *md_reg = md_info->md_pll_base;

	/*To avoid AP/MD interface delay,
	 * dump 3 times, and buy-in the 3rd dump value.
	 */

	cldma_read32(md_reg->md_boot_stats0, 0);	/* dummy read */
	cldma_read32(md_reg->md_boot_stats0, 0);	/* dummy read */
	CCCI_NOTICE_LOG(md->index, TAG,
		"md_boot_stats0:0x%X\n",
		cldma_read32(md_reg->md_boot_stats0, 0));

	cldma_read32(md_reg->md_boot_stats1, 0);	/* dummy read */
	cldma_read32(md_reg->md_boot_stats1, 0);	/* dummy read */
	CCCI_NOTICE_LOG(md->index, TAG,
		"md_boot_stats1:0x%X\n",
		cldma_read32(md_reg->md_boot_stats1, 0));
}

static void md_cd_get_md_bootup_status(
	struct ccci_modem *md, unsigned int *buff, int length)
{
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;
	struct md_pll_reg *md_reg = md_info->md_pll_base;

	CCCI_NOTICE_LOG(md->index, TAG, "md_boot_stats len %d\n", length);

	if (length < 2) {
		md_cd_dump_md_bootup_status(md);
		return;
	}

	cldma_read32(md_reg->md_boot_stats0, 0);	/* dummy read */
	cldma_read32(md_reg->md_boot_stats0, 0);	/* dummy read */
	buff[0] = cldma_read32(md_reg->md_boot_stats0, 0);

	cldma_read32(md_reg->md_boot_stats1, 0);	/* dummy read */
	cldma_read32(md_reg->md_boot_stats1, 0);	/* dummy read */
	buff[1] = cldma_read32(md_reg->md_boot_stats1, 0);
	CCCI_NOTICE_LOG(md->index, TAG,
		"md_boot_stats0 / 1:0x%X / 0x%X\n", buff[0], buff[1]);

}

static int dump_emi_last_bm(struct ccci_modem *md)
{
	u32 buf_len = 1024;
	u32 i, j;
	char temp_char;
	char *buf = NULL;
	char *temp_buf = NULL;

	buf = kzalloc(buf_len, GFP_ATOMIC);
	if (!buf) {
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"alloc memory failed for emi last bm\n");
		return -1;
	}
#ifdef CONFIG_MTK_EMI_BWL
	dump_last_bm(buf, buf_len);
#endif
	CCCI_MEM_LOG_TAG(md->index, TAG, "Dump EMI last bm\n");
	buf[buf_len - 1] = '\0';
	temp_buf = buf;
	for (i = 0, j = 1; i < buf_len - 1; i++, j++) {
		if (buf[i] == 0x0) /* 0x0 end of hole string. */
			break;
		if (buf[i] == 0x0A && j < 256) {
			/* 0x0A stands for end of string, no 0x0D */
			buf[i] = '\0';
			CCCI_MEM_LOG(md->index, TAG,
				"%s\n", temp_buf);/* max 256 bytes */
			temp_buf = buf + i + 1;
			j = 0;
		} else if (unlikely(j >= 255)) {
			/* ccci_mem_log max buffer length: 256,
			 * but dm log maybe only less than 50 bytes.
			 */
			temp_char = buf[i];
			buf[i] = '\0';
			CCCI_MEM_LOG(md->index, TAG, "%s\n", temp_buf);
			temp_buf = buf + i;
			j = 0;
			buf[i] = temp_char;
		}
	}

	kfree(buf);

	return 0;
}

static void md_cd_dump_debug_register(struct ccci_modem *md)
{
	/* MD no need dump because of bus hang happened - open for debug */
	struct ccci_per_md *per_md_data = &md->per_md_data;
	unsigned int reg_value[2] = { 0 };
	unsigned int ccif_sram[
		CCCI_EE_SIZE_CCIF_SRAM/sizeof(unsigned int)] = { 0 };
	void __iomem *dump_reg0;

	/*dump_emi_latency();*/
	dump_emi_last_bm(md);

	md_cd_get_md_bootup_status(md, reg_value, 2);
	md->ops->dump_info(md, DUMP_FLAG_CCIF, ccif_sram, 0);
	/* copy from HS1 timeout */
	if ((reg_value[0] == 0) && (ccif_sram[1] == 0))
		return;
	else if (!((reg_value[0] == 0x54430007) || (reg_value[0] == 0) ||
		(reg_value[0] >= 0x53310000 && reg_value[0] <= 0x533100FF)))
		return;
	if (unlikely(in_interrupt())) {
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"In interrupt, skip dump MD debug register.\n");
		return;
	}
	md_cd_lock_modem_clock_src(1);

	/* 1. pre-action */
	if (per_md_data->md_dbg_dump_flag &
		 (MD_DBG_DUMP_ALL & ~(1 << MD_DBG_DUMP_SMEM))) {
		dump_reg0 = ioremap_nocache(MD1_OPEN_DEBUG_APB_CLK, 0x1000);
		ccci_write32(dump_reg0, 0x430, 0x1);
		udelay(1000);
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"md_dbg_sys:0x%X\n", cldma_read32(dump_reg0, 0x430));
		iounmap(dump_reg0);
	} else {
		md_cd_lock_modem_clock_src(0);
		return;
	}

	/* 1. PC Monitor */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_PCMON)) {
		CCCI_MEM_LOG_TAG(md->index, TAG, "Dump MD PC monitor\n");
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"common: 0x%X\n", (MD_PC_MONITOR_BASE + 0x800));
		/* Stop all PCMon */
		dump_reg0 =
		 ioremap_nocache(MD_PC_MONITOR_BASE, MD_PC_MONITOR_LEN);
		ccci_write32(dump_reg0, 0x800, 0x22); /* stop MD PCMon */
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x800), 0x100);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, dump_reg0 + 0x900, 0x60);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xA00), 0x60);
		/* core0 */
		CCCI_MEM_LOG_TAG(md->index, TAG, "core0/1: [0]0x%X, [1]0x%X\n",
				MD_PC_MONITOR_BASE,
				(MD_PC_MONITOR_BASE + 0x400));
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, dump_reg0, 0x400);
		/* core1 */
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x400), 0x400);
		/* Resume PCMon */
		ccci_write32(dump_reg0, 0x800, 0x11);
		ccci_read32(dump_reg0, 0x800);
		iounmap(dump_reg0);
	}

	/* 2. dump PLL */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_PLL)) {
		CCCI_MEM_LOG_TAG(md->index, TAG, "Dump MD PLL\n");
		/* MD CLKSW */
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"CLKSW: [0]0x%X, [1]0x%X, [2]0x%X\n",
			MD_CLKSW_BASE, (MD_CLKSW_BASE + 0x100),
			(MD_CLKSW_BASE + 0xF00));
		dump_reg0 = ioremap_nocache(MD_CLKSW_BASE, MD_CLKSW_LEN);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, dump_reg0, 0xD4);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x100), 0x18);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xF00), 0x8);
		iounmap(dump_reg0);
		/* MD PLLMIXED */
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"PLLMIXED:[0]0x%X,[1]0x%X,[2]0x%X,[3]0x%X,[4]0x%X,[5]0x%X,[6]0x%X,[7]0x%X,[8]0x%X,[9]0x%X\n",
			MD_PLL_MIXED_BASE,
			(MD_PLL_MIXED_BASE + 0x100),
			(MD_PLL_MIXED_BASE + 0x200),
			(MD_PLL_MIXED_BASE + 0x300),
			(MD_PLL_MIXED_BASE + 0x400),
			(MD_PLL_MIXED_BASE + 0x500),
			(MD_PLL_MIXED_BASE + 0x600),
			(MD_PLL_MIXED_BASE + 0xC00),
			(MD_PLL_MIXED_BASE + 0xD00),
			(MD_PLL_MIXED_BASE + 0xF00));
		dump_reg0 =
		 ioremap_nocache(MD_PLL_MIXED_BASE, MD_PLL_MIXED_LEN);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, dump_reg0, 0x68);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x100), 0x18);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x200), 0x8);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x300), 0x1C);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x400), 0x5C);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x500), 0xD0);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x600), 0x10);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xC00), 0x48);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xD00), 0x8);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xF00), 0x14);
		iounmap(dump_reg0);
		/* MD CLKCTL */
		CCCI_MEM_LOG_TAG(md->index, TAG, "CLKCTL: [0]0x%X, [1]0x%X\n",
			MD_CLKCTL_BASE, (MD_CLKCTL_BASE + 0x100));
		dump_reg0 = ioremap_nocache(MD_CLKCTL_BASE, MD_CLKCTL_LEN);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, dump_reg0, 0x1C);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x100), 0x20);
		iounmap(dump_reg0);
		/* MD GLOBAL CON */
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"GLOBAL CON: [0]0x%X, [1]0x%X, [2]0x%X, [3]0x%X, [4]0x%X, [5]0x%X, [6]0x%X, [7]0x%X, [8]0x%X\n",
			MD_GLOBALCON_BASE,
			(MD_GLOBALCON_BASE + 0x100),
			(MD_GLOBALCON_BASE + 0x200),
			(MD_GLOBALCON_BASE + 0x300),
			(MD_GLOBALCON_BASE + 0x800),
			(MD_GLOBALCON_BASE + 0x900),
			(MD_GLOBALCON_BASE + 0xC00),
			(MD_GLOBALCON_BASE + 0xD00),
			(MD_GLOBALCON_BASE + 0xF00));
		dump_reg0 =
		 ioremap_nocache(MD_GLOBALCON_BASE, MD_GLOBALCON_LEN);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, dump_reg0, 0xA0);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x100), 0x10);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x200), 0x98);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x300), 0x24);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x800), 0x8);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x900), 0x8);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xC00), 0x1C);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xD00), 4);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0xF00), 8);
		iounmap(dump_reg0);
	}

	/* 3. Bus status */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_BUS)) {
#if defined(CONFIG_MACH_MT6765)
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"Dump MD Bus status: [0]0x%X, [1]0x%X, [2]0x%X, [3]0x%X\n",
			MD_BUS_REG_BASE0, MD_BUS_REG_BASE1,
			MD_BUS_REG_BASE2, MD_BUS_REG_BASE3);
		dump_reg0 = ioremap_nocache(MD_BUS_REG_BASE0, MD_BUS_REG_LEN0);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, dump_reg0, MD_BUS_REG_LEN0);
		iounmap(dump_reg0);
#elif defined(CONFIG_MACH_MT6761)
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"Dump MD Bus status: [0]0x%X, [1]0x%X, [2]0x%X\n",
			MD_BUS_REG_BASE1, MD_BUS_REG_BASE2,
			MD_BUS_REG_BASE3);
#endif
		dump_reg0 = ioremap_nocache(MD_BUS_REG_BASE1, MD_BUS_REG_LEN1);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, dump_reg0, MD_BUS_REG_LEN1);
		iounmap(dump_reg0);
		dump_reg0 = ioremap_nocache(MD_BUS_REG_BASE2, MD_BUS_REG_LEN2);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, dump_reg0, MD_BUS_REG_LEN2);
		iounmap(dump_reg0);
		dump_reg0 = ioremap_nocache(MD_BUS_REG_BASE3, MD_BUS_REG_LEN3);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, dump_reg0, MD_BUS_REG_LEN3);
		iounmap(dump_reg0);
	}

	/* 4. Bus REC */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_BUSREC)) {
#if defined(CONFIG_MACH_MT6765)
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"Dump MD Bus REC: [0]0x%X, [1]0x%X, [2]0x%X\n",
			MD_MCU_MO_BUSREC_BASE, MD_INFRA_BUSREC_BASE,
			MD_BUSREC_LAY_BASE);
#elif defined(CONFIG_MACH_MT6761)
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"Dump MD Bus REC: [0]0x%X, [1]0x%X\n",
			MD_MCU_MO_BUSREC_BASE, MD_INFRA_BUSREC_BASE);
#endif
		dump_reg0 =
		 ioremap_nocache(MD_MCU_MO_BUSREC_BASE, MD_MCU_MO_BUSREC_LEN);
		ccci_write32(dump_reg0, 0x10, 0x0); /* stop */
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x0), 0x104);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x200), 0x18);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x300), 0x30);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x400), 0x18);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x500), 0x30);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x700), 0x51C);
		ccci_write32(dump_reg0, 0x10, 0x1); /* re-start */
		iounmap(dump_reg0);
		dump_reg0 =
		 ioremap_nocache(MD_INFRA_BUSREC_BASE, MD_INFRA_BUSREC_LEN);
		ccci_write32(dump_reg0, 0x10, 0x0); /* stop */
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x0), 0x104);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x200), 0x18);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x300), 0x30);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x400), 0x18);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x500), 0x30);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x700), 0x51C);
		ccci_write32(dump_reg0, 0x10, 0x1);/* re-start */
		iounmap(dump_reg0);
#if defined(CONFIG_ARCH_MT6765)
		dump_reg0 =
		 ioremap_nocache(MD_BUSREC_LAY_BASE, MD_BUSREC_LAY_LEN);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, dump_reg0, 0x8);
		iounmap(dump_reg0);
#endif
	}

	/* 5. ECT */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_ECT)) {
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"Dump MD ECT: [0]0x%X, [1]0x%X, [2]0x%X, [3]0x%X\n",
			MD_ECT_REG_BASE0, MD_ECT_REG_BASE1,
			(MD_ECT_REG_BASE2 + 0x14), (MD_ECT_REG_BASE2 + 0x0C));
		dump_reg0 =
		 ioremap_nocache(MD_ECT_REG_BASE0, MD_ECT_REG_LEN0);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, dump_reg0, MD_ECT_REG_LEN0);
		iounmap(dump_reg0);
		dump_reg0 =
		 ioremap_nocache(MD_ECT_REG_BASE1, MD_ECT_REG_LEN1);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, dump_reg0, MD_ECT_REG_LEN1);
		iounmap(dump_reg0);
		dump_reg0 =
		 ioremap_nocache(MD_ECT_REG_BASE2, MD_ECT_REG_LEN2);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x14), 0x4);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x0C), 0x4);
		iounmap(dump_reg0);
	}

	/*avoid deadlock and set bus protect*/
	if (per_md_data->md_dbg_dump_flag & ((1 << MD_DBG_DUMP_TOPSM) |
			(1 << MD_DBG_DUMP_MDRGU) | (1 << MD_DBG_DUMP_OST))) {
		RAnd2W(md->hw_info->plat_val->infra_ao_base,
			INFRA_AP2MD_DUMMY_REG,
			(~(0x1 << INFRA_AP2MD_DUMMY_BIT)));
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"ap2md dummy reg 0x%X: 0x%X\n", INFRA_AP2MD_DUMMY_REG,
			cldma_read32(md->hw_info->plat_val->infra_ao_base,
			INFRA_AP2MD_DUMMY_REG));
		/*disable MD to AP*/
		cldma_write32(md->hw_info->plat_val->infra_ao_base,
			INFRA_MD2PERI_PROT_SET,
			(0x1 << INFRA_MD2PERI_PROT_BIT));
		while ((cldma_read32(md->hw_info->plat_val->infra_ao_base,
			INFRA_MD2PERI_PROT_RDY)
			& (0x1 << INFRA_MD2PERI_PROT_BIT))
			!= (0x1 << INFRA_MD2PERI_PROT_BIT))
			;
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"md2peri: en[0x%X], rdy[0x%X]\n",
			cldma_read32(md->hw_info->plat_val->infra_ao_base,
			INFRA_MD2PERI_PROT_EN),
			cldma_read32(md->hw_info->plat_val->infra_ao_base,
			INFRA_MD2PERI_PROT_RDY));
		/*make sure AP to MD is enabled*/
		cldma_write32(md->hw_info->plat_val->infra_ao_base,
			INFRA_PERI2MD_PROT_CLR,
			(0x1 << INFRA_PERI2MD_PROT_BIT));
		while ((cldma_read32(md->hw_info->plat_val->infra_ao_base,
			INFRA_PERI2MD_PROT_RDY)
			& (0x1 << INFRA_PERI2MD_PROT_BIT)))
			;
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"peri2md: en[0x%X], rdy[0x%X]\n",
			cldma_read32(md->hw_info->plat_val->infra_ao_base,
			INFRA_PERI2MD_PROT_EN),
			cldma_read32(md->hw_info->plat_val->infra_ao_base,
			INFRA_PERI2MD_PROT_RDY));
	}

	/* 6. TOPSM */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_TOPSM)) {
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"Dump MD TOPSM status: 0x%X\n", MD_TOPSM_REG_BASE);
		dump_reg0 =
		 ioremap_nocache(MD_TOPSM_REG_BASE, MD_TOPSM_REG_LEN);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, dump_reg0, MD_TOPSM_REG_LEN);
		iounmap(dump_reg0);
	}

	/* 7. MD RGU */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_MDRGU)) {
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"Dump MD RGU: [0]0x%X, [1]0x%X\n",
			MD_RGU_REG_BASE, (MD_RGU_REG_BASE + 0x200));
		dump_reg0 =
		 ioremap_nocache(MD_RGU_REG_BASE, MD_RGU_REG_LEN);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, dump_reg0, 0xCC);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x200), 0x5C);
		iounmap(dump_reg0);
	}

	/* 8 OST */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_OST)) {
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"Dump MD OST status: [0]0x%X, [1]0x%X\n",
			MD_OST_STATUS_BASE, (MD_OST_STATUS_BASE + 0x200));
		dump_reg0 =
		 ioremap_nocache(MD_OST_STATUS_BASE, 0x300);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, dump_reg0, 0xF0);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, (dump_reg0 + 0x200), 0x8);
		iounmap(dump_reg0);
		/* 9 CSC */
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"Dump MD CSC: 0x%X\n", MD_CSC_REG_BASE);
		dump_reg0 =
		 ioremap_nocache(MD_CSC_REG_BASE, MD_CSC_REG_LEN);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, dump_reg0, MD_CSC_REG_LEN);
		iounmap(dump_reg0);
		/* 10 ELM */
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"Dump MD ELM: 0x%X\n", MD_ELM_REG_BASE);
		dump_reg0 =
		 ioremap_nocache(MD_ELM_REG_BASE, 0x480);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, dump_reg0, 0x480);
		iounmap(dump_reg0);
	}

	/* Clear flags for wdt timeout dump MDRGU */
	md->per_md_data.md_dbg_dump_flag &= (~((1 << MD_DBG_DUMP_TOPSM)
		| (1 << MD_DBG_DUMP_MDRGU) | (1 << MD_DBG_DUMP_OST)));

	md_cd_lock_modem_clock_src(0);

}

//static void md_cd_check_md_DCM(struct md_cd_ctrl *md_ctrl)
//{
//}

static void md_cd_check_emi_state(struct ccci_modem *md, int polling)
{
}

static int md_start_platform(struct ccci_modem *md)
{
	int ret = 0;

	reg_vmodem = devm_regulator_get_optional(&md->plat_dev->dev, "_vmodem");
	if (IS_ERR(reg_vmodem)) {
		ret = PTR_ERR(reg_vmodem);
		if ((ret != -ENODEV) && md->plat_dev->dev.of_node) {
			CCCI_ERROR_LOG(md->index, TAG,
				"get regulator(PMIC) fail: ret = %d\n", ret);
			return ret;
		}
	}
	reg_vsram = devm_regulator_get_optional(&md->plat_dev->dev, "_vcore");
	if (IS_ERR(reg_vsram)) {
		ret = PTR_ERR(reg_vsram);
		if ((ret != -ENODEV) && md->plat_dev->dev.of_node) {
			CCCI_ERROR_LOG(md->index, TAG,
				"get regulator(PMIC1) fail: ret = %d\n", ret);
			return ret;
		}
	}

	return ret;
}

static void md1_pmic_setting_on(void)
{
	int ret = 0;

	ret = regulator_set_voltage(reg_vmodem, 800000, 800000);
	if (ret)
		CCCI_ERROR_LOG(-1, TAG, "pmic_vmodem setting on fail\n");
	ret = regulator_sync_voltage(reg_vmodem);
	if (ret)
		CCCI_ERROR_LOG(-1, TAG, "pmic_vmodem setting on fail\n");

//	ret = regulator_set_voltage(reg_vsram, 800000, 800000);
//	if (ret)
//		CCCI_ERROR_LOG(-1, TAG, "pmic_vsram setting on fail\n");
//	ret = regulator_sync_voltage(reg_vsram);
//	if (ret)
//		CCCI_ERROR_LOG(-1, TAG, "pmic_vsram setting on fail\n");

}

static void md1_pre_access_md_reg(struct ccci_modem *md)
{
	/*clear dummy reg flag to access modem reg*/
	RAnd2W(md->hw_info->plat_val->infra_ao_base,
		INFRA_AP2MD_DUMMY_REG,
		(~(0x1 << INFRA_AP2MD_DUMMY_BIT)));
	CCCI_BOOTUP_LOG(md->index, TAG,
		"pre: ap2md dummy reg 0x%X: 0x%X\n", INFRA_AP2MD_DUMMY_REG,
		cldma_read32(md->hw_info->plat_val->infra_ao_base,
		INFRA_AP2MD_DUMMY_REG));
	/*disable MD to AP*/
	cldma_write32(md->hw_info->plat_val->infra_ao_base,
		INFRA_MD2PERI_PROT_SET,
		(0x1 << INFRA_MD2PERI_PROT_BIT));
	while ((cldma_read32(md->hw_info->plat_val->infra_ao_base,
		INFRA_MD2PERI_PROT_RDY)
		& (0x1 << INFRA_MD2PERI_PROT_BIT))
		!= (0x1 << INFRA_MD2PERI_PROT_BIT))
		;
	CCCI_BOOTUP_LOG(md->index, TAG, "md2peri: en[0x%X], rdy[0x%X]\n",
		cldma_read32(md->hw_info->plat_val->infra_ao_base,
		INFRA_MD2PERI_PROT_EN),
		cldma_read32(md->hw_info->plat_val->infra_ao_base,
		INFRA_MD2PERI_PROT_RDY));
}

static void md1_post_access_md_reg(struct ccci_modem *md)
{
	/*disable AP to MD*/
	cldma_write32(md->hw_info->plat_val->infra_ao_base,
		INFRA_PERI2MD_PROT_SET,
		(0x1 << INFRA_PERI2MD_PROT_BIT));
	while ((cldma_read32(md->hw_info->plat_val->infra_ao_base,
		INFRA_PERI2MD_PROT_RDY)
		& (0x1 << INFRA_PERI2MD_PROT_BIT))
		!= (0x1 << INFRA_PERI2MD_PROT_BIT))
		;
	CCCI_BOOTUP_LOG(md->index, TAG, "peri2md: en[0x%X], rdy[0x%X]\n",
		cldma_read32(md->hw_info->plat_val->infra_ao_base,
		INFRA_PERI2MD_PROT_EN),
		cldma_read32(md->hw_info->plat_val->infra_ao_base,
		INFRA_PERI2MD_PROT_RDY));

	/*enable MD to AP*/
	cldma_write32(md->hw_info->plat_val->infra_ao_base,
		INFRA_MD2PERI_PROT_CLR,
		(0x1 << INFRA_MD2PERI_PROT_BIT));
	while ((cldma_read32(md->hw_info->plat_val->infra_ao_base,
		INFRA_MD2PERI_PROT_RDY)
		& (0x1 << INFRA_MD2PERI_PROT_BIT)))
		;
	CCCI_BOOTUP_LOG(md->index, TAG, "md2peri: en[0x%X], rdy[0x%X]\n",
		cldma_read32(md->hw_info->plat_val->infra_ao_base,
		INFRA_MD2PERI_PROT_EN),
		cldma_read32(md->hw_info->plat_val->infra_ao_base,
		INFRA_MD2PERI_PROT_RDY));

	/*set dummy reg flag and let md access AP*/
	ROr2W(md->hw_info->plat_val->infra_ao_base, INFRA_AP2MD_DUMMY_REG,
		(0x1 << INFRA_AP2MD_DUMMY_BIT));
	CCCI_BOOTUP_LOG(md->index, TAG,
		"post: ap2md dummy reg 0x%X: 0x%X\n", INFRA_AP2MD_DUMMY_REG,
		cldma_read32(md->hw_info->plat_val->infra_ao_base,
		INFRA_AP2MD_DUMMY_REG));
}

static void md1_pll_init(struct ccci_modem *md)
{
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;
	struct md_pll_reg *md_pll = md_info->md_pll_base;
	void __iomem *map_addr = (void __iomem *)(md->hw_info->ap_mixed_base);
	int cnt = 0;
	unsigned int reg_val;

	while (1) {
		reg_val = cldma_read32(md_pll->md_top_Pll, 0x0);
		CCCI_BOOTUP_LOG(md->index, TAG, "Curr pll ver:0x%X\n", reg_val);
		if (reg_val != 0)
			break;
		msleep(20);
	}
	/* Enables clock square1 low-pass filter for 26M quality. */
	ROr2W(map_addr, 0x0, 0x2);
	udelay(100);

	/* Default md_srclkena_ack settle time = 136T 32K */
	cldma_write32(md_pll->md_top_Pll, 0x4, 0x02020E88);

	/* PLL init */
	cldma_write32(md_pll->md_top_Pll, 0x60, 0x801713B1);
	cldma_write32(md_pll->md_top_Pll, 0x58, 0x80171400);
	cldma_write32(md_pll->md_top_Pll, 0x50, 0x80229E00);
	cldma_write32(md_pll->md_top_Pll, 0x48, 0x80204E00);
	cldma_write32(md_pll->md_top_Pll, 0x40, 0x80213C00);

	while ((cldma_read32(md_pll->md_top_Pll, 0xC00) >> 14) & 0x1)
		;

	RAnd2W(md_pll->md_top_Pll, 0x64, ~(0x80));

#if defined(CONFIG_ARCH_MT6765)
	cldma_write32(md_pll->md_top_Pll, 0x104, 0x4C43100);
#endif
	cldma_write32(md_pll->md_top_Pll, 0x10, 0x100010);
	do {
		reg_val = cldma_read32(md_pll->md_top_Pll, 0x10);
		cnt++;
		if ((cnt % 5) == 0) {
			CCCI_BOOTUP_LOG(md->index, TAG,
				"pll init: rewrite 0x100010(%d)\n", cnt);
				cldma_write32(md_pll->md_top_Pll,
				0x10, 0x100010);
		}
		msleep(20);
	} while (reg_val != 0x100010);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"pll init: check 0x100010[0x%X], cnt:%d\n", reg_val, cnt);

	while ((cldma_read32(md_pll->md_top_clkSW, 0x84) & 0x8000) != 0x8000) {
		msleep(20);
		CCCI_BOOTUP_LOG(md->index, TAG,
			"pll init: [0x%x]=0x%x\n", MDTOP_CLKSW_BASE + 0x84,
			cldma_read32(md_pll->md_top_clkSW, 0x84));
	}

	ROr2W(md_pll->md_top_clkSW, 0x24, 0x3);
	ROr2W(md_pll->md_top_clkSW, 0x24, 0x58103FC);
	ROr2W(md_pll->md_top_clkSW, 0x28, 0x10);

	cldma_write32(md_pll->md_top_clkSW, 0x20, 0x1);

	cldma_write32(md_pll->md_top_Pll, 0x314, 0xFFFF);
	cldma_write32(md_pll->md_top_Pll, 0x318, 0xFFFF);

	/*make a record that means MD pll has been initialized.*/
	cldma_write32(md_pll->md_top_Pll, 0xF00, 0x62930000);
	CCCI_BOOTUP_LOG(md->index, TAG, "pll init: end\n");
}



static int md_cd_vcore_config(unsigned int md_id, unsigned int hold_req)
{
	int ret = 0;
	static int is_hold;
	static struct mtk_pm_qos_request md_qos_vcore_request;

	CCCI_BOOTUP_LOG(md_id, TAG,
		"%s: is_hold=%d, hold_req=%d\n",
		__func__, is_hold, hold_req);
	if (hold_req && is_hold == 0) {
		mtk_pm_qos_add_request(&md_qos_vcore_request,
		MTK_PM_QOS_VCORE_OPP, VCORE_OPP_0);
		is_hold = 1;
	} else if (hold_req == 0 && is_hold) {
		mtk_pm_qos_remove_request(&md_qos_vcore_request);
		is_hold = 0;
	} else
		CCCI_ERROR_LOG(md_id, TAG,
			"invalid hold_req: is_hold=%d, hold_req=%d\n",
			is_hold, hold_req);

	if (ret)
		CCCI_ERROR_LOG(md_id, TAG,
			"%s fail: ret=%d, hold_req=%d\n",
			__func__, ret, hold_req);

	return ret;
}

static int md_cd_soft_power_off(struct ccci_modem *md, unsigned int mode)
{
	clk_buf_set_by_flightmode(true);
	return 0;
}

static int md_cd_soft_power_on(struct ccci_modem *md, unsigned int mode)
{
	clk_buf_set_by_flightmode(false);
	return 0;
}

static int md_cd_power_on(struct ccci_modem *md)
{
	int ret = 0;
	unsigned int reg_value;
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;
	struct md_hw_info *hw_info = md->hw_info;

	/* step 1: modem clock setting */
	reg_value = ccci_read32(hw_info->ap_topclkgen_base, 0);
	reg_value &= ~((1<<8)|(1<<9));
	ccci_write32(hw_info->ap_topclkgen_base, 0, reg_value);
	CCCI_BOOTUP_LOG(md->index, CORE,
	"%s: set md1_clk_mod =0x%x\n",
	__func__, ccci_read32(hw_info->ap_topclkgen_base, 0));

	/* step 2: PMIC setting */
	md1_pmic_setting_on();

	/* steip 3: power on MD_INFRA and MODEM_TOP */
	switch (md->index) {
	case MD_SYS1:
		clk_buf_set_by_flightmode(false);
		CCCI_BOOTUP_LOG(md->index, TAG, "enable md sys clk\n");
		ret = clk_prepare_enable(clk_table[0].clk_ref);
		CCCI_BOOTUP_LOG(md->index, TAG,
			"enable md sys clk done,ret = %d\n", ret);
		kicker_pbm_by_md(KR_MD1, true);
		CCCI_BOOTUP_LOG(md->index, TAG,
			"Call end kicker_pbm_by_md(0,true)\n");
		break;
	}
	if (ret)
		return ret;

	md1_pre_access_md_reg(md);

	/* step 4: MD srcclkena setting */
	reg_value = ccci_read32(md->hw_info->plat_val->infra_ao_base,
		INFRA_AO_MD_SRCCLKENA);
	reg_value &= ~(0xFF);
	reg_value |= 0x21;
	ccci_write32(md->hw_info->plat_val->infra_ao_base,
		INFRA_AO_MD_SRCCLKENA, reg_value);
	CCCI_BOOTUP_LOG(md->index, CORE,
		"%s: set md1_srcclkena bit(0x1000_0F0C)=0x%x\n",
		__func__,
		ccci_read32(md->hw_info->plat_val->infra_ao_base,
			INFRA_AO_MD_SRCCLKENA));

#ifdef FEATURE_INFORM_NFC_VSIM_CHANGE
	/* notify NFC */
	inform_nfc_vsim_change(md->index, 1, 0);
#endif
	/* step 5: pll init */
	CCCI_BOOTUP_LOG(md->index, CORE,
		"%s: md1_pll_init ++++\n", __func__);
	md1_pll_init(md);
	CCCI_BOOTUP_LOG(md->index, CORE,
		"%s: md1_pll_init ----\n", __func__);

	/* step 6: disable MD WDT */
	cldma_write32(md_info->md_rgu_base, WDT_MD_MODE, WDT_MD_MODE_KEY);

	return 0;
}

//static int md_cd_bootup_cleanup(struct ccci_modem *md, int success)
//{
//	return 0;
//}

static int md_cd_let_md_go(struct ccci_modem *md)
{
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;

	if (MD_IN_DEBUG(md))
		return -1;
	CCCI_BOOTUP_LOG(md->index, TAG, "set MD boot slave\n");

	/* make boot vector take effect */
	cldma_write32(md_info->md_boot_slave_En, 0, 1);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"MD boot slave = 0x%x\n",
		cldma_read32(md_info->md_boot_slave_En, 0));

	md1_post_access_md_reg(md);
	return 0;
}

static int md_cd_power_off(struct ccci_modem *md, unsigned int timeout)
{
	int ret = 0;
	unsigned int reg_value;
	struct md_hw_info *hw_info = md->hw_info;

#ifdef FEATURE_INFORM_NFC_VSIM_CHANGE
	/* notify NFC */
	inform_nfc_vsim_change(md->index, 0, 0);
#endif

	/* power off MD_INFRA and MODEM_TOP */
	switch (md->index) {
	case MD_SYS1:
		/* 1. power off MD MTCMOS */
		clk_disable_unprepare(clk_table[0].clk_ref);
		/* 2. disable srcclkena */
		CCCI_BOOTUP_LOG(md->index, TAG, "disable md1 clk\n");
		reg_value =
			ccci_read32(md->hw_info->plat_val->infra_ao_base,
			INFRA_AO_MD_SRCCLKENA);
		reg_value &= ~(0xFF);
		ccci_write32(md->hw_info->plat_val->infra_ao_base,
			INFRA_AO_MD_SRCCLKENA, reg_value);
		CCCI_BOOTUP_LOG(md->index, CORE,
			"%s: set md1_srcclkena=0x%x\n",
			__func__,
			ccci_read32(md->hw_info->plat_val->infra_ao_base,
				INFRA_AO_MD_SRCCLKENA));
		CCCI_BOOTUP_LOG(md->index, TAG, "Call md1_pmic_setting_off\n");

		clk_buf_set_by_flightmode(true);
		/* 3. PMIC off */
		//md1_pmic_setting_off();

		/* 4. gating md related clock */
		reg_value = ccci_read32(hw_info->ap_topclkgen_base, 0);
		reg_value |= ((1<<8)|(1<<9));
		ccci_write32(hw_info->ap_topclkgen_base, 0, reg_value);
		CCCI_BOOTUP_LOG(md->index, CORE,
			"%s: set md1_clk_mod =0x%x\n",
			__func__,
			ccci_read32(hw_info->ap_topclkgen_base, 0));

		/* 5. DLPT */
		kicker_pbm_by_md(KR_MD1, false);
		CCCI_BOOTUP_LOG(md->index, TAG,
			"Call end kicker_pbm_by_md(0,false)\n");
		break;
	}
	return ret;
}

void ccci_modem_plt_resume(struct ccci_modem *md)
{
	CCCI_NORMAL_LOG(0, TAG, "[%s] md->hif_flag = %d\n",
			__func__, md->hif_flag);

	//if (md->hif_flag & (1 << CLDMA_HIF_ID))
	//	ccci_cldma_restore_reg(md);
}

int ccci_modem_plt_suspend(struct ccci_modem *md)
{
	CCCI_NORMAL_LOG(0, TAG, "[%s] md->hif_flag = %d\n",
			__func__, md->hif_flag);

	return 0;
}

static int ccci_modem_remove(struct platform_device *dev)
{
	return 0;
}

static void ccci_modem_shutdown(struct platform_device *dev)
{
}

static int ccci_modem_suspend(struct platform_device *dev, pm_message_t state)
{
	struct ccci_modem *md = (struct ccci_modem *)dev->dev.platform_data;

	CCCI_DEBUG_LOG(md->index, TAG, "%s\n", __func__);
	return 0;
}

static int ccci_modem_resume(struct platform_device *dev)
{
	struct ccci_modem *md = (struct ccci_modem *)dev->dev.platform_data;

	CCCI_DEBUG_LOG(md->index, TAG, "%s\n", __func__);
	return 0;
}

static int ccci_modem_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	if (pdev == NULL) {
		CCCI_ERROR_LOG(MD_SYS1, TAG, "%s pdev == NULL\n", __func__);
		return -1;
	}
	return ccci_modem_suspend(pdev, PMSG_SUSPEND);
}

static int ccci_modem_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	if (pdev == NULL) {
		CCCI_ERROR_LOG(MD_SYS1, TAG, "%s pdev == NULL\n", __func__);
		return -1;
	}
	return ccci_modem_resume(pdev);
}

static int ccci_modem_pm_restore_noirq(struct device *device)
{
	struct ccci_modem *md = (struct ccci_modem *)device->platform_data;

	/* set flag for next md_start */
	md->per_md_data.config.setting |= MD_SETTING_RELOAD;
	md->per_md_data.config.setting |= MD_SETTING_FIRST_BOOT;
	/* restore IRQ */
#ifdef FEATURE_PM_IPO_H
	irq_set_irq_type(md_ctrl->cldma_irq_id, IRQF_TRIGGER_HIGH);
	irq_set_irq_type(md_ctrl->md_wdt_irq_id, IRQF_TRIGGER_RISING);
#endif
	return 0;
}

static int ccci_modem_probe(struct platform_device *plat_dev)
{
	struct ccci_dev_cfg dev_cfg;
	int ret;
	struct md_hw_info *md_hw;

	/* Allocate modem hardware info structure memory */
	md_hw = kzalloc(sizeof(struct md_hw_info), GFP_KERNEL);
	if (md_hw == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"%s:alloc md hw mem fail\n", __func__);
		return -1;
	}
	ret = md_cd_get_modem_hw_info(plat_dev, &dev_cfg, md_hw);
	if (ret != 0) {
		CCCI_ERROR_LOG(-1, TAG,
			"%s:get hw info fail(%d)\n", __func__, ret);
		kfree(md_hw);
		md_hw = NULL;
		return -1;
	}
#ifdef CCCI_KMODULE_ENABLE
	ccci_init();
#endif

	ret = ccci_modem_init_common(plat_dev, &dev_cfg, md_hw);
	if (ret < 0) {
		kfree(md_hw);
		md_hw = NULL;
	}

	return ret;
}

static const struct dev_pm_ops ccci_modem_pm_ops = {
	.suspend = ccci_modem_pm_suspend,
	.resume = ccci_modem_pm_resume,
	.freeze = ccci_modem_pm_suspend,
	.thaw = ccci_modem_pm_resume,
	.poweroff = ccci_modem_pm_suspend,
	.restore = ccci_modem_pm_resume,
	.restore_noirq = ccci_modem_pm_restore_noirq,
};

#ifdef CONFIG_OF
static const struct of_device_id ccci_modem_of_ids[] = {
	{.compatible = "mediatek,mddriver-mt6765",},
	{}
};
#endif

static struct platform_driver ccci_modem_driver = {

	.driver = {
		   .name = "driver_modem_mt6765",
#ifdef CONFIG_OF
		   .of_match_table = ccci_modem_of_ids,
#endif

#ifdef CONFIG_PM
		   .pm = &ccci_modem_pm_ops,
#endif
		   },
	.probe = ccci_modem_probe,
	.remove = ccci_modem_remove,
	.shutdown = ccci_modem_shutdown,
	.suspend = ccci_modem_suspend,
	.resume = ccci_modem_resume,
};

static int __init modem_cd_init(void)
{
	int ret;

	ret = platform_driver_register(&ccci_modem_driver);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG,
			"clmda modem platform driver register fail(%d)\n",
			ret);
		return ret;
	}
	return 0;
}

module_init(modem_cd_init);

MODULE_AUTHOR("CCCI");
MODULE_DESCRIPTION("CCCI modem driver v0.1");
MODULE_LICENSE("GPL");

