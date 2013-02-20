/* Copyright (c) 2009-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/debugfs.h>
#include <asm/uaccess.h>
#include <mach/qdsp5v2/qdsp5afecmdi.h>
#include <mach/qdsp5v2/qdsp5afemsg.h>
#include <mach/qdsp5v2/afe.h>
#include <mach/msm_adsp.h>
#include <mach/debug_mm.h>

#define AFE_MAX_TIMEOUT 500 /* 500 ms */
#define AFE_MAX_CLNT 6 /* 6 HW path defined so far */
#define GETDEVICEID(x) ((x) - 1)

struct msm_afe_state {
	struct msm_adsp_module *mod;
	struct msm_adsp_ops    adsp_ops;
	struct mutex           lock;
	u8                     in_use;
	u8                     codec_config[AFE_MAX_CLNT];
	wait_queue_head_t      wait;
	u8			aux_conf_flag;
};

#ifdef CONFIG_DEBUG_FS
static struct dentry *debugfs_afelb;
#endif


static struct msm_afe_state the_afe_state;

#define afe_send_queue(afe, cmd, len) \
  msm_adsp_write(afe->mod, QDSP_apuAfeQueue, \
	cmd, len)

static void afe_dsp_event(void *data, unsigned id, size_t len,
			    void (*getevent)(void *ptr, size_t len))
{
	struct msm_afe_state *afe = data;

	MM_DBG("msg_id %d \n", id);

	switch (id) {
	case AFE_APU_MSG_CODEC_CONFIG_ACK: {
		struct afe_msg_codec_config_ack afe_ack;
		getevent(&afe_ack, AFE_APU_MSG_CODEC_CONFIG_ACK_LEN);
		MM_DBG("%s: device_id: %d device activity: %d\n", __func__,
		afe_ack.device_id, afe_ack.device_activity);
		if (afe_ack.device_activity == AFE_MSG_CODEC_CONFIG_DISABLED)
			afe->codec_config[GETDEVICEID(afe_ack.device_id)] = 0;
		else
			afe->codec_config[GETDEVICEID(afe_ack.device_id)] =
			afe_ack.device_activity;

		wake_up(&afe->wait);
		break;
	}
	case AFE_APU_MSG_VOC_TIMING_SUCCESS:
		MM_INFO("Received VOC_TIMING_SUCCESS message from AFETASK\n");
		break;
	case ADSP_MESSAGE_ID:
		MM_DBG("Received ADSP event: module enable/disable(audpptask)");
		break;
	default:
		MM_ERR("unexpected message from afe \n");
	}

	return;
}

static void afe_dsp_codec_config(struct msm_afe_state *afe,
	u8 path_id, u8 enable, struct msm_afe_config *config)
{
	struct afe_cmd_codec_config cmd;

	MM_DBG("%s() %p\n", __func__, config);
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AFE_CMD_CODEC_CONFIG_CMD;
	cmd.device_id = path_id;
	cmd.activity = enable;
	if (config) {
		MM_DBG("%s: sample_rate %x ch mode %x vol %x\n",
			__func__, config->sample_rate,
			config->channel_mode, config->volume);
		cmd.sample_rate = config->sample_rate;
		cmd.channel_mode = config->channel_mode;
		cmd.volume = config->volume;
	}
	afe_send_queue(afe, &cmd, sizeof(cmd));
}
/* Function is called after afe module been enabled */
void afe_loopback(int enable)
{
	struct afe_cmd_loopback cmd;
	struct msm_afe_state *afe;

	afe = &the_afe_state;
	MM_DBG("enable %d\n", enable);
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AFE_CMD_LOOPBACK;
	if (enable)
		cmd.enable_flag = AFE_LOOPBACK_ENABLE_COMMAND;

	afe_send_queue(afe, &cmd, sizeof(cmd));
}
EXPORT_SYMBOL(afe_loopback);

void afe_ext_loopback(int enable, int rx_copp_id, int tx_copp_id)
{
	struct afe_cmd_ext_loopback cmd;
	struct msm_afe_state *afe;

	afe = &the_afe_state;
	MM_DBG("enable %d\n", enable);
	if ((rx_copp_id == 0) && (tx_copp_id == 0)) {
		afe_loopback(enable);
	} else {
		memset(&cmd, 0, sizeof(cmd));
		cmd.cmd_id = AFE_CMD_EXT_LOOPBACK;
		cmd.source_id = tx_copp_id;
		cmd.dst_id = rx_copp_id;
		if (enable)
			cmd.enable_flag = AFE_LOOPBACK_ENABLE_COMMAND;

		afe_send_queue(afe, &cmd, sizeof(cmd));
	}
}
EXPORT_SYMBOL(afe_ext_loopback);

void afe_device_volume_ctrl(u16 device_id, u16 device_volume)
{
	struct afe_cmd_device_volume_ctrl cmd;
	struct msm_afe_state *afe;

	afe = &the_afe_state;
	MM_DBG("device 0x%4x volume 0x%4x\n", device_id, device_volume);
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AFE_CMD_DEVICE_VOLUME_CTRL;
	cmd.device_id = device_id;
	cmd.device_volume = device_volume;
	afe_send_queue(afe, &cmd, sizeof(cmd));
}
EXPORT_SYMBOL(afe_device_volume_ctrl);

int afe_enable(u8 path_id, struct msm_afe_config *config)
{
	struct msm_afe_state *afe = &the_afe_state;
	int rc;

	MM_DBG("%s: path %d\n", __func__, path_id);
	if ((GETDEVICEID(path_id) < 0) || (GETDEVICEID(path_id) > 5)) {
		MM_ERR("Invalid path_id: %d\n", path_id);
		return -EINVAL;
	}
	mutex_lock(&afe->lock);
	if (!afe->in_use && !afe->aux_conf_flag) {
		/* enable afe */
		rc = msm_adsp_get("AFETASK", &afe->mod, &afe->adsp_ops, afe);
		if (rc < 0) {
			MM_ERR("%s: failed to get AFETASK module\n", __func__);
			goto error_adsp_get;
		}
		rc = msm_adsp_enable(afe->mod);
		if (rc < 0)
			goto error_adsp_enable;
	}
	/* Issue codec config command */
	afe_dsp_codec_config(afe, path_id, 1, config);
	rc = wait_event_timeout(afe->wait,
		afe->codec_config[GETDEVICEID(path_id)],
		msecs_to_jiffies(AFE_MAX_TIMEOUT));
	if (!rc) {
		MM_ERR("AFE failed to respond within %d ms\n", AFE_MAX_TIMEOUT);
		rc = -ENODEV;
		if (!afe->in_use) {
			if (!afe->aux_conf_flag ||
			(afe->aux_conf_flag &&
			(path_id == AFE_HW_PATH_AUXPCM_RX ||
			path_id == AFE_HW_PATH_AUXPCM_TX))) {
				/* clean up if there is no client */
				msm_adsp_disable(afe->mod);
				msm_adsp_put(afe->mod);
				afe->aux_conf_flag = 0;
				afe->mod = NULL;
			}
		}

	} else {
		rc = 0;
		afe->in_use++;
	}

	mutex_unlock(&afe->lock);
	return rc;

error_adsp_enable:
	msm_adsp_put(afe->mod);
	afe->mod = NULL;
error_adsp_get:
	mutex_unlock(&afe->lock);
	return rc;
}
EXPORT_SYMBOL(afe_enable);

int afe_config_fm_codec(int fm_enable, uint16_t source)
{
	struct afe_cmd_fm_codec_config cmd;
	struct msm_afe_state *afe = &the_afe_state;
	int rc = 0;
	int i = 0;
	unsigned short *ptrmem = (unsigned short *)&cmd;

	MM_INFO(" configure fm codec\n");
	mutex_lock(&afe->lock);
	if (!afe->in_use) {
		/* enable afe */
		rc = msm_adsp_get("AFETASK", &afe->mod, &afe->adsp_ops, afe);
		if (rc < 0) {
			MM_ERR("%s: failed to get AFETASK module\n", __func__);
			goto error_adsp_get;
		}
		rc = msm_adsp_enable(afe->mod);
		if (rc < 0)
			goto error_adsp_enable;
	}
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AFE_CMD_FM_RX_ROUTING_CMD;
	cmd.enable = fm_enable;
	cmd.device_id = source;

	for (i = 0; i < sizeof(cmd)/2; i++, ++ptrmem)
		MM_DBG("cmd[%d]=0x%04x\n", i, *ptrmem);
	afe_send_queue(afe, &cmd, sizeof(cmd));

	mutex_unlock(&afe->lock);
	return rc;
error_adsp_enable:
	msm_adsp_put(afe->mod);
	afe->mod = NULL;
error_adsp_get:
	mutex_unlock(&afe->lock);
	return rc;
}
EXPORT_SYMBOL(afe_config_fm_codec);

int afe_config_fm_volume(uint16_t volume)
{
	struct afe_cmd_fm_volume_config cmd;
	struct msm_afe_state *afe = &the_afe_state;
	int rc = 0;

	MM_INFO(" configure fm volume\n");
	mutex_lock(&afe->lock);
	if (!afe->in_use) {
		/* enable afe */
		rc = msm_adsp_get("AFETASK", &afe->mod, &afe->adsp_ops, afe);
		if (rc < 0) {
			MM_ERR("%s: failed to get AFETASK module\n", __func__);
			goto error_adsp_get;
		}
		rc = msm_adsp_enable(afe->mod);
		if (rc < 0)
			goto error_adsp_enable;
	}
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AFE_CMD_FM_PLAYBACK_VOLUME_CMD;
	cmd.volume = volume;

	afe_send_queue(afe, &cmd, sizeof(cmd));

	mutex_unlock(&afe->lock);
	return rc;
error_adsp_enable:
	msm_adsp_put(afe->mod);
	afe->mod = NULL;
error_adsp_get:
	mutex_unlock(&afe->lock);
	return rc;
}
EXPORT_SYMBOL(afe_config_fm_volume);

int afe_config_fm_calibration_gain(uint16_t device_id,
			uint16_t calibration_gain)
{
	struct afe_cmd_fm_calibgain_config cmd;
	struct msm_afe_state *afe = &the_afe_state;
	int rc = 0;

	MM_INFO("Configure for rx device = 0x%4x, gain = 0x%4x\n", device_id,
			calibration_gain);
	mutex_lock(&afe->lock);
	if (!afe->in_use) {
		/* enable afe */
		rc = msm_adsp_get("AFETASK", &afe->mod, &afe->adsp_ops, afe);
		if (rc < 0) {
			MM_ERR("%s: failed to get AFETASK module\n", __func__);
			goto error_adsp_get;
		}
		rc = msm_adsp_enable(afe->mod);
		if (rc < 0)
			goto error_adsp_enable;
	}
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AFE_CMD_FM_CALIBRATION_GAIN_CMD;
	cmd.device_id = device_id;
	cmd.calibration_gain = calibration_gain;

	afe_send_queue(afe, &cmd, sizeof(cmd));

	mutex_unlock(&afe->lock);
	return rc;
error_adsp_enable:
	msm_adsp_put(afe->mod);
	afe->mod = NULL;
error_adsp_get:
	mutex_unlock(&afe->lock);
	return rc;
}
EXPORT_SYMBOL(afe_config_fm_calibration_gain);

int afe_config_aux_codec(int pcm_ctl_value, int aux_codec_intf_value,
				int data_format_pad)
{
	struct afe_cmd_aux_codec_config cmd;
	struct msm_afe_state *afe = &the_afe_state;
	int rc = 0;

	MM_DBG(" configure aux codec \n");
	mutex_lock(&afe->lock);
	if (!afe->in_use && !afe->aux_conf_flag) {
		/* enable afe */
		rc = msm_adsp_get("AFETASK", &afe->mod, &afe->adsp_ops, afe);
		if (rc < 0) {
			MM_ERR("%s: failed to get AFETASK module\n", __func__);
			goto error_adsp_get;
		}
		rc = msm_adsp_enable(afe->mod);
		if (rc < 0)
			goto error_adsp_enable;
	}
	afe->aux_conf_flag = 1;
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AFE_CMD_AUX_CODEC_CONFIG_CMD;
	cmd.dma_path_ctl = 0;
	cmd.pcm_ctl = pcm_ctl_value;
	cmd.eight_khz_int_mode = 0;
	cmd.aux_codec_intf_ctl = aux_codec_intf_value;
	cmd.data_format_padding_info = data_format_pad;

	afe_send_queue(afe, &cmd, sizeof(cmd));

	mutex_unlock(&afe->lock);
	return rc;
error_adsp_enable:
	msm_adsp_put(afe->mod);
	afe->mod = NULL;
error_adsp_get:
	mutex_unlock(&afe->lock);
	return rc;
}
EXPORT_SYMBOL(afe_config_aux_codec);

int afe_config_rmc_block(struct acdb_rmc_block *acdb_rmc)
{
	struct afe_cmd_cfg_rmc cmd;
	struct msm_afe_state *afe = &the_afe_state;
	int rc = 0;
	int i = 0;
	unsigned short *ptrmem = (unsigned short *)&cmd;

	MM_DBG(" configure rmc block\n");
	mutex_lock(&afe->lock);
	if (!afe->in_use && !afe->mod) {
		/* enable afe */
		rc = msm_adsp_get("AFETASK", &afe->mod, &afe->adsp_ops, afe);
		if (rc < 0) {
			MM_DBG("%s: failed to get AFETASK module\n", __func__);
			goto error_adsp_get;
		}
		rc = msm_adsp_enable(afe->mod);
		if (rc < 0)
			goto error_adsp_enable;
	}
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_id = AFE_CMD_CFG_RMC_PARAMS;

	cmd.rmc_mode = acdb_rmc->rmc_enable;
	cmd.rmc_ipw_length_ms =	acdb_rmc->rmc_ipw_length_ms;
	cmd.rmc_peak_length_ms = acdb_rmc->rmc_peak_length_ms;
	cmd.rmc_init_pulse_length_ms = acdb_rmc->rmc_init_pulse_length_ms;
	cmd.rmc_total_int_length_ms = acdb_rmc->rmc_total_int_length_ms;
	cmd.rmc_rampupdn_length_ms = acdb_rmc->rmc_rampupdn_length_ms;
	cmd.rmc_delay_length_ms = acdb_rmc->rmc_delay_length_ms;
	cmd.rmc_detect_start_threshdb = acdb_rmc->rmc_detect_start_threshdb;
	cmd.rmc_init_pulse_threshdb = acdb_rmc->rmc_init_pulse_threshdb;

	for (i = 0; i < sizeof(cmd)/2; i++, ++ptrmem)
		MM_DBG("cmd[%d]=0x%04x\n", i, *ptrmem);
	afe_send_queue(afe, &cmd, sizeof(cmd));

	mutex_unlock(&afe->lock);
	return rc;
error_adsp_enable:
	msm_adsp_put(afe->mod);
	afe->mod = NULL;
error_adsp_get:
	mutex_unlock(&afe->lock);
	return rc;
}
EXPORT_SYMBOL(afe_config_rmc_block);

int afe_disable(u8 path_id)
{
	struct msm_afe_state *afe = &the_afe_state;
	int rc;

	mutex_lock(&afe->lock);

	BUG_ON(!afe->in_use);
	MM_DBG("%s() path_id:%d codec state:%d\n", __func__, path_id,
	afe->codec_config[GETDEVICEID(path_id)]);
	afe_dsp_codec_config(afe, path_id, 0, NULL);
	rc = wait_event_timeout(afe->wait,
		!afe->codec_config[GETDEVICEID(path_id)],
		msecs_to_jiffies(AFE_MAX_TIMEOUT));
	if (!rc) {
		MM_ERR("AFE failed to respond within %d ms\n", AFE_MAX_TIMEOUT);
		rc = -1;
	} else
		rc = 0;
	afe->in_use--;
	MM_DBG("%s() in_use:%d \n", __func__, afe->in_use);
	if (!afe->in_use) {
		msm_adsp_disable(afe->mod);
		msm_adsp_put(afe->mod);
		afe->aux_conf_flag = 0;
		afe->mod = NULL;
	}
	mutex_unlock(&afe->lock);
	return rc;
}
EXPORT_SYMBOL(afe_disable);


#ifdef CONFIG_DEBUG_FS
static int afe_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	MM_INFO("debug intf %s\n", (char *) file->private_data);
	return 0;
}

static ssize_t afe_debug_write(struct file *filp,
	const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char *lb_str = filp->private_data;
	char cmd;

	if (get_user(cmd, ubuf))
		return -EFAULT;

	MM_INFO("%s %c\n", lb_str, cmd);

	if (!strcmp(lb_str, "afe_loopback")) {
		switch (cmd) {
		case '1':
			afe_loopback(1);
			break;
		case '0':
			afe_loopback(0);
			break;
		}
	}

	return cnt;
}

static const struct file_operations afe_debug_fops = {
	.open = afe_debug_open,
	.write = afe_debug_write
};
#endif

static int __init afe_init(void)
{
	struct msm_afe_state *afe = &the_afe_state;

	MM_INFO("AFE driver init\n");

	memset(afe, 0, sizeof(struct msm_afe_state));
	afe->adsp_ops.event = afe_dsp_event;
	mutex_init(&afe->lock);
	init_waitqueue_head(&afe->wait);

#ifdef CONFIG_DEBUG_FS
	debugfs_afelb = debugfs_create_file("afe_loopback",
	S_IFREG | S_IWUGO, NULL, (void *) "afe_loopback",
	&afe_debug_fops);
#endif

	return 0;
}

static void __exit afe_exit(void)
{
	MM_INFO("AFE driver exit\n");
#ifdef CONFIG_DEBUG_FS
	if (debugfs_afelb)
		debugfs_remove(debugfs_afelb);
#endif
	if (the_afe_state.mod)
		msm_adsp_put(the_afe_state.mod);
	return;
}

module_init(afe_init);
module_exit(afe_exit);

MODULE_DESCRIPTION("MSM AFE driver");
MODULE_LICENSE("GPL v2");
