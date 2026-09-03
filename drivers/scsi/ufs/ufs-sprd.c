// SPDX-License-Identifier: GPL-2.0-only
/*
 * UFS Host Controller driver for Unisoc specific extensions
 *
 * Copyright (C) 2022 Unisoc, Inc.
 *
 */

#include <linux/irqreturn.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <trace/hooks/ufshcd.h>
#include <linux/regulator/driver.h>
#include <../drivers/regulator/internal.h>
#include <linux/nvmem-consumer.h>

#include "ufs.h"
#include "ufshcd.h"
#include "ufshcd-pltfrm.h"
#include "ufs-sprd.h"
#include "ufs-sprd-ioctl.h"
#include "ufs-sprd-rpmb.h"
#include "ufs-sprd-bootdevice.h"
#include "ufs-sprd-debug.h"
#include "ufshpb.h"
#include "ufs-sprd-health-device.h"

int get_boot_mode(struct ufs_hba *hba)
{
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct device_node *cmdline_node;
	const char *cmd_line;
	int ret;

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmd_line);
	if (ret)
		return ret;

	if (strstr(cmd_line, "sprdboot.mode=cali"))
		host->cali_mode_enable = true;

	return 0;
}

int ufs_efuse_calib_data(struct platform_device *pdev,
				const char *cell_name)
{
	struct nvmem_cell *cell;
	void *buf;
	u32 calib_data;
	size_t len;

	if (!pdev)
		return -EINVAL;

	cell = nvmem_cell_get(&pdev->dev, cell_name);
	if (IS_ERR_OR_NULL(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	if (IS_ERR_OR_NULL(buf)) {
		nvmem_cell_put(cell);
		return PTR_ERR(buf);
	}

	memcpy(&calib_data, buf, min(len, sizeof(u32)));

	kfree(buf);
	nvmem_cell_put(cell);
	return calib_data;
}

void ufs_sprd_get_gic_reg(struct ufs_hba *hba)
{
	int ret;
	u32 regdata[4];
	u32 gic_reg_satrt;
	u32 gic_reg_size;
	struct device_node *gic_node;
	struct irq_desc *ufs_irq_desc;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	gic_node = of_find_compatible_node(NULL, NULL, "arm,gic-v3");
	if (!gic_node) {
		dev_err(hba->dev, "get gic node failed!\n");
		return;
	}

	ret = of_property_read_u32_array(gic_node, "reg", regdata, 4);
	if (ret < 0) {
		dev_err(hba->dev, "get gic reg failed!\n");
		return;
	}

	ufs_irq_desc = irq_to_desc(hba->irq);
	if (!ufs_irq_desc) {
		dev_err(hba->dev, "get ufs_irq_desc failed!\n");
		return;
	}

	/* gic enable addr = gic base addr + gic enable offset + hardware irq / 32 * 4 */
	gic_reg_satrt = regdata[1] + 0x100 + (ufs_irq_desc->irq_data.hwirq / 32) * 4;
	gic_reg_size = regdata[3];

	host->gic_reg_enable = ioremap(gic_reg_satrt, gic_reg_size);
	if (!host->gic_reg_enable)
		dev_err(hba->dev, "gic reg ioremap failed!\n");
}

void ufs_sprd_print_gic_reg(struct ufs_hba *hba)
{
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	if (host->gic_reg_enable) {
		dev_err(hba->dev,
			"gic:enable=0x%x gic:pending=0x%x gic:error=0x%x\n",
			readl(host->gic_reg_enable), readl(host->gic_reg_enable + 0x100),
			readl(host->gic_reg_enable + 0xd00));
	}
}

int ufs_sprd_get_syscon_reg(struct device_node *np, struct syscon_ufs *reg,
			    const char *name)
{
	struct regmap *regmap;
	u32 syscon_args[2];

	regmap = syscon_regmap_lookup_by_phandle_args(np, name, 2, syscon_args);
	if (IS_ERR(regmap)) {
		pr_err("read ufs syscon %s regmap fail\n", name);
		reg->regmap = NULL;
		reg->reg = 0x0;
		reg->mask = 0x0;
		return -EINVAL;
	}
	reg->regmap = regmap;
	reg->reg = syscon_args[0];
	reg->mask = syscon_args[1];

	return 0;
}

static void ufs_sprd_vh_prepare_command(void *data, struct ufs_hba *hba,
					struct request *rq,
					struct ufshcd_lrb *lrbp,
					int *err)
{
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	if (unlikely(host->ffu_is_process == TRUE))
		prepare_command_send_in_ffu_state(hba, lrbp, err);

	return;
}

static void ufs_sprd_vh_update_sdev(void *data, struct scsi_device *sdev)
{
	struct ufs_hba *hba = shost_priv(sdev->host);
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	if (sdev->lun < UFS_MAX_GENERAL_LUN)
		host->sdev_ufs_lu[sdev->lun] = sdev;

	/* Disable UFS fua to prevent write performance degradation */
	sdev->broken_fua = 1;

	/*
	 * The cpus_share_cache interface always return true on k515,
	 * causing blk_mq_complete_request to occur in the ufs_work
	 * context that calls it.
	 * The configuration flag QUEUE_FLAG_SAME_FORCE can make need_ipi return true
	 * and let complete happen on the corresponding cpu that sends the request.
	 */
	if (sdev->request_queue)
		blk_queue_flag_set(QUEUE_FLAG_SAME_FORCE, sdev->request_queue);
}

static void ufs_sprd_vh_send_uic_cmd(void *data, struct ufs_hba *hba,
				     struct uic_command *ucmd, int str)
{
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	if (host->priv_vh_send_uic)
		host->priv_vh_send_uic(hba, ucmd, str);

	ufs_sprd_uic_cmd_record(hba, ucmd, str);
}

static void ufs_sprd_vh_compl_cmd(void *data,
				struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	if (sprd_ufs_debug_is_supported(hba) == TRUE) {
		if (lrbp->cmd)
			ufshcd_common_trace(hba, UFS_TRACE_COMPLETED, &lrbp->task_tag);
		else if (lrbp->command_type == UTP_CMD_TYPE_DEV_MANAGE ||
			 lrbp->command_type == UTP_CMD_TYPE_UFS_STORAGE)
			ufshcd_common_trace(hba, UFS_TRACE_DEV_COMPLETED, &lrbp->task_tag);
	}

	ufs_sprd_perf_trace(hba, lrbp);
}

static void ufs_sprd_vh_send_tm_cmd(void *data, struct ufs_hba *hba,
				    int tag, int str)
{
	struct utp_task_req_desc *descp = &hba->utmrdl_base_addr[tag];
	struct ufs_tm_cmd_info tm_tmp = {};

	if (sprd_ufs_debug_is_supported(hba) == TRUE) {
		tm_tmp.lun = be32_to_cpu(descp->upiu_req.input_param1);
		tm_tmp.tag = be32_to_cpu(descp->upiu_req.input_param2);
		tm_tmp.tm_func = (be32_to_cpu(descp->upiu_req.req_header.dword_1) >> 16) & 0xff;
		if (str == UFS_TM_SEND) {
			ufshcd_common_trace(hba, UFS_TRACE_TM_SEND, &tm_tmp);
		} else {
			tm_tmp.ocs = le32_to_cpu(descp->header.dword_2) & MASK_OCS;
			tm_tmp.param1 = descp->upiu_rsp.output_param1;
			if (str == UFS_TM_ERR)
				ufshcd_common_trace(hba, UFS_TRACE_TM_ERR, &tm_tmp);
			else
				ufshcd_common_trace(hba, UFS_TRACE_TM_COMPLETED, &tm_tmp);
		}
	}
}

static void ufs_sprd_vh_check_int_errors(void *data,
					struct ufs_hba *hba,
					bool queue_eh_work)
{
	if (queue_eh_work && sprd_ufs_debug_is_supported(hba) == TRUE)
		ufshcd_common_trace(hba, UFS_TRACE_INT_ERROR, NULL);

	if (queue_eh_work)
		ufshcd_vops_dbg_register_dump(hba);
}

static void ufs_sprd_vh_send_cmd(void *data,
				  struct ufs_hba *hba,
				  struct ufshcd_lrb *lrbp)
{
	struct utp_transfer_req_desc *req_desc;
	u32 data_direction;
	u32 dword_0, crypto;
	unsigned char *cdb = NULL;
	struct scsi_cmnd *cmd = lrbp->cmd;
	struct utp_upiu_req *ucd_req_ptr = lrbp->ucd_req_ptr;
	unsigned short cdb_len;

	req_desc = lrbp->utr_descriptor_ptr;
	dword_0 = le32_to_cpu(req_desc->header.dword_0);
	data_direction = dword_0 & (UTP_DEVICE_TO_HOST | UTP_HOST_TO_DEVICE);
	crypto = dword_0 & UTP_REQ_DESC_CRYPTO_ENABLE_CMD;
	if (!data_direction && crypto) {
		dword_0 &= ~(UTP_REQ_DESC_CRYPTO_ENABLE_CMD);
		req_desc->header.dword_0 = cpu_to_le32(dword_0);
	}

	if (lrbp->cmd) {
		cdb = lrbp->cmd->cmnd;
		if (cdb && cdb[0] == UFSHPB_READ) {
			cdb[0] = READ_16;
			cdb[15] = cdb[14];
			cdb[14] = 0;
			cdb[1] = 0x0;
			cdb_len = min_t(unsigned short, cmd->cmd_len, UFS_CDB_SIZE);
			memset(ucd_req_ptr->sc.cdb, 0, UFS_CDB_SIZE);
			memcpy(ucd_req_ptr->sc.cdb, cmd->cmnd, cdb_len);
		}
	}

	if (sprd_ufs_debug_is_supported(hba) == TRUE) {
		if (!!lrbp->cmd)
			ufshcd_common_trace(hba, UFS_TRACE_SEND, &lrbp->task_tag);
		else
			ufshcd_common_trace(hba, UFS_TRACE_DEV_SEND, &lrbp->task_tag);
	}
}


static const struct of_device_id ufs_sprd_of_match[] = {
	{ .compatible = "sprd,ufshc-ums9620", .data = &ufs_hba_sprd_ums9620_vops },
	{ .compatible = "sprd,ufshc-ums9621", .data = &ufs_hba_sprd_ums9621_vops },
	{ .compatible = "sprd,ufshc-ums9230", .data = &ufs_hba_sprd_ums9230_vops },
	{},
};

static int ufs_sprd_probe(struct platform_device *pdev)
{
	int err;
	struct device *dev = &pdev->dev;
	struct ufs_hba *hba;
	const struct of_device_id *of_id;

	register_trace_android_vh_ufs_prepare_command(ufs_sprd_vh_prepare_command, NULL);
	register_trace_android_vh_ufs_update_sdev(ufs_sprd_vh_update_sdev, NULL);
	register_trace_android_vh_ufs_send_uic_command(ufs_sprd_vh_send_uic_cmd, NULL);
	register_trace_android_vh_ufs_compl_command(ufs_sprd_vh_compl_cmd, NULL);
	register_trace_android_vh_ufs_send_tm_command(ufs_sprd_vh_send_tm_cmd, NULL);
	register_trace_android_vh_ufs_check_int_errors(ufs_sprd_vh_check_int_errors, NULL);
	register_trace_android_vh_ufs_send_command(ufs_sprd_vh_send_cmd, NULL);

	/* Perform generic probe */
	of_id = of_match_node(ufs_sprd_of_match, pdev->dev.of_node);
	err = ufshcd_pltfrm_init(pdev, of_id->data);
	if (err) {
		dev_err(dev, "ufshcd_pltfrm_init() failed %d\n", err);
		goto out;
	}

	hba = platform_get_drvdata(pdev);
	ufs_sprd_rpmb_add(hba);
	sprd_ufs_proc_init(hba);
	ufs_sprd_sysfs_add_health_device_nodes(hba);
	ufs_sprd_sysfs_add_nodes(hba);
	ufs_sprd_perf_work_init(hba);
out:
	return err;
}

static void ufs_sprd_shutdown(struct platform_device *pdev)
{
	struct ufs_hba *hba =  platform_get_drvdata(pdev);

	if (ufs_perf && ufs_perf->work_initialized)
		cancel_work_sync(&ufs_perf->perf_work);
	sprd_ufs_proc_exit();
	ufs_sprd_rpmb_remove(hba);
	ufshcd_pltfrm_shutdown(pdev);
}

static int ufs_sprd_remove(struct platform_device *pdev)
{
	struct ufs_hba *hba =  platform_get_drvdata(pdev);

	if (ufs_perf && ufs_perf->work_initialized)
		cancel_work_sync(&ufs_perf->perf_work);
	pm_runtime_get_sync(&(pdev)->dev);
	sprd_ufs_proc_exit();
	ufs_sprd_rpmb_remove(hba);
	ufshcd_remove(hba);
	return 0;
}

static int ufs_sprd_system_resume(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	bool vcc_state = hba->vreg_info.vcc->enabled;
	bool vcc_aon = hba->vreg_info.vcc->reg->always_on;
	int ret;

	ret = ufshcd_system_resume(dev);
	if (ret)
		return ret;

	/* Makesure UFS VCC power up */
	if (!vcc_aon && vcc_state == false && hba->vreg_info.vcc->enabled == true)
		usleep_range(100, 200);

	return 0;
}

static int ufs_sprd_system_suspend(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	bool vcc_aon = hba->vreg_info.vcc->reg->always_on;
	int ret;

	if (ufs_perf && ufs_perf->work_initialized)
		flush_work(&ufs_perf->perf_work);

	if (host->check_stat_after_suspend) {
		ret = host->check_stat_after_suspend(hba, PRE_CHANGE);
		if (ret)
			return ret;
	}

	ret = ufshcd_system_suspend(dev);
	if (ret)
		return ret;

	/* Makesure that VCC drop below 0.1V after turning off */
	if (!vcc_aon && hba->vreg_info.vcc->enabled == false)
		usleep_range(30000, 31000);

	/* Check whether HC is abnormal after EE H8 */
	if (host->check_stat_after_suspend) {
		ret = host->check_stat_after_suspend(hba, POST_CHANGE);
		if (ret) {
			/* Recovery from bad status, retry suspend */
			ufs_sprd_system_resume(dev);
			return ret;
		}
	}

	return 0;
}

static int ufs_sprd_runtime_suspend(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	bool vcc_aon = hba->vreg_info.vcc->reg->always_on;
	int ret;

	ret = ufshcd_runtime_suspend(dev);
	if (ret)
		return ret;

	/* Makesure that VCC drop below 0.1V after turning off */
	if (!vcc_aon && hba->vreg_info.vcc->enabled == false)
		usleep_range(30000, 31000);

	return 0;
}

static int ufs_sprd_runtime_resume(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	bool vcc_state = hba->vreg_info.vcc->enabled;
	bool vcc_aon = hba->vreg_info.vcc->reg->always_on;
	int ret;

	ret = ufshcd_runtime_resume(dev);
	if (ret)
		return ret;

	/* Makesure UFS VCC power up */
	if (!vcc_aon && vcc_state == false && hba->vreg_info.vcc->enabled == true)
		usleep_range(100, 200);

	return 0;
}

static const struct dev_pm_ops ufs_sprd_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ufs_sprd_system_suspend, ufs_sprd_system_resume)
	SET_RUNTIME_PM_OPS(ufs_sprd_runtime_suspend, ufs_sprd_runtime_resume, NULL)
	.prepare	 = ufshcd_suspend_prepare,
	.complete	 = ufshcd_resume_complete,
};

static struct platform_driver ufs_sprd_pltform = {
	.probe = ufs_sprd_probe,
	.remove = ufs_sprd_remove,
	.shutdown = ufs_sprd_shutdown,
	.driver = {
		.name = "ufshcd-sprd",
		.pm = &ufs_sprd_pm_ops,
		.of_match_table = of_match_ptr(ufs_sprd_of_match),
	},
};
module_platform_driver(ufs_sprd_pltform);

MODULE_DESCRIPTION("SPRD Specific UFSHCI driver");
MODULE_LICENSE("GPL v2");
