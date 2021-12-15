/* SPDX-License-Identifier: GPL-2.0 */
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
#include "ccci_config.h"
#include <linux/clk.h>
#include <mach/mtk_pbm.h>
#include <clk-mt6768-pg.h>

//#define FEATURE_CLK_BUF
#ifdef FEATURE_CLK_BUF
#include <mtk_clkbuf_ctl.h>
#endif
#ifdef CONFIG_MTK_EMI_BWL
#include <emi_mbw.h>
#endif

#ifdef FEATURE_INFORM_NFC_VSIM_CHANGE
#include <mach/mt6605.h>
#endif

#include "include/pmic_api_buck.h"
#include <mt-plat/upmu_common.h>
#include <mtk_spm_sleep.h>

#include <linux/pm_qos.h>
#include <helio-dvfsrc-opp.h>

#include "ccci_core.h"
#include "ccci_platform.h"

#include "md_sys1_platform.h"
#include "cldma_reg.h"
#include "modem_reg_base.h"
#include <linux/arm-smccc.h>

static struct ccci_clk_node clk_table[] = {
	{ NULL,	"scp-sys-md1-main"},

};
#if defined(CONFIG_PINCTRL_ELBRUS)
static struct pinctrl *mdcldma_pinctrl;
#endif

//unsigned int devapc_check_flag;

static void __iomem *md_sram_pd_psmcusys_base;
static void __iomem *md_cldma_misc_base;

#define TAG "mcd"

static size_t mdreg_write32(size_t reg_id, size_t value)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_GET_INFO, reg_id, value,
		0, 0, 0, 0, 0, &res);

	return res.a0;
}

#define ROr2W(a, b, c)  cldma_write32(a, b, (cldma_read32(a, b)|c))
#define RAnd2W(a, b, c)  cldma_write32(a, b, (cldma_read32(a, b)&c))
#define RabIsc(a, b, c) ((cldma_read32(a, b)&c) != c)

static int md_cd_io_remap_md_side_register(struct ccci_modem *md);
static void md_cd_dump_debug_register(struct ccci_modem *md);
static void md_cd_dump_md_bootup_status(struct ccci_modem *md);
static void md_cd_get_md_bootup_status(struct ccci_modem *md,
	unsigned int *buff, int length);
static void md_cd_check_emi_state(struct ccci_modem *md, int polling);
//static int md_start_platform(struct ccci_modem *md);
static int md_cd_power_on(struct ccci_modem *md);
static int md_cd_power_off(struct ccci_modem *md, unsigned int timeout);
static int md_cd_soft_power_off(struct ccci_modem *md, unsigned int mode);
static int md_cd_soft_power_on(struct ccci_modem *md, unsigned int mode);
static int md_cd_let_md_go(struct ccci_modem *md);
static void md_cd_lock_cldma_clock_src(int locked);
static void md_cd_lock_modem_clock_src(int locked);

int ccci_modem_remove(struct platform_device *dev);
void ccci_modem_shutdown(struct platform_device *dev);
int ccci_modem_suspend(struct platform_device *dev, pm_message_t state);
int ccci_modem_resume(struct platform_device *dev);
int ccci_modem_pm_suspend(struct device *device);
int ccci_modem_pm_resume(struct device *device);
int ccci_modem_pm_restore_noirq(struct device *device);
int md_cd_vcore_config(unsigned int md_id, unsigned int hold_req);



struct ccci_plat_ops md_cd_plat_ptr = {
	//.init = &ccci_platform_init_6765,
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
//	.start_platform = md_start_platform,
	.power_on = &md_cd_power_on,
	.let_md_go = &md_cd_let_md_go,
	.power_off = &md_cd_power_off,
	.vcore_config = &md_cd_vcore_config,
};

void md1_subsys_debug_dump(enum subsys_id sys)
{
	struct ccci_modem *md = NULL;

	if (sys != SYS_MD1)
		return;
		/* add debug dump */

	CCCI_NORMAL_LOG(0, TAG, "%s\n", __func__);
	md = ccci_md_get_modem_by_id(0);
	if (md != NULL) {
		CCCI_NORMAL_LOG(0, TAG, "%s dump start\n", __func__);
		md->ops->dump_info(md, DUMP_FLAG_CCIF_REG | DUMP_FLAG_CCIF |
			DUMP_FLAG_REG | DUMP_FLAG_QUEUE_0_1 |
			DUMP_MD_BOOTUP_STATUS, NULL, 0);
		mdelay(1000);
		md->ops->dump_info(md, DUMP_FLAG_REG, NULL, 0);
	}
	CCCI_NORMAL_LOG(0, TAG, "%s exit\n", __func__);
}

struct pg_callbacks md1_subsys_handle = {
	.debug_dump = md1_subsys_debug_dump,
};

int md_cd_get_modem_hw_info(struct platform_device *dev_ptr,
	struct ccci_dev_cfg *dev_cfg, struct md_hw_info *hw_info)
{
	struct device_node *node = NULL;
	struct device_node *node_infrao = NULL;
	int idx = 0;
	struct cldma_hw_info *cldma_hw;

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

	cldma_hw = kzalloc(sizeof(struct cldma_hw_info), GFP_KERNEL);
	if (cldma_hw == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"%s:alloc cldma hw mem fail\n", __func__);
		return -1;
	}

	memset(hw_info, 0, sizeof(struct md_hw_info));
	hw_info->hif_hw_info = cldma_hw;

	switch (dev_cfg->index) {
	case 0:		/* MD_SYS1 */
		dev_cfg->major = 0;
		dev_cfg->minor_base = 0;
		of_property_read_u32(dev_ptr->dev.of_node,
			"mediatek,cldma_capability", &dev_cfg->capability);

		cldma_hw->cldma_ap_ao_base =
			(unsigned long)of_iomap(dev_ptr->dev.of_node, 0);
		cldma_hw->cldma_ap_pdn_base =
			(unsigned long)of_iomap(dev_ptr->dev.of_node, 1);
		hw_info->ap_ccif_base =
			(unsigned long)of_iomap(dev_ptr->dev.of_node, 2);
		hw_info->md_ccif_base =
			(unsigned long)of_iomap(dev_ptr->dev.of_node, 3);
		if (!(cldma_hw->cldma_ap_ao_base &&
			cldma_hw->cldma_ap_pdn_base &&
			hw_info->ap_ccif_base && hw_info->md_ccif_base)) {
			CCCI_ERROR_LOG(-1, TAG, "%s: hw_info of_iomap failed\n",
				       dev_ptr->dev.of_node->full_name);
			return -1;
		}
		cldma_hw->cldma_irq_id =
			irq_of_parse_and_map(dev_ptr->dev.of_node, 0);
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

		node = of_find_compatible_node(NULL, NULL,
				"mediatek,mdcldmamisc");
		if (node) {
			md_cldma_misc_base = of_iomap(node, 0);
			if (!md_cldma_misc_base) {
				CCCI_ERROR_LOG(-1, TAG,
				"%s: md_cldma_misc_base of_iomap failed\n",
				       node->full_name);
				return -1;
			}
		} else
			CCCI_BOOTUP_LOG(dev_cfg->index, TAG,
				"warning: no md cldma misc in dts\n");

		/*
		 * Device tree using none flag to register irq,
		 * sensitivity has set at "irq_of_parse_and_map"
		 */
		cldma_hw->cldma_irq_flags = IRQF_TRIGGER_NONE;
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
			"mediatek,infracfg_ao");
		if (node_infrao)
			md_cd_plat_val_ptr.infra_ao_base = of_iomap(node_infrao, 0);
		else
			md_cd_plat_val_ptr.infra_ao_base =
				ioremap_nocache(0x10001000 , 0x1000);

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
			clk_table[idx].clk_ref = devm_clk_get(
				&dev_ptr->dev, clk_table[idx].clk_name);
			if (IS_ERR(clk_table[idx].clk_ref)) {
				CCCI_ERROR_LOG(dev_cfg->index, TAG,
					"md%d get %s failed\n",
					dev_cfg->index + 1,
					clk_table[idx].clk_name);
				clk_table[idx].clk_ref = NULL;
			}
		}
		node = of_find_compatible_node(NULL, NULL, "mediatek,apmixed");
		if (node)
			hw_info->ap_mixed_base = (unsigned long)of_iomap(node, 0);
		else
			hw_info->ap_mixed_base = (unsigned long)ioremap_nocache(0x1000c000, 0x1000);
		if (!hw_info->ap_mixed_base) {
			CCCI_ERROR_LOG(-1, TAG,
				"%s: hw_info->ap_mixed_base of_iomap failed\n",
				node->full_name);
			return -1;
		}
		CCCI_NORMAL_LOG(-1, TAG, "%s:ap_mixed_base=%lx\n",
			__func__, hw_info->ap_mixed_base);
		node = of_find_compatible_node(NULL, NULL, "mediatek,topckgen");
		if (node)
			hw_info->ap_topclkgen_base = of_iomap(node, 0);
		else
			hw_info->ap_topclkgen_base =
				ioremap_nocache(0x10000000, 4);
		if (!hw_info->ap_topclkgen_base) {
			CCCI_ERROR_LOG(-1, TAG,
			"%s:ioremap topclkgen base address fail\n", __func__);
			return -1;
		}
		break;
	default:
		return -1;
	}

	if (cldma_hw->cldma_ap_ao_base == 0 ||
		cldma_hw->cldma_ap_pdn_base == 0) {
		CCCI_ERROR_LOG(dev_cfg->index, TAG,
			"ap_cldma: ao_base=0x%p, pdn_base=0x%p\n",
			(void *)cldma_hw->cldma_ap_ao_base,
			(void *)cldma_hw->cldma_ap_pdn_base);
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
	if (cldma_hw->cldma_irq_id == 0 ||
		hw_info->ap_ccif_irq0_id == 0 ||
		hw_info->ap_ccif_irq1_id == 0 ||
		hw_info->md_wdt_irq_id == 0) {
		CCCI_ERROR_LOG(dev_cfg->index, TAG,
			"cldma_irq:%d,ccif_irq0:%d,ccif_irq0:%d,md_wdt_irq:%d\n",
			cldma_hw->cldma_irq_id, hw_info->ap_ccif_irq0_id,
			hw_info->ap_ccif_irq1_id, hw_info->md_wdt_irq_id);
		return -1;
	}

	CCCI_DEBUG_LOG(dev_cfg->index, TAG,
		"dev_major:%d,minor_base:%d,capability:%d\n",
		dev_cfg->major, dev_cfg->minor_base, dev_cfg->capability);
	CCCI_DEBUG_LOG(dev_cfg->index, TAG,
		     "ap_cldma: ao_base=0x%p, pdn_base=0x%p\n",
		(void *)cldma_hw->cldma_ap_ao_base,
		(void *)cldma_hw->cldma_ap_pdn_base);

	CCCI_DEBUG_LOG(dev_cfg->index, TAG,
		"ap_ccif_base:0x%p, md_ccif_base:0x%p\n",
					(void *)hw_info->ap_ccif_base,
					(void *)hw_info->md_ccif_base);
	CCCI_DEBUG_LOG(dev_cfg->index, TAG,
		"cldma_irq:%d,ccif_irq0:%d,ccif_irq1:%d,md_wdt_irq:%d\n",
		cldma_hw->cldma_irq_id, hw_info->ap_ccif_irq0_id,
		hw_info->ap_ccif_irq1_id, hw_info->md_wdt_irq_id);
	register_pg_callback(&md1_subsys_handle);
	return 0;
}

static int md_cd_io_remap_md_side_register(struct ccci_modem *md)
{
	struct md_pll_reg *md_reg;
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;

	md_info->md_boot_slave_En = ioremap_nocache(
		md->hw_info->md_boot_slave_En, 0x4);
	md_info->md_rgu_base = ioremap_nocache(
		md->hw_info->md_rgu_base, 0x300);
	md_info->md_global_con0 = ioremap_nocache(MD_GLOBAL_CON0, 0x4);


	md_reg = kzalloc(sizeof(struct md_pll_reg), GFP_KERNEL);
	if (md_reg == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"cldma_sw_init:alloc md reg map mem fail\n");
		return -1;
	}
	/*needed by bootup flow start*/
	md_reg->md_top_Pll = ioremap_nocache(MDTOP_PLLMIXED_BASE,
		MDTOP_PLLMIXED_LENGTH);
	md_reg->md_top_clkSW = ioremap_nocache(MDTOP_CLKSW_BASE,
		MDTOP_CLKSW_LENGTH);
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

void md_cd_lock_cldma_clock_src(int locked)
{
	/* spm_ap_mdsrc_req(locked); */
}

void md_cd_lock_modem_clock_src(int locked)
{
	spm_ap_mdsrc_req(locked);
}

void md_cd_dump_md_bootup_status(struct ccci_modem *md)
{
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;
	struct md_pll_reg *md_reg = md_info->md_pll_base;

	/*
	 * To avoid AP/MD interface delay,
	 * dump 3 times, and buy-in the 3rd dump value.
	 */

	cldma_read32(md_reg->md_boot_stats0, 0);	/* dummy read */
	cldma_read32(md_reg->md_boot_stats0, 0);	/* dummy read */
	CCCI_NOTICE_LOG(md->index, TAG, "md_boot_stats0:0x%X\n",
		cldma_read32(md_reg->md_boot_stats0, 0));

	cldma_read32(md_reg->md_boot_stats1, 0);	/* dummy read */
	cldma_read32(md_reg->md_boot_stats1, 0);	/* dummy read */
	CCCI_NOTICE_LOG(md->index, TAG, "md_boot_stats1:0x%X\n",
		cldma_read32(md_reg->md_boot_stats1, 0));
}

void md_cd_get_md_bootup_status(struct ccci_modem *md, unsigned int *buff,
	int length)
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
			CCCI_MEM_LOG(md->index, TAG, "%s\n", temp_buf);
			/* max 256 bytes */
			temp_buf = buf + i + 1;
			j = 0;
		} else if (unlikely(j >= 255)) {
			/*
			 * ccci_mem_log max buffer length: 256,
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

void md_cd_dump_debug_register(struct ccci_modem *md)
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
		CCCI_MEM_LOG_TAG(md->index, TAG, "md_dbg_sys:0x%X\n",
			cldma_read32(dump_reg0, 0x430));
		iounmap(dump_reg0);
	} else {
		md_cd_lock_modem_clock_src(0);
		return;
	}

	/* 1. PC Monitor */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_PCMON)) {
		CCCI_MEM_LOG_TAG(md->index, TAG, "Dump MD PC monitor\n");
		CCCI_MEM_LOG_TAG(md->index, TAG, "common: 0x%X\n",
			(MD_PC_MONITOR_BASE + 0x800));
		/* Stop all PCMon */
		dump_reg0 = ioremap_nocache(MD_PC_MONITOR_BASE,
			MD_PC_MONITOR_LEN);
		mdreg_write32(MD_REG_PC_MONITOR, 0x22); /* stop MD PCMon */
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x800), 0x100);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			dump_reg0 + 0x900, 0x60);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0xA00), 0x60);
		/* core0 */
		CCCI_MEM_LOG_TAG(md->index, TAG, "core0/1: [0]0x%X, [1]0x%X\n",
			MD_PC_MONITOR_BASE, (MD_PC_MONITOR_BASE + 0x400));
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			dump_reg0, 0x400);
		/* core1 */
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
		(dump_reg0 + 0x400), 0x400);
		/* Resume PCMon */
		mdreg_write32(MD_REG_PC_MONITOR, 0x11);
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
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			dump_reg0, 0xD4);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x100), 0x18);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0xF00), 0x8);
		iounmap(dump_reg0);
		/* MD PLLMIXED */
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"PLLMIXED:[0]0x%X,[1]0x%X,[2]0x%X,[3]0x%X,[4]0x%X,[5]0x%X,[6]0x%X,[7]0x%X,[8]0x%X,[9]0x%X\n",
			MD_PLL_MIXED_BASE, (MD_PLL_MIXED_BASE + 0x100),
			(MD_PLL_MIXED_BASE + 0x200),
			(MD_PLL_MIXED_BASE + 0x300),
			(MD_PLL_MIXED_BASE + 0x400),
			(MD_PLL_MIXED_BASE + 0x500),
			(MD_PLL_MIXED_BASE + 0x600),
			(MD_PLL_MIXED_BASE + 0xC00),
			(MD_PLL_MIXED_BASE + 0xD00),
			(MD_PLL_MIXED_BASE + 0xF00));
		dump_reg0 = ioremap_nocache(MD_PLL_MIXED_BASE,
			MD_PLL_MIXED_LEN);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			dump_reg0, 0x68);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x100), 0x18);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x200), 0x8);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x300), 0x1C);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x400), 0x5C);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x500), 0xD0);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x600), 0x10);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0xC00), 0x48);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0xD00), 0x8);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0xF00), 0x14);
		iounmap(dump_reg0);
		/* MD CLKCTL */
		CCCI_MEM_LOG_TAG(md->index, TAG, "CLKCTL: [0]0x%X, [1]0x%X\n",
			MD_CLKCTL_BASE, (MD_CLKCTL_BASE + 0x110));
		dump_reg0 = ioremap_nocache(MD_CLKCTL_BASE, MD_CLKCTL_LEN);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			dump_reg0, 0x1C);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x110), 0x20);
		iounmap(dump_reg0);
		/* MD GLOBAL CON */
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"GLOBAL CON: [0]0x%X, [1]0x%X, [2]0x%X, [3]0x%X, [4]0x%X, [5]0x%X, [6]0x%X, [7]0x%X, [8]0x%X\n",
			MD_GLOBALCON_BASE, (MD_GLOBALCON_BASE + 0x100),
			(MD_GLOBALCON_BASE + 0x200),
			(MD_GLOBALCON_BASE + 0x300),
			(MD_GLOBALCON_BASE + 0x800),
			(MD_GLOBALCON_BASE + 0x900),
			(MD_GLOBALCON_BASE + 0xC00),
			(MD_GLOBALCON_BASE + 0xD00),
			(MD_GLOBALCON_BASE + 0xF00));
		dump_reg0 = ioremap_nocache(MD_GLOBALCON_BASE,
			MD_GLOBALCON_LEN);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			dump_reg0, 0xA0);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x100), 0x10);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x200), 0x98);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x300), 0x24);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x800), 0x8);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x900), 0x8);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0xC00), 0x1C);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0xD00), 4);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0xF00), 8);
		iounmap(dump_reg0);
	}

	/* 3. Bus status */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_BUS)) {
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"Dump MD Bus status: [0]0x%X, [1]0x%X, [2]0x%X, [3]0x%X\n",
			MD_BUS_REG_BASE0, MD_BUS_REG_BASE1, MD_BUS_REG_BASE2,
			MD_BUS_REG_BASE3);
		dump_reg0 = ioremap_nocache(MD_BUS_REG_BASE0, MD_BUS_REG_LEN0);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP, dump_reg0,
			MD_BUS_REG_LEN0);
		iounmap(dump_reg0);
		dump_reg0 = ioremap_nocache(MD_BUS_REG_BASE1, MD_BUS_REG_LEN1);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP, dump_reg0,
			MD_BUS_REG_LEN1);
		iounmap(dump_reg0);
		dump_reg0 = ioremap_nocache(MD_BUS_REG_BASE2, MD_BUS_REG_LEN2);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			dump_reg0, MD_BUS_REG_LEN2);
		iounmap(dump_reg0);
		dump_reg0 = ioremap_nocache(MD_BUS_REG_BASE3, MD_BUS_REG_LEN3);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			dump_reg0, MD_BUS_REG_LEN3);
		iounmap(dump_reg0);
	}

	/* 4. Bus REC */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_BUSREC)) {
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"Dump MD Bus REC: [0]0x%X, [1]0x%X\n",
			MD_MCU_MO_BUSREC_BASE, MD_INFRA_BUSREC_BASE);
		dump_reg0 = ioremap_nocache(MD_MCU_MO_BUSREC_BASE,
			MD_MCU_MO_BUSREC_LEN);
		mdreg_write32(MD_REG_MDMCU_BUSMON, 0x0); /* stop */
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x0), 0x104);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x200), 0x18);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x300), 0x30);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x400), 0x18);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x500), 0x30);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x700), 0x51C);
		mdreg_write32(MD_REG_MDMCU_BUSMON, 0x1); /* re-start */
		iounmap(dump_reg0);
		dump_reg0 = ioremap_nocache(MD_INFRA_BUSREC_BASE,
			MD_INFRA_BUSREC_LEN);
		mdreg_write32(MD_REG_MDINFRA_BUSMON, 0x0); /* stop */
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x0), 0x104);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x200), 0x18);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x300), 0x30);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x400), 0x18);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x500), 0x30);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x700), 0x51C);
		mdreg_write32(MD_REG_MDINFRA_BUSMON, 0x1);/* re-start */
		iounmap(dump_reg0);
	}

	/* 5. ECT */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_ECT)) {
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"Dump MD ECT: [0]0x%X, [1]0x%X, [2]0x%X, [3]0x%X\n",
			MD_ECT_REG_BASE0, MD_ECT_REG_BASE1,
			(MD_ECT_REG_BASE2 + 0x14), (MD_ECT_REG_BASE2 + 0x0C));
		dump_reg0 = ioremap_nocache(MD_ECT_REG_BASE0, MD_ECT_REG_LEN0);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP, dump_reg0,
			MD_ECT_REG_LEN0);
		iounmap(dump_reg0);
		dump_reg0 = ioremap_nocache(MD_ECT_REG_BASE1, MD_ECT_REG_LEN1);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP, dump_reg0,
			MD_ECT_REG_LEN1);
		iounmap(dump_reg0);
		dump_reg0 = ioremap_nocache(MD_ECT_REG_BASE2, MD_ECT_REG_LEN2);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x14), 0x4);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x0C), 0x4);
		iounmap(dump_reg0);
	}

	/*avoid deadlock and set bus protect*/
	if (per_md_data->md_dbg_dump_flag & ((1 << MD_DBG_DUMP_TOPSM) |
		(1 << MD_DBG_DUMP_MDRGU) | (1 << MD_DBG_DUMP_OST))) {
		RAnd2W(md->hw_info->plat_val->infra_ao_base, INFRA_AP2MD_DUMMY_REG,
			(~(0x1 << INFRA_AP2MD_DUMMY_BIT)));
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"ap2md dummy reg 0x%X: 0x%X\n", INFRA_AP2MD_DUMMY_REG,
			cldma_read32(md->hw_info->plat_val->infra_ao_base, INFRA_AP2MD_DUMMY_REG));
		/*disable MD to AP*/
		cldma_write32(md->hw_info->plat_val->infra_ao_base, INFRA_MD2PERI_PROT_SET,
			(0x1 << INFRA_MD2PERI_PROT_BIT));
		while ((cldma_read32(md->hw_info->plat_val->infra_ao_base,
			INFRA_MD2PERI_PROT_RDY)&(0x1<<INFRA_MD2PERI_PROT_BIT))
				!= (0x1 << INFRA_MD2PERI_PROT_BIT))
			;
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"md2peri: en[0x%X], rdy[0x%X]\n",
			cldma_read32(md->hw_info->plat_val->infra_ao_base, INFRA_MD2PERI_PROT_EN),
			cldma_read32(md->hw_info->plat_val->infra_ao_base, INFRA_MD2PERI_PROT_RDY));
		/*make sure AP to MD is enabled*/
		cldma_write32(md->hw_info->plat_val->infra_ao_base, INFRA_PERI2MD_PROT_CLR,
			(0x1 << INFRA_PERI2MD_PROT_BIT));
		while ((cldma_read32(md->hw_info->plat_val->infra_ao_base,
			INFRA_PERI2MD_PROT_RDY)&(0x1<<INFRA_PERI2MD_PROT_BIT)))
			;
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"peri2md: en[0x%X], rdy[0x%X]\n",
			cldma_read32(md->hw_info->plat_val->infra_ao_base, INFRA_PERI2MD_PROT_EN),
			cldma_read32(md->hw_info->plat_val->infra_ao_base, INFRA_PERI2MD_PROT_RDY));
	}

	/* 6. TOPSM */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_TOPSM)) {
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"Dump MD TOPSM status: 0x%X\n", MD_TOPSM_REG_BASE);
		dump_reg0 = ioremap_nocache(MD_TOPSM_REG_BASE,
			MD_TOPSM_REG_LEN);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP, dump_reg0,
			MD_TOPSM_REG_LEN);
		iounmap(dump_reg0);
	}

	/* 7. MD RGU */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_MDRGU)) {
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"Dump MD RGU: [0]0x%X, [1]0x%X\n",
			MD_RGU_REG_BASE, (MD_RGU_REG_BASE + 0x200));
		dump_reg0 = ioremap_nocache(MD_RGU_REG_BASE, MD_RGU_REG_LEN);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			dump_reg0, 0xCC);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x200), 0x5C);
		iounmap(dump_reg0);
	}

	/* 8 OST */
	if (per_md_data->md_dbg_dump_flag & (1 << MD_DBG_DUMP_OST)) {
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"Dump MD OST status: [0]0x%X, [1]0x%X\n",
			MD_OST_STATUS_BASE, (MD_OST_STATUS_BASE + 0x200));
		dump_reg0 = ioremap_nocache(MD_OST_STATUS_BASE, 0x300);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			dump_reg0, 0xF0);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(dump_reg0 + 0x200), 0x8);
		iounmap(dump_reg0);
		/* 9 CSC */
		CCCI_MEM_LOG_TAG(md->index, TAG, "Dump MD CSC: 0x%X\n",
		MD_CSC_REG_BASE);
		dump_reg0 = ioremap_nocache(MD_CSC_REG_BASE, MD_CSC_REG_LEN);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP, dump_reg0,
			MD_CSC_REG_LEN);
		iounmap(dump_reg0);
		/* 10 ELM */
		CCCI_MEM_LOG_TAG(md->index, TAG, "Dump MD ELM: 0x%X\n",
		MD_ELM_REG_BASE);
		dump_reg0 = ioremap_nocache(MD_ELM_REG_BASE, 0x480);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			dump_reg0, 0x480);
		iounmap(dump_reg0);
	}

		/* Clear flags for wdt timeout dump MDRGU */
	md->per_md_data.md_dbg_dump_flag &= (~((1 << MD_DBG_DUMP_TOPSM)
		| (1 << MD_DBG_DUMP_MDRGU) | (1 << MD_DBG_DUMP_OST)));

	md_cd_lock_modem_clock_src(0);

}

int md_cd_pccif_send(struct ccci_modem *md, int channel_id)
{
	int busy = 0;
	struct md_hw_info *hw_info = md->hw_info;

	md_cd_lock_modem_clock_src(1);

	busy = cldma_read32(hw_info->md_pcore_pccif_base, APCCIF_BUSY);
	if (busy & (1 << channel_id)) {
		md_cd_lock_modem_clock_src(0);
		return -1;
	}
	cldma_write32(hw_info->md_pcore_pccif_base, APCCIF_BUSY,
		1 << channel_id);
	cldma_write32(hw_info->md_pcore_pccif_base, APCCIF_TCHNUM, channel_id);

	md_cd_lock_modem_clock_src(0);
	return 0;
}

void md_cd_dump_pccif_reg(struct ccci_modem *md)
{
	struct md_hw_info *hw_info = md->hw_info;

	md_cd_lock_modem_clock_src(1);

	CCCI_MEM_LOG_TAG(md->index, TAG, "AP_CON(%p)=%x\n",
		hw_info->md_pcore_pccif_base + APCCIF_CON,
		cldma_read32(hw_info->md_pcore_pccif_base, APCCIF_CON));
	CCCI_MEM_LOG_TAG(md->index, TAG, "AP_BUSY(%p)=%x\n",
		hw_info->md_pcore_pccif_base + APCCIF_BUSY,
		cldma_read32(hw_info->md_pcore_pccif_base, APCCIF_BUSY));
	CCCI_MEM_LOG_TAG(md->index, TAG, "AP_START(%p)=%x\n",
		hw_info->md_pcore_pccif_base + APCCIF_START,
		cldma_read32(hw_info->md_pcore_pccif_base, APCCIF_START));
	CCCI_MEM_LOG_TAG(md->index, TAG, "AP_TCHNUM(%p)=%x\n",
		hw_info->md_pcore_pccif_base + APCCIF_TCHNUM,
		cldma_read32(hw_info->md_pcore_pccif_base,
		APCCIF_TCHNUM));
	CCCI_MEM_LOG_TAG(md->index, TAG, "AP_RCHNUM(%p)=%x\n",
		hw_info->md_pcore_pccif_base + APCCIF_RCHNUM,
		cldma_read32(hw_info->md_pcore_pccif_base, APCCIF_RCHNUM));
	CCCI_MEM_LOG_TAG(md->index, TAG, "AP_ACK(%p)=%x\n",
		hw_info->md_pcore_pccif_base + APCCIF_ACK,
		cldma_read32(hw_info->md_pcore_pccif_base, APCCIF_ACK));

	md_cd_lock_modem_clock_src(0);
}

void md_cd_check_md_DCM(struct md_cd_ctrl *md_ctrl)
{
}

void md_cd_check_emi_state(struct ccci_modem *md, int polling)
{
}

#if 0
static void md1_pcore_sram_turn_on(struct ccci_modem *md)
{
	int i;
	void __iomem *base = md_sram_pd_psmcusys_base;

	for (i = 31; i >= 0; i--)
		RAnd2W(base, 0x160, ~(0x1 << i));

	for (i = 31; i >= 0; i--)
		RAnd2W(base, 0x164, ~(0x1 << i));

	for (i = 31; i >= 0; i--)
		RAnd2W(base, 0x168, ~(0x1 << i));

	for (i = 31; i >= 0; i--)
		RAnd2W(base, 0x16C, ~(0x1 << i));

	for (i = 31; i >= 0; i--)
		RAnd2W(base, 0x170, ~(0x1 << i));

	for (i = 31; i >= 0; i--)
		RAnd2W(base, 0x174, ~(0x1 << i));

	for (i = 31; i >= 0; i--)
		RAnd2W(base, 0x178, ~(0x1 << i));

	for (i = 31; i >= 0; i--)
		RAnd2W(base, 0x17C, ~(0x1 << i));

	for (i = 31; i >= 0; i--)
		RAnd2W(base, 0x180, ~(0x1 << i));

	for (i = 31; i >= 0; i--)
		RAnd2W(base, 0x184, ~(0x1 << i));

	for (i = 31; i >= 0; i--)
		RAnd2W(base, 0x188, ~(0x1 << i));

	for (i = 31; i >= 0; i--)
		RAnd2W(base, 0x18C, ~(0x1 << i));

	for (i = 31; i >= 0; i--)
		RAnd2W(base, 0x190, ~(0x1 << i));

	CCCI_BOOTUP_LOG(md->index, TAG,
		"md1_pcore_sram reg: 0x%X, 0x%X, 0x%X, 0x%X 0x%X, 0x%X, 0x%X\n"
		, ccci_read32(base, 0x160), ccci_read32(base, 0x164),
		ccci_read32(base, 0x168),
		ccci_read32(base, 0x184), ccci_read32(base, 0x188),
		ccci_read32(base, 0x18C),
		ccci_read32(base, 0x190));
}

static void md1_enable_mcu_clock(struct ccci_modem *md)
{
	void __iomem *base = md_sram_pd_psmcusys_base;

	ROr2W(base, 0x50, (0x1 << 20));
	ROr2W(base, 0x54, (0x1 << 20));
	RAnd2W(base, 0x840, 0xFFFFFFFE);
	ccci_write8(base, 0x850, 0x87);
	RAnd2W(base, 0xA34, 0xFFFFFFFE);

	CCCI_BOOTUP_LOG(md->index, TAG,
		"md1_mcu reg: 0x%X, 0x%X, 0x%X, 0x%X, 0x%X\n",
		ccci_read32(base, 0x50), ccci_read32(base, 0x54),
		ccci_read32(base, 0x840),
		ccci_read32(base, 0x850), ccci_read32(base, 0xA34));
}

static void md1_pcore_sram_on(struct ccci_modem *md)
{
	CCCI_NORMAL_LOG(md->index, TAG, "%s enter\n", __func__);

	md1_pcore_sram_turn_on(md);
	md1_enable_mcu_clock(md);

	CCCI_BOOTUP_LOG(md->index, TAG, "%s exit\n", __func__);
}
#endif

void md1_pmic_setting_off(void)
{
	vmd1_pmic_setting_off();
}

void md1_pmic_setting_on(void)
{
	vmd1_pmic_setting_on();
}

/* callback for system power off*/
void ccci_power_off(void)
{
	vmd1_pmic_setting_off();
}

static void md1_pre_access_md_reg(struct ccci_modem *md)
{
	/*clear dummy reg flag to access modem reg*/
	RAnd2W(md->hw_info->plat_val->infra_ao_base, INFRA_AP2MD_DUMMY_REG,
		(~(0x1 << INFRA_AP2MD_DUMMY_BIT)));
	CCCI_BOOTUP_LOG(md->index, TAG, "pre: ap2md dummy reg 0x%X: 0x%X\n",
		INFRA_AP2MD_DUMMY_REG,
		cldma_read32(md->hw_info->plat_val->infra_ao_base, INFRA_AP2MD_DUMMY_REG));
	/*disable MD to AP*/
	cldma_write32(md->hw_info->plat_val->infra_ao_base, INFRA_MD2PERI_PROT_SET,
		(0x1 << INFRA_MD2PERI_PROT_BIT));
	while ((cldma_read32(md->hw_info->plat_val->infra_ao_base,
		INFRA_MD2PERI_PROT_RDY) & (0x1 << INFRA_MD2PERI_PROT_BIT))
			!= (0x1 << INFRA_MD2PERI_PROT_BIT))
		;
	CCCI_BOOTUP_LOG(md->index, TAG, "md2peri: en[0x%X], rdy[0x%X]\n",
		cldma_read32(md->hw_info->plat_val->infra_ao_base, INFRA_MD2PERI_PROT_EN),
		cldma_read32(md->hw_info->plat_val->infra_ao_base, INFRA_MD2PERI_PROT_RDY));
}

static void md1_post_access_md_reg(struct ccci_modem *md)
{
	/*disable AP to MD*/
	cldma_write32(md->hw_info->plat_val->infra_ao_base, INFRA_PERI2MD_PROT_SET,
		(0x1 << INFRA_PERI2MD_PROT_BIT));
	while ((cldma_read32(md->hw_info->plat_val->infra_ao_base,
		INFRA_PERI2MD_PROT_RDY) & (0x1 << INFRA_PERI2MD_PROT_BIT))
			!= (0x1 << INFRA_PERI2MD_PROT_BIT))
		;
	CCCI_BOOTUP_LOG(md->index, TAG, "peri2md: en[0x%X], rdy[0x%X]\n",
		cldma_read32(md->hw_info->plat_val->infra_ao_base, INFRA_PERI2MD_PROT_EN),
		cldma_read32(md->hw_info->plat_val->infra_ao_base, INFRA_PERI2MD_PROT_RDY));

	/*enable MD to AP*/
	cldma_write32(md->hw_info->plat_val->infra_ao_base, INFRA_MD2PERI_PROT_CLR,
		(0x1 << INFRA_MD2PERI_PROT_BIT));
	while ((cldma_read32(md->hw_info->plat_val->infra_ao_base,
		INFRA_MD2PERI_PROT_RDY) & (0x1 << INFRA_MD2PERI_PROT_BIT)))
		;
	CCCI_BOOTUP_LOG(md->index, TAG, "md2peri: en[0x%X], rdy[0x%X]\n",
		cldma_read32(md->hw_info->plat_val->infra_ao_base, INFRA_MD2PERI_PROT_EN),
		cldma_read32(md->hw_info->plat_val->infra_ao_base, INFRA_MD2PERI_PROT_RDY));

	/*set dummy reg flag and let md access AP*/
	ROr2W(md->hw_info->plat_val->infra_ao_base, INFRA_AP2MD_DUMMY_REG,
		(0x1 << INFRA_AP2MD_DUMMY_BIT));
	CCCI_BOOTUP_LOG(md->index, TAG,
		"post: ap2md dummy reg 0x%X: 0x%X\n", INFRA_AP2MD_DUMMY_REG,
		cldma_read32(md->hw_info->plat_val->infra_ao_base, INFRA_AP2MD_DUMMY_REG));
}

void md1_pll_init(struct ccci_modem *md)
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

	/* Default md_srclkena_ack settle time = 147T 32K */
	cldma_write32(md_pll->md_top_Pll, 0x4, 0x02020E93);

	/* PLL init */
	cldma_write32(md_pll->md_top_Pll, 0x60, 0x801713B1);
	cldma_write32(md_pll->md_top_Pll, 0x58, 0x80171400);
	cldma_write32(md_pll->md_top_Pll, 0x50, 0x80229E00);
	cldma_write32(md_pll->md_top_Pll, 0x48, 0x80204E00);
	cldma_write32(md_pll->md_top_Pll, 0x40, 0x80213C00);

	while ((cldma_read32(md_pll->md_top_Pll, 0xC00) >> 14) & 0x1)
		;

	RAnd2W(md_pll->md_top_Pll, 0x64, ~(0x80));

	cldma_write32(md_pll->md_top_Pll, 0x104, 0x4C43100);
	cldma_write32(md_pll->md_top_Pll, 0x10, 0x100010);
	do {
		reg_val = cldma_read32(md_pll->md_top_Pll, 0x10);
		cnt++;
		if ((cnt % 5) == 0) {
			CCCI_BOOTUP_LOG(md->index, TAG,
				"pll init: rewrite 0x100010(%d)\n", cnt);
			cldma_write32(md_pll->md_top_Pll, 0x10, 0x100010);
		}
		msleep(20);
	} while (reg_val != 0x100010);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"pll init: check 0x100010[0x%X], cnt:%d\n", reg_val, cnt);

	while ((cldma_read32(md_pll->md_top_clkSW, 0x84) & 0x8000) != 0x8000) {
		msleep(20);
		CCCI_BOOTUP_LOG(md->index, TAG, "pll init: [0x%x]=0x%x\n",
			MDTOP_CLKSW_BASE + 0x84,
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

void __attribute__((weak)) kicker_pbm_by_md(enum pbm_kicker kicker, bool status)
{
}

int md_cd_vcore_config(unsigned int md_id, unsigned int hold_req)
{
	int ret = 0;
	static int is_hold;
	static struct mtk_pm_qos_request md_qos_vcore_request;

	CCCI_BOOTUP_LOG(md_id, TAG,
		"%s: is_hold=%d, hold_req=%d\n", __func__,
		is_hold, hold_req);
	if (hold_req && is_hold == 0) {
		mtk_pm_qos_add_request(&md_qos_vcore_request, MTK_PM_QOS_VCORE_OPP,
			VCORE_OPP_0);
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
			"%s fail: ret=%d, hold_req=%d\n", __func__,
			ret, hold_req);

	return ret;
}

int md_cd_soft_power_off(struct ccci_modem *md, unsigned int mode)
{
#ifdef FEATURE_CLK_BUF
	clk_buf_set_by_flightmode(true);
#endif
	return 0;
}

int md_cd_soft_power_on(struct ccci_modem *md, unsigned int mode)
{
#ifdef FEATURE_CLK_BUF
	clk_buf_set_by_flightmode(false);
#endif
	return 0;
}

int md_cd_power_on(struct ccci_modem *md)
{
	int ret = 0;
	unsigned int reg_value;
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;
	struct md_hw_info *hw_info = md->hw_info;

	/* step 1: modem clock setting */
	reg_value = ccci_read32(hw_info->ap_topclkgen_base, 0);
	reg_value &= ~((1<<8)|(1<<9));
	ccci_write32(hw_info->ap_topclkgen_base, 0, reg_value);
	CCCI_BOOTUP_LOG(md->index, CORE, "%s: set md1_clk_mod =0x%x\n",
		__func__, ccci_read32(hw_info->ap_topclkgen_base, 0));

	/* step 2: PMIC setting */
	md1_pmic_setting_on();

	/* steip 3: power on MD_INFRA and MODEM_TOP */

#ifdef FEATURE_CLK_BUF
	clk_buf_set_by_flightmode(false);
#endif
	CCCI_BOOTUP_LOG(md->index, TAG, "enable md sys clk\n");
	ret = clk_prepare_enable(clk_table[0].clk_ref);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"enable md sys clk done,ret = %d\n", ret);
	kicker_pbm_by_md(KR_MD1, true);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"Call end kicker_pbm_by_md(0,true)\n");
	if (ret)
		return ret;

	md1_pre_access_md_reg(md);

	/* step 4: MD srcclkena setting */
	reg_value = ccci_read32(md->hw_info->plat_val->infra_ao_base, INFRA_AO_MD_SRCCLKENA);
	reg_value &= ~(0xFF);
	reg_value |= 0x21;
	ccci_write32(md->hw_info->plat_val->infra_ao_base, INFRA_AO_MD_SRCCLKENA, reg_value);
	CCCI_BOOTUP_LOG(md->index, CORE,
		"%s: set md1_srcclkena bit(0x1000_0F0C)=0x%x\n", __func__,
		     ccci_read32(md->hw_info->plat_val->infra_ao_base, INFRA_AO_MD_SRCCLKENA));

#ifdef FEATURE_INFORM_NFC_VSIM_CHANGE
	/* notify NFC */
	inform_nfc_vsim_change(md->index, 1, 0);
#endif
	/* step 5: pll init */
	CCCI_BOOTUP_LOG(md->index, CORE, "%s: md1_pll_init +++++\n", __func__);
	md1_pll_init(md);
	CCCI_BOOTUP_LOG(md->index, CORE, "%s: md1_pll_init -----\n", __func__);

	/* step 6: disable MD WDT */
	cldma_write32(md_info->md_rgu_base, WDT_MD_MODE, WDT_MD_MODE_KEY);

	return 0;
}

int md_cd_bootup_cleanup(struct ccci_modem *md, int success)
{
	return 0;
}

int md_cd_let_md_go(struct ccci_modem *md)
{
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;

	if (MD_IN_DEBUG(md))
		return -1;
	CCCI_BOOTUP_LOG(md->index, TAG, "set MD boot slave\n");

	cldma_write32(md_info->md_boot_slave_En, 0, 1);
	CCCI_BOOTUP_LOG(md->index, TAG, "MD boot slave = 0x%x\n",
		cldma_read32(md_info->md_boot_slave_En, 0));

	md1_post_access_md_reg(md);
	return 0;
}

int md_cd_power_off(struct ccci_modem *md, unsigned int timeout)
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
		reg_value = ccci_read32(md->hw_info->plat_val->infra_ao_base, INFRA_AO_MD_SRCCLKENA);
		reg_value &= ~(0xFF);
		ccci_write32(md->hw_info->plat_val->infra_ao_base, INFRA_AO_MD_SRCCLKENA, reg_value);
		CCCI_BOOTUP_LOG(md->index, CORE,
			"%s: set md1_srcclkena=0x%x\n", __func__,
			     ccci_read32(md->hw_info->plat_val->infra_ao_base, INFRA_AO_MD_SRCCLKENA));
		CCCI_BOOTUP_LOG(md->index, TAG, "Call md1_pmic_setting_off\n");
#ifdef FEATURE_CLK_BUF
		clk_buf_set_by_flightmode(true);
#endif
		/* 3. PMIC off */
		md1_pmic_setting_off();

		/* 4. gating md related clock */
		reg_value = ccci_read32(hw_info->ap_topclkgen_base, 0);
		reg_value |= ((1<<8)|(1<<9));
		ccci_write32(hw_info->ap_topclkgen_base, 0, reg_value);
		CCCI_BOOTUP_LOG(md->index, CORE,
			"%s: set md1_clk_mod =0x%x\n", __func__,
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

int ccci_modem_remove(struct platform_device *dev)
{
	return 0;
}

void ccci_modem_shutdown(struct platform_device *dev)
{
}

int ccci_modem_suspend(struct platform_device *dev, pm_message_t state)
{
	struct ccci_modem *md = (struct ccci_modem *)dev->dev.platform_data;

	CCCI_DEBUG_LOG(md->index, TAG, "%s\n", __func__);
	return 0;
}

int ccci_modem_resume(struct platform_device *dev)
{
	struct ccci_modem *md = (struct ccci_modem *)dev->dev.platform_data;

	CCCI_DEBUG_LOG(md->index, TAG, "%s\n", __func__);
	return 0;
}

int ccci_modem_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	if (pdev == NULL) {
		CCCI_ERROR_LOG(MD_SYS1, TAG, "%s pdev == NULL\n", __func__);
		return -1;
	}
	return ccci_modem_suspend(pdev, PMSG_SUSPEND);
}

int ccci_modem_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	if (pdev == NULL) {
		CCCI_ERROR_LOG(MD_SYS1, TAG, "%s pdev == NULL\n", __func__);
		return -1;
	}
	return ccci_modem_resume(pdev);
}

int ccci_modem_pm_restore_noirq(struct device *device)
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

void ccci_modem_restore_reg(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl =
		(struct md_cd_ctrl *)ccci_hif_get_by_id(CLDMA_HIF_ID);
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;
	enum MD_STATE md_state = ccci_fsm_get_md_state(md->index);
	int i;
	unsigned long flags;
	unsigned int val = 0;
	dma_addr_t bk_addr = 0;

	if (md_state == GATED ||
		md_state == WAITING_TO_STOP ||
		md_state == INVALID) {
		CCCI_NORMAL_LOG(md->index, TAG,
			"Resume no need reset cldma for md_state=%d\n",
			md_state);
		return;
	}
	cldma_write32(md_info->ap_ccif_base,
		APCCIF_CON, 0x01);	/* arbitration */

	if (cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_TQSAR(0))
		|| cldma_reg_get_4msb_val(md_ctrl->cldma_ap_ao_base,
			CLDMA_AP_UL_START_ADDR_4MSB, md_ctrl->txq[0].index)) {
		CCCI_NORMAL_LOG(md->index, TAG,
			"Resume cldma pdn register: No need  ...\n");
		spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
		if (!(cldma_read32(md_ctrl->cldma_ap_ao_base,
			CLDMA_AP_SO_STATUS))) {
			cldma_write32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_SO_RESUME_CMD,
				CLDMA_BM_ALL_QUEUE & 0x1);
			cldma_read32(md_ctrl->cldma_ap_pdn_base,
				CLDMA_AP_SO_RESUME_CMD); /* dummy read */
		} else
			CCCI_NORMAL_LOG(md->index, TAG,
				"Resume cldma ao register: No need  ...\n");
		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	} else {
		CCCI_NORMAL_LOG(md->index, TAG,
			"Resume cldma pdn register ...11\n");
		spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
		/* re-config 8G mode flag for pd register*/
		cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_UL_CFG,
			cldma_read32(md_ctrl->cldma_ap_pdn_base,
			CLDMA_AP_UL_CFG) | 0x40);
#endif
		cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_SO_RESUME_CMD, CLDMA_BM_ALL_QUEUE & 0x1);
		cldma_read32(md_ctrl->cldma_ap_pdn_base,
			CLDMA_AP_SO_RESUME_CMD); /* dummy read */

		/* set start address */
		for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++) {
			if (cldma_read32(md_ctrl->cldma_ap_ao_base,
				CLDMA_AP_TQCPBAK(md_ctrl->txq[i].index)) == 0
				 && cldma_reg_get_4msb_val(
				 md_ctrl->cldma_ap_ao_base,
					CLDMA_AP_UL_CURRENT_ADDR_BK_4MSB,
					md_ctrl->txq[i].index) == 0) {
				if (i != 7) /* Queue 7 not used currently */
					CCCI_DEBUG_LOG(md->index,
					TAG, "Resume CH(%d) current bak:== 0\n"
					, i);
				cldma_reg_set_tx_start_addr(
					md_ctrl->cldma_ap_pdn_base,
					md_ctrl->txq[i].index,
					md_ctrl->txq[i].tr_done->gpd_addr);
				cldma_reg_set_tx_start_addr_bk(
					md_ctrl->cldma_ap_ao_base,
					md_ctrl->txq[i].index,
					md_ctrl->txq[i].tr_done->gpd_addr);
			} else {
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
				val = cldma_reg_get_4msb_val(
					md_ctrl->cldma_ap_ao_base,
					CLDMA_AP_UL_CURRENT_ADDR_BK_4MSB,
					md_ctrl->txq[i].index);
				/*set high bits*/
				bk_addr = val;
				bk_addr <<= 32;
#else
				bk_addr = 0;
#endif
				/*set low bits*/
				val = cldma_read32(md_ctrl->cldma_ap_ao_base,
					CLDMA_AP_TQCPBAK(
					md_ctrl->txq[i].index));
				bk_addr |= val;
				cldma_reg_set_tx_start_addr(
					md_ctrl->cldma_ap_pdn_base,
					md_ctrl->txq[i].index, bk_addr);
				cldma_reg_set_tx_start_addr_bk(
					md_ctrl->cldma_ap_ao_base,
					md_ctrl->txq[i].index, bk_addr);
			}
		}
		/* wait write done*/
		wmb();
		/* start all Tx and Rx queues */
		md_ctrl->txq_started = 0;
		md_ctrl->txq_active |= CLDMA_BM_ALL_QUEUE;
		/* cldma_write32(md_ctrl->cldma_ap_pdn_base,
		 * CLDMA_AP_SO_START_CMD, CLDMA_BM_ALL_QUEUE);
		 */
		/* cldma_read32(md_ctrl->cldma_ap_pdn_base,
		 * CLDMA_AP_SO_START_CMD); // dummy read
		 */
		/* md_ctrl->rxq_active |= CLDMA_BM_ALL_QUEUE; */
		/* enable L2 DONE and ERROR interrupts */
		ccci_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_L2TIMCR0,
			CLDMA_TX_INT_DONE | CLDMA_TX_INT_QUEUE_EMPTY
			| CLDMA_TX_INT_ERROR);
		/* enable all L3 interrupts */
		cldma_write32(md_ctrl->cldma_ap_pdn_base,
		CLDMA_AP_L3TIMCR0, CLDMA_BM_INT_ALL);
		cldma_write32(md_ctrl->cldma_ap_pdn_base,
			CLDMA_AP_L3TIMCR1, CLDMA_BM_INT_ALL);
		cldma_write32(md_ctrl->cldma_ap_pdn_base,
			CLDMA_AP_L3RIMCR0, CLDMA_BM_INT_ALL);
		cldma_write32(md_ctrl->cldma_ap_pdn_base,
			CLDMA_AP_L3RIMCR1, CLDMA_BM_INT_ALL);
		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
		CCCI_NORMAL_LOG(md->index, TAG,
			"Resume cldma pdn register done\n");
	}
}

