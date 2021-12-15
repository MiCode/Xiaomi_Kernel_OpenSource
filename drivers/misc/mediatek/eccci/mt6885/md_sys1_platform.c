/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
#include <linux/clk.h>
#include <mtk_pbm.h>
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
//#include <clk-mt6885-pg.h>
#include <clk-mt6893-pg.h>
#include "ccci_core.h"
#include "ccci_platform.h"

#include "md_sys1_platform.h"
#include "modem_secure_base.h"
#include "modem_reg_base.h"
#include "ap_md_reg_dump.h"
#include <devapc_public.h>
#include "ccci_fsm.h"

static struct ccci_clk_node clk_table[] = {
	{ NULL, "scp-sys-md1-main"},
};

extern unsigned int devapc_check_flag;
extern spinlock_t devapc_flag_lock;

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

int ccci_modem_remove(struct platform_device *dev);
void ccci_modem_shutdown(struct platform_device *dev);
int ccci_modem_suspend(struct platform_device *dev, pm_message_t state);
int ccci_modem_resume(struct platform_device *dev);
int ccci_modem_pm_suspend(struct device *device);
int ccci_modem_pm_resume(struct device *device);
int ccci_modem_pm_restore_noirq(struct device *device);


static struct ccci_plat_ops md_cd_plat_ptr = {
	.md_dump_reg = &md_dump_register_6885,
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
static int s_md_start_completed;

int ccif_read32(void *b, unsigned long a)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&devapc_flag_lock, flags);
	ret = ((devapc_check_flag == 1) ?
			ioread32((void __iomem *)((b)+(a))) : 0);
	spin_unlock_irqrestore(&devapc_flag_lock, flags);

	return ret;
}

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

/*devapc_violation_triggered*/
/* need fix, avoid build error, use option */
#if IS_ENABLED(CONFIG_MTK_DEVAPC) && !IS_ENABLED(CONFIG_DEVAPC_LEGACY)
static enum devapc_cb_status devapc_dump_adv_cb(uint32_t vio_addr)
{
	int count;

	CCCI_NORMAL_LOG(0, TAG,
		"[%s] vio_addr: 0x%x; is normal mdee: %d\n",
		__func__, vio_addr, ccci_fsm_is_normal_mdee());

	if (ccci_fsm_get_md_state(0) == EXCEPTION &&
		ccci_fsm_is_normal_mdee()) {
		count = ccci_fsm_increase_devapc_dump_counter();

		CCCI_NORMAL_LOG(0, TAG,
			"[%s] count: %d\n", __func__, count);

		if (count == 1)
			ccci_md_dump_in_interrupt((char *)__func__);

		return DEVAPC_NOT_KE;

	} else {
		ccci_md_dump_in_interrupt((char *)__func__);

		return DEVAPC_OK;
	}
}

static struct devapc_vio_callbacks devapc_test_handle = {
	.id = INFRA_SUBSYS_MD,
	.debug_dump_adv = devapc_dump_adv_cb,
};

void __weak register_devapc_vio_callback(
		struct devapc_vio_callbacks *viocb)
{
	CCCI_ERROR_LOG(-1, TAG, "[%s] is not supported!\n", __func__);
}

void ccci_md_devapc_register_cb(void)
{
	/*register handle function*/
	register_devapc_vio_callback(&devapc_test_handle);
}
#endif

void ccci_md_dump_in_interrupt(char *user_info)
{
	struct ccci_modem *md = NULL;

	CCCI_NORMAL_LOG(0, TAG, "%s called by %s\n", __func__, user_info);
	md = ccci_md_get_modem_by_id(0);
	if (md != NULL) {
		CCCI_NORMAL_LOG(0, TAG, "%s dump start\n", __func__);
		md->ops->dump_info(md, DUMP_FLAG_CCIF_REG | DUMP_FLAG_CCIF |
			DUMP_FLAG_REG | DUMP_FLAG_QUEUE_0_1 |
			DUMP_MD_BOOTUP_STATUS, NULL, 0);
	} else
		CCCI_NORMAL_LOG(0, TAG, "%s error\n", __func__);
	CCCI_NORMAL_LOG(0, TAG, "%s exit\n", __func__);
}
EXPORT_SYMBOL(ccci_md_dump_in_interrupt);

void ccci_md_debug_dump(char *user_info)
{
	struct ccci_modem *md = NULL;

	if (!s_md_start_completed) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: md start no call.\n", __func__);
		return;
	}

	CCCI_NORMAL_LOG(0, TAG, "%s called by %s\n", __func__, user_info);
	md = ccci_md_get_modem_by_id(0);
	if (md != NULL) {
		CCCI_NORMAL_LOG(0, TAG, "%s dump start\n", __func__);
		md->ops->dump_info(md, DUMP_FLAG_CCIF_REG | DUMP_FLAG_CCIF |
			DUMP_FLAG_REG | DUMP_FLAG_QUEUE_0_1 |
			DUMP_MD_BOOTUP_STATUS, NULL, 0);
		mdelay(1000);
		md->ops->dump_info(md, DUMP_FLAG_REG, NULL, 0);
	} else
		CCCI_NORMAL_LOG(0, TAG, "%s error\n", __func__);
	CCCI_NORMAL_LOG(0, TAG, "%s exit\n", __func__);
}
EXPORT_SYMBOL(ccci_md_debug_dump);

int md_cd_get_modem_hw_info(struct platform_device *dev_ptr,
	struct ccci_dev_cfg *dev_cfg, struct md_hw_info *hw_info)
{
	struct device_node *node = NULL;
	int idx = 0;
	int retval;

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
		node = of_find_compatible_node(NULL, NULL,
			"mediatek,infracfg_ao");
		md_cd_plat_val_ptr.infra_ao_base = of_iomap(node, 0);

		hw_info->plat_ptr = &md_cd_plat_ptr;
		hw_info->plat_val = &md_cd_plat_val_ptr;
		if ((hw_info->plat_ptr == NULL) || (hw_info->plat_val == NULL))
			return -1;
		hw_info->plat_val->offset_epof_md1 = CCCI_EE_OFFSET_EPOF_MD1;
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

		if (!IS_ERR(clk_table[0].clk_ref)) {
			CCCI_BOOTUP_LOG(dev_cfg->index, TAG,
				"dummy md sys clk\n");
			retval = clk_prepare_enable(clk_table[0].clk_ref);
			if (retval)
				CCCI_ERROR_LOG(dev_cfg->index, TAG,
					"error: enable mtcmos: ret: %d\n",
					retval);
			CCCI_BOOTUP_LOG(dev_cfg->index, TAG,
				"dummy md sys clk done\n");
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
		/* for ccif5 */
		node = of_find_compatible_node(NULL, NULL,
			"mediatek,md_ccif5");
		if (node) {
			hw_info->md_ccif5_base = of_iomap(node, 0);
			if (!hw_info->md_ccif5_base) {
				CCCI_ERROR_LOG(dev_cfg->index, TAG,
				"ccif5_base fail: 0x%p!\n",
				(void *)hw_info->md_ccif5_base);
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

	/* Get spm sleep base */
	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");
	hw_info->spm_sleep_base = (unsigned long)of_iomap(node, 0);
	if (!hw_info->spm_sleep_base) {
		CCCI_ERROR_LOG(0, TAG,
			"%s: spm_sleep_base of_iomap failed\n",
			__func__);
		return -1;
	}
	CCCI_INIT_LOG(-1, TAG, "spm_sleep_base:0x%lx\n",
			(unsigned long)hw_info->spm_sleep_base);

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

int md_cd_io_remap_md_side_register(struct ccci_modem *md)
{
	struct md_pll_reg *md_reg = NULL;
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;

	/* call internal_dump io_remap */
	md_io_remap_internal_dump_register(md);

	md_info->md_boot_slave_En =
	 ioremap_nocache(md->hw_info->md_boot_slave_En, 0x4);
	md_info->md_rgu_base =
	 ioremap_nocache(md->hw_info->md_rgu_base, 0x300);

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

void md_cd_lock_cldma_clock_src(int locked)
{
	/* spm_ap_mdsrc_req(locked); */
}

void md_cd_lock_modem_clock_src(int locked)
{
	size_t ret;

	/* spm_ap_mdsrc_req(locked); */
	mt_secure_call(MD_CLOCK_REQUEST,
			MD_REG_AP_MDSRC_REQ, locked, 0, 0, 0, 0);
	if (locked) {
		int settle = mt_secure_call(MD_CLOCK_REQUEST,
				MD_REG_AP_MDSRC_SETTLE, 0, 0, 0, 0, 0);

		if (!(settle > 0 && settle < 10))
			settle = 3;

		mdelay(settle);
		ret = mt_secure_call(MD_CLOCK_REQUEST,
				MD_REG_AP_MDSRC_ACK, 0, 0, 0, 0, 0);

		CCCI_NOTICE_LOG(-1, TAG,
				"settle = %d; ret = %lu\n", settle, ret);
	}
}

void md_cd_dump_md_bootup_status(struct ccci_modem *md)
{
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;
	struct md_pll_reg *md_reg = md_info->md_pll_base;

	/*To avoid AP/MD interface delay,
	 * dump 3 times, and buy-in the 3rd dump value.
	 */

	ccci_write32(md_reg->md_boot_stats_select, 0, 2);
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	CCCI_NOTICE_LOG(md->index, TAG,
		"md_boot_stats0:0x%X\n",
		ccci_read32(md_reg->md_boot_stats, 0));

	ccci_write32(md_reg->md_boot_stats_select, 0, 3);
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	CCCI_NOTICE_LOG(md->index, TAG,
		"md_boot_stats1:0x%X\n",
		ccci_read32(md_reg->md_boot_stats, 0));
}

void md_cd_get_md_bootup_status(
	struct ccci_modem *md, unsigned int *buff, int length)
{
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;
	struct md_pll_reg *md_reg = md_info->md_pll_base;

	CCCI_NOTICE_LOG(md->index, TAG, "md_boot_stats len %d\n", length);

	if (length < 2 || buff == NULL) {
		md_cd_dump_md_bootup_status(md);
		return;
	}

	ccci_write32(md_reg->md_boot_stats_select, 0, 2);
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	buff[0] = ccci_read32(md_reg->md_boot_stats, 0);

	ccci_write32(md_reg->md_boot_stats_select, 0, 3);
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

void md_cd_dump_debug_register(struct ccci_modem *md)
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

	md_cd_lock_modem_clock_src(1);

	/* This function needs to be cancelled temporarily for bringup */
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

void md_cd_check_emi_state(struct ccci_modem *md, int polling)
{
}

void md1_pmic_setting_on(void)
{
	CCCI_NORMAL_LOG(-1, "POWER ON", "vmd1_pmic_setting_on() start.\n");
	CCCI_BOOTUP_LOG(-1, TAG, "[POWER ON] vmd1_pmic_setting_on() start.\n");
	vmd1_pmic_setting_on();
	CCCI_NORMAL_LOG(-1, "POWER ON", "vmd1_pmic_setting_on() end.\n");
	CCCI_BOOTUP_LOG(-1, TAG, "[POWER ON] vmd1_pmic_setting_on() end.\n");
}

void md1_pmic_setting_off(void)
{
	vmd1_pmic_setting_off();
}

/* callback for system power off*/
void ccci_power_off(void)
{
	md1_pmic_setting_on();
}

void __attribute__((weak)) kicker_pbm_by_md(enum pbm_kicker kicker,
	bool status)
{
	CCCI_NORMAL_LOG(-1, TAG, "kicker_pbm_by_md() no support.\n");
	CCCI_BOOTUP_LOG(-1, TAG, "kicker_pbm_by_md() no support.\n");
}

#ifdef FEATURE_CLK_BUF
static void flight_mode_set_by_atf(struct ccci_modem *md,
		unsigned int flightMode)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_FLIGHT_MODE_SET,
		flightMode, 0, 0, 0, 0, 0, &res);

	CCCI_BOOTUP_LOG(md->index, TAG,
		"[%s] flag_1=%lu, flag_2=%lu, flag_3=%lu, flag_4=%lu\n",
		__func__, res.a0, res.a1, res.a2, res.a3);
}
#endif

int md_cd_soft_power_off(struct ccci_modem *md, unsigned int mode)
{
#ifdef FEATURE_CLK_BUF
	flight_mode_set_by_atf(md, true);
#endif
	return 0;
}

int md_cd_soft_power_on(struct ccci_modem *md, unsigned int mode)
{
#ifdef FEATURE_CLK_BUF
	flight_mode_set_by_atf(md, false);
#endif
	return 0;
}

int md_start_platform(struct ccci_modem *md)
{
	struct arm_smccc_res res;
	int timeout = 100; /* 100 * 20ms = 2s */
	int ret = -1;

	s_md_start_completed = 1;

	if ((md->per_md_data.config.setting&MD_SETTING_FIRST_BOOT) == 0)
		return 0;

	while (timeout > 0) {
		ret = mt_secure_call(MD_POWER_CONFIG, MD_CHECK_DONE,
			0, 0, 0, 0, 0);
		if (!ret) {
			CCCI_BOOTUP_LOG(md->index, TAG, "BROM PASS\n");
			break;
		}
		timeout--;
		msleep(20);
	}
	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_POWER_CONFIG,
		MD_CHECK_FLAG, 0, 0, 0, 0, 0, &res);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"flag_1=%lu, flag_2=%lu, flag_3=%lu, flag_4=%lu\n",
		res.a0, res.a1, res.a2, res.a3);

	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_POWER_CONFIG,
		MD_BOOT_STATUS, 0, 0, 0, 0, 0, &res);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"AP: boot_ret=%lu, boot_status_0=%lX, boot_status_1=%lX\n",
		res.a0, res.a1, res.a2);

	if (ret != 0) {
		/* BROM */
		CCCI_ERROR_LOG(md->index, TAG, "BROM Failed\n");
	}
	md_cd_power_off(md, 0);
	return ret;
}


static int mtk_ccci_cfg_srclken_o1_on(struct ccci_modem *md)
{
	unsigned int val;
	struct md_hw_info *hw_info = md->hw_info;

	if (hw_info->spm_sleep_base) {
		CCCI_NORMAL_LOG(-1, "POWER ON", "%s() start.\n", __func__);
		CCCI_BOOTUP_LOG(-1, TAG, "[POWER ON] %s() start.\n", __func__);

		ccci_write32(hw_info->spm_sleep_base, 0, 0x0B160001);
		val = ccci_read32(hw_info->spm_sleep_base, 0);
		CCCI_BOOTUP_LOG(-1, TAG, "[POWER ON] spm_sleep_base: val:0x%x\n", val);

		val = ccci_read32(hw_info->spm_sleep_base, 8);
		CCCI_BOOTUP_LOG(-1, TAG, "[POWER ON] spm_sleep_base+8: val:0x%x +\n", val);
		val |= 0x1<<21;
		ccci_write32(hw_info->spm_sleep_base, 8, val);
		val = ccci_read32(hw_info->spm_sleep_base, 8);
		CCCI_NORMAL_LOG(-1, "POWER ON", "spm_sleep_base+8: val:0x%x -\n", val);
		CCCI_BOOTUP_LOG(-1, TAG, "[POWER ON] spm_sleep_base+8: val:0x%x -\n", val);
		CCCI_NORMAL_LOG(-1, "POWER ON", "%s() end.\n", __func__);
		CCCI_BOOTUP_LOG(-1, TAG, "[POWER ON] %s() end.\n", __func__);
	}
	return 0;
}

int md_cd_power_on(struct ccci_modem *md)
{
	int ret = 0;
	unsigned int reg_value;

	/* step 1: PMIC setting */
	md1_pmic_setting_on();

	/* step 2: MD srcclkena setting */
	reg_value = ccci_read32(infra_ao_base, INFRA_AO_MD_SRCCLKENA);
	reg_value &= ~(0xFF);
	reg_value |= 0x21;
	ccci_write32(infra_ao_base, INFRA_AO_MD_SRCCLKENA, reg_value);
	CCCI_BOOTUP_LOG(md->index, CORE,
		"%s: set md1_srcclkena bit(0x1000_0F0C)=0x%x\n",
		__func__, ccci_read32(infra_ao_base, INFRA_AO_MD_SRCCLKENA));

	mtk_ccci_cfg_srclken_o1_on(md);
	/* steip 3: power on MD_INFRA and MODEM_TOP */
	switch (md->index) {
	case MD_SYS1:
#ifdef FEATURE_CLK_BUF
		flight_mode_set_by_atf(md, false);
#endif
		CCCI_NORMAL_LOG(-1, "POWER ON", "clk_prepare_enable() start.\n");
		CCCI_BOOTUP_LOG(-1, TAG, "[POWER ON] clk_prepare_enable() start.\n");
		ret = clk_prepare_enable(clk_table[0].clk_ref);
		CCCI_NORMAL_LOG(-1, "POWER ON",
			"clk_prepare_enable() end. ret=%d\n", ret);
		CCCI_BOOTUP_LOG(-1, TAG,
			"[POWER ON] clk_prepare_enable() end. ret=%d\n", ret);
		kicker_pbm_by_md(KR_MD1, true);
		CCCI_BOOTUP_LOG(md->index, TAG,
			"Call end kicker_pbm_by_md(0,true)\n");
		break;
	}
	if (ret)
		return ret;

#ifdef FEATURE_INFORM_NFC_VSIM_CHANGE
	/* notify NFC */
	CCCI_NORMAL_LOG(-1, "POWER ON", "inform_nfc_vsim_change() start.\n");
	CCCI_BOOTUP_LOG(-1, TAG, "[POWER ON] inform_nfc_vsim_change() start.\n");
	inform_nfc_vsim_change(md->index, 1, 0);
	CCCI_NORMAL_LOG(-1, "POWER ON", "inform_nfc_vsim_change() end.\n");
	CCCI_BOOTUP_LOG(-1, TAG, "[POWER ON] inform_nfc_vsim_change() end.\n");
#endif
	return 0;
}

int md_cd_bootup_cleanup(struct ccci_modem *md, int success)
{
	return 0;
}

int md_cd_let_md_go(struct ccci_modem *md)
{
	struct arm_smccc_res res;

	CCCI_NORMAL_LOG(-1, "POWER ON", "%s() start.\n", __func__);
	CCCI_BOOTUP_LOG(-1, TAG, "[POWER ON] %s() start.\n", __func__);

	if (MD_IN_DEBUG(md))
		return -1;
	CCCI_BOOTUP_LOG(md->index, TAG, "set MD boot slave\n");

	/* make boot vector take effect */
	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_POWER_CONFIG,
		MD_KERNEL_BOOT_UP, 0, 0, 0, 0, 0, &res);

	CCCI_NORMAL_LOG(md->index, "POWER ON",
		"MD: boot_ret=%lu, boot_status_0=%lu, boot_status_1=%lu\n",
		res.a0, res.a1, res.a2);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"[POWER ON] MD: boot_ret=%lu, boot_status_0=%lu, boot_status_1=%lu\n",
		res.a0, res.a1, res.a2);

	CCCI_NORMAL_LOG(-1, "POWER ON", "%s() end.\n", __func__);
	CCCI_BOOTUP_LOG(-1, TAG, "[POWER ON] %s() end.\n", __func__);

	return 0;
}

int md_cd_power_off(struct ccci_modem *md, unsigned int timeout)
{
	int ret = 0;
	unsigned int reg_value;

#ifdef FEATURE_INFORM_NFC_VSIM_CHANGE
	/* notify NFC */
	CCCI_NORMAL_LOG(-1, "POWER OFF", "inform_nfc_vsim_change() start.\n");
	CCCI_BOOTUP_LOG(-1, TAG, "[POWER OFF] inform_nfc_vsim_change() start.\n");
	inform_nfc_vsim_change(md->index, 0, 0);
	CCCI_NORMAL_LOG(-1, "POWER OFF", "inform_nfc_vsim_change() end.\n");
	CCCI_BOOTUP_LOG(-1, TAG, "[POWER OFF] inform_nfc_vsim_change() end.\n");
#endif

	/* power off MD_INFRA and MODEM_TOP */
	switch (md->index) {
	case MD_SYS1:
		/* 1. power off MD MTCMOS */
		CCCI_NORMAL_LOG(-1, "POWER OFF", "clk_disable_unprepare() start.\n");
		CCCI_BOOTUP_LOG(-1, TAG, "[POWER OFF] clk_disable_unprepare() start.\n");
		clk_disable_unprepare(clk_table[0].clk_ref);
		CCCI_NORMAL_LOG(-1, "POWER OFF", "clk_disable_unprepare() end.\n");
		CCCI_BOOTUP_LOG(-1, TAG, "[POWER OFF] clk_disable_unprepare() end.\n");
		/* 2. disable srcclkena */
		CCCI_BOOTUP_LOG(md->index, TAG, "disable md1 clk\n");
		reg_value = ccci_read32(infra_ao_base, INFRA_AO_MD_SRCCLKENA);
		reg_value &= ~(0xFF);
		ccci_write32(infra_ao_base, INFRA_AO_MD_SRCCLKENA, reg_value);
		CCCI_BOOTUP_LOG(md->index, CORE,
			"%s: set md1_srcclkena=0x%x\n", __func__,
			ccci_read32(infra_ao_base, INFRA_AO_MD_SRCCLKENA));
		CCCI_BOOTUP_LOG(md->index, TAG, "Call md1_pmic_setting_off\n");
#ifdef FEATURE_CLK_BUF
		flight_mode_set_by_atf(md, true);
#endif
		/* 3. PMIC off */
		md1_pmic_setting_off();

		/* 5. DLPT */
		kicker_pbm_by_md(KR_MD1, false);
		CCCI_BOOTUP_LOG(md->index, TAG,
			"Call end kicker_pbm_by_md(0,false)\n");
		break;
	}
	return ret;
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

void ccci_hif_cldma_restore_reg(struct ccci_modem *md)
{
}

void ccci_modem_restore_reg(struct ccci_modem *md)
{
	enum MD_STATE md_state = ccci_fsm_get_md_state(md->index);

	if (md_state == GATED || md_state == WAITING_TO_STOP ||
		md_state == INVALID) {
		CCCI_NORMAL_LOG(md->index, TAG,
			"Resume no need restore for md_state=%d\n", md_state);
		return;
	}

	if (md->hif_flag & (1 << CLDMA_HIF_ID))
		ccci_hif_cldma_restore_reg(md);

	ccci_hif_resume(md->index, md->hif_flag);
}

int ccci_modem_plt_suspend(void)
{
	struct ccci_modem *md;

	CCCI_DEBUG_LOG(0, TAG, "%s\n", __func__);
	md = ccci_md_get_modem_by_id(0);
	if (md != NULL)
		ccci_hif_suspend(md->index, md->hif_flag);
	return 0;
}

void ccci_modem_plt_resume(void)
{
	struct ccci_modem *md;

	CCCI_DEBUG_LOG(0, TAG, "%s\n", __func__);
	md = ccci_md_get_modem_by_id(0);
	if (md != NULL)
		ccci_modem_restore_reg(md);
}

