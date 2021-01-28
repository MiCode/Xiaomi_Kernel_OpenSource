// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/wait.h>
#include <linux/sched/clock.h> /* local_clock() */
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/netdevice.h>
#include <linux/random.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include "ccci_config.h"
#include "ccci_core.h"
#include "ccci_modem.h"
#include "ccci_bm.h"
#include "ccci_platform.h"
#if (MD_GENERATION <= 6292)
#include <mach/mtk_pbm.h>
#include "ccif_c2k_platform.h"
#include "ccci_hif.h"
#include "hif/ccci_hif_ccif.h"

#define TAG "c2k"

static irqreturn_t md_cd_wdt_isr(int irq, void *data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;

	/*1. disable MD WDT */
#ifdef ENABLE_MD_WDT_DBG
	unsigned int state;

	state = ccif_read32(md->md_rgu_base, C2K_WDT_MD_STA);
	ccif_write32(md->md_rgu_base, C2K_WDT_MD_MODE, C2K_WDT_MD_MODE_KEY);
	CCCI_NORMAL_LOG(md->index, TAG,
		"WDT IRQ disabled for debug, state=%X\n", state);
#endif
	CCCI_NORMAL_LOG(md->index, TAG, "MD WDT IRQ\n");
	ccci_event_log("md%d: MD WDT IRQ\n", md->index);

	ccci_fsm_recv_md_interrupt(md->index, MD_IRQ_WDT);
	return IRQ_HANDLED;
}

static void md_ccif_exception(struct ccci_modem *md, enum HIF_EX_STAGE stage)
{
	CCCI_NORMAL_LOG(md->index, TAG, "MD exception HIF %d\n", stage);
	switch (stage) {
	case HIF_EX_INIT:
		/* Rx dispatch does NOT depend on queue index
		 * in port structure, so it still can find right port.
		 */
		md_ccif_send(CCIF_HIF_ID, H2D_EXCEPTION_ACK);
		break;
	case HIF_EX_INIT_DONE:
		break;
	case HIF_EX_CLEARQ_DONE:
		ccci_hif_dump_status(md->hif_flag, DUMP_FLAG_CCIF, 0);
		md_ccif_reset_queue(CCIF_HIF_ID, 0);
		md_ccif_send(CCIF_HIF_ID, H2D_EXCEPTION_CLEARQ_ACK);
		break;
	case HIF_EX_ALLQ_RESET:
		md->per_md_data.is_in_ee_dump = 1;
		break;
	default:
		break;
	};
}

static int md_ccif_ee_handshake(struct ccci_modem *md, int timeout)
{
	/* seems sometime MD send D2H_EXCEPTION_INIT_DONE and
	 * D2H_EXCEPTION_CLEARQ_DONE together
	 */
	/* polling_ready(md_ctrl, D2H_EXCEPTION_INIT); */
	md_ccif_exception(md, HIF_EX_INIT);
	ccif_polling_ready(CCIF_HIF_ID, D2H_EXCEPTION_INIT_DONE);
	md_ccif_exception(md, HIF_EX_INIT_DONE);

	ccif_polling_ready(CCIF_HIF_ID, D2H_EXCEPTION_CLEARQ_DONE);
	md_ccif_exception(md, HIF_EX_CLEARQ_DONE);

	ccif_polling_ready(CCIF_HIF_ID, D2H_EXCEPTION_ALLQ_RESET);
	md_ccif_exception(md, HIF_EX_ALLQ_RESET);

	return 0;
}


static int md_ccif_op_init(struct ccci_modem *md)
{
	CCCI_NORMAL_LOG(md->index, TAG, "CCIF modem is initializing\n");

	return 0;
}
/*used for throttling feature - end*/
static int md_ccif_op_start(struct ccci_modem *md)
{
	char img_err_str[IMG_ERR_STR_LEN];
	struct ccci_modem *md1 = NULL;
	int ret = 0;
	unsigned int retry_cnt = 0;
	struct ccci_per_md *per_md_data = ccci_get_per_md_data(md->index);

	/*something do once*/
	if (md->per_md_data.config.setting & MD_SETTING_FIRST_BOOT) {
		CCCI_BOOTUP_LOG(md->index, TAG, "CCIF modem is first boot\n");
		ccci_md_clear_smem(md->index, 1);
		md1 = ccci_md_get_modem_by_id(MD_SYS1);
		if (md1) {
			while (md1->per_md_data.config.setting &
					MD_SETTING_FIRST_BOOT) {
				msleep(20);
				if (retry_cnt++ > 1000) {
					CCCI_ERROR_LOG(md->index, TAG,
					"wait MD1 start time out\n");
					break;
				}
			}
			CCCI_BOOTUP_LOG(md->index, TAG,
				"wait for MD1 starting done\n");
		} else
			CCCI_ERROR_LOG(md->index, TAG,
				"get MD1 modem struct fail\n");
		md_ccif_ring_buf_init(CCIF_HIF_ID);
		CCCI_BOOTUP_LOG(md->index, TAG,
			"modem capability 0x%x\n",
			md->per_md_data.md_capability);
		md->per_md_data.config.setting &= ~MD_SETTING_FIRST_BOOT;
	} else {
		ccci_md_clear_smem(md->index, 0);
	}

#ifdef FEATURE_BSI_BPI_SRAM_CFG
	ccci_set_bsi_bpi_SRAM_cfg(md, 1, MD_FLIGHT_MODE_NONE);
#endif

	/*enable ccif clk*/
	ccci_set_clk_cg(md, 1);
	/* 0. init security, as security depends on dummy_char,
	 * which is ready very late.
	 */
	ccci_init_security();
	md_ccif_sram_reset(CCIF_HIF_ID);
	md_ccif_reset_queue(CCIF_HIF_ID, 1);
	per_md_data->data_usb_bypass = 0;
	md->per_md_data.is_in_ee_dump = 0;
	md->is_force_asserted = 0;
	CCCI_NORMAL_LOG(md->index, TAG, "CCIF modem is starting\n");
	/*1. load modem image */
	if (!modem_run_env_ready(md->index)) {
		if (md->per_md_data.config.setting & MD_SETTING_FIRST_BOOT
		    || md->per_md_data.config.setting & MD_SETTING_RELOAD) {
			ret =
			    ccci_load_firmware(md->index,
				&md->per_md_data.img_info[IMG_MD],
				img_err_str, md->per_md_data.img_post_fix,
				&md->plat_dev->dev);
			if (ret < 0) {
				CCCI_ERROR_LOG(md->index, TAG,
					"load firmware fail, %s\n",
					img_err_str);
				goto out;
			}
			/*load_std_firmware returns MD image size */
			ret = 0;
			md->per_md_data.config.setting &= ~MD_SETTING_RELOAD;
		}
	} else {
		CCCI_NORMAL_LOG(md->index, TAG,
			"C2K modem image ready, bypass load\n");
		ret = ccci_get_md_check_hdr_inf(md->index,
				&md->per_md_data.img_info[IMG_MD],
			md->per_md_data.img_post_fix);
		if (ret < 0) {
			CCCI_NORMAL_LOG(md->index, TAG,
				"partition read fail(%d)\n", ret);
			/*goto out; */
		} else
			CCCI_BOOTUP_LOG(md->index, TAG,
				"partition read success\n");
	}
	md->per_md_data.config.setting &= ~MD_SETTING_FIRST_BOOT;

	/*2. enable MPU */

	/*3. power on modem, do NOT touch MD register before this */
	ret = md_ccif_power_on(md);
	if (ret) {
		CCCI_ERROR_LOG(md->index, TAG, "power on MD fail %d\n", ret);
		goto out;
	}
	/*4. update mutex */
	atomic_set(&md->reset_on_going, 0);

	/*5. let modem go */
	md_ccif_let_md_go(md);
	enable_irq(md->md_wdt_irq_id);
 out:
	CCCI_NORMAL_LOG(md->index, TAG, "ccif modem started %d\n", ret);
	/*used for throttling feature - start */
	/*ccci_modem_boot_count[md->index]++;*/
	/*used for throttling feature - end */
	return ret;
}

static int md_ccif_op_stop(struct ccci_modem *md, unsigned int stop_type)
{
	int ret = 0;

	CCCI_NORMAL_LOG(md->index, TAG,
		"ccif modem is power off, stop_type=%d\n", stop_type);
	ret = md_ccif_power_off(md,
			stop_type == MD_FLIGHT_MODE_ENTER ? 100 : 0);
	CCCI_NORMAL_LOG(md->index, TAG,
		"ccif modem is power off done, %d\n", ret);

	/*disable ccif clk*/
	ccci_set_clk_cg(md, 0);

#ifdef FEATURE_BSI_BPI_SRAM_CFG
	ccci_set_bsi_bpi_SRAM_cfg(md, 0, stop_type);
#endif

	return 0;
}

static int md_ccif_op_pre_stop(struct ccci_modem *md, unsigned int stop_type)
{
	/*1. mutex check */
	if (atomic_inc_return(&md->reset_on_going) > 1) {
		CCCI_NORMAL_LOG(md->index, TAG, "One reset flow is on-going\n");
		return -CCCI_ERR_MD_IN_RESET;
	}

	CCCI_NORMAL_LOG(md->index, TAG, "ccif modem is resetting\n");
	/*2. disable IRQ (use nosync) */
	disable_irq_nosync(md->md_wdt_irq_id);

	return 0;
}

static void dump_runtime_data(struct ccci_modem *md,
	struct ap_query_md_feature *ap_feature)
{
	u8 i = 0;

	CCCI_BOOTUP_LOG(md->index, TAG,
		"head_pattern 0x%x\n", ap_feature->head_pattern);

	for (i = AT_CHANNEL_NUM; i < AP_RUNTIME_FEATURE_ID_MAX; i++) {
		CCCI_BOOTUP_LOG(md->index, TAG,
			"ap query md feature %u: mask %u, version %u\n",
			i, ap_feature->feature_set[i].support_mask,
			ap_feature->feature_set[i].version);
	}
	CCCI_BOOTUP_LOG(md->index, TAG,
		"share_memory_support 0x%x\n",
		ap_feature->share_memory_support);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"ap_runtime_data_addr 0x%x\n",
		ap_feature->ap_runtime_data_addr);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"ap_runtime_data_size 0x%x\n",
		ap_feature->ap_runtime_data_size);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"md_runtime_data_addr 0x%x\n",
		ap_feature->md_runtime_data_addr);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"md_runtime_data_size 0x%x\n",
		ap_feature->md_runtime_data_size);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"set_md_mpu_start_addr 0x%x\n",
		ap_feature->set_md_mpu_start_addr);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"set_md_mpu_total_size 0x%x\n",
		ap_feature->set_md_mpu_total_size);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"tail_pattern 0x%x\n",
		ap_feature->tail_pattern);
}

static void md_ccif_smem_sub_region_init(struct ccci_modem *md)
{
	int __iomem *addr;
	int i;
	struct ccci_smem_region *dbm =
		ccci_md_get_smem_by_user_id(md->index, SMEM_USER_RAW_DBM);

	/*Region 0, dbm */
	addr = (int __iomem *)dbm->base_ap_view_vir;
	addr[0] = 0x44444444;	/*Guard pattern 1 header */
	addr[1] = 0x44444444;	/*Guard pattern 2 header */
#ifdef DISABLE_PBM_FEATURE
	for (i = 2; i < (CCCI_SMEM_SIZE_DBM/4 + 2); i++)
		addr[i] = 0xFFFFFFFF;
#else
	for (i = 2; i < (CCCI_SMEM_SIZE_DBM/4 + 2); i++)
		addr[i] = 0x00000000;
#endif
	addr[i++] = 0x44444444;	/*Guard pattern 1 tail */
	addr[i++] = 0x44444444;	/*Guard pattern 2 tail */

	/*Notify PBM */
#ifndef DISABLE_PBM_FEATURE
	init_md_section_level(KR_MD3);
#endif
}

static void config_ap_runtime_data(struct ccci_modem *md,
	struct ap_query_md_feature *ap_rt_data)
{
	struct ccci_feature_support s_info[4];
	struct ccci_smem_region *runtime_data =
		ccci_md_get_smem_by_user_id(md->index,
			SMEM_USER_RAW_RUNTIME_DATA);
	struct ccci_smem_region *md2md =
		ccci_md_get_smem_by_user_id(md->index,
			SMEM_USER_RAW_MD2MD);

	/* Notice: ccif_write8 is invalid,
	 * so must write 4 features at the same time
	 */
	s_info[0].version = 0;	/*AT_CHANNEL_NUM*/
	/*CCCI_FEATURE_OPTIONAL_SUPPORT;*/
	s_info[0].support_mask = CCCI_FEATURE_OPTIONAL_SUPPORT;
	s_info[1].version = 0;
	s_info[1].support_mask = 0;
	s_info[2].version = 0;
	s_info[2].support_mask = 0;
	s_info[3].version = 0;
	s_info[3].support_mask = 0;
	ccif_write32(&ap_rt_data->feature_set[0], 0,
		s_info[0].version << 4 | s_info[0].support_mask);

	ccif_write32(&ap_rt_data->head_pattern, 0,
		AP_FEATURE_QUERY_PATTERN);

	ccif_write32(&ap_rt_data->share_memory_support, 0,
		INTERNAL_MODEM);

	ccif_write32(&ap_rt_data->ap_runtime_data_addr, 0,
		runtime_data->base_md_view_phy);
	ccif_write32(&ap_rt_data->ap_runtime_data_size, 0,
		CCCI_SMEM_SIZE_RUNTIME_AP);

	ccif_write32(&ap_rt_data->md_runtime_data_addr, 0,
		runtime_data->base_md_view_phy + CCCI_SMEM_SIZE_RUNTIME_AP);
	ccif_write32(&ap_rt_data->md_runtime_data_size, 0,
		CCCI_SMEM_SIZE_RUNTIME_MD);

	ccif_write32(&ap_rt_data->set_md_mpu_start_addr, 0,
		md->mem_layout.md_bank4_noncacheable_total.base_md_view_phy
		+ md2md->size);
	ccif_write32(&ap_rt_data->set_md_mpu_total_size, 0,
		md->mem_layout.md_bank4_noncacheable_total.size
		- md2md->size);

	ccif_write32(&ap_rt_data->tail_pattern, 0,
		AP_FEATURE_QUERY_PATTERN);
}

static int md_ccif_op_send_runtime_data(struct ccci_modem *md,
	unsigned int tx_ch, unsigned int txqno, int skb_from_pool)
{
	int packet_size =
		sizeof(struct ap_query_md_feature)
		+ sizeof(struct ccci_header);
	struct ap_query_md_feature *ap_rt_data = NULL;
	int ret;

	ap_rt_data =
	(struct ap_query_md_feature *)ccif_hif_fill_rt_header(CCIF_HIF_ID,
	packet_size, tx_ch, txqno);

	config_ap_runtime_data(md, ap_rt_data);

	dump_runtime_data(md, ap_rt_data);

	md_ccif_smem_sub_region_init(md);

	ret = md_ccif_send(CCIF_HIF_ID, H2D_SRAM);
	return ret;
}

static int md_ccif_op_force_assert(struct ccci_modem *md,
	enum MD_COMM_TYPE type)
{
	CCCI_NORMAL_LOG(md->index, TAG, "force assert MD using %d\n", type);
	if (type == CCIF_INTERRUPT)
		md_ccif_send(CCIF_HIF_ID, AP_MD_SEQ_ERROR);
	return 0;

}

static inline void clear_md1_md3_smem(struct ccci_modem *md)
{
	struct ccci_smem_region *region;

	CCCI_NORMAL_LOG(md->index, TAG, "%s start\n", __func__);
	region = ccci_md_get_smem_by_user_id(md->index, SMEM_USER_RAW_MD2MD);

	if (!region) {
		CCCI_NORMAL_LOG(md->index, TAG, "%s error\n", __func__);
		return;
	}
	memset_io(region->base_ap_view_vir, 0, region->size);
}

static int md_ccif_op_reset_pccif(struct ccci_modem *md)
{
	reset_md1_md3_pccif(md);
	clear_md1_md3_smem(md);
	return 0;
}

static int md_ccif_dump_info(struct ccci_modem *md, enum MODEM_DUMP_FLAG flag,
	void *buff, int length)
{
	/*normal EE */
	if (flag & DUMP_FLAG_MD_WDT)
		dump_c2k_register(md, 1);
	/*MD boot fail EE */
	if (flag & DUMP_FLAG_CCIF_REG)
		dump_c2k_register(md, 2);
	/*runtime data, boot, long time no response EE */
	ccci_hif_dump_status(md->hif_flag, flag, length);

	return 0;
}

static int md_ccif_ee_callback(struct ccci_modem *md, enum MODEM_EE_FLAG flag)
{
	if (flag & EE_FLAG_ENABLE_WDT)
		enable_irq(md->md_wdt_irq_id);

	if (flag & EE_FLAG_DISABLE_WDT)
		disable_irq_nosync(md->md_wdt_irq_id);

	return 0;
}

static struct ccci_modem_ops md_ccif_ops = {
	.init = &md_ccif_op_init,
	.start = &md_ccif_op_start,
	.stop = &md_ccif_op_stop,
	.pre_stop = &md_ccif_op_pre_stop,
	.send_runtime_data = &md_ccif_op_send_runtime_data,
	.ee_handshake = &md_ccif_ee_handshake,
	.force_assert = &md_ccif_op_force_assert,
	.dump_info = &md_ccif_dump_info,
	.ee_callback = &md_ccif_ee_callback,
	.reset_pccif = &md_ccif_op_reset_pccif,
};

static void md_ccif_hw_init(struct ccci_modem *md)
{
	int ret;

	md_ccif_io_remap_md_side_register(md);

	/*request IRQ */
	md->md_wdt_irq_id = md->hw_info->md_wdt_irq_id;
	ret = request_irq(md->md_wdt_irq_id, md_cd_wdt_isr,
			md->md_wdt_irq_flags, "MD2_WDT", md);
	if (ret) {
		CCCI_ERROR_LOG(md->index, TAG,
			"request MD_WDT IRQ(%d) error %d\n",
			md->md_wdt_irq_id, ret);
		return;
	}
	/*to balance the first start */
	disable_irq_nosync(md->md_wdt_irq_id);
}

static int md_ccif_probe(struct platform_device *dev)
{
	struct ccci_modem *md;
	int md_id, ret;
	struct ccci_dev_cfg dev_cfg;
	struct md_hw_info *md_hw;

	/*Allocate modem hardware info structure memory */
	md_hw = kzalloc(sizeof(struct md_hw_info), GFP_KERNEL);
	if (md_hw == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"%s:alloc md hw mem fail\n", __func__);
		return -1;
	}

	ret = md_ccif_get_modem_hw_info(dev, &dev_cfg, md_hw);
	if (ret != 0) {
		CCCI_ERROR_LOG(-1, TAG,
			"%s:get hw info fail(%d)\n", __func__, ret);
		kfree(md_hw);
		md_hw = NULL;
		return -1;
	}

	if (!get_modem_is_enabled(dev_cfg.index)) {
		CCCI_ERROR_LOG(dev_cfg.index, TAG,
			"modem %d not enable\n", dev_cfg.index + 1);
		kfree(md_hw);
		md_hw = NULL;
		return -1;
	}

	/*Allocate md ctrl memory and do initialize */
	md = ccci_md_alloc(sizeof(struct md_sys3_info));
	if (md == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"%s:alloc modem ctrl mem fail\n", __func__);
		kfree(md_hw);
		md_hw = NULL;
		return -1;
	}

	md->index = md_id = dev_cfg.index;
	md->per_md_data.md_capability = dev_cfg.capability;
	md->plat_dev = dev;
	md->hw_info = md_hw;

	CCCI_INIT_LOG(md_id, TAG, "modem ccif module probe...\n");
	snprintf(md->trm_wakelock_name, sizeof(md->trm_wakelock_name),
		"md%d_ccif_trm", md->index + 1);
	md->trm_wake_lock = wakeup_source_register(NULL, md->trm_wakelock_name);
	if (!md->trm_wake_lock) {
		CCCI_ERROR_LOG(md_id, TAG,
			"%s %d: init wakeup source fail",
			__func__, __LINE__);
		return -1;
	}


	/*init modem structure */
	md->ops = &md_ccif_ops;
	CCCI_INIT_LOG(md_id, TAG,
		"%s:md_ccif=%p,md_ctrl=%p\n", __func__,
		md, md->private_data);

	/*register modem */
	ccci_md_register(md);

	/* init modem private data */
	md_ccif_hw_init(md);

	md->hif_flag = 1 << CCIF_HIF_ID;
	ccci_hif_init(md->index, md->hif_flag);

	/*hoop up to device */
	dev->dev.platform_data = md;

	return 0;
}

int md_ccif_remove(struct platform_device *dev)
{
	return 0;
}

void md_ccif_shutdown(struct platform_device *dev)
{
}

int md_ccif_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

int md_ccif_resume(struct platform_device *dev)
{
/*
 *	struct ccci_modem *md = (struct ccci_modem *)dev->dev.platform_data;
 *	struct md_ccif_ctrl *md_ctrl = (struct md_ccif_ctrl *)md->private_data;
 *
 *	CCCI_DEBUG_LOG(md->index, TAG,
 *  "md_ccif_resume,md=0x%p,md_ctrl=0x%p\n", md, md_ctrl);
 *	ccif_write32(md_ctrl->ccif_ap_base, APCCIF_CON, 0x01);
 */
	return 0;
}

int md_ccif_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	if (pdev == NULL) {
		CCCI_ERROR_LOG(MD_SYS3, TAG, "%s pdev == NULL\n", __func__);
		return -1;
	}
	return md_ccif_suspend(pdev, PMSG_SUSPEND);
}

int md_ccif_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	if (pdev == NULL) {
		CCCI_ERROR_LOG(MD_SYS3, TAG, "%s pdev == NULL\n", __func__);
		return -1;
	}
	return md_ccif_resume(pdev);
}

int md_ccif_pm_restore_noirq(struct device *device)
{
	int ret = 0;
	struct ccci_modem *md = (struct ccci_modem *)device->platform_data;

	CCCI_DEBUG_LOG(md->index, TAG, "%s\n", __func__);
	return ret;
}

#ifdef CONFIG_PM
static const struct dev_pm_ops md_ccif_pm_ops = {
	.suspend = md_ccif_pm_suspend,
	.resume = md_ccif_pm_resume,
	.freeze = md_ccif_pm_suspend,
	.thaw = md_ccif_pm_resume,
	.poweroff = md_ccif_pm_suspend,
	.restore = md_ccif_pm_resume,
	.restore_noirq = md_ccif_pm_restore_noirq,
};
#endif

static struct platform_driver modem_ccif_driver = {
	.driver = {
		   .name = "ccif_modem",
#ifdef CONFIG_PM
		   .pm = &md_ccif_pm_ops,
#endif
		   },
	.probe = md_ccif_probe,
	.remove = md_ccif_remove,
	.shutdown = md_ccif_shutdown,
	.suspend = md_ccif_suspend,
	.resume = md_ccif_resume,
};

#ifdef CONFIG_OF
static const struct of_device_id ccif_of_ids[] = {
	/*{.compatible = "mediatek,AP_CCIF1",}, */
	{.compatible = "mediatek,ap2c2k_ccif",},
	{}
};
#endif

static int __init md_ccif_init(void)
{
	int ret;

#ifdef CONFIG_OF
	modem_ccif_driver.driver.of_match_table = ccif_of_ids;
#endif

	ret = platform_driver_register(&modem_ccif_driver);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG,
			"CCIF modem platform driver register fail(%d)\n", ret);
		return ret;
	}
	CCCI_INIT_LOG(-1, TAG,
		"CCIF C2K modem platform driver register success\n");
	return 0;
}

module_init(md_ccif_init);

MODULE_AUTHOR("Yanbin Ren <Yanbin.Ren@mediatek.com>");
MODULE_DESCRIPTION("CCIF modem driver v0.1");
MODULE_LICENSE("GPL");
#endif
