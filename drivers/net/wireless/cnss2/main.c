// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pm_wakeup.h>
#include <linux/reboot.h>
#include <linux/rwsem.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#if IS_ENABLED(CONFIG_QCOM_MINIDUMP)
#include <soc/qcom/minidump.h>
#endif

#include "cnss_plat_ipc_qmi.h"
#include "main.h"
#include "bus.h"
#include "debug.h"
#include "genl.h"

#define CNSS_DUMP_FORMAT_VER		0x11
#define CNSS_DUMP_FORMAT_VER_V2		0x22
#define CNSS_DUMP_MAGIC_VER_V2		0x42445953
#define CNSS_DUMP_NAME			"CNSS_WLAN"
#define CNSS_DUMP_DESC_SIZE		0x1000
#define CNSS_DUMP_SEG_VER		0x1
#define FILE_SYSTEM_READY		1
#define FW_READY_TIMEOUT		20000
#define FW_ASSERT_TIMEOUT		5000
#define CNSS_EVENT_PENDING		2989
#define POWER_RESET_MIN_DELAY_MS	100

#define CNSS_QUIRKS_DEFAULT		0
#ifdef CONFIG_CNSS_EMULATION
#define CNSS_MHI_TIMEOUT_DEFAULT	90000
#define CNSS_MHI_M2_TIMEOUT_DEFAULT	2000
#define CNSS_QMI_TIMEOUT_DEFAULT	90000
#else
#define CNSS_MHI_TIMEOUT_DEFAULT	0
#define CNSS_MHI_M2_TIMEOUT_DEFAULT	25
#define CNSS_QMI_TIMEOUT_DEFAULT	10000
#endif
#define CNSS_BDF_TYPE_DEFAULT		CNSS_BDF_ELF
#define CNSS_TIME_SYNC_PERIOD_DEFAULT	900000
#define CNSS_DMS_QMI_CONNECTION_WAIT_MS 50
#define CNSS_DMS_QMI_CONNECTION_WAIT_RETRY 200
#define CNSS_DAEMON_CONNECT_TIMEOUT_MS  30000
#define CNSS_CAL_DB_FILE_NAME "wlfw_cal_db.bin"
#define CNSS_CAL_START_PROBE_WAIT_RETRY_MAX 100
#define CNSS_CAL_START_PROBE_WAIT_MS	500

enum cnss_cal_db_op {
	CNSS_CAL_DB_UPLOAD,
	CNSS_CAL_DB_DOWNLOAD,
	CNSS_CAL_DB_INVALID_OP,
};

static struct cnss_plat_data *plat_env;

static DECLARE_RWSEM(cnss_pm_sem);

static struct cnss_fw_files FW_FILES_QCA6174_FW_3_0 = {
	"qwlan30.bin", "bdwlan30.bin", "otp30.bin", "utf30.bin",
	"utfbd30.bin", "epping30.bin", "evicted30.bin"
};

static struct cnss_fw_files FW_FILES_DEFAULT = {
	"qwlan.bin", "bdwlan.bin", "otp.bin", "utf.bin",
	"utfbd.bin", "epping.bin", "evicted.bin"
};

struct cnss_driver_event {
	struct list_head list;
	enum cnss_driver_event_type type;
	bool sync;
	struct completion complete;
	int ret;
	void *data;
};

static void cnss_set_plat_priv(struct platform_device *plat_dev,
			       struct cnss_plat_data *plat_priv)
{
	plat_env = plat_priv;
}

struct cnss_plat_data *cnss_get_plat_priv(struct platform_device *plat_dev)
{
	return plat_env;
}

/**
 * cnss_get_mem_seg_count - Get segment count of memory
 * @type: memory type
 * @seg: segment count
 *
 * Return: 0 on success, negative value on failure
 */
int cnss_get_mem_seg_count(enum cnss_remote_mem_type type, u32 *seg)
{
	struct cnss_plat_data *plat_priv;

	plat_priv = cnss_get_plat_priv(NULL);
	if (!plat_priv)
		return -ENODEV;

	switch (type) {
	case CNSS_REMOTE_MEM_TYPE_FW:
		*seg = plat_priv->fw_mem_seg_len;
		break;
	case CNSS_REMOTE_MEM_TYPE_QDSS:
		*seg = plat_priv->qdss_mem_seg_len;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(cnss_get_mem_seg_count);

/**
 * cnss_get_mem_segment_info - Get memory info of different type
 * @type: memory type
 * @segment: array to save the segment info
 * @seg: segment count
 *
 * Return: 0 on success, negative value on failure
 */
int cnss_get_mem_segment_info(enum cnss_remote_mem_type type,
			      struct cnss_mem_segment segment[],
			      u32 segment_count)
{
	struct cnss_plat_data *plat_priv;
	u32 i;

	plat_priv = cnss_get_plat_priv(NULL);
	if (!plat_priv)
		return -ENODEV;

	switch (type) {
	case CNSS_REMOTE_MEM_TYPE_FW:
		if (segment_count > plat_priv->fw_mem_seg_len)
			segment_count = plat_priv->fw_mem_seg_len;
		for (i = 0; i < segment_count; i++) {
			segment[i].size = plat_priv->fw_mem[i].size;
			segment[i].va = plat_priv->fw_mem[i].va;
			segment[i].pa = plat_priv->fw_mem[i].pa;
		}
		break;
	case CNSS_REMOTE_MEM_TYPE_QDSS:
		if (segment_count > plat_priv->qdss_mem_seg_len)
			segment_count = plat_priv->qdss_mem_seg_len;
		for (i = 0; i < segment_count; i++) {
			segment[i].size = plat_priv->qdss_mem[i].size;
			segment[i].va = plat_priv->qdss_mem[i].va;
			segment[i].pa = plat_priv->qdss_mem[i].pa;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(cnss_get_mem_segment_info);

int cnss_set_feature_list(struct cnss_plat_data *plat_priv,
			  enum cnss_feature_v01 feature)
{
	if (unlikely(!plat_priv || feature >= CNSS_MAX_FEATURE_V01))
		return -EINVAL;

	plat_priv->feature_list |= 1 << feature;
	return 0;
}

int cnss_get_feature_list(struct cnss_plat_data *plat_priv,
			  u64 *feature_list)
{
	if (unlikely(!plat_priv))
		return -EINVAL;

	*feature_list = plat_priv->feature_list;
	return 0;
}

static int cnss_pm_notify(struct notifier_block *b,
			  unsigned long event, void *p)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		down_write(&cnss_pm_sem);
		break;
	case PM_POST_SUSPEND:
		up_write(&cnss_pm_sem);
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block cnss_pm_notifier = {
	.notifier_call = cnss_pm_notify,
};

void cnss_pm_stay_awake(struct cnss_plat_data *plat_priv)
{
	if (atomic_inc_return(&plat_priv->pm_count) != 1)
		return;

	cnss_pr_dbg("PM stay awake, state: 0x%lx, count: %d\n",
		    plat_priv->driver_state,
		    atomic_read(&plat_priv->pm_count));
	pm_stay_awake(&plat_priv->plat_dev->dev);
}

void cnss_pm_relax(struct cnss_plat_data *plat_priv)
{
	int r = atomic_dec_return(&plat_priv->pm_count);

	WARN_ON(r < 0);

	if (r != 0)
		return;

	cnss_pr_dbg("PM relax, state: 0x%lx, count: %d\n",
		    plat_priv->driver_state,
		    atomic_read(&plat_priv->pm_count));
	pm_relax(&plat_priv->plat_dev->dev);
}

void cnss_lock_pm_sem(struct device *dev)
{
	down_read(&cnss_pm_sem);
}
EXPORT_SYMBOL(cnss_lock_pm_sem);

void cnss_release_pm_sem(struct device *dev)
{
	up_read(&cnss_pm_sem);
}
EXPORT_SYMBOL(cnss_release_pm_sem);

int cnss_get_fw_files_for_target(struct device *dev,
				 struct cnss_fw_files *pfw_files,
				 u32 target_type, u32 target_version)
{
	if (!pfw_files)
		return -ENODEV;

	switch (target_version) {
	case QCA6174_REV3_VERSION:
	case QCA6174_REV3_2_VERSION:
		memcpy(pfw_files, &FW_FILES_QCA6174_FW_3_0, sizeof(*pfw_files));
		break;
	default:
		memcpy(pfw_files, &FW_FILES_DEFAULT, sizeof(*pfw_files));
		cnss_pr_err("Unknown target version, type: 0x%X, version: 0x%X",
			    target_type, target_version);
		break;
	}

	return 0;
}
EXPORT_SYMBOL(cnss_get_fw_files_for_target);

int cnss_get_platform_cap(struct device *dev, struct cnss_platform_cap *cap)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv)
		return -ENODEV;

	if (!cap)
		return -EINVAL;

	*cap = plat_priv->cap;
	cnss_pr_dbg("Platform cap_flag is 0x%x\n", cap->cap_flag);

	return 0;
}
EXPORT_SYMBOL(cnss_get_platform_cap);

void cnss_request_pm_qos(struct device *dev, u32 qos_val)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv)
		return;

	cpu_latency_qos_add_request(&plat_priv->qos_request, qos_val);
}
EXPORT_SYMBOL(cnss_request_pm_qos);

void cnss_remove_pm_qos(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv)
		return;

	cpu_latency_qos_remove_request(&plat_priv->qos_request);
}
EXPORT_SYMBOL(cnss_remove_pm_qos);

int cnss_wlan_enable(struct device *dev,
		     struct cnss_wlan_enable_cfg *config,
		     enum cnss_driver_mode mode,
		     const char *host_version)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	int ret = 0;

	if (plat_priv->device_id == QCA6174_DEVICE_ID)
		return 0;

	if (test_bit(QMI_BYPASS, &plat_priv->ctrl_params.quirks))
		return 0;

	if (!config || !host_version) {
		cnss_pr_err("Invalid config or host_version pointer\n");
		return -EINVAL;
	}

	cnss_pr_dbg("Mode: %d, config: %pK, host_version: %s\n",
		    mode, config, host_version);

	if (mode == CNSS_WALTEST || mode == CNSS_CCPM)
		goto skip_cfg;

	ret = cnss_wlfw_wlan_cfg_send_sync(plat_priv, config, host_version);
	if (ret)
		goto out;

skip_cfg:
	ret = cnss_wlfw_wlan_mode_send_sync(plat_priv, mode);
out:
	return ret;
}
EXPORT_SYMBOL(cnss_wlan_enable);

int cnss_wlan_disable(struct device *dev, enum cnss_driver_mode mode)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	int ret = 0;

	if (plat_priv->device_id == QCA6174_DEVICE_ID)
		return 0;

	if (test_bit(QMI_BYPASS, &plat_priv->ctrl_params.quirks))
		return 0;

	ret = cnss_wlfw_wlan_mode_send_sync(plat_priv, CNSS_OFF);
	cnss_bus_free_qdss_mem(plat_priv);

	return ret;
}
EXPORT_SYMBOL(cnss_wlan_disable);

int cnss_athdiag_read(struct device *dev, u32 offset, u32 mem_type,
		      u32 data_len, u8 *output)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	int ret = 0;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		return -EINVAL;
	}

	if (plat_priv->device_id == QCA6174_DEVICE_ID)
		return 0;

	if (!test_bit(CNSS_FW_READY, &plat_priv->driver_state)) {
		cnss_pr_err("Invalid state for athdiag read: 0x%lx\n",
			    plat_priv->driver_state);
		ret = -EINVAL;
		goto out;
	}

	ret = cnss_wlfw_athdiag_read_send_sync(plat_priv, offset, mem_type,
					       data_len, output);

out:
	return ret;
}
EXPORT_SYMBOL(cnss_athdiag_read);

int cnss_athdiag_write(struct device *dev, u32 offset, u32 mem_type,
		       u32 data_len, u8 *input)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	int ret = 0;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		return -EINVAL;
	}

	if (plat_priv->device_id == QCA6174_DEVICE_ID)
		return 0;

	if (!test_bit(CNSS_FW_READY, &plat_priv->driver_state)) {
		cnss_pr_err("Invalid state for athdiag write: 0x%lx\n",
			    plat_priv->driver_state);
		ret = -EINVAL;
		goto out;
	}

	ret = cnss_wlfw_athdiag_write_send_sync(plat_priv, offset, mem_type,
						data_len, input);

out:
	return ret;
}
EXPORT_SYMBOL(cnss_athdiag_write);

int cnss_set_fw_log_mode(struct device *dev, u8 fw_log_mode)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (plat_priv->device_id == QCA6174_DEVICE_ID)
		return 0;

	return cnss_wlfw_ini_send_sync(plat_priv, fw_log_mode);
}
EXPORT_SYMBOL(cnss_set_fw_log_mode);

int cnss_set_pcie_gen_speed(struct device *dev, u8 pcie_gen_speed)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv)
		return -EINVAL;

	if (plat_priv->device_id != QCA6490_DEVICE_ID ||
	    !plat_priv->fw_pcie_gen_switch)
		return -EOPNOTSUPP;

	if (pcie_gen_speed < QMI_PCIE_GEN_SPEED_1_V01 ||
	    pcie_gen_speed > QMI_PCIE_GEN_SPEED_3_V01)
		return -EINVAL;

	cnss_pr_dbg("WLAN provided PCIE gen speed: %d\n", pcie_gen_speed);
	plat_priv->pcie_gen_speed = pcie_gen_speed;
	return 0;
}
EXPORT_SYMBOL(cnss_set_pcie_gen_speed);

static int cnss_fw_mem_ready_hdlr(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	set_bit(CNSS_FW_MEM_READY, &plat_priv->driver_state);

	ret = cnss_wlfw_tgt_cap_send_sync(plat_priv);
	if (ret)
		goto out;

	if (plat_priv->hds_enabled)
		cnss_wlfw_bdf_dnld_send_sync(plat_priv, CNSS_BDF_HDS);
	cnss_wlfw_bdf_dnld_send_sync(plat_priv, CNSS_BDF_REGDB);

	ret = cnss_wlfw_bdf_dnld_send_sync(plat_priv,
					   plat_priv->ctrl_params.bdf_type);
	if (ret)
		goto out;

	ret = cnss_bus_load_m3(plat_priv);
	if (ret)
		goto out;

	ret = cnss_wlfw_m3_dnld_send_sync(plat_priv);
	if (ret)
		goto out;

	cnss_wlfw_qdss_dnld_send_sync(plat_priv);

	return 0;
out:
	return ret;
}

static int cnss_request_antenna_sharing(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	if (!plat_priv->antenna) {
		ret = cnss_wlfw_antenna_switch_send_sync(plat_priv);
		if (ret)
			goto out;
	}

	if (test_bit(CNSS_COEX_CONNECTED, &plat_priv->driver_state)) {
		ret = coex_antenna_switch_to_wlan_send_sync_msg(plat_priv);
		if (ret)
			goto out;
	}

	ret = cnss_wlfw_antenna_grant_send_sync(plat_priv);
	if (ret)
		goto out;

	return 0;

out:
	return ret;
}

static void cnss_release_antenna_sharing(struct cnss_plat_data *plat_priv)
{
	if (test_bit(CNSS_COEX_CONNECTED, &plat_priv->driver_state))
		coex_antenna_switch_to_mdm_send_sync_msg(plat_priv);
}

static int cnss_setup_dms_mac(struct cnss_plat_data *plat_priv)
{
	u32 i;
	int ret = 0;
	struct cnss_plat_ipc_daemon_config *cfg;

	ret = cnss_qmi_get_dms_mac(plat_priv);
	if (ret == 0 && plat_priv->dms.mac_valid)
		goto qmi_send;

	/* DTSI property use-nv-mac is used to force DMS MAC address for WLAN.
	 * Thus assert on failure to get MAC from DMS even after retries
	 */
	if (plat_priv->use_nv_mac) {
		/* Check if Daemon says platform support DMS MAC provisioning */
		cfg = cnss_plat_ipc_qmi_daemon_config();
		if (cfg) {
			if (!cfg->dms_mac_addr_supported) {
				cnss_pr_err("DMS MAC address not supported\n");
				CNSS_ASSERT(0);
				return -EINVAL;
			}
		}
		for (i = 0; i < CNSS_DMS_QMI_CONNECTION_WAIT_RETRY; i++) {
			if (plat_priv->dms.mac_valid)
				break;

			ret = cnss_qmi_get_dms_mac(plat_priv);
			if (ret == 0)
				break;
			msleep(CNSS_DMS_QMI_CONNECTION_WAIT_MS);
		}
		if (!plat_priv->dms.mac_valid) {
			cnss_pr_err("Unable to get MAC from DMS after retries\n");
			CNSS_ASSERT(0);
			return -EINVAL;
		}
	}
qmi_send:
	if (plat_priv->dms.mac_valid)
		ret =
		cnss_wlfw_wlan_mac_req_send_sync(plat_priv, plat_priv->dms.mac,
						 ARRAY_SIZE(plat_priv->dms.mac));

	return ret;
}

static int cnss_cal_db_mem_update(struct cnss_plat_data *plat_priv,
				  enum cnss_cal_db_op op, u32 *size)
{
	int ret = 0;
	u32 timeout = cnss_get_timeout(plat_priv,
				       CNSS_TIMEOUT_DAEMON_CONNECTION);
	enum cnss_plat_ipc_qmi_client_id_v01 client_id =
					CNSS_PLAT_IPC_DAEMON_QMI_CLIENT_V01;

	if (op >= CNSS_CAL_DB_INVALID_OP)
		return -EINVAL;

	if (!plat_priv->cbc_file_download) {
		cnss_pr_info("CAL DB file not required as per BDF\n");
		return 0;
	}
	if (*size == 0) {
		cnss_pr_err("Invalid cal file size\n");
		return -EINVAL;
	}
	if (!test_bit(CNSS_DAEMON_CONNECTED, &plat_priv->driver_state)) {
		cnss_pr_info("Waiting for CNSS Daemon connection\n");
		ret = wait_for_completion_timeout(&plat_priv->daemon_connected,
						  msecs_to_jiffies(timeout));
		if (!ret) {
			cnss_pr_err("Daemon not yet connected\n");
			CNSS_ASSERT(0);
			return ret;
		}
	}
	if (!plat_priv->cal_mem->va) {
		cnss_pr_err("CAL DB Memory not setup for FW\n");
		return -EINVAL;
	}

	/* Copy CAL DB file contents to/from CAL_TYPE_DDR mem allocated to FW */
	if (op == CNSS_CAL_DB_DOWNLOAD) {
		cnss_pr_dbg("Initiating Calibration file download to mem\n");
		ret = cnss_plat_ipc_qmi_file_download(client_id,
						      CNSS_CAL_DB_FILE_NAME,
						      plat_priv->cal_mem->va,
						      size);
	} else {
		cnss_pr_dbg("Initiating Calibration mem upload to file\n");
		ret = cnss_plat_ipc_qmi_file_upload(client_id,
						    CNSS_CAL_DB_FILE_NAME,
						    plat_priv->cal_mem->va,
						    *size);
	}

	if (ret)
		cnss_pr_err("Cal DB file %s %s failure\n",
			    CNSS_CAL_DB_FILE_NAME,
			    op == CNSS_CAL_DB_DOWNLOAD ? "download" : "upload");
	else
		cnss_pr_dbg("Cal DB file %s %s size %d done\n",
			    CNSS_CAL_DB_FILE_NAME,
			    op == CNSS_CAL_DB_DOWNLOAD ? "download" : "upload",
			    *size);

	return ret;
}

static int cnss_cal_mem_upload_to_file(struct cnss_plat_data *plat_priv)
{
	if (plat_priv->cal_file_size > plat_priv->cal_mem->size) {
		cnss_pr_err("Cal file size is larger than Cal DB Mem size\n");
		return -EINVAL;
	}
	return cnss_cal_db_mem_update(plat_priv, CNSS_CAL_DB_UPLOAD,
				      &plat_priv->cal_file_size);
}

static int cnss_cal_file_download_to_mem(struct cnss_plat_data *plat_priv,
					 u32 *cal_file_size)
{
	/* To download pass the total size of cal DB mem allocated.
	 * After cal file is download to mem, its size is updated in
	 * return pointer
	 */
	*cal_file_size = plat_priv->cal_mem->size;
	return cnss_cal_db_mem_update(plat_priv, CNSS_CAL_DB_DOWNLOAD,
				      cal_file_size);
}

static int cnss_fw_ready_hdlr(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	u32 cal_file_size = 0;

	if (!plat_priv)
		return -ENODEV;

	cnss_pr_dbg("Processing FW Init Done..\n");
	del_timer(&plat_priv->fw_boot_timer);
	set_bit(CNSS_FW_READY, &plat_priv->driver_state);
	clear_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state);

	cnss_wlfw_send_pcie_gen_speed_sync(plat_priv);

	if (test_bit(CNSS_FW_BOOT_RECOVERY, &plat_priv->driver_state)) {
		clear_bit(CNSS_FW_BOOT_RECOVERY, &plat_priv->driver_state);
		clear_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
	}

	if (test_bit(ENABLE_WALTEST, &plat_priv->ctrl_params.quirks)) {
		ret = cnss_wlfw_wlan_mode_send_sync(plat_priv,
						    CNSS_WALTEST);
	} else if (test_bit(CNSS_IN_COLD_BOOT_CAL, &plat_priv->driver_state)) {
		cnss_request_antenna_sharing(plat_priv);
		cnss_cal_file_download_to_mem(plat_priv, &cal_file_size);
		cnss_wlfw_cal_report_req_send_sync(plat_priv, cal_file_size);
		plat_priv->cal_time = jiffies;
		ret = cnss_wlfw_wlan_mode_send_sync(plat_priv,
						    CNSS_CALIBRATION);
	} else {
		ret = cnss_setup_dms_mac(plat_priv);
		ret = cnss_bus_call_driver_probe(plat_priv);
	}

	if (ret && test_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state))
		goto out;
	else if (ret)
		goto shutdown;

	cnss_vreg_unvote_type(plat_priv, CNSS_VREG_PRIM);

	return 0;

shutdown:
	cnss_bus_dev_shutdown(plat_priv);

	clear_bit(CNSS_FW_READY, &plat_priv->driver_state);
	clear_bit(CNSS_FW_MEM_READY, &plat_priv->driver_state);

out:
	return ret;
}

static char *cnss_driver_event_to_str(enum cnss_driver_event_type type)
{
	switch (type) {
	case CNSS_DRIVER_EVENT_SERVER_ARRIVE:
		return "SERVER_ARRIVE";
	case CNSS_DRIVER_EVENT_SERVER_EXIT:
		return "SERVER_EXIT";
	case CNSS_DRIVER_EVENT_REQUEST_MEM:
		return "REQUEST_MEM";
	case CNSS_DRIVER_EVENT_FW_MEM_READY:
		return "FW_MEM_READY";
	case CNSS_DRIVER_EVENT_FW_READY:
		return "FW_READY";
	case CNSS_DRIVER_EVENT_COLD_BOOT_CAL_START:
		return "COLD_BOOT_CAL_START";
	case CNSS_DRIVER_EVENT_COLD_BOOT_CAL_DONE:
		return "COLD_BOOT_CAL_DONE";
	case CNSS_DRIVER_EVENT_REGISTER_DRIVER:
		return "REGISTER_DRIVER";
	case CNSS_DRIVER_EVENT_UNREGISTER_DRIVER:
		return "UNREGISTER_DRIVER";
	case CNSS_DRIVER_EVENT_RECOVERY:
		return "RECOVERY";
	case CNSS_DRIVER_EVENT_FORCE_FW_ASSERT:
		return "FORCE_FW_ASSERT";
	case CNSS_DRIVER_EVENT_POWER_UP:
		return "POWER_UP";
	case CNSS_DRIVER_EVENT_POWER_DOWN:
		return "POWER_DOWN";
	case CNSS_DRIVER_EVENT_IDLE_RESTART:
		return "IDLE_RESTART";
	case CNSS_DRIVER_EVENT_IDLE_SHUTDOWN:
		return "IDLE_SHUTDOWN";
	case CNSS_DRIVER_EVENT_IMS_WFC_CALL_IND:
		return "IMS_WFC_CALL_IND";
	case CNSS_DRIVER_EVENT_WLFW_TWT_CFG_IND:
		return "WLFW_TWC_CFG_IND";
	case CNSS_DRIVER_EVENT_QDSS_TRACE_REQ_MEM:
		return "QDSS_TRACE_REQ_MEM";
	case CNSS_DRIVER_EVENT_FW_MEM_FILE_SAVE:
		return "FW_MEM_FILE_SAVE";
	case CNSS_DRIVER_EVENT_QDSS_TRACE_FREE:
		return "QDSS_TRACE_FREE";
	case CNSS_DRIVER_EVENT_QDSS_TRACE_REQ_DATA:
		return "QDSS_TRACE_REQ_DATA";
	case CNSS_DRIVER_EVENT_MAX:
		return "EVENT_MAX";
	}

	return "UNKNOWN";
};

int cnss_driver_event_post(struct cnss_plat_data *plat_priv,
			   enum cnss_driver_event_type type,
			   u32 flags, void *data)
{
	struct cnss_driver_event *event;
	unsigned long irq_flags;
	int gfp = GFP_KERNEL;
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	cnss_pr_dbg("Posting event: %s(%d)%s, state: 0x%lx flags: 0x%0x\n",
		    cnss_driver_event_to_str(type), type,
		    flags ? "-sync" : "", plat_priv->driver_state, flags);

	if (type >= CNSS_DRIVER_EVENT_MAX) {
		cnss_pr_err("Invalid Event type: %d, can't post", type);
		return -EINVAL;
	}

	if (in_interrupt() || irqs_disabled())
		gfp = GFP_ATOMIC;

	event = kzalloc(sizeof(*event), gfp);
	if (!event)
		return -ENOMEM;

	cnss_pm_stay_awake(plat_priv);

	event->type = type;
	event->data = data;
	init_completion(&event->complete);
	event->ret = CNSS_EVENT_PENDING;
	event->sync = !!(flags & CNSS_EVENT_SYNC);

	spin_lock_irqsave(&plat_priv->event_lock, irq_flags);
	list_add_tail(&event->list, &plat_priv->event_list);
	spin_unlock_irqrestore(&plat_priv->event_lock, irq_flags);

	queue_work(plat_priv->event_wq, &plat_priv->event_work);

	if (!(flags & CNSS_EVENT_SYNC))
		goto out;

	if (flags & CNSS_EVENT_UNKILLABLE)
		wait_for_completion(&event->complete);
	else if (flags & CNSS_EVENT_UNINTERRUPTIBLE)
		ret = wait_for_completion_killable(&event->complete);
	else
		ret = wait_for_completion_interruptible(&event->complete);

	cnss_pr_dbg("Completed event: %s(%d), state: 0x%lx, ret: %d/%d\n",
		    cnss_driver_event_to_str(type), type,
		    plat_priv->driver_state, ret, event->ret);
	spin_lock_irqsave(&plat_priv->event_lock, irq_flags);
	if (ret == -ERESTARTSYS && event->ret == CNSS_EVENT_PENDING) {
		event->sync = false;
		spin_unlock_irqrestore(&plat_priv->event_lock, irq_flags);
		ret = -EINTR;
		goto out;
	}
	spin_unlock_irqrestore(&plat_priv->event_lock, irq_flags);

	ret = event->ret;
	kfree(event);

out:
	cnss_pm_relax(plat_priv);
	return ret;
}

/**
 * cnss_get_timeout - Get timeout for corresponding type.
 * @plat_priv: Pointer to platform driver context.
 * @cnss_timeout_type: Timeout type.
 *
 * Return: Timeout in milliseconds.
 */
unsigned int cnss_get_timeout(struct cnss_plat_data *plat_priv,
			      enum cnss_timeout_type timeout_type)
{
	unsigned int qmi_timeout = cnss_get_qmi_timeout(plat_priv);

	switch (timeout_type) {
	case CNSS_TIMEOUT_QMI:
		return qmi_timeout;
	case CNSS_TIMEOUT_POWER_UP:
		return (qmi_timeout << 2);
	case CNSS_TIMEOUT_IDLE_RESTART:
		/* In idle restart power up sequence, we have fw_boot_timer to
		 * handle FW initialization failure.
		 * It uses WLAN_MISSION_MODE_TIMEOUT, so setup 3x that time to
		 * account for FW dump collection and FW re-initialization on
		 * retry.
		 */
		return (qmi_timeout + WLAN_MISSION_MODE_TIMEOUT * 3);
	case CNSS_TIMEOUT_CALIBRATION:
		/* Similar to mission mode, in CBC if FW init fails
		 * fw recovery is tried. Thus return 2x the CBC timeout.
		 */
		return (qmi_timeout + WLAN_COLD_BOOT_CAL_TIMEOUT * 2);
	case CNSS_TIMEOUT_WLAN_WATCHDOG:
		return ((qmi_timeout << 1) + WLAN_WD_TIMEOUT_MS);
	case CNSS_TIMEOUT_RDDM:
		return CNSS_RDDM_TIMEOUT_MS;
	case CNSS_TIMEOUT_RECOVERY:
		return RECOVERY_TIMEOUT;
	case CNSS_TIMEOUT_DAEMON_CONNECTION:
		return qmi_timeout + CNSS_DAEMON_CONNECT_TIMEOUT_MS;
	default:
		return qmi_timeout;
	}
}

unsigned int cnss_get_boot_timeout(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return 0;
	}

	return cnss_get_timeout(plat_priv, CNSS_TIMEOUT_QMI);
}
EXPORT_SYMBOL(cnss_get_boot_timeout);

int cnss_power_up(struct device *dev)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	unsigned int timeout;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	cnss_pr_dbg("Powering up device\n");

	ret = cnss_driver_event_post(plat_priv,
				     CNSS_DRIVER_EVENT_POWER_UP,
				     CNSS_EVENT_SYNC, NULL);
	if (ret)
		goto out;

	if (plat_priv->device_id == QCA6174_DEVICE_ID)
		goto out;

	timeout = cnss_get_timeout(plat_priv, CNSS_TIMEOUT_POWER_UP);

	reinit_completion(&plat_priv->power_up_complete);
	ret = wait_for_completion_timeout(&plat_priv->power_up_complete,
					  msecs_to_jiffies(timeout));
	if (!ret) {
		cnss_pr_err("Timeout (%ums) waiting for power up to complete\n",
			    timeout);
		ret = -EAGAIN;
		goto out;
	}

	return 0;

out:
	return ret;
}
EXPORT_SYMBOL(cnss_power_up);

int cnss_power_down(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	cnss_pr_dbg("Powering down device\n");

	return cnss_driver_event_post(plat_priv,
				      CNSS_DRIVER_EVENT_POWER_DOWN,
				      CNSS_EVENT_SYNC, NULL);
}
EXPORT_SYMBOL(cnss_power_down);

int cnss_idle_restart(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	unsigned int timeout;
	int ret = 0;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	if (!mutex_trylock(&plat_priv->driver_ops_lock)) {
		cnss_pr_dbg("Another driver operation is in progress, ignore idle restart\n");
		return -EBUSY;
	}

	cnss_pr_dbg("Doing idle restart\n");

	reinit_completion(&plat_priv->power_up_complete);

	if (test_bit(CNSS_IN_REBOOT, &plat_priv->driver_state)) {
		cnss_pr_dbg("Reboot or shutdown is in progress, ignore idle restart\n");
		ret = -EINVAL;
		goto out;
	}

	ret = cnss_driver_event_post(plat_priv,
				     CNSS_DRIVER_EVENT_IDLE_RESTART,
				     CNSS_EVENT_SYNC_UNINTERRUPTIBLE, NULL);
	if (ret)
		goto out;

	if (plat_priv->device_id == QCA6174_DEVICE_ID) {
		ret = cnss_bus_call_driver_probe(plat_priv);
		goto out;
	}

	timeout = cnss_get_timeout(plat_priv, CNSS_TIMEOUT_IDLE_RESTART);
	ret = wait_for_completion_timeout(&plat_priv->power_up_complete,
					  msecs_to_jiffies(timeout));
	if (plat_priv->power_up_error) {
		ret = plat_priv->power_up_error;
		clear_bit(CNSS_DRIVER_IDLE_RESTART, &plat_priv->driver_state);
		cnss_pr_dbg("Power up error:%d, exiting\n",
			    plat_priv->power_up_error);
		goto out;
	}

	if (!ret) {
		/* This exception occurs after attempting retry of FW recovery.
		 * Thus we can safely power off the device.
		 */
		cnss_fatal_err("Timeout (%ums) waiting for idle restart to complete\n",
			       timeout);
		ret = -ETIMEDOUT;
		cnss_power_down(dev);
		CNSS_ASSERT(0);
		goto out;
	}

	if (test_bit(CNSS_IN_REBOOT, &plat_priv->driver_state)) {
		cnss_pr_dbg("Reboot or shutdown is in progress, ignore idle restart\n");
		del_timer(&plat_priv->fw_boot_timer);
		ret = -EINVAL;
		goto out;
	}

	mutex_unlock(&plat_priv->driver_ops_lock);
	return 0;

out:
	mutex_unlock(&plat_priv->driver_ops_lock);
	return ret;
}
EXPORT_SYMBOL(cnss_idle_restart);

int cnss_idle_shutdown(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	unsigned int timeout;
	int ret;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	if (test_bit(CNSS_IN_SUSPEND_RESUME, &plat_priv->driver_state)) {
		cnss_pr_dbg("System suspend or resume in progress, ignore idle shutdown\n");
		return -EAGAIN;
	}

	cnss_pr_dbg("Doing idle shutdown\n");

	if (!test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state) &&
	    !test_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state))
		goto skip_wait;

	reinit_completion(&plat_priv->recovery_complete);
	timeout = cnss_get_timeout(plat_priv, CNSS_TIMEOUT_RECOVERY);
	ret = wait_for_completion_timeout(&plat_priv->recovery_complete,
					  msecs_to_jiffies(timeout));
	if (!ret) {
		cnss_pr_err("Timeout (%ums) waiting for recovery to complete\n",
			    timeout);
		CNSS_ASSERT(0);
	}

skip_wait:
	return cnss_driver_event_post(plat_priv,
				      CNSS_DRIVER_EVENT_IDLE_SHUTDOWN,
				      CNSS_EVENT_SYNC_UNINTERRUPTIBLE, NULL);
}
EXPORT_SYMBOL(cnss_idle_shutdown);

static int cnss_get_resources(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	ret = cnss_get_vreg_type(plat_priv, CNSS_VREG_PRIM);
	if (ret) {
		cnss_pr_err("Failed to get vreg, err = %d\n", ret);
		goto out;
	}

	ret = cnss_get_clk(plat_priv);
	if (ret) {
		cnss_pr_err("Failed to get clocks, err = %d\n", ret);
		goto put_vreg;
	}

	ret = cnss_get_pinctrl(plat_priv);
	if (ret) {
		cnss_pr_err("Failed to get pinctrl, err = %d\n", ret);
		goto put_clk;
	}

	return 0;

put_clk:
	cnss_put_clk(plat_priv);
put_vreg:
	cnss_put_vreg_type(plat_priv, CNSS_VREG_PRIM);
out:
	return ret;
}

static void cnss_put_resources(struct cnss_plat_data *plat_priv)
{
	cnss_put_clk(plat_priv);
	cnss_put_vreg_type(plat_priv, CNSS_VREG_PRIM);
}

#if IS_ENABLED(CONFIG_ESOC) && IS_ENABLED(CONFIG_MSM_SUBSYSTEM_RESTART)
static int cnss_modem_notifier_nb(struct notifier_block *nb,
				  unsigned long code,
				  void *ss_handle)
{
	struct cnss_plat_data *plat_priv =
		container_of(nb, struct cnss_plat_data, modem_nb);
	struct cnss_esoc_info *esoc_info;

	cnss_pr_dbg("Modem notifier: event %lu\n", code);

	if (!plat_priv)
		return NOTIFY_DONE;

	esoc_info = &plat_priv->esoc_info;

	if (code == SUBSYS_AFTER_POWERUP)
		esoc_info->modem_current_status = 1;
	else if (code == SUBSYS_BEFORE_SHUTDOWN)
		esoc_info->modem_current_status = 0;
	else
		return NOTIFY_DONE;

	if (!cnss_bus_call_driver_modem_status(plat_priv,
					       esoc_info->modem_current_status))
		return NOTIFY_DONE;

	return NOTIFY_OK;
}

static int cnss_register_esoc(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct device *dev;
	struct cnss_esoc_info *esoc_info;
	struct esoc_desc *esoc_desc;
	const char *client_desc;

	dev = &plat_priv->plat_dev->dev;
	esoc_info = &plat_priv->esoc_info;

	esoc_info->notify_modem_status =
		of_property_read_bool(dev->of_node,
				      "qcom,notify-modem-status");

	if (!esoc_info->notify_modem_status)
		goto out;

	ret = of_property_read_string_index(dev->of_node, "esoc-names", 0,
					    &client_desc);
	if (ret) {
		cnss_pr_dbg("esoc-names is not defined in DT, skip!\n");
	} else {
		esoc_desc = devm_register_esoc_client(dev, client_desc);
		if (IS_ERR_OR_NULL(esoc_desc)) {
			ret = PTR_RET(esoc_desc);
			cnss_pr_err("Failed to register esoc_desc, err = %d\n",
				    ret);
			goto out;
		}
		esoc_info->esoc_desc = esoc_desc;
	}

	plat_priv->modem_nb.notifier_call = cnss_modem_notifier_nb;
	esoc_info->modem_current_status = 0;
	esoc_info->modem_notify_handler =
		subsys_notif_register_notifier(esoc_info->esoc_desc ?
					       esoc_info->esoc_desc->name :
					       "modem", &plat_priv->modem_nb);
	if (IS_ERR(esoc_info->modem_notify_handler)) {
		ret = PTR_ERR(esoc_info->modem_notify_handler);
		cnss_pr_err("Failed to register esoc notifier, err = %d\n",
			    ret);
		goto unreg_esoc;
	}

	return 0;
unreg_esoc:
	if (esoc_info->esoc_desc)
		devm_unregister_esoc_client(dev, esoc_info->esoc_desc);
out:
	return ret;
}

static void cnss_unregister_esoc(struct cnss_plat_data *plat_priv)
{
	struct device *dev;
	struct cnss_esoc_info *esoc_info;

	dev = &plat_priv->plat_dev->dev;
	esoc_info = &plat_priv->esoc_info;

	if (esoc_info->notify_modem_status)
		subsys_notif_unregister_notifier
		(esoc_info->modem_notify_handler,
		 &plat_priv->modem_nb);
	if (esoc_info->esoc_desc)
		devm_unregister_esoc_client(dev, esoc_info->esoc_desc);
}
#else
static inline int cnss_register_esoc(struct cnss_plat_data *plat_priv)
{
	return 0;
}

static inline void cnss_unregister_esoc(struct cnss_plat_data *plat_priv) {}
#endif

#if IS_ENABLED(CONFIG_MSM_SUBSYSTEM_RESTART)
static int cnss_subsys_powerup(const struct subsys_desc *subsys_desc)
{
	struct cnss_plat_data *plat_priv;
	int ret = 0;

	if (!subsys_desc->dev) {
		cnss_pr_err("dev from subsys_desc is NULL\n");
		return -ENODEV;
	}

	plat_priv = dev_get_drvdata(subsys_desc->dev);
	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	if (!plat_priv->driver_state) {
		cnss_pr_dbg("Powerup is ignored\n");
		return 0;
	}

	ret = cnss_bus_dev_powerup(plat_priv);
	if (ret)
		__pm_relax(plat_priv->recovery_ws);
	return ret;
}

static int cnss_subsys_shutdown(const struct subsys_desc *subsys_desc,
				bool force_stop)
{
	struct cnss_plat_data *plat_priv;

	if (!subsys_desc->dev) {
		cnss_pr_err("dev from subsys_desc is NULL\n");
		return -ENODEV;
	}

	plat_priv = dev_get_drvdata(subsys_desc->dev);
	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	if (!plat_priv->driver_state) {
		cnss_pr_dbg("shutdown is ignored\n");
		return 0;
	}

	return cnss_bus_dev_shutdown(plat_priv);
}

void cnss_device_crashed(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	struct cnss_subsys_info *subsys_info;

	if (!plat_priv)
		return;

	subsys_info = &plat_priv->subsys_info;
	if (subsys_info->subsys_device) {
		set_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
		subsys_set_crash_status(subsys_info->subsys_device, true);
		subsystem_restart_dev(subsys_info->subsys_device);
	}
}
EXPORT_SYMBOL(cnss_device_crashed);

static void cnss_subsys_crash_shutdown(const struct subsys_desc *subsys_desc)
{
	struct cnss_plat_data *plat_priv = dev_get_drvdata(subsys_desc->dev);

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return;
	}

	cnss_bus_dev_crash_shutdown(plat_priv);
}

static int cnss_subsys_ramdump(int enable,
			       const struct subsys_desc *subsys_desc)
{
	struct cnss_plat_data *plat_priv = dev_get_drvdata(subsys_desc->dev);

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	if (!enable)
		return 0;

	return cnss_bus_dev_ramdump(plat_priv);
}

static void cnss_recovery_work_handler(struct work_struct *work)
{
}
#else
static void cnss_recovery_work_handler(struct work_struct *work)
{
	int ret;

	struct cnss_plat_data *plat_priv =
		container_of(work, struct cnss_plat_data, recovery_work);

	if (!plat_priv->recovery_enabled)
		panic("subsys-restart: Resetting the SoC wlan crashed\n");

	cnss_bus_dev_shutdown(plat_priv);
	cnss_bus_dev_ramdump(plat_priv);
	msleep(POWER_RESET_MIN_DELAY_MS);

	ret = cnss_bus_dev_powerup(plat_priv);
	if (ret)
		__pm_relax(plat_priv->recovery_ws);

	return;
}

void cnss_device_crashed(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv)
		return;

	set_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
	schedule_work(&plat_priv->recovery_work);
}
EXPORT_SYMBOL(cnss_device_crashed);
#endif /* CONFIG_MSM_SUBSYSTEM_RESTART */

void *cnss_get_virt_ramdump_mem(struct device *dev, unsigned long *size)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	struct cnss_ramdump_info *ramdump_info;

	if (!plat_priv)
		return NULL;

	ramdump_info = &plat_priv->ramdump_info;
	*size = ramdump_info->ramdump_size;

	return ramdump_info->ramdump_va;
}
EXPORT_SYMBOL(cnss_get_virt_ramdump_mem);

static const char *cnss_recovery_reason_to_str(enum cnss_recovery_reason reason)
{
	switch (reason) {
	case CNSS_REASON_DEFAULT:
		return "DEFAULT";
	case CNSS_REASON_LINK_DOWN:
		return "LINK_DOWN";
	case CNSS_REASON_RDDM:
		return "RDDM";
	case CNSS_REASON_TIMEOUT:
		return "TIMEOUT";
	}

	return "UNKNOWN";
};

static int cnss_do_recovery(struct cnss_plat_data *plat_priv,
			    enum cnss_recovery_reason reason)
{
	plat_priv->recovery_count++;

	if (plat_priv->device_id == QCA6174_DEVICE_ID)
		goto self_recovery;

	if (test_bit(SKIP_RECOVERY, &plat_priv->ctrl_params.quirks)) {
		cnss_pr_dbg("Skip device recovery\n");
		return 0;
	}

	/* FW recovery sequence has multiple steps and firmware load requires
	 * linux PM in awake state. Thus hold the cnss wake source until
	 * WLAN MISSION enabled. CNSS_TIMEOUT_RECOVERY option should cover all
	 * time taken in this process.
	 */
	pm_wakeup_ws_event(plat_priv->recovery_ws,
			   cnss_get_timeout(plat_priv, CNSS_TIMEOUT_RECOVERY),
			   true);

	switch (reason) {
	case CNSS_REASON_LINK_DOWN:
		if (!cnss_bus_check_link_status(plat_priv)) {
			cnss_pr_dbg("Skip link down recovery as link is already up\n");
			return 0;
		}
		if (test_bit(LINK_DOWN_SELF_RECOVERY,
			     &plat_priv->ctrl_params.quirks))
			goto self_recovery;
		if (!cnss_bus_recover_link_down(plat_priv)) {
			/* clear recovery bit here to avoid skipping
			 * the recovery work for RDDM later
			 */
			clear_bit(CNSS_DRIVER_RECOVERY,
				  &plat_priv->driver_state);
			return 0;
		}
		break;
	case CNSS_REASON_RDDM:
		cnss_bus_collect_dump_info(plat_priv, false);
		break;
	case CNSS_REASON_DEFAULT:
	case CNSS_REASON_TIMEOUT:
		break;
	default:
		cnss_pr_err("Unsupported recovery reason: %s(%d)\n",
			    cnss_recovery_reason_to_str(reason), reason);
		break;
	}
	cnss_bus_device_crashed(plat_priv);

	return 0;

self_recovery:
	cnss_pr_dbg("Going for self recovery\n");
	cnss_bus_dev_shutdown(plat_priv);

	if (test_bit(LINK_DOWN_SELF_RECOVERY, &plat_priv->ctrl_params.quirks))
		clear_bit(LINK_DOWN_SELF_RECOVERY,
			  &plat_priv->ctrl_params.quirks);

	cnss_bus_dev_powerup(plat_priv);

	return 0;
}

static int cnss_driver_recovery_hdlr(struct cnss_plat_data *plat_priv,
				     void *data)
{
	struct cnss_recovery_data *recovery_data = data;
	int ret = 0;

	cnss_pr_dbg("Driver recovery is triggered with reason: %s(%d)\n",
		    cnss_recovery_reason_to_str(recovery_data->reason),
		    recovery_data->reason);

	if (!plat_priv->driver_state) {
		cnss_pr_err("Improper driver state, ignore recovery\n");
		ret = -EINVAL;
		goto out;
	}

	if (test_bit(CNSS_IN_REBOOT, &plat_priv->driver_state)) {
		cnss_pr_err("Reboot is in progress, ignore recovery\n");
		ret = -EINVAL;
		goto out;
	}

	if (test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state)) {
		cnss_pr_err("Recovery is already in progress\n");
		CNSS_ASSERT(0);
		ret = -EINVAL;
		goto out;
	}

	if (test_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state) ||
	    test_bit(CNSS_DRIVER_IDLE_SHUTDOWN, &plat_priv->driver_state)) {
		cnss_pr_err("Driver unload or idle shutdown is in progress, ignore recovery\n");
		ret = -EINVAL;
		goto out;
	}

	switch (plat_priv->device_id) {
	case QCA6174_DEVICE_ID:
		if (test_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state) ||
		    test_bit(CNSS_DRIVER_IDLE_RESTART,
			     &plat_priv->driver_state)) {
			cnss_pr_err("Driver load or idle restart is in progress, ignore recovery\n");
			ret = -EINVAL;
			goto out;
		}
		break;
	default:
		if (!test_bit(CNSS_FW_READY, &plat_priv->driver_state)) {
			set_bit(CNSS_FW_BOOT_RECOVERY,
				&plat_priv->driver_state);
		}
		break;
	}

	set_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
	ret = cnss_do_recovery(plat_priv, recovery_data->reason);

out:
	kfree(data);
	return ret;
}

int cnss_self_recovery(struct device *dev,
		       enum cnss_recovery_reason reason)
{
	cnss_schedule_recovery(dev, reason);
	return 0;
}
EXPORT_SYMBOL(cnss_self_recovery);

void cnss_schedule_recovery(struct device *dev,
			    enum cnss_recovery_reason reason)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	struct cnss_recovery_data *data;
	int gfp = GFP_KERNEL;

	if (!test_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state))
		cnss_bus_update_status(plat_priv, CNSS_FW_DOWN);

	if (test_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state) ||
	    test_bit(CNSS_DRIVER_IDLE_SHUTDOWN, &plat_priv->driver_state)) {
		cnss_pr_dbg("Driver unload or idle shutdown is in progress, ignore schedule recovery\n");
		return;
	}

	if (in_interrupt() || irqs_disabled())
		gfp = GFP_ATOMIC;

	data = kzalloc(sizeof(*data), gfp);
	if (!data)
		return;

	data->reason = reason;
	cnss_driver_event_post(plat_priv,
			       CNSS_DRIVER_EVENT_RECOVERY,
			       0, data);
}
EXPORT_SYMBOL(cnss_schedule_recovery);

int cnss_force_fw_assert(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	if (plat_priv->device_id == QCA6174_DEVICE_ID) {
		cnss_pr_info("Forced FW assert is not supported\n");
		return -EOPNOTSUPP;
	}

	if (cnss_bus_is_device_down(plat_priv)) {
		cnss_pr_info("Device is already in bad state, ignore force assert\n");
		return 0;
	}

	if (test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state)) {
		cnss_pr_info("Recovery is already in progress, ignore forced FW assert\n");
		return 0;
	}

	if (in_interrupt() || irqs_disabled())
		cnss_driver_event_post(plat_priv,
				       CNSS_DRIVER_EVENT_FORCE_FW_ASSERT,
				       0, NULL);
	else
		cnss_bus_force_fw_assert_hdlr(plat_priv);

	return 0;
}
EXPORT_SYMBOL(cnss_force_fw_assert);

int cnss_force_collect_rddm(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	unsigned int timeout;
	int ret = 0;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	if (plat_priv->device_id == QCA6174_DEVICE_ID) {
		cnss_pr_info("Force collect rddm is not supported\n");
		return -EOPNOTSUPP;
	}

	if (cnss_bus_is_device_down(plat_priv)) {
		cnss_pr_info("Device is already in bad state, wait to collect rddm\n");
		goto wait_rddm;
	}

	if (test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state)) {
		cnss_pr_info("Recovery is already in progress, wait to collect rddm\n");
		goto wait_rddm;
	}

	if (test_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state) ||
	    test_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state) ||
	    test_bit(CNSS_DRIVER_IDLE_RESTART, &plat_priv->driver_state) ||
	    test_bit(CNSS_DRIVER_IDLE_SHUTDOWN, &plat_priv->driver_state)) {
		cnss_pr_info("Loading/Unloading/idle restart/shutdown is in progress, ignore forced collect rddm\n");
		return 0;
	}

	ret = cnss_bus_force_fw_assert_hdlr(plat_priv);
	if (ret)
		return ret;

wait_rddm:
	reinit_completion(&plat_priv->rddm_complete);
	timeout = cnss_get_timeout(plat_priv, CNSS_TIMEOUT_RDDM);
	ret = wait_for_completion_timeout(&plat_priv->rddm_complete,
					  msecs_to_jiffies(timeout));
	if (!ret) {
		cnss_pr_err("Timeout (%ums) waiting for RDDM to complete\n",
			    timeout);
		ret = -ETIMEDOUT;
	} else if (ret > 0) {
		ret = 0;
	}

	return ret;
}
EXPORT_SYMBOL(cnss_force_collect_rddm);

int cnss_qmi_send_get(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!test_bit(CNSS_QMI_WLFW_CONNECTED, &plat_priv->driver_state))
		return 0;

	return cnss_bus_qmi_send_get(plat_priv);
}
EXPORT_SYMBOL(cnss_qmi_send_get);

int cnss_qmi_send_put(struct device *dev)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);

	if (!test_bit(CNSS_QMI_WLFW_CONNECTED, &plat_priv->driver_state))
		return 0;

	return cnss_bus_qmi_send_put(plat_priv);
}
EXPORT_SYMBOL(cnss_qmi_send_put);

int cnss_qmi_send(struct device *dev, int type, void *cmd,
		  int cmd_len, void *cb_ctx,
		  int (*cb)(void *ctx, void *event, int event_len))
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(dev);
	int ret;

	if (!plat_priv)
		return -ENODEV;

	if (!test_bit(CNSS_QMI_WLFW_CONNECTED, &plat_priv->driver_state))
		return -EINVAL;

	plat_priv->get_info_cb = cb;
	plat_priv->get_info_cb_ctx = cb_ctx;

	ret = cnss_wlfw_get_info_send_sync(plat_priv, type, cmd, cmd_len);
	if (ret) {
		plat_priv->get_info_cb = NULL;
		plat_priv->get_info_cb_ctx = NULL;
	}

	return ret;
}
EXPORT_SYMBOL(cnss_qmi_send);

static int cnss_cold_boot_cal_start_hdlr(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	u32 retry = 0;

	if (test_bit(CNSS_COLD_BOOT_CAL_DONE, &plat_priv->driver_state)) {
		cnss_pr_dbg("Calibration complete. Ignore calibration req\n");
		goto out;
	} else if (test_bit(CNSS_IN_COLD_BOOT_CAL, &plat_priv->driver_state)) {
		cnss_pr_dbg("Calibration in progress. Ignore new calibration req\n");
		goto out;
	}

	if (test_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state) ||
	    test_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state) ||
	    test_bit(CNSS_FW_READY, &plat_priv->driver_state)) {
		cnss_pr_err("WLAN in mission mode before cold boot calibration\n");
		CNSS_ASSERT(0);
		return -EINVAL;
	}

	while (retry++ < CNSS_CAL_START_PROBE_WAIT_RETRY_MAX) {
		if (test_bit(CNSS_PCI_PROBE_DONE, &plat_priv->driver_state))
			break;
		msleep(CNSS_CAL_START_PROBE_WAIT_MS);

		if (retry == CNSS_CAL_START_PROBE_WAIT_RETRY_MAX) {
			cnss_pr_err("Calibration start failed as PCI probe not complete\n");
			CNSS_ASSERT(0);
			ret = -EINVAL;
			goto mark_cal_fail;
		}
	}

	set_bit(CNSS_IN_COLD_BOOT_CAL, &plat_priv->driver_state);
	reinit_completion(&plat_priv->cal_complete);
	ret = cnss_bus_dev_powerup(plat_priv);
mark_cal_fail:
	if (ret) {
		complete(&plat_priv->cal_complete);
		clear_bit(CNSS_IN_COLD_BOOT_CAL, &plat_priv->driver_state);
		/* Set CBC done in driver state to mark attempt and note error
		 * since calibration cannot be retried at boot.
		 */
		plat_priv->cal_done = CNSS_CAL_FAILURE;
		set_bit(CNSS_COLD_BOOT_CAL_DONE, &plat_priv->driver_state);
	}

out:
	return ret;
}

static int cnss_cold_boot_cal_done_hdlr(struct cnss_plat_data *plat_priv,
					void *data)
{
	struct cnss_cal_info *cal_info = data;

	if (!test_bit(CNSS_IN_COLD_BOOT_CAL, &plat_priv->driver_state) ||
	    test_bit(CNSS_COLD_BOOT_CAL_DONE, &plat_priv->driver_state))
		goto out;

	switch (cal_info->cal_status) {
	case CNSS_CAL_DONE:
		cnss_pr_dbg("Calibration completed successfully\n");
		plat_priv->cal_done = true;
		break;
	case CNSS_CAL_TIMEOUT:
	case CNSS_CAL_FAILURE:
		cnss_pr_dbg("Calibration failed. Status: %d, force shutdown\n",
			    cal_info->cal_status);
		break;
	default:
		cnss_pr_err("Unknown calibration status: %u\n",
			    cal_info->cal_status);
		break;
	}

	cnss_wlfw_wlan_mode_send_sync(plat_priv, CNSS_OFF);
	cnss_bus_free_qdss_mem(plat_priv);
	cnss_release_antenna_sharing(plat_priv);
	cnss_bus_dev_shutdown(plat_priv);
	msleep(POWER_RESET_MIN_DELAY_MS);
	complete(&plat_priv->cal_complete);
	clear_bit(CNSS_IN_COLD_BOOT_CAL, &plat_priv->driver_state);
	set_bit(CNSS_COLD_BOOT_CAL_DONE, &plat_priv->driver_state);

	if (cal_info->cal_status == CNSS_CAL_DONE) {
		cnss_cal_mem_upload_to_file(plat_priv);
		if (cancel_delayed_work_sync(&plat_priv->wlan_reg_driver_work)
		   ) {
			cnss_pr_dbg("Schedule WLAN driver load\n");
			schedule_delayed_work(&plat_priv->wlan_reg_driver_work,
					      0);
		}
	}
out:
	kfree(data);
	return 0;
}

static int cnss_power_up_hdlr(struct cnss_plat_data *plat_priv)
{
	int ret;

	ret = cnss_bus_dev_powerup(plat_priv);
	if (ret)
		clear_bit(CNSS_DRIVER_IDLE_RESTART, &plat_priv->driver_state);

	return ret;
}

static int cnss_power_down_hdlr(struct cnss_plat_data *plat_priv)
{
	cnss_bus_dev_shutdown(plat_priv);

	return 0;
}

static int cnss_qdss_trace_req_mem_hdlr(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	ret = cnss_bus_alloc_qdss_mem(plat_priv);
	if (ret < 0)
		return ret;

	return cnss_wlfw_qdss_trace_mem_info_send_sync(plat_priv);
}

static void *cnss_get_fw_mem_pa_to_va(struct cnss_fw_mem *fw_mem,
				      u32 mem_seg_len, u64 pa, u32 size)
{
	int i = 0;
	u64 offset = 0;
	void *va = NULL;
	u64 local_pa;
	u32 local_size;

	for (i = 0; i < mem_seg_len; i++) {
		local_pa = (u64)fw_mem[i].pa;
		local_size = (u32)fw_mem[i].size;
		if (pa == local_pa && size <= local_size) {
			va = fw_mem[i].va;
			break;
		}
		if (pa > local_pa &&
		    pa < local_pa + local_size &&
		    pa + size <= local_pa + local_size) {
			offset = pa - local_pa;
			va = fw_mem[i].va + offset;
			break;
		}
	}
	return va;
}

static int cnss_fw_mem_file_save_hdlr(struct cnss_plat_data *plat_priv,
				      void *data)
{
	struct cnss_qmi_event_fw_mem_file_save_data *event_data = data;
	struct cnss_fw_mem *fw_mem_seg;
	int ret = 0L;
	void *va = NULL;
	u32 i, fw_mem_seg_len;

	switch (event_data->mem_type) {
	case QMI_WLFW_MEM_TYPE_DDR_V01:
		if (!plat_priv->fw_mem_seg_len)
			goto invalid_mem_save;

		fw_mem_seg = plat_priv->fw_mem;
		fw_mem_seg_len = plat_priv->fw_mem_seg_len;
		break;
	case QMI_WLFW_MEM_QDSS_V01:
		if (!plat_priv->qdss_mem_seg_len)
			goto invalid_mem_save;

		fw_mem_seg = plat_priv->qdss_mem;
		fw_mem_seg_len = plat_priv->qdss_mem_seg_len;
		break;
	default:
		goto invalid_mem_save;
	}

	for (i = 0; i < event_data->mem_seg_len; i++) {
		va = cnss_get_fw_mem_pa_to_va(fw_mem_seg, fw_mem_seg_len,
					      event_data->mem_seg[i].addr,
					      event_data->mem_seg[i].size);
		if (!va) {
			cnss_pr_err("Fail to find matching va of pa %pa for mem type: %d\n",
				    &event_data->mem_seg[i].addr,
				    event_data->mem_type);
			ret = -EINVAL;
			break;
		}
		ret = cnss_genl_send_msg(va, CNSS_GENL_MSG_TYPE_QDSS,
					 event_data->file_name,
					 event_data->mem_seg[i].size);
		if (ret < 0) {
			cnss_pr_err("Fail to save fw mem data: %d\n",
				    ret);
			break;
		}
	}
	kfree(data);
	return ret;

invalid_mem_save:
	cnss_pr_err("FW Mem type %d not allocated. Invalid save request\n",
		    event_data->mem_type);
	kfree(data);
	return -EINVAL;
}

static int cnss_qdss_trace_free_hdlr(struct cnss_plat_data *plat_priv)
{
	cnss_bus_free_qdss_mem(plat_priv);

	return 0;
}

static int cnss_qdss_trace_req_data_hdlr(struct cnss_plat_data *plat_priv,
					 void *data)
{
	int ret = 0;
	struct cnss_qmi_event_fw_mem_file_save_data *event_data = data;

	if (!plat_priv)
		return -ENODEV;

	ret = cnss_wlfw_qdss_data_send_sync(plat_priv, event_data->file_name,
					    event_data->total_size);

	kfree(data);
	return ret;
}

static void cnss_driver_event_work(struct work_struct *work)
{
	struct cnss_plat_data *plat_priv =
		container_of(work, struct cnss_plat_data, event_work);
	struct cnss_driver_event *event;
	unsigned long flags;
	int ret = 0;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		return;
	}

	cnss_pm_stay_awake(plat_priv);

	spin_lock_irqsave(&plat_priv->event_lock, flags);

	while (!list_empty(&plat_priv->event_list)) {
		event = list_first_entry(&plat_priv->event_list,
					 struct cnss_driver_event, list);
		list_del(&event->list);
		spin_unlock_irqrestore(&plat_priv->event_lock, flags);

		cnss_pr_dbg("Processing driver event: %s%s(%d), state: 0x%lx\n",
			    cnss_driver_event_to_str(event->type),
			    event->sync ? "-sync" : "", event->type,
			    plat_priv->driver_state);

		switch (event->type) {
		case CNSS_DRIVER_EVENT_SERVER_ARRIVE:
			ret = cnss_wlfw_server_arrive(plat_priv, event->data);
			break;
		case CNSS_DRIVER_EVENT_SERVER_EXIT:
			ret = cnss_wlfw_server_exit(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_REQUEST_MEM:
			ret = cnss_bus_alloc_fw_mem(plat_priv);
			if (ret)
				break;
			ret = cnss_wlfw_respond_mem_send_sync(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_FW_MEM_READY:
			ret = cnss_fw_mem_ready_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_FW_READY:
			ret = cnss_fw_ready_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_COLD_BOOT_CAL_START:
			ret = cnss_cold_boot_cal_start_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_COLD_BOOT_CAL_DONE:
			ret = cnss_cold_boot_cal_done_hdlr(plat_priv,
							   event->data);
			break;
		case CNSS_DRIVER_EVENT_REGISTER_DRIVER:
			ret = cnss_bus_register_driver_hdlr(plat_priv,
							    event->data);
			break;
		case CNSS_DRIVER_EVENT_UNREGISTER_DRIVER:
			ret = cnss_bus_unregister_driver_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_RECOVERY:
			ret = cnss_driver_recovery_hdlr(plat_priv,
							event->data);
			break;
		case CNSS_DRIVER_EVENT_FORCE_FW_ASSERT:
			ret = cnss_bus_force_fw_assert_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_IDLE_RESTART:
			set_bit(CNSS_DRIVER_IDLE_RESTART,
				&plat_priv->driver_state);
			/* fall through */
		case CNSS_DRIVER_EVENT_POWER_UP:
			ret = cnss_power_up_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_IDLE_SHUTDOWN:
			set_bit(CNSS_DRIVER_IDLE_SHUTDOWN,
				&plat_priv->driver_state);
			/* fall through */
		case CNSS_DRIVER_EVENT_POWER_DOWN:
			ret = cnss_power_down_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_IMS_WFC_CALL_IND:
			ret = cnss_process_wfc_call_ind_event(plat_priv,
							      event->data);
			break;
		case CNSS_DRIVER_EVENT_WLFW_TWT_CFG_IND:
			ret = cnss_process_twt_cfg_ind_event(plat_priv,
							     event->data);
			break;
		case CNSS_DRIVER_EVENT_QDSS_TRACE_REQ_MEM:
			ret = cnss_qdss_trace_req_mem_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_FW_MEM_FILE_SAVE:
			ret = cnss_fw_mem_file_save_hdlr(plat_priv,
							 event->data);
			break;
		case CNSS_DRIVER_EVENT_QDSS_TRACE_FREE:
			ret = cnss_qdss_trace_free_hdlr(plat_priv);
			break;
		case CNSS_DRIVER_EVENT_QDSS_TRACE_REQ_DATA:
			ret = cnss_qdss_trace_req_data_hdlr(plat_priv,
							    event->data);
			break;
		default:
			cnss_pr_err("Invalid driver event type: %d",
				    event->type);
			kfree(event);
			spin_lock_irqsave(&plat_priv->event_lock, flags);
			continue;
		}

		spin_lock_irqsave(&plat_priv->event_lock, flags);
		if (event->sync) {
			event->ret = ret;
			complete(&event->complete);
			continue;
		}
		spin_unlock_irqrestore(&plat_priv->event_lock, flags);

		kfree(event);

		spin_lock_irqsave(&plat_priv->event_lock, flags);
	}
	spin_unlock_irqrestore(&plat_priv->event_lock, flags);

	cnss_pm_relax(plat_priv);
}

int cnss_va_to_pa(struct device *dev, size_t size, void *va, dma_addr_t dma,
		  phys_addr_t *pa, unsigned long attrs)
{
	struct sg_table sgt;
	int ret;

	ret = dma_get_sgtable_attrs(dev, &sgt, va, dma, size, attrs);
	if (ret) {
		cnss_pr_err("Failed to get sgtable for va: 0x%pK, dma: %pa, size: 0x%zx, attrs: 0x%x\n",
			    va, &dma, size, attrs);
		return -EINVAL;
	}

	*pa = page_to_phys(sg_page(sgt.sgl));
	sg_free_table(&sgt);

	return 0;
}

#if IS_ENABLED(CONFIG_MSM_SUBSYSTEM_RESTART)
int cnss_register_subsys(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct cnss_subsys_info *subsys_info;

	subsys_info = &plat_priv->subsys_info;

	subsys_info->subsys_desc.name = "wlan";
	subsys_info->subsys_desc.owner = THIS_MODULE;
	subsys_info->subsys_desc.powerup = cnss_subsys_powerup;
	subsys_info->subsys_desc.shutdown = cnss_subsys_shutdown;
	subsys_info->subsys_desc.ramdump = cnss_subsys_ramdump;
	subsys_info->subsys_desc.crash_shutdown = cnss_subsys_crash_shutdown;
	subsys_info->subsys_desc.dev = &plat_priv->plat_dev->dev;

	subsys_info->subsys_device = subsys_register(&subsys_info->subsys_desc);
	if (IS_ERR(subsys_info->subsys_device)) {
		ret = PTR_ERR(subsys_info->subsys_device);
		cnss_pr_err("Failed to register subsys, err = %d\n", ret);
		goto out;
	}

	subsys_info->subsys_handle =
		subsystem_get(subsys_info->subsys_desc.name);
	if (!subsys_info->subsys_handle) {
		cnss_pr_err("Failed to get subsys_handle!\n");
		ret = -EINVAL;
		goto unregister_subsys;
	} else if (IS_ERR(subsys_info->subsys_handle)) {
		ret = PTR_ERR(subsys_info->subsys_handle);
		cnss_pr_err("Failed to do subsystem_get, err = %d\n", ret);
		goto unregister_subsys;
	}

	return 0;

unregister_subsys:
	subsys_unregister(subsys_info->subsys_device);
out:
	return ret;
}

void cnss_unregister_subsys(struct cnss_plat_data *plat_priv)
{
	struct cnss_subsys_info *subsys_info;

	subsys_info = &plat_priv->subsys_info;
	subsystem_put(subsys_info->subsys_handle);
	subsys_unregister(subsys_info->subsys_device);
}

static void *cnss_create_ramdump_device(struct cnss_plat_data *plat_priv)
{
	struct cnss_subsys_info *subsys_info = &plat_priv->subsys_info;

	return create_ramdump_device(subsys_info->subsys_desc.name,
				     subsys_info->subsys_desc.dev);
}

static void cnss_destroy_ramdump_device(struct cnss_plat_data *plat_priv,
					void *ramdump_dev)
{
	destroy_ramdump_device(ramdump_dev);
}

int cnss_do_ramdump(struct cnss_plat_data *plat_priv)
{
	struct cnss_ramdump_info *ramdump_info = &plat_priv->ramdump_info;
	struct ramdump_segment segment;

	memset(&segment, 0, sizeof(segment));
	segment.v_address = (void __iomem *)ramdump_info->ramdump_va;
	segment.size = ramdump_info->ramdump_size;

	return qcom_ramdump(ramdump_info->ramdump_dev, &segment, 1);
}

int cnss_do_elf_ramdump(struct cnss_plat_data *plat_priv)
{
	struct cnss_ramdump_info_v2 *info_v2 = &plat_priv->ramdump_info_v2;
	struct cnss_dump_data *dump_data = &info_v2->dump_data;
	struct cnss_dump_seg *dump_seg = info_v2->dump_data_vaddr;
	struct ramdump_segment *ramdump_segs, *s;
	struct cnss_dump_meta_info meta_info = {0};
	int i, ret = 0;

	ramdump_segs = kcalloc(dump_data->nentries + 1,
			       sizeof(*ramdump_segs),
			       GFP_KERNEL);
	if (!ramdump_segs)
		return -ENOMEM;

	s = ramdump_segs + 1;
	for (i = 0; i < dump_data->nentries; i++) {
		if (dump_seg->type >= CNSS_FW_DUMP_TYPE_MAX) {
			cnss_pr_err("Unsupported dump type: %d",
				    dump_seg->type);
			continue;
		}

		if (meta_info.entry[dump_seg->type].entry_start == 0) {
			meta_info.entry[dump_seg->type].type = dump_seg->type;
			meta_info.entry[dump_seg->type].entry_start = i + 1;
		}
		meta_info.entry[dump_seg->type].entry_num++;

		s->address = dump_seg->address;
		s->v_address = (void __iomem *)dump_seg->v_address;
		s->size = dump_seg->size;
		s++;
		dump_seg++;
	}

	meta_info.magic = CNSS_RAMDUMP_MAGIC;
	meta_info.version = CNSS_RAMDUMP_VERSION;
	meta_info.chipset = plat_priv->device_id;
	meta_info.total_entries = CNSS_FW_DUMP_TYPE_MAX;

	ramdump_segs->v_address = (void __iomem *)(&meta_info);
	ramdump_segs->size = sizeof(meta_info);

	ret = qcom_elf_ramdump(info_v2->ramdump_dev, ramdump_segs,
			       dump_data->nentries + 1);
	kfree(ramdump_segs);

	return ret;
}
#else
static int cnss_panic_handler(struct notifier_block *nb, unsigned long action,
			      void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(nb, struct cnss_plat_data, panic_nb);

	cnss_bus_dev_crash_shutdown(plat_priv);

	return NOTIFY_DONE;
}

int cnss_register_subsys(struct cnss_plat_data *plat_priv)
{
	int ret;

	if (!plat_priv)
		return -ENODEV;

	plat_priv->panic_nb.notifier_call = cnss_panic_handler;
	ret = atomic_notifier_chain_register(&panic_notifier_list,
					     &plat_priv->panic_nb);
	if (ret) {
		cnss_pr_err("Failed to register panic handler\n");
		return -EINVAL;
	}

	return 0;
}

void cnss_unregister_subsys(struct cnss_plat_data *plat_priv)
{
	int ret;

	ret = atomic_notifier_chain_unregister(&panic_notifier_list,
					       &plat_priv->panic_nb);
	if (ret)
		cnss_pr_err("Failed to unregister panic handler\n");
}

#if IS_ENABLED(CONFIG_QCOM_MEMORY_DUMP_V2)
static void *cnss_create_ramdump_device(struct cnss_plat_data *plat_priv)
{
	return &plat_priv->plat_dev->dev;
}

static void cnss_destroy_ramdump_device(struct cnss_plat_data *plat_priv,
					void *ramdump_dev)
{
}
#endif

#if IS_ENABLED(CONFIG_QCOM_RAMDUMP)
int cnss_do_ramdump(struct cnss_plat_data *plat_priv)
{
	struct cnss_ramdump_info *ramdump_info = &plat_priv->ramdump_info;
	struct qcom_dump_segment segment;
	struct list_head head;

	INIT_LIST_HEAD(&head);
	memset(&segment, 0, sizeof(segment));
	segment.va = ramdump_info->ramdump_va;
	segment.size = ramdump_info->ramdump_size;
	list_add(&segment.node, &head);

	return qcom_dump(&head, ramdump_info->ramdump_dev);
}

int cnss_do_elf_ramdump(struct cnss_plat_data *plat_priv)
{
	struct cnss_ramdump_info_v2 *info_v2 = &plat_priv->ramdump_info_v2;
	struct cnss_dump_data *dump_data = &info_v2->dump_data;
	struct cnss_dump_seg *dump_seg = info_v2->dump_data_vaddr;
	struct qcom_dump_segment *seg;
	struct cnss_dump_meta_info meta_info = {0};
	struct list_head head;
	int i, ret = 0;

	if (!dump_enabled()) {
		cnss_pr_info("Dump collection is not enabled\n");
		return ret;
	}

	INIT_LIST_HEAD(&head);
	for (i = 0; i < dump_data->nentries; i++) {
		if (dump_seg->type >= CNSS_FW_DUMP_TYPE_MAX) {
			cnss_pr_err("Unsupported dump type: %d",
				    dump_seg->type);
			continue;
		}

		seg = kcalloc(1, sizeof(*seg), GFP_KERNEL);
		if (!seg)
			continue;

		if (meta_info.entry[dump_seg->type].entry_start == 0) {
			meta_info.entry[dump_seg->type].type = dump_seg->type;
			meta_info.entry[dump_seg->type].entry_start = i + 1;
		}
		meta_info.entry[dump_seg->type].entry_num++;
		seg->da = dump_seg->address;
		seg->va = dump_seg->v_address;
		seg->size = dump_seg->size;
		list_add_tail(&seg->node, &head);
		dump_seg++;
	}

	seg = kcalloc(1, sizeof(*seg), GFP_KERNEL);
	if (!seg)
		goto do_elf_dump;

	meta_info.magic = CNSS_RAMDUMP_MAGIC;
	meta_info.version = CNSS_RAMDUMP_VERSION;
	meta_info.chipset = plat_priv->device_id;
	meta_info.total_entries = CNSS_FW_DUMP_TYPE_MAX;
	seg->va = &meta_info;
	seg->size = sizeof(meta_info);
	list_add(&seg->node, &head);

do_elf_dump:
	ret = qcom_elf_dump(&head, info_v2->ramdump_dev, ELF_CLASS);

	while (!list_empty(&head)) {
		seg = list_first_entry(&head, struct qcom_dump_segment, node);
		list_del(&seg->node);
		kfree(seg);
	}

	return ret;
}
#else
int cnss_do_ramdump(struct cnss_plat_data *plat_priv)
{
	return 0;
}

int cnss_do_elf_ramdump(struct cnss_plat_data *plat_priv)
{
	return 0;
}
#endif /* CONFIG_QCOM_RAMDUMP */
#endif /* CONFIG_MSM_SUBSYSTEM_RESTART */

#if IS_ENABLED(CONFIG_QCOM_MEMORY_DUMP_V2)
static int cnss_init_dump_entry(struct cnss_plat_data *plat_priv)
{
	struct cnss_ramdump_info *ramdump_info;
	struct msm_dump_entry dump_entry;

	ramdump_info = &plat_priv->ramdump_info;
	ramdump_info->dump_data.addr = ramdump_info->ramdump_pa;
	ramdump_info->dump_data.len = ramdump_info->ramdump_size;
	ramdump_info->dump_data.version = CNSS_DUMP_FORMAT_VER;
	ramdump_info->dump_data.magic = CNSS_DUMP_MAGIC_VER_V2;
	strlcpy(ramdump_info->dump_data.name, CNSS_DUMP_NAME,
		sizeof(ramdump_info->dump_data.name));
	dump_entry.id = MSM_DUMP_DATA_CNSS_WLAN;
	dump_entry.addr = virt_to_phys(&ramdump_info->dump_data);

	return msm_dump_data_register_nominidump(MSM_DUMP_TABLE_APPS,
						&dump_entry);
}

static int cnss_register_ramdump_v1(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct device *dev;
	struct cnss_ramdump_info *ramdump_info;
	u32 ramdump_size = 0;

	dev = &plat_priv->plat_dev->dev;
	ramdump_info = &plat_priv->ramdump_info;

	if (of_property_read_u32(dev->of_node, "qcom,wlan-ramdump-dynamic",
				 &ramdump_size) == 0) {
		ramdump_info->ramdump_va =
			dma_alloc_coherent(dev, ramdump_size,
					   &ramdump_info->ramdump_pa,
					   GFP_KERNEL);

		if (ramdump_info->ramdump_va)
			ramdump_info->ramdump_size = ramdump_size;
	}

	cnss_pr_dbg("ramdump va: %pK, pa: %pa\n",
		    ramdump_info->ramdump_va, &ramdump_info->ramdump_pa);

	if (ramdump_info->ramdump_size == 0) {
		cnss_pr_info("Ramdump will not be collected");
		goto out;
	}

	ret = cnss_init_dump_entry(plat_priv);
	if (ret) {
		cnss_pr_err("Failed to setup dump table, err = %d\n", ret);
		goto free_ramdump;
	}

	ramdump_info->ramdump_dev = cnss_create_ramdump_device(plat_priv);
	if (!ramdump_info->ramdump_dev) {
		cnss_pr_err("Failed to create ramdump device!");
		ret = -ENOMEM;
		goto free_ramdump;
	}

	return 0;
free_ramdump:
	dma_free_coherent(dev, ramdump_info->ramdump_size,
			  ramdump_info->ramdump_va, ramdump_info->ramdump_pa);
out:
	return ret;
}

static void cnss_unregister_ramdump_v1(struct cnss_plat_data *plat_priv)
{
	struct device *dev;
	struct cnss_ramdump_info *ramdump_info;

	dev = &plat_priv->plat_dev->dev;
	ramdump_info = &plat_priv->ramdump_info;

	if (ramdump_info->ramdump_dev)
		cnss_destroy_ramdump_device(plat_priv,
					    ramdump_info->ramdump_dev);

	if (ramdump_info->ramdump_va)
		dma_free_coherent(dev, ramdump_info->ramdump_size,
				  ramdump_info->ramdump_va,
				  ramdump_info->ramdump_pa);
}

/**
 * cnss_ignore_dump_data_reg_fail - Ignore Ramdump table register failure
 * @ret: Error returned by msm_dump_data_register_nominidump
 *
 * For Lahaina GKI boot, we dont have support for mem dump feature. So
 * ignore failure.
 *
 * Return: Same given error code if mem dump feature enabled, 0 otherwise
 */
static int cnss_ignore_dump_data_reg_fail(int ret)
{
	return ret;
}

static int cnss_register_ramdump_v2(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct cnss_ramdump_info_v2 *info_v2;
	struct cnss_dump_data *dump_data;
	struct msm_dump_entry dump_entry;
	struct device *dev = &plat_priv->plat_dev->dev;
	u32 ramdump_size = 0;

	info_v2 = &plat_priv->ramdump_info_v2;
	dump_data = &info_v2->dump_data;

	if (of_property_read_u32(dev->of_node, "qcom,wlan-ramdump-dynamic",
				 &ramdump_size) == 0)
		info_v2->ramdump_size = ramdump_size;

	cnss_pr_dbg("Ramdump size 0x%lx\n", info_v2->ramdump_size);

	info_v2->dump_data_vaddr = kzalloc(CNSS_DUMP_DESC_SIZE, GFP_KERNEL);
	if (!info_v2->dump_data_vaddr)
		return -ENOMEM;

	dump_data->paddr = virt_to_phys(info_v2->dump_data_vaddr);
	dump_data->version = CNSS_DUMP_FORMAT_VER_V2;
	dump_data->magic = CNSS_DUMP_MAGIC_VER_V2;
	dump_data->seg_version = CNSS_DUMP_SEG_VER;
	strlcpy(dump_data->name, CNSS_DUMP_NAME,
		sizeof(dump_data->name));
	dump_entry.id = MSM_DUMP_DATA_CNSS_WLAN;
	dump_entry.addr = virt_to_phys(dump_data);

	ret = msm_dump_data_register_nominidump(MSM_DUMP_TABLE_APPS,
						&dump_entry);
	if (ret) {
		ret = cnss_ignore_dump_data_reg_fail(ret);
		cnss_pr_err("Failed to setup dump table, %s (%d)\n",
			    ret ? "Error" : "Ignoring", ret);
		goto free_ramdump;
	}

	info_v2->ramdump_dev = cnss_create_ramdump_device(plat_priv);
	if (!info_v2->ramdump_dev) {
		cnss_pr_err("Failed to create ramdump device!\n");
		ret = -ENOMEM;
		goto free_ramdump;
	}

	return 0;

free_ramdump:
	kfree(info_v2->dump_data_vaddr);
	info_v2->dump_data_vaddr = NULL;
	return ret;
}

static void cnss_unregister_ramdump_v2(struct cnss_plat_data *plat_priv)
{
	struct cnss_ramdump_info_v2 *info_v2;

	info_v2 = &plat_priv->ramdump_info_v2;

	if (info_v2->ramdump_dev)
		cnss_destroy_ramdump_device(plat_priv, info_v2->ramdump_dev);

	kfree(info_v2->dump_data_vaddr);
	info_v2->dump_data_vaddr = NULL;
	info_v2->dump_data_valid = false;
}

int cnss_register_ramdump(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	switch (plat_priv->device_id) {
	case QCA6174_DEVICE_ID:
		ret = cnss_register_ramdump_v1(plat_priv);
		break;
	case QCA6290_DEVICE_ID:
	case QCA6390_DEVICE_ID:
	case QCA6490_DEVICE_ID:
	case WCN7850_DEVICE_ID:
		ret = cnss_register_ramdump_v2(plat_priv);
		break;
	default:
		cnss_pr_err("Unknown device ID: 0x%lx\n", plat_priv->device_id);
		ret = -ENODEV;
		break;
	}
	return ret;
}

void cnss_unregister_ramdump(struct cnss_plat_data *plat_priv)
{
	switch (plat_priv->device_id) {
	case QCA6174_DEVICE_ID:
		cnss_unregister_ramdump_v1(plat_priv);
		break;
	case QCA6290_DEVICE_ID:
	case QCA6390_DEVICE_ID:
	case QCA6490_DEVICE_ID:
	case WCN7850_DEVICE_ID:
		cnss_unregister_ramdump_v2(plat_priv);
		break;
	default:
		cnss_pr_err("Unknown device ID: 0x%lx\n", plat_priv->device_id);
		break;
	}
}
#else
int cnss_register_ramdump(struct cnss_plat_data *plat_priv)
{
	return 0;
}

void cnss_unregister_ramdump(struct cnss_plat_data *plat_priv) {}
#endif /* CONFIG_QCOM_MEMORY_DUMP_V2 */

#if IS_ENABLED(CONFIG_QCOM_MINIDUMP)
int cnss_minidump_add_region(struct cnss_plat_data *plat_priv,
			     enum cnss_fw_dump_type type, int seg_no,
			     void *va, phys_addr_t pa, size_t size)
{
	struct md_region md_entry;
	int ret;

	switch (type) {
	case CNSS_FW_IMAGE:
		snprintf(md_entry.name, sizeof(md_entry.name), "FBC_%X",
			 seg_no);
		break;
	case CNSS_FW_RDDM:
		snprintf(md_entry.name, sizeof(md_entry.name), "RDDM_%X",
			 seg_no);
		break;
	case CNSS_FW_REMOTE_HEAP:
		snprintf(md_entry.name, sizeof(md_entry.name), "RHEAP_%X",
			 seg_no);
		break;
	default:
		cnss_pr_err("Unknown dump type ID: %d\n", type);
		return -EINVAL;
	}

	md_entry.phys_addr = pa;
	md_entry.virt_addr = (uintptr_t)va;
	md_entry.size = size;
	md_entry.id = MSM_DUMP_DATA_CNSS_WLAN;

	cnss_pr_dbg("Mini dump region: %s, va: %pK, pa: %pa, size: 0x%zx\n",
		    md_entry.name, va, &pa, size);

	ret = msm_minidump_add_region(&md_entry);
	if (ret < 0)
		cnss_pr_err("Failed to add mini dump region, err = %d\n", ret);

	return ret;
}

int cnss_minidump_remove_region(struct cnss_plat_data *plat_priv,
				enum cnss_fw_dump_type type, int seg_no,
				void *va, phys_addr_t pa, size_t size)
{
	struct md_region md_entry;
	int ret;

	switch (type) {
	case CNSS_FW_IMAGE:
		snprintf(md_entry.name, sizeof(md_entry.name), "FBC_%X",
			 seg_no);
		break;
	case CNSS_FW_RDDM:
		snprintf(md_entry.name, sizeof(md_entry.name), "RDDM_%X",
			 seg_no);
		break;
	case CNSS_FW_REMOTE_HEAP:
		snprintf(md_entry.name, sizeof(md_entry.name), "RHEAP_%X",
			 seg_no);
		break;
	default:
		cnss_pr_err("Unknown dump type ID: %d\n", type);
		return -EINVAL;
	}

	md_entry.phys_addr = pa;
	md_entry.virt_addr = (uintptr_t)va;
	md_entry.size = size;
	md_entry.id = MSM_DUMP_DATA_CNSS_WLAN;

	cnss_pr_dbg("Remove mini dump region: %s, va: %pK, pa: %pa, size: 0x%zx\n",
		    md_entry.name, va, &pa, size);

	ret = msm_minidump_remove_region(&md_entry);
	if (ret)
		cnss_pr_err("Failed to remove mini dump region, err = %d\n",
			    ret);

	return ret;
}
#else
int cnss_minidump_add_region(struct cnss_plat_data *plat_priv,
			     enum cnss_fw_dump_type type, int seg_no,
			     void *va, phys_addr_t pa, size_t size)
{
	return 0;
}

int cnss_minidump_remove_region(struct cnss_plat_data *plat_priv,
				enum cnss_fw_dump_type type, int seg_no,
				void *va, phys_addr_t pa, size_t size)
{
	return 0;
}
#endif /* CONFIG_QCOM_MINIDUMP */

int cnss_request_firmware_direct(struct cnss_plat_data *plat_priv,
				 const struct firmware **fw_entry,
				 const char *filename)
{
	if (IS_ENABLED(CONFIG_CNSS_REQ_FW_DIRECT))
		return request_firmware_direct(fw_entry, filename,
					       &plat_priv->plat_dev->dev);
	else
		return firmware_request_nowarn(fw_entry, filename,
					       &plat_priv->plat_dev->dev);
}

#if IS_ENABLED(CONFIG_INTERCONNECT)
/**
 * cnss_register_bus_scale() - Setup interconnect voting data
 * @plat_priv: Platform data structure
 *
 * For different interconnect path configured in device tree setup voting data
 * for list of bandwidth requirements.
 *
 * Result: 0 for success. -EINVAL if not configured
 */
static int cnss_register_bus_scale(struct cnss_plat_data *plat_priv)
{
	int ret = -EINVAL;
	u32 idx, i, j, cfg_arr_size, *cfg_arr = NULL;
	struct cnss_bus_bw_info *bus_bw_info, *tmp;
	struct device *dev = &plat_priv->plat_dev->dev;

	INIT_LIST_HEAD(&plat_priv->icc.list_head);
	ret = of_property_read_u32(dev->of_node,
				   "qcom,icc-path-count",
				   &plat_priv->icc.path_count);
	if (ret) {
		cnss_pr_err("Platform Bus Interconnect path not configured\n");
		return -EINVAL;
	}
	ret = of_property_read_u32(plat_priv->plat_dev->dev.of_node,
				   "qcom,bus-bw-cfg-count",
				   &plat_priv->icc.bus_bw_cfg_count);
	if (ret) {
		cnss_pr_err("Failed to get Bus BW Config table size\n");
		goto cleanup;
	}
	cfg_arr_size = plat_priv->icc.path_count *
			 plat_priv->icc.bus_bw_cfg_count * CNSS_ICC_VOTE_MAX;
	cfg_arr = kcalloc(cfg_arr_size, sizeof(*cfg_arr), GFP_KERNEL);
	if (!cfg_arr) {
		cnss_pr_err("Failed to alloc cfg table mem\n");
		ret = -ENOMEM;
		goto cleanup;
	}

	ret = of_property_read_u32_array(plat_priv->plat_dev->dev.of_node,
					 "qcom,bus-bw-cfg", cfg_arr,
					 cfg_arr_size);
	if (ret) {
		cnss_pr_err("Invalid Bus BW Config Table\n");
		goto cleanup;
	}

	cnss_pr_dbg("ICC Path_Count: %d BW_CFG_Count: %d\n",
		    plat_priv->icc.path_count, plat_priv->icc.bus_bw_cfg_count);

	for (idx = 0; idx < plat_priv->icc.path_count; idx++) {
		bus_bw_info = devm_kzalloc(dev, sizeof(*bus_bw_info),
					   GFP_KERNEL);
		if (!bus_bw_info) {
			ret = -ENOMEM;
			goto out;
		}
		ret = of_property_read_string_index(dev->of_node,
						    "interconnect-names", idx,
						    &bus_bw_info->icc_name);
		if (ret)
			goto out;

		bus_bw_info->icc_path =
			of_icc_get(&plat_priv->plat_dev->dev,
				   bus_bw_info->icc_name);

		if (IS_ERR(bus_bw_info->icc_path))  {
			ret = PTR_ERR(bus_bw_info->icc_path);
			if (ret != -EPROBE_DEFER) {
				cnss_pr_err("Failed to get Interconnect path for %s. Err: %d\n",
					    bus_bw_info->icc_name, ret);
				goto out;
			}
		}

		bus_bw_info->cfg_table =
			devm_kcalloc(dev, plat_priv->icc.bus_bw_cfg_count,
				     sizeof(*bus_bw_info->cfg_table),
				     GFP_KERNEL);
		if (!bus_bw_info->cfg_table) {
			ret = -ENOMEM;
			goto out;
		}
		cnss_pr_dbg("ICC Vote CFG for path: %s\n",
			    bus_bw_info->icc_name);
		for (i = 0, j = (idx * plat_priv->icc.bus_bw_cfg_count *
		     CNSS_ICC_VOTE_MAX);
		     i < plat_priv->icc.bus_bw_cfg_count;
		     i++, j += 2) {
			bus_bw_info->cfg_table[i].avg_bw = cfg_arr[j];
			bus_bw_info->cfg_table[i].peak_bw = cfg_arr[j + 1];
			cnss_pr_dbg("ICC Vote BW: %d avg: %d peak: %d\n",
				    i, bus_bw_info->cfg_table[i].avg_bw,
				    bus_bw_info->cfg_table[i].peak_bw);
		}
		list_add_tail(&bus_bw_info->list,
			      &plat_priv->icc.list_head);
	}
	kfree(cfg_arr);
	return 0;
out:
	list_for_each_entry_safe(bus_bw_info, tmp,
				 &plat_priv->icc.list_head, list) {
		list_del(&bus_bw_info->list);
	}
cleanup:
	kfree(cfg_arr);
	memset(&plat_priv->icc, 0, sizeof(plat_priv->icc));
	return ret;
}

static void cnss_unregister_bus_scale(struct cnss_plat_data *plat_priv)
{
	struct cnss_bus_bw_info *bus_bw_info, *tmp;

	list_for_each_entry_safe(bus_bw_info, tmp,
				 &plat_priv->icc.list_head, list) {
		list_del(&bus_bw_info->list);
		if (bus_bw_info->icc_path)
			icc_put(bus_bw_info->icc_path);
	}
	memset(&plat_priv->icc, 0, sizeof(plat_priv->icc));
}
#else
static int cnss_register_bus_scale(struct cnss_plat_data *plat_priv)
{
	return 0;
}

static void cnss_unregister_bus_scale(struct cnss_plat_data *plat_priv) {}
#endif /* CONFIG_INTERCONNECT */

void cnss_daemon_connection_update_cb(void *cb_ctx, bool status)
{
	struct cnss_plat_data *plat_priv = cb_ctx;

	if (!plat_priv) {
		cnss_pr_err("%s: Invalid context\n", __func__);
		return;
	}
	if (status) {
		cnss_pr_info("CNSS Daemon connected\n");
		set_bit(CNSS_DAEMON_CONNECTED, &plat_priv->driver_state);
		complete(&plat_priv->daemon_connected);
	} else {
		cnss_pr_info("CNSS Daemon disconnected\n");
		reinit_completion(&plat_priv->daemon_connected);
		clear_bit(CNSS_DAEMON_CONNECTED, &plat_priv->driver_state);
	}
}

static ssize_t enable_hds_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct cnss_plat_data *plat_priv = dev_get_drvdata(dev);
	unsigned int enable_hds = 0;

	if (!plat_priv)
		return -ENODEV;

	if (sscanf(buf, "%du", &enable_hds) != 1) {
		cnss_pr_err("Invalid enable_hds sysfs command\n");
		return -EINVAL;
	}

	if (enable_hds)
		plat_priv->hds_enabled = true;
	else
		plat_priv->hds_enabled = false;

	cnss_pr_dbg("%s HDS file download, count is %zu\n",
		    plat_priv->hds_enabled ? "Enable" : "Disable", count);

	return count;
}

static ssize_t recovery_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct cnss_plat_data *plat_priv = dev_get_drvdata(dev);
	unsigned int recovery = 0;

	if (!plat_priv)
		return -ENODEV;

	if (sscanf(buf, "%du", &recovery) != 1) {
		cnss_pr_err("Invalid recovery sysfs command\n");
		return -EINVAL;
	}

	if (recovery)
		plat_priv->recovery_enabled = true;
	else
		plat_priv->recovery_enabled = false;

	cnss_pr_dbg("%s WLAN recovery, count is %zu\n",
		    plat_priv->recovery_enabled ? "Enable" : "Disable", count);

	return count;
}

static ssize_t shutdown_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct cnss_plat_data *plat_priv = dev_get_drvdata(dev);

	if (plat_priv) {
		set_bit(CNSS_IN_REBOOT, &plat_priv->driver_state);
		del_timer(&plat_priv->fw_boot_timer);
		complete_all(&plat_priv->power_up_complete);
		complete_all(&plat_priv->cal_complete);
	}

	cnss_pr_dbg("Received shutdown notification\n");

	return count;
}

static ssize_t fs_ready_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	int fs_ready = 0;
	struct cnss_plat_data *plat_priv = dev_get_drvdata(dev);

	if (sscanf(buf, "%du", &fs_ready) != 1)
		return -EINVAL;

	cnss_pr_dbg("File system is ready, fs_ready is %d, count is %zu\n",
		    fs_ready, count);

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return count;
	}

	if (test_bit(QMI_BYPASS, &plat_priv->ctrl_params.quirks)) {
		cnss_pr_dbg("QMI is bypassed\n");
		return count;
	}

	switch (plat_priv->device_id) {
	case QCA6290_DEVICE_ID:
	case QCA6390_DEVICE_ID:
	case QCA6490_DEVICE_ID:
	case WCN7850_DEVICE_ID:
		break;
	default:
		cnss_pr_err("Not supported for device ID 0x%lx\n",
			    plat_priv->device_id);
		return count;
	}

	if (fs_ready == FILE_SYSTEM_READY && plat_priv->cbc_enabled) {
		cnss_driver_event_post(plat_priv,
				       CNSS_DRIVER_EVENT_COLD_BOOT_CAL_START,
				       0, NULL);
	}

	return count;
}

static ssize_t qdss_trace_start_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct cnss_plat_data *plat_priv = dev_get_drvdata(dev);

	wlfw_qdss_trace_start(plat_priv);
	cnss_pr_dbg("Received QDSS start command\n");
	return count;
}

static ssize_t qdss_trace_stop_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct cnss_plat_data *plat_priv = dev_get_drvdata(dev);
	u32 option = 0;

	if (sscanf(buf, "%du", &option) != 1)
		return -EINVAL;

	wlfw_qdss_trace_stop(plat_priv, option);
	cnss_pr_dbg("Received QDSS stop command\n");
	return count;
}

static ssize_t qdss_conf_download_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct cnss_plat_data *plat_priv = dev_get_drvdata(dev);

	cnss_wlfw_qdss_dnld_send_sync(plat_priv);
	cnss_pr_dbg("Received QDSS download config command\n");
	return count;
}

static ssize_t hw_trace_override_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct cnss_plat_data *plat_priv = dev_get_drvdata(dev);
	int tmp = 0;

	if (sscanf(buf, "%du", &tmp) != 1)
		return -EINVAL;

	plat_priv->hw_trc_override = tmp;
	cnss_pr_dbg("Received QDSS hw_trc_override indication\n");
	return count;
}

static DEVICE_ATTR_WO(fs_ready);
static DEVICE_ATTR_WO(shutdown);
static DEVICE_ATTR_WO(recovery);
static DEVICE_ATTR_WO(enable_hds);
static DEVICE_ATTR_WO(qdss_trace_start);
static DEVICE_ATTR_WO(qdss_trace_stop);
static DEVICE_ATTR_WO(qdss_conf_download);
static DEVICE_ATTR_WO(hw_trace_override);

static struct attribute *cnss_attrs[] = {
	&dev_attr_fs_ready.attr,
	&dev_attr_shutdown.attr,
	&dev_attr_recovery.attr,
	&dev_attr_enable_hds.attr,
	&dev_attr_qdss_trace_start.attr,
	&dev_attr_qdss_trace_stop.attr,
	&dev_attr_qdss_conf_download.attr,
	&dev_attr_hw_trace_override.attr,
	NULL,
};

static struct attribute_group cnss_attr_group = {
	.attrs = cnss_attrs,
};

static int cnss_create_sysfs_link(struct cnss_plat_data *plat_priv)
{
	struct device *dev = &plat_priv->plat_dev->dev;
	int ret;

	ret = sysfs_create_link(kernel_kobj, &dev->kobj, "cnss");
	if (ret) {
		cnss_pr_err("Failed to create cnss link, err = %d\n",
			    ret);
		goto out;
	}

	/* This is only for backward compatibility. */
	ret = sysfs_create_link(kernel_kobj, &dev->kobj, "shutdown_wlan");
	if (ret) {
		cnss_pr_err("Failed to create shutdown_wlan link, err = %d\n",
			    ret);
		goto rm_cnss_link;
	}

	return 0;

rm_cnss_link:
	sysfs_remove_link(kernel_kobj, "cnss");
out:
	return ret;
}

static void cnss_remove_sysfs_link(struct cnss_plat_data *plat_priv)
{
	sysfs_remove_link(kernel_kobj, "shutdown_wlan");
	sysfs_remove_link(kernel_kobj, "cnss");
}

static int cnss_create_sysfs(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	ret = devm_device_add_group(&plat_priv->plat_dev->dev,
				    &cnss_attr_group);
	if (ret) {
		cnss_pr_err("Failed to create cnss device group, err = %d\n",
			    ret);
		goto out;
	}

	cnss_create_sysfs_link(plat_priv);

	return 0;
out:
	return ret;
}

static void cnss_remove_sysfs(struct cnss_plat_data *plat_priv)
{
	cnss_remove_sysfs_link(plat_priv);
	devm_device_remove_group(&plat_priv->plat_dev->dev, &cnss_attr_group);
}

static int cnss_event_work_init(struct cnss_plat_data *plat_priv)
{
	spin_lock_init(&plat_priv->event_lock);
	plat_priv->event_wq = alloc_workqueue("cnss_driver_event",
					      WQ_UNBOUND, 1);
	if (!plat_priv->event_wq) {
		cnss_pr_err("Failed to create event workqueue!\n");
		return -EFAULT;
	}

	INIT_WORK(&plat_priv->event_work, cnss_driver_event_work);
	INIT_LIST_HEAD(&plat_priv->event_list);

	return 0;
}

static void cnss_event_work_deinit(struct cnss_plat_data *plat_priv)
{
	destroy_workqueue(plat_priv->event_wq);
}

static int cnss_reboot_notifier(struct notifier_block *nb,
				unsigned long action,
				void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(nb, struct cnss_plat_data, reboot_nb);

	set_bit(CNSS_IN_REBOOT, &plat_priv->driver_state);
	del_timer(&plat_priv->fw_boot_timer);
	complete_all(&plat_priv->power_up_complete);
	complete_all(&plat_priv->cal_complete);
	cnss_pr_dbg("Reboot is in progress with action %d\n", action);

	return NOTIFY_DONE;
}

static int cnss_misc_init(struct cnss_plat_data *plat_priv)
{
	int ret;

	timer_setup(&plat_priv->fw_boot_timer,
		    cnss_bus_fw_boot_timeout_hdlr, 0);

	ret = register_pm_notifier(&cnss_pm_notifier);
	if (ret)
		cnss_pr_err("Failed to register PM notifier, err = %d\n", ret);

	plat_priv->reboot_nb.notifier_call = cnss_reboot_notifier;
	ret = register_reboot_notifier(&plat_priv->reboot_nb);
	if (ret)
		cnss_pr_err("Failed to register reboot notifier, err = %d\n",
			    ret);

	ret = device_init_wakeup(&plat_priv->plat_dev->dev, true);
	if (ret)
		cnss_pr_err("Failed to init platform device wakeup source, err = %d\n",
			    ret);

	INIT_WORK(&plat_priv->recovery_work, cnss_recovery_work_handler);
	init_completion(&plat_priv->power_up_complete);
	init_completion(&plat_priv->cal_complete);
	init_completion(&plat_priv->rddm_complete);
	init_completion(&plat_priv->recovery_complete);
	init_completion(&plat_priv->daemon_connected);
	mutex_init(&plat_priv->dev_lock);
	mutex_init(&plat_priv->driver_ops_lock);
	plat_priv->recovery_ws =
		wakeup_source_register(&plat_priv->plat_dev->dev,
				       "CNSS_FW_RECOVERY");
	if (!plat_priv->recovery_ws)
		cnss_pr_err("Failed to setup FW recovery wake source\n");

	ret = cnss_plat_ipc_register(CNSS_PLAT_IPC_DAEMON_QMI_CLIENT_V01,
				     cnss_daemon_connection_update_cb,
				     plat_priv);
	if (ret)
		cnss_pr_err("QMI IPC connection call back register failed, err = %d\n",
			    ret);

	return 0;
}

static void cnss_misc_deinit(struct cnss_plat_data *plat_priv)
{
	cnss_plat_ipc_unregister(CNSS_PLAT_IPC_DAEMON_QMI_CLIENT_V01,
				 plat_priv);
	complete_all(&plat_priv->recovery_complete);
	complete_all(&plat_priv->rddm_complete);
	complete_all(&plat_priv->cal_complete);
	complete_all(&plat_priv->power_up_complete);
	complete_all(&plat_priv->daemon_connected);
	device_init_wakeup(&plat_priv->plat_dev->dev, false);
	unregister_reboot_notifier(&plat_priv->reboot_nb);
	unregister_pm_notifier(&cnss_pm_notifier);
	del_timer(&plat_priv->fw_boot_timer);
	wakeup_source_unregister(plat_priv->recovery_ws);
}

static void cnss_init_control_params(struct cnss_plat_data *plat_priv)
{
	plat_priv->ctrl_params.quirks = CNSS_QUIRKS_DEFAULT;

	plat_priv->cbc_enabled = !IS_ENABLED(CONFIG_CNSS_EMULATION) &&
		of_property_read_bool(plat_priv->plat_dev->dev.of_node,
				      "qcom,wlan-cbc-enabled");

	plat_priv->ctrl_params.mhi_timeout = CNSS_MHI_TIMEOUT_DEFAULT;
	plat_priv->ctrl_params.mhi_m2_timeout = CNSS_MHI_M2_TIMEOUT_DEFAULT;
	plat_priv->ctrl_params.qmi_timeout = CNSS_QMI_TIMEOUT_DEFAULT;
	plat_priv->ctrl_params.bdf_type = CNSS_BDF_TYPE_DEFAULT;
	plat_priv->ctrl_params.time_sync_period = CNSS_TIME_SYNC_PERIOD_DEFAULT;
	/* Set adsp_pc_enabled default value to true as ADSP pc is always
	 * enabled by default
	 */
	plat_priv->adsp_pc_enabled = true;
}

static void cnss_get_pm_domain_info(struct cnss_plat_data *plat_priv)
{
	struct device *dev = &plat_priv->plat_dev->dev;

	plat_priv->use_pm_domain =
		of_property_read_bool(dev->of_node, "use-pm-domain");

	cnss_pr_dbg("use-pm-domain is %d\n", plat_priv->use_pm_domain);
}

static void cnss_get_wlaon_pwr_ctrl_info(struct cnss_plat_data *plat_priv)
{
	struct device *dev = &plat_priv->plat_dev->dev;

	plat_priv->set_wlaon_pwr_ctrl =
		of_property_read_bool(dev->of_node, "qcom,set-wlaon-pwr-ctrl");

	cnss_pr_dbg("set_wlaon_pwr_ctrl is %d\n",
		    plat_priv->set_wlaon_pwr_ctrl);
}

static bool cnss_use_fw_path_with_prefix(struct cnss_plat_data *plat_priv)
{
	return (of_property_read_bool(plat_priv->plat_dev->dev.of_node,
				      "qcom,converged-dt") ||
	       of_property_read_bool(plat_priv->plat_dev->dev.of_node,
				     "qcom,same-dt-multi-dev"));
}

static const struct platform_device_id cnss_platform_id_table[] = {
	{ .name = "qca6174", .driver_data = QCA6174_DEVICE_ID, },
	{ .name = "qca6290", .driver_data = QCA6290_DEVICE_ID, },
	{ .name = "qca6390", .driver_data = QCA6390_DEVICE_ID, },
	{ .name = "qca6490", .driver_data = QCA6490_DEVICE_ID, },
	{ .name = "wcn7850", .driver_data = WCN7850_DEVICE_ID, },
	{ },
};

static const struct of_device_id cnss_of_match_table[] = {
	{
		.compatible = "qcom,cnss",
		.data = (void *)&cnss_platform_id_table[0]},
	{
		.compatible = "qcom,cnss-qca6290",
		.data = (void *)&cnss_platform_id_table[1]},
	{
		.compatible = "qcom,cnss-qca6390",
		.data = (void *)&cnss_platform_id_table[2]},
	{
		.compatible = "qcom,cnss-qca6490",
		.data = (void *)&cnss_platform_id_table[3]},
	{
		.compatible = "qcom,cnss-wcn7850",
		.data = (void *)&cnss_platform_id_table[4]},
	{ },
};
MODULE_DEVICE_TABLE(of, cnss_of_match_table);

static inline bool
cnss_use_nv_mac(struct cnss_plat_data *plat_priv)
{
	return of_property_read_bool(plat_priv->plat_dev->dev.of_node,
				     "use-nv-mac");
}

static int cnss_probe(struct platform_device *plat_dev)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv;
	const struct of_device_id *of_id;
	const struct platform_device_id *device_id;
	int retry = 0;

	if (cnss_get_plat_priv(plat_dev)) {
		cnss_pr_err("Driver is already initialized!\n");
		ret = -EEXIST;
		goto out;
	}

	of_id = of_match_device(cnss_of_match_table, &plat_dev->dev);
	if (!of_id || !of_id->data) {
		cnss_pr_err("Failed to find of match device!\n");
		ret = -ENODEV;
		goto out;
	}

	device_id = of_id->data;

	plat_priv = devm_kzalloc(&plat_dev->dev, sizeof(*plat_priv),
				 GFP_KERNEL);
	if (!plat_priv) {
		ret = -ENOMEM;
		goto out;
	}

	plat_priv->plat_dev = plat_dev;
	plat_priv->device_id = device_id->driver_data;
	plat_priv->bus_type = cnss_get_bus_type(plat_priv->device_id);
	plat_priv->use_nv_mac = cnss_use_nv_mac(plat_priv);
	plat_priv->use_fw_path_with_prefix =
		cnss_use_fw_path_with_prefix(plat_priv);
	cnss_set_plat_priv(plat_dev, plat_priv);
	platform_set_drvdata(plat_dev, plat_priv);
	INIT_LIST_HEAD(&plat_priv->vreg_list);
	INIT_LIST_HEAD(&plat_priv->clk_list);

	cnss_get_pm_domain_info(plat_priv);
	cnss_get_wlaon_pwr_ctrl_info(plat_priv);
	cnss_get_tcs_info(plat_priv);
	cnss_get_cpr_info(plat_priv);
	cnss_aop_mbox_init(plat_priv);
	cnss_init_control_params(plat_priv);

	ret = cnss_get_resources(plat_priv);
	if (ret)
		goto reset_ctx;

	ret = cnss_register_esoc(plat_priv);
	if (ret)
		goto free_res;

	ret = cnss_register_bus_scale(plat_priv);
	if (ret)
		goto unreg_esoc;

	ret = cnss_create_sysfs(plat_priv);
	if (ret)
		goto unreg_bus_scale;

	ret = cnss_event_work_init(plat_priv);
	if (ret)
		goto remove_sysfs;

	ret = cnss_qmi_init(plat_priv);
	if (ret)
		goto deinit_event_work;

	ret = cnss_dms_init(plat_priv);
	if (ret)
		goto deinit_qmi;

	ret = cnss_debugfs_create(plat_priv);
	if (ret)
		goto deinit_dms;

	ret = cnss_misc_init(plat_priv);
	if (ret)
		goto destroy_debugfs;

	/* Make sure all platform related init are done before
	 * device power on and bus init.
	 */
	if (!test_bit(SKIP_DEVICE_BOOT, &plat_priv->ctrl_params.quirks)) {
retry:
		ret = cnss_power_on_device(plat_priv);
		if (ret)
			goto deinit_misc;

		ret = cnss_bus_init(plat_priv);
		if (ret) {
			if ((ret != -EPROBE_DEFER) &&
			    retry++ < POWER_ON_RETRY_MAX_TIMES) {
				cnss_power_off_device(plat_priv);
				cnss_pr_dbg("Retry cnss_bus_init #%d\n", retry);
				msleep(POWER_ON_RETRY_DELAY_MS * retry);
				goto retry;
			}
			goto power_off;
		}
	}

	cnss_register_coex_service(plat_priv);
	cnss_register_ims_service(plat_priv);

	ret = cnss_genl_init();
	if (ret < 0)
		cnss_pr_err("CNSS genl init failed %d\n", ret);

	cnss_pr_info("Platform driver probed successfully.\n");

	return 0;

power_off:
	if (!test_bit(SKIP_DEVICE_BOOT, &plat_priv->ctrl_params.quirks))
		cnss_power_off_device(plat_priv);
deinit_misc:
	cnss_misc_deinit(plat_priv);
destroy_debugfs:
	cnss_debugfs_destroy(plat_priv);
deinit_dms:
	cnss_dms_deinit(plat_priv);
deinit_qmi:
	cnss_qmi_deinit(plat_priv);
deinit_event_work:
	cnss_event_work_deinit(plat_priv);
remove_sysfs:
	cnss_remove_sysfs(plat_priv);
unreg_bus_scale:
	cnss_unregister_bus_scale(plat_priv);
unreg_esoc:
	cnss_unregister_esoc(plat_priv);
free_res:
	cnss_put_resources(plat_priv);
reset_ctx:
	platform_set_drvdata(plat_dev, NULL);
	cnss_set_plat_priv(plat_dev, NULL);
out:
	return ret;
}

static int cnss_remove(struct platform_device *plat_dev)
{
	struct cnss_plat_data *plat_priv = platform_get_drvdata(plat_dev);

	cnss_genl_exit();
	cnss_unregister_ims_service(plat_priv);
	cnss_unregister_coex_service(plat_priv);
	cnss_bus_deinit(plat_priv);
	cnss_misc_deinit(plat_priv);
	cnss_debugfs_destroy(plat_priv);
	cnss_dms_deinit(plat_priv);
	cnss_qmi_deinit(plat_priv);
	cnss_event_work_deinit(plat_priv);
	cnss_remove_sysfs(plat_priv);
	cnss_unregister_bus_scale(plat_priv);
	cnss_unregister_esoc(plat_priv);
	cnss_put_resources(plat_priv);

	if (!IS_ERR_OR_NULL(plat_priv->mbox_chan))
		mbox_free_channel(plat_priv->mbox_chan);

	platform_set_drvdata(plat_dev, NULL);
	plat_env = NULL;

	return 0;
}

static struct platform_driver cnss_platform_driver = {
	.probe  = cnss_probe,
	.remove = cnss_remove,
	.driver = {
		.name = "cnss2",
		.of_match_table = cnss_of_match_table,
#ifdef CONFIG_CNSS_ASYNC
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
#endif
	},
};

/**
 * cnss_is_valid_dt_node_found - Check if valid device tree node present
 *
 * Valid device tree node means a node with "compatible" property from the
 * device match table and "status" property is not disabled.
 *
 * Return: true if valid device tree node found, false if not found
 */
static bool cnss_is_valid_dt_node_found(void)
{
	struct device_node *dn = NULL;

	for_each_matching_node(dn, cnss_of_match_table) {
		if (of_device_is_available(dn))
			break;
	}

	if (dn)
		return true;

	return false;
}

static int __init cnss_initialize(void)
{
	int ret = 0;

	if (!cnss_is_valid_dt_node_found())
		return -ENODEV;

	cnss_debug_init();
	ret = platform_driver_register(&cnss_platform_driver);
	if (ret)
		cnss_debug_deinit();

	return ret;
}

static void __exit cnss_exit(void)
{
	platform_driver_unregister(&cnss_platform_driver);
	cnss_debug_deinit();
}

module_init(cnss_initialize);
module_exit(cnss_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CNSS2 Platform Driver");
