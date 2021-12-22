/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
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
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
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

#ifdef CONFIG_MTK_QOS_SUPPORT
#include <linux/pm_qos.h>
#include <helio-dvfsrc-opp.h>
#endif
#include <clk-mt6781-pg.h>
#include "ccci_core.h"
#include "ccci_platform.h"

#include "md_sys1_platform.h"
#include "modem_reg_base.h"
#ifdef CCCI_PLATFORM_MT6781
#include "ap_md_reg_dump_6781.h"
#else
#include "ap_md_reg_dump.h"
#endif
#include "modem_secure_base.h"

static struct ccci_clk_node clk_table[] = {
	{ NULL,	"scp-sys-md1-main"},

};

#define TAG "mcd"

#define ROr2W(a, b, c)  ccci_write32(a, b, (ccci_read32(a, b)|c))
#define RAnd2W(a, b, c)  ccci_write32(a, b, (ccci_read32(a, b)&c))
#define RabIsc(a, b, c) ((ccci_read32(a, b)&c) != c)

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

struct ccci_plat_ops md_cd_plat_ptr = {
	.remap_md_reg = &md_cd_io_remap_md_side_register,
	.lock_cldma_clock_src = &md_cd_lock_cldma_clock_src,
	.lock_modem_clock_src = &md_cd_lock_modem_clock_src,
	.dump_md_bootup_status = &md_cd_dump_md_bootup_status,
	.get_md_bootup_status = &md_cd_get_md_bootup_status,
	.debug_reg = &md_cd_dump_debug_register,
	.check_emi_state = &md_cd_check_emi_state,
	.soft_power_off = &md_cd_soft_power_off,
	.soft_power_on = &md_cd_soft_power_on,
	.start_platform = &md_start_platform,
	.power_on = &md_cd_power_on,
	.let_md_go = &md_cd_let_md_go,
	.power_off = &md_cd_power_off,
	.vcore_config = &md_cd_vcore_config,
};

void md_cldma_hw_reset(unsigned char md_id)
{
}

void md1_subsys_debug_dump(enum subsys_id sys)
{
	struct ccci_modem *md;

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

void ccci_dump(void)
{
	md1_subsys_debug_dump(SYS_MD1);
}
EXPORT_SYMBOL(ccci_dump);

int md_cd_get_modem_hw_info(struct platform_device *dev_ptr,
	struct ccci_dev_cfg *dev_cfg, struct md_hw_info *hw_info)
{
	struct device_node *node = NULL;
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

	switch (dev_cfg->index) {
	case 0:		/* MD_SYS1 */
		dev_cfg->major = 0;
		dev_cfg->minor_base = 0;
		of_property_read_u32(dev_ptr->dev.of_node,
			"mediatek,cldma_capability", &dev_cfg->capability);

		hw_info->ap_ccif_base =
		 (unsigned long)of_iomap(dev_ptr->dev.of_node, 0);
		hw_info->md_ccif_base =
		 (unsigned long)of_iomap(dev_ptr->dev.of_node, 1);

		hw_info->md_wdt_irq_id =
		 irq_of_parse_and_map(dev_ptr->dev.of_node, 0);
		hw_info->ap_ccif_irq0_id =
		 irq_of_parse_and_map(dev_ptr->dev.of_node, 1);
		hw_info->ap_ccif_irq1_id =
		 irq_of_parse_and_map(dev_ptr->dev.of_node, 2);

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

		hw_info->sram_size = CCIF_SRAM_SIZE;
		hw_info->md_boot_slave_En = MD_BOOT_VECTOR_EN;
		of_property_read_u32(dev_ptr->dev.of_node,
			"mediatek,md_generation", &md_cd_plat_val_ptr.md_gen);
		node = of_find_compatible_node(NULL, NULL,
			"mediatek,infracfg_ao");
		md_cd_plat_val_ptr.infra_ao_base = of_iomap(node, 0);

		hw_info->plat_ptr = &md_cd_plat_ptr;
		hw_info->plat_val = &md_cd_plat_val_ptr;
		if ((hw_info->plat_ptr == NULL) || (hw_info->plat_val == NULL))
			return -1;
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
			"mediatek,md_ccif4");
		if (node) {
			hw_info->md_ccif4_base = of_iomap(node, 0);
			if (!hw_info->md_ccif4_base) {
				CCCI_ERROR_LOG(dev_cfg->index, TAG,
				"ccif4_base fail: 0x%p!\n",
				(void *)hw_info->md_ccif4_base);
				return -1;
			}
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
			hw_info->ap_ccif_irq0_id, hw_info->ap_ccif_irq1_id,
			hw_info->md_wdt_irq_id);
		return -1;
	}
#ifdef CCCI_PLATFORM_MT6781
	/* Get spm sleep base */
	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");
	if (!node) {
		CCCI_ERROR_LOG(dev_cfg->index, TAG,
			"%s: can't find node:mediatek,sleep\n",
			__func__);
		return -1;
	}
	hw_info->spm_sleep_base = (unsigned long)of_iomap(node, 0);
	if (!hw_info->spm_sleep_base) {
		CCCI_ERROR_LOG(dev_cfg->index, TAG,
			"%s: spm_sleep_base of_iomap failed\n",
			__func__);
		return -1;
	}
	CCCI_NORMAL_LOG(dev_cfg->index, TAG, "spm_sleep_base:0x%lx\n",
		(unsigned long)hw_info->spm_sleep_base);
#endif
	CCCI_DEBUG_LOG(dev_cfg->index, TAG,
		"dev_major:%d,minor_base:%d,capability:%d\n",
		dev_cfg->major, dev_cfg->minor_base, dev_cfg->capability);

	CCCI_DEBUG_LOG(dev_cfg->index, TAG,
		"ap_ccif_base:0x%p, md_ccif_base:0x%p\n",
					(void *)hw_info->ap_ccif_base,
					(void *)hw_info->md_ccif_base);
	CCCI_DEBUG_LOG(dev_cfg->index, TAG,
		"ccif_irq0:%d,ccif_irq1:%d,md_wdt_irq:%d\n",
		hw_info->ap_ccif_irq0_id, hw_info->ap_ccif_irq1_id,
		hw_info->md_wdt_irq_id);
	register_pg_callback(&md1_subsys_handle);
	return 0;
}

void ccci_set_clk_by_id(int idx, unsigned int on)
{
	int ret = 0;

	if (idx >= ARRAY_SIZE(clk_table) || idx < 0)
		return;
	else if (clk_table[idx].clk_ref == NULL)
		return;
	else if (on) {
		ret = clk_prepare_enable(clk_table[idx].clk_ref);
		if (ret)
			CCCI_ERROR_LOG(-1, TAG,
				"%s: idx = %d, on=%d,ret=%d\n",
				__func__, idx, on, ret);
	} else
		clk_disable_unprepare(clk_table[idx].clk_ref);
}

static int md_cd_io_remap_md_side_register(struct ccci_modem *md)
{
	struct md_pll_reg *md_reg;
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;

	md_info->md_boot_slave_En =
		ioremap_nocache(md->hw_info->md_boot_slave_En, 0x4);

	md_reg = kzalloc(sizeof(struct md_pll_reg), GFP_KERNEL);
	if (md_reg == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
		 "md_sw_init:alloc md reg map mem fail\n");
		return -1;
	}

	md_reg->md_boot_stats_select =
		ioremap_nocache(MD1_BOOT_STATS_SELECT, 4);
	md_reg->md_boot_stats = ioremap_nocache(MD1_CFG_BOOT_STATS, 4);
	/*just for dump end*/
#ifdef CCCI_PLATFORM_MT6781
	md_reg->md_l2sram_base = ioremap_nocache(MD_L2SRAM_BASE, MD_L2SRAM_SIZE);
#endif
	md_info->md_pll_base = md_reg;

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
#ifdef CCCI_PLATFORM_MT6781
	struct arm_smccc_res res = {0};
	int settle;

	/* spm_ap_mdsrc_req(locked); */
	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_CLOCK_REQUEST,
		MD_REG_AP_MDSRC_REQ, locked, 0, 0, 0, 0, &res);

	if (locked) {
		arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_CLOCK_REQUEST,
			MD_REG_AP_MDSRC_SETTLE, 0, 0, 0, 0, 0, &res);

		if (res.a0 != 0 && res.a0 < 10)
			settle = res.a0;
		else
			settle = 3;

		mdelay(settle);

		arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_CLOCK_REQUEST,
			MD_REG_AP_MDSRC_ACK, 0, 0, 0, 0, 0, &res);
	}
#else
	spm_ap_mdsrc_req(locked);
#endif
}

static void md_cd_dump_md_bootup_status(struct ccci_modem *md)
{
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;
	struct md_pll_reg *md_reg = md_info->md_pll_base;

	/*To avoid AP/MD interface delay,
	 * dump 3 times, and buy-in the 3rd dump value.
	 */

	ccci_write32(md_reg->md_boot_stats_select, 0, 0);
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	CCCI_NOTICE_LOG(md->index, TAG,
		"md_boot_stats0:0x%X\n",
		ccci_read32(md_reg->md_boot_stats, 0));

	ccci_write32(md_reg->md_boot_stats_select, 0, 1);
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	CCCI_NOTICE_LOG(md->index, TAG,
		"md_boot_stats1:0x%X\n",
		ccci_read32(md_reg->md_boot_stats, 0));
}

static void md_cd_get_md_bootup_status(
	struct ccci_modem *md, unsigned int *buff, int length)
{
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;
	struct md_pll_reg *md_reg = md_info->md_pll_base;

	CCCI_NOTICE_LOG(md->index, TAG, "md_boot_stats len %d\n", length);

	if (length < 2 || buff == NULL) {
		md_cd_dump_md_bootup_status(md);
		return;
	}

	ccci_write32(md_reg->md_boot_stats_select, 0, 0);
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	buff[0] = ccci_read32(md_reg->md_boot_stats, 0);

	ccci_write32(md_reg->md_boot_stats_select, 0, 1);
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	buff[1] = ccci_read32(md_reg->md_boot_stats, 0);
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

void __weak dump_emi_outstanding(void)
{
	CCCI_DEBUG_LOG(-1, TAG, "No %s\n", __func__);
}

static void md_cd_dump_debug_register(struct ccci_modem *md)
{
	/* MD no need dump because of bus hang happened - open for debug */
	unsigned int reg_value[2] = { 0 };
	unsigned int ccif_sram[
		CCCI_EE_SIZE_CCIF_SRAM/sizeof(unsigned int)] = { 0 };

	/*dump_emi_latency();*/
	dump_emi_outstanding();
	dump_emi_last_bm(md);

	md_cd_get_md_bootup_status(md, reg_value, 2);
	md->ops->dump_info(md, DUMP_FLAG_CCIF, ccif_sram, 0);
	/* copy from HS1 timeout */
	if ((reg_value[0] == 0) && (ccif_sram[1] == 0))
		return;
	else if (!((reg_value[0] == 0x5443000C) || (reg_value[0] == 0) ||
		(reg_value[0] >= 0x53310000 && reg_value[0] <= 0x533100FF)))
		return;
	if (unlikely(in_interrupt())) {
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"In interrupt, skip dump MD debug register.\n");
		return;
	}
	/* disable internal_md_dump_debug_register for bring-up*/
	/* need enable after bring-up */

	md_cd_lock_modem_clock_src(1);

	internal_md_dump_debug_register(md->index);

	md_cd_lock_modem_clock_src(0);

}

int md_cd_pccif_send(struct ccci_modem *md, int channel_id)
{
	int busy = 0;
	struct md_hw_info *hw_info = md->hw_info;

	md_cd_lock_modem_clock_src(1);

	busy = ccif_read32(hw_info->md_pcore_pccif_base, APCCIF_BUSY);
	if (busy & (1 << channel_id)) {
		md_cd_lock_modem_clock_src(0);
		return -1;
	}
	ccif_write32(hw_info->md_pcore_pccif_base,
		APCCIF_BUSY, 1 << channel_id);
	ccif_write32(hw_info->md_pcore_pccif_base,
		APCCIF_TCHNUM, channel_id);

	md_cd_lock_modem_clock_src(0);
	return 0;
}

void md_cd_dump_pccif_reg(struct ccci_modem *md)
{
	struct md_hw_info *hw_info = md->hw_info;

	md_cd_lock_modem_clock_src(1);

	CCCI_MEM_LOG_TAG(md->index, TAG,
		"AP_CON(%p)=%x\n",
		hw_info->md_pcore_pccif_base + APCCIF_CON,
		ccif_read32(hw_info->md_pcore_pccif_base, APCCIF_CON));
	CCCI_MEM_LOG_TAG(md->index, TAG,
		"AP_BUSY(%p)=%x\n",
		hw_info->md_pcore_pccif_base + APCCIF_BUSY,
		ccif_read32(hw_info->md_pcore_pccif_base, APCCIF_BUSY));
	CCCI_MEM_LOG_TAG(md->index, TAG,
		"AP_START(%p)=%x\n",
		hw_info->md_pcore_pccif_base + APCCIF_START,
		ccif_read32(hw_info->md_pcore_pccif_base, APCCIF_START));
	CCCI_MEM_LOG_TAG(md->index, TAG,
		"AP_TCHNUM(%p)=%x\n",
		hw_info->md_pcore_pccif_base + APCCIF_TCHNUM,
		ccif_read32(hw_info->md_pcore_pccif_base, APCCIF_TCHNUM));
	CCCI_MEM_LOG_TAG(md->index, TAG,
		"AP_RCHNUM(%p)=%x\n",
		hw_info->md_pcore_pccif_base + APCCIF_RCHNUM,
		ccif_read32(hw_info->md_pcore_pccif_base, APCCIF_RCHNUM));
	CCCI_MEM_LOG_TAG(md->index, TAG,
		"AP_ACK(%p)=%x\n",
		hw_info->md_pcore_pccif_base + APCCIF_ACK,
		ccif_read32(hw_info->md_pcore_pccif_base, APCCIF_ACK));

	md_cd_lock_modem_clock_src(0);
}

static void md_cd_check_emi_state(struct ccci_modem *md, int polling)
{
}

void md1_pmic_setting_on(void)
{
	vmd1_pmic_setting_on();
}

/* callback for system power off*/
void ccci_power_off(void)
{
	md1_pmic_setting_on();
}

void __attribute__((weak)) kicker_pbm_by_md(enum pbm_kicker kicker,
	bool status)
{
}

static int md_cd_soft_power_off(struct ccci_modem *md, unsigned int mode)
{
#ifdef FEATURE_CLK_BUF
	clk_buf_set_by_flightmode(true);
#endif
	return 0;
}

static int md_cd_soft_power_on(struct ccci_modem *md, unsigned int mode)
{
#ifdef FEATURE_CLK_BUF
	clk_buf_set_by_flightmode(false);
#endif
	return 0;
}

static int md_start_platform(struct ccci_modem *md)
{
#ifndef BY_PASS_MD_BROM
	struct device_node *node = NULL;
	void __iomem *sec_ao_base = NULL;
	int timeout = 100; /* 100 * 20ms = 2s */
	int ret = -1;
	int retval = 0;

	if ((md->per_md_data.config.setting&MD_SETTING_FIRST_BOOT) == 0)
		return 0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,security_ao");
	if (node) {
		sec_ao_base = of_iomap(node, 0);
		if (sec_ao_base == NULL) {
			CCCI_ERROR_LOG(md->index, TAG, "sec_ao_base NULL\n");
			return -1;
		}
	} else {
		CCCI_ERROR_LOG(md->index, TAG, "sec_ao NULL\n");
		return -1;
	}

	while (timeout > 0) {
		if (ccci_read32(sec_ao_base, 0x824) == 0x01 &&
			ccci_read32(sec_ao_base, 0x828) == 0x01 &&
			ccci_read32(sec_ao_base, 0x82C) == 0x01 &&
			ccci_read32(sec_ao_base, 0x830) == 0x01) {
			CCCI_BOOTUP_LOG(md->index, TAG, "BROM Pass\n");
			ret = 0;
			break;
		}
		timeout--;
		msleep(20);
	}

	CCCI_BOOTUP_LOG(md->index, TAG, "dummy md sys clk\n");
	retval = clk_prepare_enable(clk_table[0].clk_ref); /* match lk on */
	if (retval)
		CCCI_ERROR_LOG(md->index, TAG,
			"dummy md sys clk fail: ret = %d\n", retval);
	CCCI_BOOTUP_LOG(md->index, TAG, "dummy md sys clk done\n");
	md_cd_dump_md_bootup_status(md);

	md_cd_power_off(md, 0);

	if (ret != 0) {
		/* BROM */
		CCCI_ERROR_LOG(md->index, TAG,
			"BROM Failed: 0x%x, 0x%x, 0x%x, 0x%x\n",
			ccci_read32(sec_ao_base, 0x824),
			ccci_read32(sec_ao_base, 0x828),
			ccci_read32(sec_ao_base, 0x82C),
			ccci_read32(sec_ao_base, 0x830));
	}

	return ret;
#endif
	CCCI_NORMAL_LOG(-1, TAG, "by pass MD BROM\n");
	return 0;
}

#ifdef CCCI_PLATFORM_MT6781
static int mtk_ccci_cfg_srclken_o1_on(struct ccci_modem *md)
{
	unsigned int val;
	struct md_hw_info *hw_info = md->hw_info;

	CCCI_NORMAL_LOG(-1, TAG, "%s:spm_sleep_base:0x%lx\n",
		__func__, (unsigned long)hw_info->spm_sleep_base);
	if (hw_info->spm_sleep_base) {
		ccci_write32(hw_info->spm_sleep_base, 0, 0x0B160001);
		val = ccci_read32(hw_info->spm_sleep_base, 0);
		CCCI_NORMAL_LOG(-1, TAG, "spm_sleep_base: val:0x%x\n", val);

		val = ccci_read32(hw_info->spm_sleep_base, 8);
		CCCI_NORMAL_LOG(-1, TAG, "spm_sleep_base+8: val:0x%x +\n", val);
		val |= 0x1 << 21;
		ccci_write32(hw_info->spm_sleep_base, 8, val);
		val = ccci_read32(hw_info->spm_sleep_base, 8);
		CCCI_NORMAL_LOG(-1, TAG, "spm_sleep_base+8: val:0x%x -\n", val);
	}
	return 0;
}
#endif

#ifdef BY_PASS_MD_BROM
static int bypass_md_brom(struct ccci_modem *md)
{
	void __iomem *mdbrom_reg;

	CCCI_NORMAL_LOG(md->index, TAG, "%s:bypass MD brom\n",
		__func__);
	// unlock to write boot slave jump address
	mdbrom_reg = ioremap_nocache(0x20060000, 0x200);
	if (mdbrom_reg == NULL) {
		CCCI_ERROR_LOG(md->index, TAG,
			"md brom reg ioremap 0x1000 bytes from 0x20060000 fail\n");
		return -1;
	}
	CCCI_NORMAL_LOG(md->index, TAG, "%s:mdbrom_reg=%px\n",
		__func__, mdbrom_reg);
	ccci_write32(mdbrom_reg, 0x10C, 0x5500);
	// write 0x0 to boot slave jump address
	ccci_write32(mdbrom_reg, 0x104, 0x0);
	// update boot slave jump address
	ccci_write32(mdbrom_reg, 0x108, 0x1);
	iounmap(mdbrom_reg);

	return 0;
}
#endif

static int md_cd_power_on(struct ccci_modem *md)
{
	int ret = 0;
#ifndef CCCI_PLATFORM_MT6781
	unsigned int reg_value;
#endif

	/* step 1: PMIC setting */
	md1_pmic_setting_on();
	/* only use in mt6781 power_on flow */
#ifdef CCCI_PLATFORM_MT6781
	ret = mtk_ccci_cfg_srclken_o1_on(md);
	if (ret != 0) {
		CCCI_ERROR_LOG(0, TAG,
			"%s:config srclken_o1 fail\n",
			__func__);
		return ret;
	}
	CCCI_NORMAL_LOG(md->index, TAG, "%s: set srclken_o1_on done ret=%d\n",
		__func__, ret);
#endif

#ifndef CCCI_PLATFORM_MT6781
	/* step 2: MD srcclkena setting */
	reg_value = ccci_read32(infra_ao_base, INFRA_AO_MD_SRCCLKENA);
	reg_value &= ~(0xFF);
	reg_value |= 0x21;
	ccci_write32(infra_ao_base, INFRA_AO_MD_SRCCLKENA, reg_value);
	CCCI_BOOTUP_LOG(md->index, CORE,
		"%s: set md1_srcclkena bit(0x1000_0F0C)=0x%x\n",
		__func__, ccci_read32(infra_ao_base, INFRA_AO_MD_SRCCLKENA));
#endif

	/* steip 3: power on MD_INFRA and MODEM_TOP */
	switch (md->index) {
	case MD_SYS1:
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
		break;
	}
	if (ret)
		return ret;

#ifdef FEATURE_INFORM_NFC_VSIM_CHANGE
	/* notify NFC */
	inform_nfc_vsim_change(md->index, 1, 0);
#endif

#ifdef BY_PASS_MD_BROM
	ret = bypass_md_brom(md);
	if (ret != 0) {
		CCCI_ERROR_LOG(md->index, TAG,
			"bypass md brom fail ret=%d,exit\n", ret);
		return ret;
	}
#endif
	return 0;
}

int md_cd_bootup_cleanup(struct ccci_modem *md, int success)
{
	return 0;
}

static int md_cd_let_md_go(struct ccci_modem *md)
{
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;

	if (MD_IN_DEBUG(md))
		return -1;
	CCCI_BOOTUP_LOG(md->index, TAG, "set MD boot slave\n");

	/* make boot vector take effect */
	ccci_write32(md_info->md_boot_slave_En, 0, 1);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"MD boot slave = 0x%x\n",
		ccci_read32(md_info->md_boot_slave_En, 0));

	return 0;
}

static int md_cd_power_off(struct ccci_modem *md, unsigned int timeout)
{
	int ret = 0;
	unsigned int reg_value;

#ifdef FEATURE_INFORM_NFC_VSIM_CHANGE
	/* notify NFC */
	inform_nfc_vsim_change(md->index, 0, 0);
#endif

	/* power off MD_INFRA and MODEM_TOP */
	switch (md->index) {
	case MD_SYS1:
		/* 1. power off MD MTCMOS */
//md1_subsys_debug_dump(SYS_MD1);
		clk_disable_unprepare(clk_table[0].clk_ref);
		/* 2. disable srcclkena */
		CCCI_BOOTUP_LOG(md->index, TAG, "disable md1 clk\n");
		reg_value = ccci_read32(md_cd_plat_val_ptr.infra_ao_base, INFRA_AO_MD_SRCCLKENA);
		reg_value &= ~(0xFF);
		ccci_write32(md_cd_plat_val_ptr.infra_ao_base, INFRA_AO_MD_SRCCLKENA, reg_value);
		CCCI_BOOTUP_LOG(md->index, CORE,
			"%s: set md1_srcclkena=0x%x\n", __func__,
			ccci_read32(md_cd_plat_val_ptr.infra_ao_base, INFRA_AO_MD_SRCCLKENA));
		CCCI_BOOTUP_LOG(md->index, TAG, "Call md1_pmic_setting_off\n");
#ifdef FEATURE_CLK_BUF
		clk_buf_set_by_flightmode(true);
#endif
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

