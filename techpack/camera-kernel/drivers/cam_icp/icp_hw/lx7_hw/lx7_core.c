// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#include <linux/of_address.h>
#include <linux/qcom_scm.h>
#include <linux/soc/qcom/mdt_loader.h>

#include "cam_cpas_api.h"
#include "cam_debug_util.h"
#include "cam_hw.h"
#include "cam_hw_intf.h"
#include "cam_icp_hw_mgr_intf.h"
#include "cam_icp_hw_intf.h"
#include "hfi_intf.h"
#include "hfi_sys_defs.h"
#include "cam_icp_utils.h"
#include "lx7_core.h"
#include "lx7_reg.h"
#include "lx7_soc.h"
#include "cam_common_util.h"

#define TZ_STATE_SUSPEND 0
#define TZ_STATE_RESUME  1

#define LX7_GEN_PURPOSE_REG_OFFSET 0x20

#define ICP_FW_NAME_MAX_SIZE    32

#define PC_POLL_DELAY_US 100
#define PC_POLL_TIMEOUT_US 10000

static int cam_lx7_ubwc_configure(struct cam_hw_soc_info *soc_info)
{
	int i = 0, rc, ddr_type;
	struct lx7_soc_info *soc_priv;
	uint32_t ipe_ubwc_cfg[UBWC_CONFIG_MAX];
	uint32_t bps_ubwc_cfg[UBWC_CONFIG_MAX];

	if (!soc_info || !soc_info->soc_private) {
		CAM_ERR(CAM_ICP, "invalid LX7 soc info");
		return -EINVAL;
	}

	soc_priv = soc_info->soc_private;

	ddr_type = of_fdt_get_ddrtype();
	if (ddr_type == DDR_TYPE_LPDDR5 || ddr_type == DDR_TYPE_LPDDR5X)
		i = 1;

	ipe_ubwc_cfg[0] = soc_priv->ubwc_cfg.ipe_fetch[i];
	ipe_ubwc_cfg[1] = soc_priv->ubwc_cfg.ipe_write[i];

	bps_ubwc_cfg[0] = soc_priv->ubwc_cfg.bps_fetch[i];
	bps_ubwc_cfg[1] = soc_priv->ubwc_cfg.bps_write[i];

	rc = hfi_cmd_ubwc_config_ext(ipe_ubwc_cfg, bps_ubwc_cfg);
	if (rc)	{
		CAM_ERR(CAM_ICP, "failed to write UBWC config rc=%d", rc);
		return rc;
	}

	return 0;
}

static int cam_lx7_cpas_vote(struct cam_lx7_core_info *core_info,
			struct cam_icp_cpas_vote *vote)
{
	int rc;

	if (!core_info || !vote)
		return -EINVAL;

	if (vote->ahb_vote_valid) {
		rc = cam_cpas_update_ahb_vote(core_info->cpas_handle,
					&vote->ahb_vote);
		if (rc) {
			CAM_ERR(CAM_ICP, "AHB vote update failed rc=%d", rc);
			return rc;
		}
	}

	if (vote->axi_vote_valid) {
		rc = cam_cpas_update_axi_vote(core_info->cpas_handle,
					&vote->axi_vote);
		if (rc) {
			CAM_ERR(CAM_ICP, "AXI vote update failed rc=%d", rc);
			return rc;
		}
	}

	return 0;
}

static bool cam_lx7_cpas_cb(uint32_t handle, void *user_data,
			struct cam_cpas_irq_data *irq_data)
{
	bool ret = false;
	(void)user_data;

	if (!irq_data)
		return false;

	switch (irq_data->irq_type) {
	case CAM_CAMNOC_IRQ_IPE_BPS_UBWC_DECODE_ERROR:
		CAM_ERR_RATE_LIMIT(CAM_ICP,
				"IPE/BPS UBWC decode error status=0x%08x",
				irq_data->u.dec_err.decerr_status.value);
		ret = true;
	case CAM_CAMNOC_IRQ_IPE_BPS_UBWC_ENCODE_ERROR:
		CAM_ERR_RATE_LIMIT(CAM_ICP,
				"IPE/BPS UBWC encode error status=0x%08x",
				irq_data->u.enc_err.encerr_status.value);
		ret = true;
	default:
		CAM_ERR(CAM_ICP, "unhandled irq_type=%d", irq_data->irq_type);
		break;
	}

	return ret;
}

int cam_lx7_cpas_register(struct cam_hw_intf *lx7_intf)
{
	struct cam_cpas_register_params params;
	struct cam_hw_info *lx7_info;
	struct cam_lx7_core_info *core_info;
	int rc;

	if (!lx7_intf)
		return -EINVAL;

	lx7_info = lx7_intf->hw_priv;

	params.dev = lx7_info->soc_info.dev;
	params.cell_index = lx7_intf->hw_idx;
	params.cam_cpas_client_cb = cam_lx7_cpas_cb;
	params.userdata = NULL;

	strlcpy(params.identifier, "icp", CAM_HW_IDENTIFIER_LENGTH);

	rc = cam_cpas_register_client(&params);
	if (rc)
		return rc;

	core_info = lx7_info->core_info;
	core_info->cpas_handle = params.client_handle;

	return rc;
}

int cam_lx7_cpas_unregister(struct cam_hw_intf *lx7_intf)
{
	struct cam_hw_info *lx7_info;
	struct cam_lx7_core_info *core_info;

	if (!lx7_intf)
		return -EINVAL;

	lx7_info = lx7_intf->hw_priv;
	core_info = lx7_info->core_info;

	return cam_cpas_unregister_client(core_info->cpas_handle);
}

static int __lx7_cpas_start(struct cam_lx7_core_info *core_info,
			struct cam_icp_cpas_vote *vote)
{
	int rc;

	if (!core_info || core_info->cpas_start)
		return -EINVAL;

	rc = cam_cpas_start(core_info->cpas_handle,
			&vote->ahb_vote, &vote->axi_vote);
	if (rc) {
		CAM_ERR(CAM_ICP, "failed to start cpas rc=%d", rc);
		return rc;
	}

	core_info->cpas_start = true;

	return 0;
}

static int cam_lx7_cpas_start(struct cam_lx7_core_info *core_info)
{
	struct cam_icp_cpas_vote vote;

	vote.ahb_vote.type = CAM_VOTE_ABSOLUTE;
	vote.ahb_vote.vote.level = CAM_LOWSVS_VOTE;
	vote.axi_vote.num_paths = 1;

	vote.axi_vote.axi_path[0].path_data_type = CAM_ICP_DEFAULT_AXI_PATH;
	vote.axi_vote.axi_path[0].transac_type = CAM_ICP_DEFAULT_AXI_TRANSAC;
	vote.axi_vote.axi_path[0].camnoc_bw = CAM_ICP_BW_BYTES_VOTE;
	vote.axi_vote.axi_path[0].mnoc_ab_bw = CAM_ICP_BW_BYTES_VOTE;
	vote.axi_vote.axi_path[0].mnoc_ib_bw = CAM_ICP_BW_BYTES_VOTE;
	vote.axi_vote.axi_path[0].ddr_ab_bw = CAM_ICP_BW_BYTES_VOTE;
	vote.axi_vote.axi_path[0].ddr_ib_bw = CAM_ICP_BW_BYTES_VOTE;

	return __lx7_cpas_start(core_info, &vote);
}

static int cam_lx7_cpas_stop(struct cam_lx7_core_info *core_info)
{
	int rc;

	if (!core_info || !core_info->cpas_start)
		return -EINVAL;

	rc = cam_cpas_stop(core_info->cpas_handle);
	if (rc) {
		CAM_ERR(CAM_ICP, "failed to stop cpas rc=%d", rc);
		return rc;
	}

	core_info->cpas_start = false;

	return 0;
}

int cam_lx7_hw_init(void *priv, void *args, uint32_t arg_size)
{
	struct cam_hw_info *lx7 = priv;
	unsigned long flags;
	int rc;

	if (!lx7) {
		CAM_ERR(CAM_ICP, "LX7 device info cannot be NULL");
		return -EINVAL;
	}

	spin_lock_irqsave(&lx7->hw_lock, flags);
	if (lx7->hw_state == CAM_HW_STATE_POWER_UP) {
		spin_unlock_irqrestore(&lx7->hw_lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&lx7->hw_lock, flags);

	rc = cam_lx7_cpas_start(lx7->core_info);
	if (rc)
		return rc;

	rc = cam_lx7_soc_resources_enable(&lx7->soc_info);
	if (rc) {
		CAM_ERR(CAM_ICP, "failed to enable soc resources rc=%d", rc);
		goto soc_fail;
	}

	spin_lock_irqsave(&lx7->hw_lock, flags);
	lx7->hw_state = CAM_HW_STATE_POWER_UP;
	spin_unlock_irqrestore(&lx7->hw_lock, flags);

	return 0;

soc_fail:
	cam_lx7_cpas_stop(lx7->core_info);
	return rc;
}

int cam_lx7_hw_deinit(void *priv, void *args, uint32_t arg_size)
{
	struct cam_hw_info *lx7_info = priv;
	unsigned long flags;
	int rc;

	if (!lx7_info) {
		CAM_ERR(CAM_ICP, "LX7 device info cannot be NULL");
		return -EINVAL;
	}

	spin_lock_irqsave(&lx7_info->hw_lock, flags);
	if (lx7_info->hw_state == CAM_HW_STATE_POWER_DOWN) {
		spin_unlock_irqrestore(&lx7_info->hw_lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&lx7_info->hw_lock, flags);

	rc = cam_lx7_soc_resources_disable(&lx7_info->soc_info);
	if (rc)
		CAM_WARN(CAM_ICP,
			"failed to disable soc resources rc=%d", rc);

	rc = cam_lx7_cpas_stop(lx7_info->core_info);
	if (rc)
		CAM_WARN(CAM_ICP, "cpas stop failed rc=%d", rc);

	spin_lock_irqsave(&lx7_info->hw_lock, flags);
	lx7_info->hw_state = CAM_HW_STATE_POWER_DOWN;
	spin_unlock_irqrestore(&lx7_info->hw_lock, flags);

	return rc;
}

static void prepare_boot(struct cam_hw_info *lx7_info,
			struct cam_icp_boot_args *args)
{
	struct cam_lx7_core_info *core_info = lx7_info->core_info;
	unsigned long flags;

	if (!args->use_sec_pil) {
		core_info->fw_params.fw_buf = args->firmware.iova;
		core_info->fw_params.fw_kva_addr = args->firmware.kva;
		core_info->fw_params.fw_buf_len = args->firmware.len;
	}

	spin_lock_irqsave(&lx7_info->hw_lock, flags);
	core_info->irq_cb.data = args->irq_cb.data;
	core_info->irq_cb.cb = args->irq_cb.cb;
	spin_unlock_irqrestore(&lx7_info->hw_lock, flags);
}

static void prepare_shutdown(struct cam_hw_info *lx7_info)
{
	struct cam_lx7_core_info *core_info = lx7_info->core_info;
	unsigned long flags;

	core_info->fw_params.fw_buf = 0x0;
	core_info->fw_params.fw_kva_addr = 0x0;
	core_info->fw_params.fw_buf_len = 0x0;

	spin_lock_irqsave(&lx7_info->hw_lock, flags);
	core_info->irq_cb.data = NULL;
	core_info->irq_cb.cb = NULL;
	spin_unlock_irqrestore(&lx7_info->hw_lock, flags);
}

/* Used if ICP_SYS is not protected */
static int __cam_lx7_power_collapse(struct cam_hw_info *lx7_info)
{
	uint32_t status = 0;
	void __iomem *base;

	if (!lx7_info) {
		CAM_ERR(CAM_ICP, "invalid lx7 dev info");
		return -EINVAL;
	}

	base = lx7_info->soc_info.reg_map[LX7_SYS_BASE].mem_base;

	/**
	 * Need to poll here to confirm that FW has triggered WFI
	 * and Host can then proceed. No interrupt is expected
	 * from FW at this time.
	 */
	if (cam_common_read_poll_timeout(base + ICP_LX7_SYS_STATUS,
		PC_POLL_DELAY_US, PC_POLL_TIMEOUT_US,
		ICP_LX7_STANDBYWFI, ICP_LX7_STANDBYWFI,
		&status)) {
		CAM_ERR(CAM_ICP, "WFI poll timed out: status=0x%08x", status);
		return -ETIMEDOUT;
	}

	cam_io_w_mb(0x0, base + ICP_LX7_SYS_CONTROL);
	return 0;
}

/* Used if ICP_SYS is not protected */
static int __cam_lx7_power_resume(struct cam_hw_info *lx7_info)
{
	void __iomem *base;
	struct lx7_soc_info    *soc_priv;
	struct cam_hw_soc_info *soc_info;

	if (!lx7_info) {
		CAM_ERR(CAM_ICP, "invalid lx7 dev info");
		return -EINVAL;
	}

	soc_info = &lx7_info->soc_info;
	soc_priv = soc_info->soc_private;
	base = lx7_info->soc_info.reg_map[LX7_SYS_BASE].mem_base;

	cam_io_w_mb(ICP_LX7_FUNC_RESET,
		base + ICP_LX7_SYS_RESET);

	if (soc_priv->icp_qos_val)
		cam_io_w_mb(soc_priv->icp_qos_val,
			base + ICP_LX7_SYS_ACCESS);

	cam_io_w_mb(ICP_LX7_EN_CPU,
		base + ICP_LX7_SYS_CONTROL);

	return 0;
}

/* Used for non secure FW load */
static int32_t __cam_non_sec_load_fw(void *device_priv)
{
	int32_t rc = 0;
	uint32_t fw_size;
	char firmware_name[ICP_FW_NAME_MAX_SIZE] = {0};
	const char               *fw_name;
	const uint8_t            *fw_start = NULL;
	struct cam_hw_info       *lx7_dev = device_priv;
	struct cam_hw_soc_info   *soc_info = NULL;
	struct cam_lx7_core_info *core_info = NULL;
	struct platform_device   *pdev = NULL;

	if (!device_priv) {
		CAM_ERR(CAM_ICP, "Invalid cam_dev_info");
		return -EINVAL;
	}

	soc_info = &lx7_dev->soc_info;
	core_info = (struct cam_lx7_core_info *)lx7_dev->core_info;
	pdev = soc_info->pdev;

	/**
	 * Use paddr to map 0xE0400000 and 0xE0420000 as these
	 * addresses are routed internally by the core. These segments
	 * are used by the firmware to make use of the rom packing feature.
	 */

	rc = of_property_read_string(pdev->dev.of_node, "fw_name",
		&fw_name);
	if (rc) {
		CAM_ERR(CAM_ICP, "FW image name not found");
		return rc;
	}

	/* Account for ".elf" size [4 characters] */
	if (strlen(fw_name) >= (ICP_FW_NAME_MAX_SIZE - 4)) {
		CAM_ERR(CAM_ICP, "Invalid fw name %s", fw_name);
		return -EINVAL;
	}

	scnprintf(firmware_name, ARRAY_SIZE(firmware_name),
		"%s.elf", fw_name);

	rc = firmware_request_nowarn(&core_info->fw_params.fw_elf,
		firmware_name, &pdev->dev);
	if (rc) {
		CAM_ERR(CAM_ICP, "Failed to locate %s fw: %d",
			firmware_name, rc);
		return rc;
	}

	if (!core_info->fw_params.fw_elf) {
		CAM_ERR(CAM_ICP, "Invalid elf size");
		rc = -EINVAL;
		goto fw_download_failed;
	}

	fw_start = core_info->fw_params.fw_elf->data;

	rc = cam_icp_validate_fw(fw_start, EM_XTENSA);
	if (rc) {
		CAM_ERR(CAM_ICP, "fw elf validation failed");
		goto fw_download_failed;
	}

	rc = cam_icp_get_fw_size(fw_start, &fw_size);
	if (rc) {
		CAM_ERR(CAM_ICP, "unable to get fw size");
		goto fw_download_failed;
	}

	if (core_info->fw_params.fw_buf_len < fw_size) {
		CAM_ERR(CAM_ICP, "mismatch in fw size: %u %llu",
			fw_size, core_info->fw_params.fw_buf_len);
		rc = -EINVAL;
		goto fw_download_failed;
	}

	rc = cam_icp_program_fw(fw_start,
		core_info->fw_params.fw_kva_addr);
	if (rc) {
		CAM_ERR(CAM_ICP, "fw program is failed");
		goto fw_download_failed;
	}

fw_download_failed:
	release_firmware(core_info->fw_params.fw_elf);
	return rc;
}

/* Used for non secure FW load */
static int cam_lx7_non_sec_boot(
	struct cam_hw_info *lx7_info,
	struct cam_icp_boot_args *args,
	uint32_t arg_size)
{
	int rc;

	if (!lx7_info || !args) {
		CAM_ERR(CAM_ICP,
			"invalid args: lx7_dev=%pK args=%pK",
			lx7_info, args);
		return -EINVAL;
	}

	if (arg_size != sizeof(struct cam_icp_boot_args)) {
		CAM_ERR(CAM_ICP, "invalid boot args size");
		return -EINVAL;
	}

	if (lx7_info->soc_info.num_mem_block != LX7_BASE_MAX) {
		CAM_ERR(CAM_ICP, "check ICP SYS reg config in DT..");
		return -EINVAL;
	}

	prepare_boot(lx7_info, args);

	rc = __cam_non_sec_load_fw(lx7_info);
	if (rc) {
		CAM_ERR(CAM_ICP,
			"firmware download failed rc=%d", rc);
		goto err;
	}

	rc = __cam_lx7_power_resume(lx7_info);
	if (rc) {
		CAM_ERR(CAM_ICP, "lx7 boot failed rc=%d", rc);
		goto err;
	}

	return 0;

err:
	prepare_shutdown(lx7_info);
	return rc;
}

#if IS_REACHABLE(CONFIG_QCOM_MDT_LOADER)
static int __load_firmware(struct platform_device *pdev)
{
	const char *fw_name;
	const struct firmware *firmware = NULL;
	char firmware_name[ICP_FW_NAME_MAX_SIZE] = {0};
	void *vaddr = NULL;
	struct device_node *node;
	struct resource res;
	phys_addr_t res_start;
	size_t res_size;
	ssize_t fw_size;
	int rc;

	if (!pdev) {
		CAM_ERR(CAM_ICP, "invalid args");
		return -EINVAL;
	}

	rc = of_property_read_string(pdev->dev.of_node, "fw_name",
		&fw_name);
	if (rc) {
		CAM_ERR(CAM_ICP, "FW image name not found");
		return rc;
	}

	/* Account for ".mdt" size [4 characters] */
	if (strlen(fw_name) >= (ICP_FW_NAME_MAX_SIZE - 4)) {
		CAM_ERR(CAM_ICP, "Invalid fw name %s", fw_name);
		return -EINVAL;
	}

	scnprintf(firmware_name, ARRAY_SIZE(firmware_name),
		"%s.mdt", fw_name);

	node = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!node) {
		CAM_ERR(CAM_ICP, "firmware memory region not found");
		return -ENODEV;
	}

	rc = of_address_to_resource(node, 0, &res);
	of_node_put(node);
	if (rc) {
		CAM_ERR(CAM_ICP, "missing firmware resource address rc=%d", rc);
		return rc;
	}

	res_start = res.start;
	res_size = (size_t)resource_size(&res);

	rc = firmware_request_nowarn(&firmware, firmware_name, &pdev->dev);
	if (rc) {
		CAM_ERR(CAM_ICP,
			"error requesting %s firmware rc=%d",
			firmware_name, rc);
		return rc;
	}

	/* Make sure carveout and binary sizes are compatible */
	fw_size = qcom_mdt_get_size(firmware);
	if (fw_size < 0 || res_size < (size_t)fw_size) {
		CAM_ERR(CAM_ICP,
			"carveout[sz=%zu] not big enough for firmware[sz=%zd]",
			res_size, fw_size);
		rc = -EINVAL;
		goto out;
	}

	vaddr = ioremap_wc(res_start, res_size);
	if (!vaddr) {
		CAM_ERR(CAM_ICP, "unable to map firmware carveout");
		rc = -ENOMEM;
		goto out;
	}

	rc = qcom_mdt_load(&pdev->dev, firmware, firmware_name, CAM_FW_PAS_ID,
			vaddr, res_start, res_size, NULL);
	if (rc) {
		CAM_ERR(CAM_ICP, "failed to load firmware rc=%d", rc);
		goto out;
	}

out:
	if (vaddr)
		iounmap(vaddr);

	release_firmware(firmware);
	return rc;
}
#endif

static int cam_lx7_boot(struct cam_hw_info *lx7_info,
			struct cam_icp_boot_args *args,
			uint32_t arg_size)
{
	int rc;
	struct cam_lx7_core_info *core_info = NULL;

	if (!IS_REACHABLE(CONFIG_QCOM_MDT_LOADER))
		return -EOPNOTSUPP;

	if (!lx7_info || !args) {
		CAM_ERR(CAM_ICP,
			"invalid args: lx7_info=%pK args=%pK",
			lx7_info, args);
		return -EINVAL;
	}

	if (arg_size != sizeof(struct cam_icp_boot_args)) {
		CAM_ERR(CAM_ICP, "invalid boot args size");
		return -EINVAL;
	}

	core_info = (struct cam_lx7_core_info *)lx7_info->core_info;
	prepare_boot(lx7_info, args);

#if IS_REACHABLE(CONFIG_QCOM_MDT_LOADER)
	rc = __load_firmware(lx7_info->soc_info.pdev);
	if (rc) {
		CAM_ERR(CAM_ICP, "firmware loading failed rc=%d", rc);
		goto err;
	}
#endif

	rc = qcom_scm_pas_auth_and_reset(CAM_FW_PAS_ID);
	if (rc) {
		CAM_ERR(CAM_ICP, "auth and reset failed rc=%d", rc);
		goto err;
	}

	core_info->use_sec_pil = true;
	return 0;
err:
	prepare_shutdown(lx7_info);
	return rc;
}

static int cam_lx7_shutdown(struct cam_hw_info *lx7_info)
{
	int rc = 0;
	struct cam_lx7_core_info *core_info =
		(struct cam_lx7_core_info *)lx7_info->core_info;

	prepare_shutdown(lx7_info);

	if (core_info->use_sec_pil)
		rc = qcom_scm_pas_shutdown(CAM_FW_PAS_ID);
	else {
		void __iomem *base;

		base = lx7_info->soc_info.reg_map[LX7_SYS_BASE].mem_base;
		cam_io_w_mb(0x0, base + ICP_LX7_SYS_CONTROL);
	}

	core_info->use_sec_pil = false;
	return rc;
}

/* API controls collapse/resume of ICP */
static int cam_lx7_core_control(
	struct cam_hw_info *lx7_info,
	uint32_t state)
{
	int rc = 0;
	struct cam_lx7_core_info *core_info =
		(struct cam_lx7_core_info *)lx7_info->core_info;

	if (core_info->use_sec_pil) {
		rc = qcom_scm_set_remote_state(state, CAM_FW_PAS_ID);
		if (rc)
			CAM_ERR(CAM_ICP,
				"remote state set to %s failed rc=%d IB_status0=0x%x IB_Status1=0x%x PFault=0x%x",
				state == TZ_STATE_RESUME ? "resume" : "suspend", rc,
				cam_io_r_mb(lx7_info->soc_info.reg_map[LX7_CIRQ_BASE].mem_base +
					ICP_LX7_CIRQ_IB_STATUS0),
				cam_io_r_mb(lx7_info->soc_info.reg_map[LX7_CIRQ_BASE].mem_base +
					ICP_LX7_CIRQ_IB_STATUS1),
				cam_io_r_mb(lx7_info->soc_info.reg_map[LX7_CIRQ_BASE].mem_base +
					ICP_LX7_CIRQ_PFAULT_INFO));
	} else {
		if (state == TZ_STATE_RESUME) {
			rc = __cam_lx7_power_resume(lx7_info);
			if (rc)
				CAM_ERR(CAM_ICP, "lx7 resume failed rc=%d", rc);
		} else {
			rc = __cam_lx7_power_collapse(lx7_info);
			if (rc)
				CAM_ERR(CAM_ICP, "lx7 collapse failed rc=%d", rc);
		}
	}

	return rc;
}

static inline int cam_lx7_download_fw(
	struct cam_hw_info *lx7_info,
	struct cam_icp_boot_args *args,
	uint32_t arg_size)
{
	int rc;

	if (args->use_sec_pil)
		rc = cam_lx7_boot(
			lx7_info, args, arg_size);
	else
		rc = cam_lx7_non_sec_boot(
			lx7_info, args, arg_size);

	return rc;
}

static int __cam_lx7_update_clk_rate(
	struct cam_hw_info *lx7_info,
	int32_t *clk_lvl)
{
	int32_t clk_level = 0, rc;
	struct cam_ahb_vote       ahb_vote;
	struct cam_lx7_core_info *core_info = NULL;
	struct cam_hw_soc_info   *soc_info = NULL;

	if (!clk_lvl) {
		CAM_ERR(CAM_ICP, "Invalid args");
		return -EINVAL;
	}

	soc_info = &lx7_info->soc_info;
	core_info = lx7_info->core_info;
	if (!core_info || !soc_info) {
		CAM_ERR(CAM_ICP, "Invalid args");
		return -EINVAL;
	}

	clk_level = *((int32_t *)clk_lvl);
	CAM_DBG(CAM_ICP,
		"Update ICP clock to level [%d]", clk_level);
	rc = cam_lx7_update_clk_rate(soc_info, clk_level);
	if (rc)
		CAM_WARN(CAM_ICP,
			"Failed to update clk to level: %d rc: %d",
			clk_level, rc);

	ahb_vote.type = CAM_VOTE_ABSOLUTE;
	ahb_vote.vote.level = clk_level;
	rc = cam_cpas_update_ahb_vote(
		core_info->cpas_handle, &ahb_vote);
	if (rc)
		CAM_WARN(CAM_ICP,
			"Failed to update ahb vote rc: %d", rc);

	return rc;
}

static int __cam_lx7_fw_mini_dump(
	struct cam_lx7_core_info       *core_info,
	struct cam_icp_hw_dump_args    *args)
{
	u8                          *dest;
	u8                          *src;
	struct cam_icp_hw_dump_args *dump_args = args;

	if (!core_info || !dump_args) {
		CAM_ERR(CAM_ICP, "invalid params %pK %pK", core_info, dump_args);
		return -EINVAL;
	}

	if (!core_info->fw_params.fw_kva_addr || !dump_args->cpu_addr) {
		CAM_ERR(CAM_ICP, "invalid params %pK, 0x%zx",
		    core_info->fw_params.fw_kva_addr, dump_args->cpu_addr);
		return -EINVAL;
	}

	if (dump_args->buf_len < core_info->fw_params.fw_buf_len) {
		CAM_WARN(CAM_ICP, "Insufficient Len %lu fw_len %llu",
			dump_args->buf_len, core_info->fw_params.fw_buf_len);
		return -ENOSPC;
	}

	dest = (u8 *)dump_args->cpu_addr;
	src = (u8 *)core_info->fw_params.fw_kva_addr;
	memcpy_fromio(dest, src, core_info->fw_params.fw_buf_len);
	dump_args->offset = core_info->fw_params.fw_buf_len;
	return 0;
}

int cam_lx7_process_cmd(void *priv, uint32_t cmd_type,
			void *args, uint32_t arg_size)
{
	struct cam_hw_info *lx7_info = priv;
	int rc = -EINVAL;

	if (!lx7_info) {
		CAM_ERR(CAM_ICP, "LX7 device info cannot be NULL");
		return -EINVAL;
	}

	switch (cmd_type) {
	case CAM_ICP_CMD_PROC_SHUTDOWN:
		rc = cam_lx7_shutdown(lx7_info);
		break;
	case CAM_ICP_CMD_PROC_BOOT:
		rc = cam_lx7_download_fw(lx7_info, args, arg_size);
		break;
	case CAM_ICP_CMD_POWER_COLLAPSE:
		rc = cam_lx7_core_control(lx7_info, TZ_STATE_SUSPEND);
		break;
	case CAM_ICP_CMD_POWER_RESUME:
		rc = cam_lx7_core_control(lx7_info, TZ_STATE_RESUME);
		break;
	case CAM_ICP_CMD_VOTE_CPAS:
		rc = cam_lx7_cpas_vote(lx7_info->core_info, args);
		break;
	case CAM_ICP_CMD_CPAS_START:
		rc = __lx7_cpas_start(lx7_info->core_info, args);
		break;
	case CAM_ICP_CMD_CPAS_STOP:
		rc = cam_lx7_cpas_stop(lx7_info->core_info);
		break;
	case CAM_ICP_CMD_UBWC_CFG:
		rc = cam_lx7_ubwc_configure(&lx7_info->soc_info);
		break;
	case CAM_ICP_SEND_INIT:
		hfi_send_system_cmd(HFI_CMD_SYS_INIT, 0, 0);
		rc = 0;
		break;
	case CAM_ICP_CMD_PC_PREP:
		hfi_send_system_cmd(HFI_CMD_SYS_PC_PREP, 0, 0);
		rc = 0;
		break;
	case CAM_ICP_CMD_CLK_UPDATE: {
		rc = __cam_lx7_update_clk_rate(lx7_info, args);
		break;
	}
	case CAM_ICP_CMD_HW_DUMP:
		/* Not supported for lx7 */
		rc = 0;
		break;
	case CAM_ICP_CMD_HW_MINI_DUMP: {
		rc = __cam_lx7_fw_mini_dump(lx7_info->core_info, args);
		break;
	}
	default:
		CAM_ERR(CAM_ICP, "invalid command type=%u", cmd_type);
		break;
	}

	return rc;
}

irqreturn_t cam_lx7_handle_irq(int irq_num, void *data)
{
	struct cam_hw_info *lx7_info = data;
	struct cam_lx7_core_info *core_info = NULL;
	bool recover = false;
	uint32_t status = 0;
	void __iomem *cirq_base;

	if (!lx7_info) {
		CAM_ERR(CAM_ICP, "invalid LX7 device info");
		return IRQ_NONE;
	}

	cirq_base = lx7_info->soc_info.reg_map[LX7_CIRQ_BASE].mem_base;

	status = cam_io_r_mb(cirq_base + ICP_LX7_CIRQ_OB_STATUS);

	cam_io_w_mb(status, cirq_base + ICP_LX7_CIRQ_OB_CLEAR);
	cam_io_w_mb(LX7_IRQ_CLEAR_CMD, cirq_base + ICP_LX7_CIRQ_OB_IRQ_CMD);

	if (status & LX7_WDT_BITE_WS0) {
		/* WD clear sequence - SW listens only to WD0 */
		cam_io_w_mb(0x0,
			lx7_info->soc_info.reg_map[LX7_WD0_BASE].mem_base +
			ICP_LX7_WD_CTRL);
		cam_io_w_mb(0x1,
			lx7_info->soc_info.reg_map[LX7_WD0_BASE].mem_base +
			ICP_LX7_WD_INTCLR);
		CAM_ERR_RATE_LIMIT(CAM_ICP, "Fatal: Watchdog Bite from LX7");
		recover = true;
	}

	core_info = lx7_info->core_info;

	spin_lock(&lx7_info->hw_lock);
	if (core_info->irq_cb.cb)
		core_info->irq_cb.cb(core_info->irq_cb.data,
						recover);
	spin_unlock(&lx7_info->hw_lock);

	return IRQ_HANDLED;
}

void cam_lx7_irq_raise(void *priv)
{
	struct cam_hw_info *lx7_info = priv;

	if (!lx7_info) {
		CAM_ERR(CAM_ICP, "invalid LX7 device info");
		return;
	}

	cam_io_w_mb(LX7_HOST2ICPINT,
		lx7_info->soc_info.reg_map[LX7_CIRQ_BASE].mem_base +
		ICP_LX7_CIRQ_HOST2ICPINT);
}

void cam_lx7_irq_enable(void *priv)
{
	struct cam_hw_info *lx7_info = priv;

	if (!lx7_info) {
		CAM_ERR(CAM_ICP, "invalid LX7 device info");
		return;
	}

	cam_io_w_mb(LX7_WDT_BITE_WS0 | LX7_ICP2HOSTINT,
		lx7_info->soc_info.reg_map[LX7_CIRQ_BASE].mem_base +
		ICP_LX7_CIRQ_OB_MASK);
}

void __iomem *cam_lx7_iface_addr(void *priv)
{
	struct cam_hw_info *lx7_info = priv;
	void __iomem *base;

	if (!lx7_info) {
		CAM_ERR(CAM_ICP, "invalid LX7 device info");
		return ERR_PTR(-EINVAL);
	}

	base = lx7_info->soc_info.reg_map[LX7_CSR_BASE].mem_base;

	return base + LX7_GEN_PURPOSE_REG_OFFSET;
}
