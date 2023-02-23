// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include "ccci_config.h"
#include "ccci_common_config.h"
#include <linux/clk.h>
#include <mtk_pbm.h>
#include <mt-plat/mtk-clkbuf-bridge.h>
#ifdef USING_PM_RUNTIME
#include <linux/pm_runtime.h>
#else
#include <dt-bindings/clock/mt6779-clk.h>
#endif
#ifdef CONFIG_MTK_EMI_BWL
#include <emi_mbw.h>
#endif

#ifdef FEATURE_INFORM_NFC_VSIM_CHANGE
#include <mach/mt6605.h>
#endif

#ifdef CONFIG_MTK_QOS_SUPPORT
#include <linux/pm_qos.h>
#include <helio-dvfsrc-opp.h>
#endif

#include <linux/regulator/consumer.h> /* for MD PMIC */
#include <clk-mt6779-pg.h>
#include <mtk_spm_sleep.h>

#include "ccci_core.h"
#include "ccci_platform.h"

#include "md_sys1_platform.h"
#include "modem_secure_base.h"
#include "modem_reg_base.h"
#include "ap_md_reg_dump.h"

static struct regulator *reg_vmodem, *reg_vsram;
#include "hif/ccci_hif_dpmaif.h"

static struct ccci_clk_node clk_table[] = {
/* #ifdef USING_PM_RUNTIME */
	{ NULL, "scp-sys-md1-main"},
/* #endif */
	{ NULL, "infra-dpmaif-clk"},
	{ NULL, "infra-ccif-ap"},
	{ NULL, "infra-ccif-md"},
	{ NULL, "infra-ccif1-ap"},
	{ NULL, "infra-ccif1-md"},
	{ NULL, "infra-ccif2-ap"},
	{ NULL, "infra-ccif2-md"},
	{ NULL, "infra-ccif4-md"},
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
static void md_cd_lock_modem_clock_src(int locked);
//static void md_cldma_hw_reset(unsigned char md_id);
static void ccci_set_clk_cg(struct ccci_modem *md, unsigned int on);

int ccci_modem_remove(struct platform_device *dev);
void ccci_modem_shutdown(struct platform_device *dev);
int ccci_modem_suspend(struct platform_device *dev, pm_message_t state);
int ccci_modem_resume(struct platform_device *dev);
int ccci_modem_pm_suspend(struct device *device);
int ccci_modem_pm_resume(struct device *device);
int ccci_modem_pm_restore_noirq(struct device *device);


static struct ccci_plat_ops md_cd_plat_ptr = {
	.init = &ccci_platform_init_6779,
	.md_dump_reg = &md_dump_register_6779,
	//.cldma_hw_rst = &md_cldma_hw_reset,
	.set_clk_cg = &ccci_set_clk_cg,
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
	.vcore_config = NULL,
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

#ifdef ENABLE_DEBUG_DUMP /* Fix me! */
void ccci_dump(void)
{
	md1_subsys_debug_dump(SYS_MD1);
}
EXPORT_SYMBOL(ccci_dump);
#endif

int md_cd_get_modem_hw_info(struct platform_device *dev_ptr,
	struct ccci_dev_cfg *dev_cfg, struct md_hw_info *hw_info)
{
	struct device_node *node = NULL;
	struct device_node *node_infrao = NULL;
	int idx = 0;
#ifdef USING_PM_RUNTIME
	int retval = 0;
#endif

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
		hw_info->md_rgu_base = MD_RGU_BASE;
		hw_info->md_boot_slave_En = MD_BOOT_VECTOR_EN;
		of_property_read_u32(dev_ptr->dev.of_node,
			"mediatek,md_generation", &md_cd_plat_val_ptr.md_gen);
		node_infrao = of_find_compatible_node(NULL, NULL,
			"mediatek,mt6779-infracfg_ao");
		md_cd_plat_val_ptr.infra_ao_base = of_iomap(node_infrao, 0);

		hw_info->plat_ptr = &md_cd_plat_ptr;
		hw_info->plat_val = &md_cd_plat_val_ptr;
		if ((hw_info->plat_ptr == NULL) || (hw_info->plat_val == NULL))
			return -1;
		hw_info->plat_val->offset_epof_md1 = 7*1024+0x234;
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
#ifdef USING_PM_RUNTIME
	pm_runtime_enable(&dev_ptr->dev);
	dev_pm_syscore_device(&dev_ptr->dev, true);

	CCCI_BOOTUP_LOG(dev_cfg->index, TAG, "md mtcmos pm get start\n");
	retval = pm_runtime_get_sync(&dev_ptr->dev); /* match lk on */
	if (retval)
		CCCI_BOOTUP_LOG(dev_cfg->index, TAG,
			"md mtcmos pm getfail: ret = %d\n", retval);

	CCCI_BOOTUP_LOG(dev_cfg->index, TAG, "md mtcmos pm get done\n");
#endif

	return 0;
}

/* md1 sys_clk_cg no need set in this API*/
static void ccci_set_clk_cg(struct ccci_modem *md, unsigned int on)
{
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

	md_info->md_pll_base = md_reg;

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
	spm_ap_mdsrc_req(locked);
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
	md_cd_lock_modem_clock_src(1);
	if (md->hw_info->plat_ptr->md_dump_reg)
		md->hw_info->plat_ptr->md_dump_reg(md->index);

	md_cd_lock_modem_clock_src(0);

}

#ifndef CCCI_KMODULE_ENABLE
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
#endif
static void md_cd_check_emi_state(struct ccci_modem *md, int polling)
{
}

static void md1_pmic_setting_on(void)
{
	int ret = 0;

	ret = regulator_set_voltage(reg_vmodem, 875000, 875000);
	if (ret)
		CCCI_ERROR_LOG(-1, TAG, "pmic_vmodem setting on fail\n");
	ret = regulator_sync_voltage(reg_vmodem);
	if (ret)
		CCCI_ERROR_LOG(-1, TAG, "pmic_vmodem setting on fail\n");

	ret = regulator_set_voltage(reg_vsram, 993750, 993750);
	if (ret)
		CCCI_ERROR_LOG(-1, TAG, "pmic_vsram setting on fail\n");
	ret = regulator_sync_voltage(reg_vsram);
	if (ret)
		CCCI_ERROR_LOG(-1, TAG, "pmic_vsram setting on fail\n");

}

void __attribute__((weak)) kicker_pbm_by_md(enum pbm_kicker kicker,
	bool status)
{
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

static int md_start_platform(struct ccci_modem *md)
{
	struct device_node *node = NULL;
	void __iomem *sec_ao_base = NULL;
	int timeout = 100; /* 100 * 20ms = 2s */
	int ret = -1;
#ifndef USING_PM_RUNTIME
	int retval = 0;
#endif

	if ((md->per_md_data.config.setting&MD_SETTING_FIRST_BOOT) == 0)
		return 0;

	reg_vmodem = devm_regulator_get_optional(&md->plat_dev->dev, "vmodem");
	if (IS_ERR(reg_vmodem)) {
		ret = PTR_ERR(reg_vmodem);
		if ((ret != -ENODEV) && md->plat_dev->dev.of_node) {
			CCCI_ERROR_LOG(md->index, TAG,
				"get regulator(PMIC) fail: ret = %d\n", ret);
			return -1;
		}
	}
	reg_vsram = devm_regulator_get_optional(&md->plat_dev->dev, "vsram");
	if (IS_ERR(reg_vsram)) {
		ret = PTR_ERR(reg_vsram);
		if ((ret != -ENODEV) && md->plat_dev->dev.of_node) {
			CCCI_ERROR_LOG(md->index, TAG,
				"get regulator(PMIC1) fail: ret = %d\n", ret);
			return -1;
		}
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,security_ao");
	if (node) {
		sec_ao_base = of_iomap(node, 0);
		if (sec_ao_base == NULL) {
			CCCI_ERROR_LOG(md->index, TAG, "sec_ao_base NULL\n");
			return -1;
		}
	} else {
		sec_ao_base = ioremap_nocache(0x1001a000, 4);
		if (sec_ao_base == NULL) {
			CCCI_ERROR_LOG(md->index, TAG, "sec_ao NULL\n");
			return -1;
		}
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
#ifndef USING_PM_RUNTIME
	CCCI_BOOTUP_LOG(md->index, TAG, "dummy md sys clk\n");
	retval = clk_prepare_enable(clk_table[0].clk_ref); /* match lk on */
	if (retval)
		CCCI_ERROR_LOG(md->index, TAG,
			"dummy md sys clk fail: ret = %d\n", retval);
	CCCI_BOOTUP_LOG(md->index, TAG, "dummy md sys clk done\n");
#endif

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
}

static int md_cd_power_on(struct ccci_modem *md)
{
	int ret = 0;
	unsigned int reg_value;

	/* step 1: PMIC setting */
	md1_pmic_setting_on();

	/* step 2: MD srcclkena setting */
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

	/* steip 3: power on MD_INFRA and MODEM_TOP */
	switch (md->index) {
	case MD_SYS1:
		clk_buf_set_by_flightmode(false);
		CCCI_BOOTUP_LOG(md->index, TAG, "enable md sys clk\n");
#ifdef USING_PM_RUNTIME
		pm_runtime_get_sync(&md->plat_dev->dev);
#else
		ret = clk_prepare_enable(clk_table[0].clk_ref);
#endif

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
	/* Get infra cfg ao base */

	/* power off MD_INFRA and MODEM_TOP */
	switch (md->index) {
	case MD_SYS1:
		/* 1. power off MD MTCMOS */
#ifdef USING_PM_RUNTIME
		pm_runtime_put_sync(&md->plat_dev->dev);
		CCCI_BOOTUP_LOG(md->index, TAG, "PM:disable md1 clk\n");
#else
		clk_disable_unprepare(clk_table[0].clk_ref);
		CCCI_BOOTUP_LOG(md->index, TAG, "CCF:disable md1 clk\n");
#endif
		/* 2. disable srcclkena */

		CCCI_BOOTUP_LOG(md->index, TAG, "disable md1 clk\n");
		reg_value =
			ccci_read32(md->hw_info->plat_val->infra_ao_base,
			INFRA_AO_MD_SRCCLKENA);
		reg_value &= ~(0xFF);
		ccci_write32(md->hw_info->plat_val->infra_ao_base,
			INFRA_AO_MD_SRCCLKENA, reg_value);
		CCCI_BOOTUP_LOG(md->index, CORE,
			"%s: set md1_srcclkena=0x%x\n", __func__,
			ccci_read32(md->hw_info->plat_val->infra_ao_base,
			INFRA_AO_MD_SRCCLKENA));
		CCCI_BOOTUP_LOG(md->index, TAG, "Call md1_pmic_setting_off\n");

		clk_buf_set_by_flightmode(true);

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
	enum MD_STATE md_state = ccci_fsm_get_md_state(md->index);

	CCCI_NORMAL_LOG(0, TAG, "[%s] md->hif_flag = %d\n",
			__func__, md->hif_flag);

	if (md_state == GATED || md_state == WAITING_TO_STOP ||
		md_state == INVALID) {
		CCCI_NORMAL_LOG(md->index, TAG,
			"Resume no need restore for md_state=%d\n", md_state);
		return;
	}

	ccci_hif_resume(md->index, md->hif_flag);
}

int ccci_modem_plt_suspend(struct ccci_modem *md)
{
	CCCI_NORMAL_LOG(0, TAG, "[%s] md->hif_flag = %d\n",
			__func__, md->hif_flag);

	ccci_hif_suspend(md->index, md->hif_flag);

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

/* no support atf-1.4, so write scp smem addr to scp reg direct */
void ccci_notify_set_scpmem(void)
{
	unsigned long long key = 0;
	struct device_node *node = NULL;
	void __iomem *ap_ccif2_base;
	unsigned long long scp_smem_addr = 0;
	int size = 0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,ap_ccif2");
	if (node) {
		ap_ccif2_base = of_iomap(node, 0);
		if (!ap_ccif2_base) {
			CCCI_ERROR_LOG(-1, TAG, "ap_ccif2_base fail\n");
			return;
		}
	} else {
		CCCI_ERROR_LOG(-1, TAG, "can't find node ccif2 !\n");
		return;
	}
	scp_smem_addr = (unsigned long long) get_smem_phy_start_addr(MD_SYS1,
		SMEM_USER_CCISM_SCP, &size);
	if (scp_smem_addr) {
		ccci_write32(ap_ccif2_base, 0x100, (unsigned int)SCP_SMEM_KEY);
		ccci_write32(ap_ccif2_base, 0x104, (unsigned int)(SCP_SMEM_KEY >> 32));
		ccci_write32(ap_ccif2_base, 0x108, (unsigned int)scp_smem_addr);
		ccci_write32(ap_ccif2_base, 0x10c, (unsigned int)(scp_smem_addr >> 32));

		key = (unsigned long long) ccci_read32(ap_ccif2_base, 0x104);
		key = (key << 32 ) |
			((unsigned long long) ccci_read32(ap_ccif2_base, 0x100));
		CCCI_NORMAL_LOG(MD_SYS1, TAG,
			"%s: scp_smem_addr 0x%llx size: 0x%x  magic key: 0x%llx\n",
			__func__, scp_smem_addr, size, key);
	} else
		CCCI_ERROR_LOG(MD_SYS1, TAG, "%s get_smem fail\n", __func__);
}

int ccci_modem_suspend_noirq(struct device *dev)
{
	return dpmaif_suspend_noirq(dev);
}

int ccci_modem_resume_noirq(struct device *dev)
{
	return dpmaif_resume_noirq(dev);
}

