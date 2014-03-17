/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/diagchar.h>
#include <linux/platform_device.h>
#include <linux/kmemleak.h>
#include <linux/delay.h>
#include "diagchar.h"
#include "diagfwd.h"
#include "diagfwd_cntl.h"
#include "diagfwd_hsic.h"
#include "diag_dci.h"
#include "diagmem.h"
/* tracks which peripheral is undergoing SSR */
static uint16_t reg_dirty;
#define HDR_SIZ 8

void diag_clean_reg_fn(struct work_struct *work)
{
	struct diag_smd_info *smd_info = container_of(work,
						struct diag_smd_info,
						diag_notify_update_smd_work);
	if (!smd_info)
		return;

	pr_debug("diag: clean registration for peripheral: %d\n",
		smd_info->peripheral);

	reg_dirty |= smd_info->peripheral_mask;
	diag_clear_reg(smd_info->peripheral);
	reg_dirty ^= smd_info->peripheral_mask;

	/* Reset the feature mask flag */
	driver->rcvd_feature_mask[smd_info->peripheral] = 0;

	smd_info->notify_context = 0;
}

void diag_cntl_smd_work_fn(struct work_struct *work)
{
	struct diag_smd_info *smd_info = container_of(work,
						struct diag_smd_info,
						diag_general_smd_work);

	if (!smd_info || smd_info->type != SMD_CNTL_TYPE)
		return;

	if (smd_info->general_context == UPDATE_PERIPHERAL_STM_STATE) {
		if (driver->peripheral_supports_stm[smd_info->peripheral] ==
								ENABLE_STM) {
			int status = 0;
			int index = smd_info->peripheral;
			status = diag_send_stm_state(smd_info,
				(uint8_t)(driver->stm_state_requested[index]));
			if (status == 1)
				driver->stm_state[index] =
					driver->stm_state_requested[index];
		}
	}
	smd_info->general_context = 0;
}

void diag_cntl_stm_notify(struct diag_smd_info *smd_info, int action)
{
	if (!smd_info || smd_info->type != SMD_CNTL_TYPE)
		return;

	if (action == CLEAR_PERIPHERAL_STM_STATE) {
		driver->peripheral_supports_stm[smd_info->peripheral] =
								DISABLE_STM;
		/*
		 * Turn off STM for now until such time as the
		 * tools can support SSR
		 */
		driver->stm_state[smd_info->peripheral] = DISABLE_STM;
		driver->stm_state_requested[smd_info->peripheral] = DISABLE_STM;
	}
}

static void process_stm_feature(struct diag_smd_info *smd_info,
			      uint8_t feature_mask)
{
	if (feature_mask & F_DIAG_OVER_STM) {
		driver->peripheral_supports_stm[smd_info->peripheral] =
								ENABLE_STM;
		smd_info->general_context = UPDATE_PERIPHERAL_STM_STATE;
		queue_work(driver->diag_cntl_wq,
				&(smd_info->diag_general_smd_work));
	} else {
		driver->peripheral_supports_stm[smd_info->peripheral] =
								DISABLE_STM;
	}
}

static void process_hdlc_encoding_feature(struct diag_smd_info *smd_info,
					uint8_t feature_mask)
{
	/*
	 * Check if apps supports hdlc encoding and the
	 * peripheral supports apps hdlc encoding
	 */
	if (driver->supports_apps_hdlc_encoding &&
		(feature_mask & F_DIAG_HDLC_ENCODE_IN_APPS_MASK)) {
		driver->smd_data[smd_info->peripheral].encode_hdlc =
						ENABLE_APPS_HDLC_ENCODING;
		if (driver->separate_cmdrsp[smd_info->peripheral] &&
			smd_info->peripheral < NUM_SMD_CMD_CHANNELS)
			driver->smd_cmd[smd_info->peripheral].encode_hdlc =
						ENABLE_APPS_HDLC_ENCODING;
	} else {
		driver->smd_data[smd_info->peripheral].encode_hdlc =
						DISABLE_APPS_HDLC_ENCODING;
		if (driver->separate_cmdrsp[smd_info->peripheral] &&
			smd_info->peripheral < NUM_SMD_CMD_CHANNELS)
			driver->smd_cmd[smd_info->peripheral].encode_hdlc =
						DISABLE_APPS_HDLC_ENCODING;
	}
}

/* Process the data read from the smd control channel */
int diag_process_smd_cntl_read_data(struct diag_smd_info *smd_info, void *buf,
								int total_recd)
{
	int data_len = 0, type = -1, count_bytes = 0, j, flag = 0;
	struct bindpkt_params_per_process *pkt_params =
		kzalloc(sizeof(struct bindpkt_params_per_process), GFP_KERNEL);
	struct diag_ctrl_msg *msg;
	struct cmd_code_range *range;
	struct bindpkt_params *temp;

	if (pkt_params == NULL) {
		pr_alert("diag: In %s, Memory allocation failure\n",
			__func__);
		return 0;
	}

	if (!smd_info) {
		pr_err("diag: In %s, No smd info. Not able to read.\n",
			__func__);
		kfree(pkt_params);
		return 0;
	}

	while (count_bytes + HDR_SIZ <= total_recd) {
		type = *(uint32_t *)(buf);
		data_len = *(uint32_t *)(buf + 4);
		if (type < DIAG_CTRL_MSG_REG ||
				 type > DIAG_CTRL_MSG_LAST) {
			pr_alert("diag: In %s, Invalid Msg type %d proc %d",
				 __func__, type, smd_info->peripheral);
			break;
		}
		if (data_len < 0 || data_len > total_recd) {
			pr_alert("diag: In %s, Invalid data len %d, total_recd: %d, proc %d",
				 __func__, data_len, total_recd,
				 smd_info->peripheral);
			break;
		}
		count_bytes = count_bytes+HDR_SIZ+data_len;
		if (type == DIAG_CTRL_MSG_REG && total_recd >= count_bytes) {
			msg = buf+HDR_SIZ;
			range = buf+HDR_SIZ+
					sizeof(struct diag_ctrl_msg);
			if (msg->count_entries == 0) {
				pr_debug("diag: In %s, received reg tbl with no entries\n",
								__func__);
				buf = buf + HDR_SIZ + data_len;
				continue;
			}
			pkt_params->count = msg->count_entries;
			pkt_params->params = kzalloc(pkt_params->count *
				sizeof(struct bindpkt_params), GFP_KERNEL);
			if (!pkt_params->params) {
				pr_alert("diag: In %s, Memory alloc fail for cmd_code: %d, subsys: %d\n",
						__func__, msg->cmd_code,
						msg->subsysid);
				buf = buf + HDR_SIZ + data_len;
				continue;
			}
			temp = pkt_params->params;
			for (j = 0; j < pkt_params->count; j++) {
				temp->cmd_code = msg->cmd_code;
				temp->subsys_id = msg->subsysid;
				temp->client_id = smd_info->peripheral;
				temp->proc_id = NON_APPS_PROC;
				temp->cmd_code_lo = range->cmd_code_lo;
				temp->cmd_code_hi = range->cmd_code_hi;
				range++;
				temp++;
			}
			flag = 1;
			/* peripheral undergoing SSR should not
			 * record new registration
			 */
			if (!(reg_dirty & smd_info->peripheral_mask))
				diagchar_ioctl(NULL, DIAG_IOCTL_COMMAND_REG,
						(unsigned long)pkt_params);
			else
				pr_err("diag: drop reg proc %d\n",
						smd_info->peripheral);
			kfree(pkt_params->params);
		} else if (type == DIAG_CTRL_MSG_FEATURE &&
				total_recd >= count_bytes) {
			uint8_t feature_mask = 0;
			int feature_mask_len = *(int *)(buf+8);
			if (feature_mask_len > 0) {
				int periph = smd_info->peripheral;
				driver->rcvd_feature_mask[smd_info->peripheral]
									= 1;
				feature_mask = *(uint8_t *)(buf+12);
				if (periph == MODEM_DATA)
					driver->log_on_demand_support =
						feature_mask &
					F_DIAG_LOG_ON_DEMAND_RSP_ON_MASTER;
				/*
				 * If apps supports separate cmd/rsp channels
				 * and the peripheral supports separate cmd/rsp
				 * channels
				 */
				if (driver->supports_separate_cmdrsp &&
					(feature_mask & F_DIAG_REQ_RSP_CHANNEL))
					driver->separate_cmdrsp[periph] =
							ENABLE_SEPARATE_CMDRSP;
				else
					driver->separate_cmdrsp[periph] =
							DISABLE_SEPARATE_CMDRSP;
				/*
				 * Check if apps supports hdlc encoding and the
				 * peripheral supports apps hdlc encoding
				 */
				process_hdlc_encoding_feature(smd_info,
								feature_mask);
				if (feature_mask_len > 1) {
					feature_mask = *(uint8_t *)(buf+13);
					process_stm_feature(smd_info,
								feature_mask);
				}
			}
			flag = 1;
		} else if (type != DIAG_CTRL_MSG_REG) {
			flag = 1;
		}
		buf = buf + HDR_SIZ + data_len;
	}
	kfree(pkt_params);

	return flag;
}

static int diag_compute_real_time(int idx)
{
	int real_time = MODE_REALTIME;
	if (driver->proc_active_mask == 0) {
		/*
		 * There are no DCI or Memory Device processes. Diag should
		 * be in Real Time mode irrespective of USB connection
		 */
		real_time = MODE_REALTIME;
	} else if (driver->proc_rt_vote_mask[idx] & driver->proc_active_mask) {
		/*
		 * Atleast one process is alive and is voting for Real Time
		 * data - Diag should be in real time mode irrespective of USB
		 * connection.
		 */
		real_time = MODE_REALTIME;
	} else if (driver->usb_connected) {
		/*
		 * If USB is connected, check individual process. If Memory
		 * Device Mode is active, set the mode requested by Memory
		 * Device process. Set to realtime mode otherwise.
		 */
		if ((driver->proc_rt_vote_mask[idx] &
						DIAG_PROC_MEMORY_DEVICE) == 0)
			real_time = MODE_NONREALTIME;
		else
			real_time = MODE_REALTIME;
	} else {
		/*
		 * We come here if USB is not connected and the active
		 * processes are voting for Non realtime mode.
		 */
		real_time = MODE_NONREALTIME;
	}
	return real_time;
}

static void diag_create_diag_mode_ctrl_pkt(unsigned char *dest_buf,
					   int real_time)
{
	struct diag_ctrl_msg_diagmode diagmode;
	int msg_size = sizeof(struct diag_ctrl_msg_diagmode);

	if (!dest_buf)
		return;

	diagmode.ctrl_pkt_id = DIAG_CTRL_MSG_DIAGMODE;
	diagmode.ctrl_pkt_data_len = DIAG_MODE_PKT_LEN;
	diagmode.version = 1;
	diagmode.sleep_vote = real_time ? 1 : 0;
	/*
	 * 0 - Disables real-time logging (to prevent
	 *     frequent APPS wake-ups, etc.).
	 * 1 - Enable real-time logging
	 */
	diagmode.real_time = real_time;
	diagmode.use_nrt_values = 0;
	diagmode.commit_threshold = 0;
	diagmode.sleep_threshold = 0;
	diagmode.sleep_time = 0;
	diagmode.drain_timer_val = 0;
	diagmode.event_stale_timer_val = 0;

	memcpy(dest_buf, &diagmode, msg_size);
}

void diag_update_proc_vote(uint16_t proc, uint8_t vote, int index)
{
	int i;

	mutex_lock(&driver->real_time_mutex);
	if (vote)
		driver->proc_active_mask |= proc;
	else {
		driver->proc_active_mask &= ~proc;
		if (index == ALL_PROC) {
			for (i = 0; i < DIAG_NUM_PROC; i++)
				driver->proc_rt_vote_mask[i] |= proc;
		} else {
			driver->proc_rt_vote_mask[index] |= proc;
		}
	}
	mutex_unlock(&driver->real_time_mutex);
}

void diag_update_real_time_vote(uint16_t proc, uint8_t real_time, int index)
{
	int i;

	mutex_lock(&driver->real_time_mutex);
	if (index == ALL_PROC) {
		for (i = 0; i < DIAG_NUM_PROC; i++) {
			if (real_time)
				driver->proc_rt_vote_mask[i] |= proc;
			else
				driver->proc_rt_vote_mask[i] &= ~proc;
		}
	} else {
		if (real_time)
			driver->proc_rt_vote_mask[index] |= proc;
		else
			driver->proc_rt_vote_mask[index] &= ~proc;
	}
	mutex_unlock(&driver->real_time_mutex);
}


#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
static void diag_send_diag_mode_update_by_hsic(int index, int real_time)
{
	unsigned char *buf = NULL;
	int err = 0;
	struct diag_dci_header_t dci_header;
	int dci_header_size = sizeof(struct diag_dci_header_t);
	int msg_size = sizeof(struct diag_ctrl_msg_diagmode);
	uint32_t write_len = 0;

	if (index < 0 || index > MAX_HSIC_DCI_CH) {
		pr_err("diag: Invalid HSIC channel in %s, index: %d\n",
							__func__, index);
		return;
	}

	if (real_time != MODE_REALTIME && real_time != MODE_NONREALTIME) {
		pr_err("diag: Invalid real time value in %s, type: %d\n",
							__func__, real_time);
		return;
	}

	if (!diag_hsic_dci[index].hsic_ch) {
		pr_debug("diag: In %s, hsic dci channel %d is not enabled.\n",
							__func__, index);
		return;
	}

	buf = dci_get_buffer_from_bridge(index);
	if (!buf) {
		pr_err("diag: In %s, unable to get dci buffers to write data\n",
			__func__);
		return;
	}
	/* Frame the DCI header */
	dci_header.start = CONTROL_CHAR;
	dci_header.version = 1;
	dci_header.length = msg_size + 1;
	dci_header.cmd_code = DCI_CONTROL_PKT_CODE;

	memcpy(buf + write_len, &dci_header, dci_header_size);
	write_len += dci_header_size;
	diag_create_diag_mode_ctrl_pkt(buf + write_len, real_time);
	write_len += msg_size;
	*(buf + write_len) = CONTROL_CHAR; /* End Terminator */
	write_len += sizeof(uint8_t);
	err = diag_dci_write_bridge(index, buf, write_len);
	if (err != write_len) {
		pr_err("diag: cannot send nrt mode ctrl pkt, err: %d\n", err);
		diagmem_free(driver, buf, POOL_TYPE_HSIC_DCI_WRITE + index);
	} else {
		driver->real_time_mode[index + 1] = real_time;
	}
}
#else
static inline void diag_send_diag_mode_update_by_hsic(int index, int real_time)
{
}
#endif

#ifdef CONFIG_DIAG_OVER_USB
void diag_real_time_work_fn(struct work_struct *work)
{
	int temp_real_time = MODE_REALTIME, i, j;

	for (i = 0; i < DIAG_NUM_PROC; i++) {
		temp_real_time = diag_compute_real_time(i);
		if (temp_real_time == driver->real_time_mode[i]) {
			pr_debug("diag: did not update real time mode on proc %d, already in the req mode %d",
				i, temp_real_time);
			continue;
		}

		if (i == DIAG_LOCAL_PROC) {
			for (j = 0; j < NUM_SMD_CONTROL_CHANNELS; j++)
				diag_send_diag_mode_update_by_smd(
					&driver->smd_cntl[j], temp_real_time);
		} else {
			diag_send_diag_mode_update_by_hsic(i - 1,
							   temp_real_time);
		}
	}

	if (driver->real_time_update_busy > 0)
		driver->real_time_update_busy--;
}
#else
void diag_real_time_work_fn(struct work_struct *work)
{
	int temp_real_time = MODE_REALTIME, i, j;

	for (i = 0; i < DIAG_NUM_PROC; i++) {
		if (driver->proc_active_mask == 0) {
			/*
			 * There are no DCI or Memory Device processes.
			 * Diag should be in Real Time mode.
			 */
			temp_real_time = MODE_REALTIME;
		} else if (!(driver->proc_rt_vote_mask[i] &
						driver->proc_active_mask)) {
			/* No active process is voting for real time mode */
			temp_real_time = MODE_NONREALTIME;
		}
		if (temp_real_time == driver->real_time_mode[i]) {
			pr_debug("diag: did not update real time mode on proc %d, already in the req mode %d",
				i, temp_real_time);
			continue;
		}

		if (i == DIAG_LOCAL_PROC) {
			for (j = 0; j < NUM_SMD_CONTROL_CHANNELS; j++)
				diag_send_diag_mode_update_by_smd(
					&driver->smd_cntl[j], temp_real_time);
		} else {
			diag_send_diag_mode_update_by_hsic(i - 1,
								temp_real_time);
		}
	}

	if (driver->real_time_update_busy > 0)
		driver->real_time_update_busy--;
}
#endif

void diag_send_diag_mode_update_by_smd(struct diag_smd_info *smd_info,
							int real_time)
{
	char buf[sizeof(struct diag_ctrl_msg_diagmode)];
	int msg_size = sizeof(struct diag_ctrl_msg_diagmode);
	int wr_size = -ENOMEM, retry_count = 0, timer;
	struct diag_smd_info *data = NULL;

	if (!smd_info || smd_info->type != SMD_CNTL_TYPE) {
		pr_err("diag: In %s, invalid channel info, smd_info: %p type: %d\n",
					__func__, smd_info,
					((smd_info) ? smd_info->type : -1));
		return;
	}

	if (smd_info->peripheral < MODEM_DATA ||
					smd_info->peripheral > WCNSS_DATA) {
		pr_err("diag: In %s, invalid peripheral %d\n", __func__,
							smd_info->peripheral);
		return;
	}

	data = &driver->smd_data[smd_info->peripheral];
	if (!data)
		return;

	diag_create_diag_mode_ctrl_pkt(buf, real_time);

	mutex_lock(&driver->diag_cntl_mutex);
	if (smd_info->ch) {
		while (retry_count < 3) {
			mutex_lock(&smd_info->smd_ch_mutex);
			wr_size = smd_write(smd_info->ch, buf, msg_size);
			mutex_unlock(&smd_info->smd_ch_mutex);
			if (wr_size == -ENOMEM) {
				/*
				 * The smd channel is full. Delay while
				 * smd processes existing data and smd
				 * has memory become available. The delay
				 * of 2000 was determined empirically as
				 * best value to use.
				 */
				retry_count++;
				for (timer = 0; timer < 5; timer++)
					udelay(2000);
			} else {
				data =
				&driver->smd_data[smd_info->peripheral];
				driver->real_time_mode[DIAG_LOCAL_PROC] =
								real_time;
				break;
			}
		}
		if (wr_size != msg_size)
			pr_err("diag: proc %d fail feature update %d, tried %d",
				smd_info->peripheral,
				wr_size, msg_size);
	} else {
		pr_err("diag: ch invalid, feature update on proc %d\n",
				smd_info->peripheral);
	}
	process_lock_enabling(&data->nrt_lock, real_time);

	mutex_unlock(&driver->diag_cntl_mutex);
}

int diag_send_stm_state(struct diag_smd_info *smd_info,
			  uint8_t stm_control_data)
{
	struct diag_ctrl_msg_stm stm_msg;
	int msg_size = sizeof(struct diag_ctrl_msg_stm);
	int retry_count = 0;
	int wr_size = 0;
	int success = 0;

	if (!smd_info || (smd_info->type != SMD_CNTL_TYPE) ||
		(driver->peripheral_supports_stm[smd_info->peripheral] ==
								DISABLE_STM)) {
		return -EINVAL;
	}

	if (smd_info->ch) {
		stm_msg.ctrl_pkt_id = 21;
		stm_msg.ctrl_pkt_data_len = 5;
		stm_msg.version = 1;
		stm_msg.control_data = stm_control_data;
		while (retry_count < 3) {
			mutex_lock(&smd_info->smd_ch_mutex);
			wr_size = smd_write(smd_info->ch, &stm_msg, msg_size);
			mutex_unlock(&smd_info->smd_ch_mutex);
			if (wr_size == -ENOMEM) {
				/*
				 * The smd channel is full. Delay while
				 * smd processes existing data and smd
				 * has memory become available. The delay
				 * of 10000 was determined empirically as
				 * best value to use.
				 */
				retry_count++;
				usleep_range(10000, 10000);
			} else {
				success = 1;
				break;
			}
		}
		if (wr_size != msg_size) {
			pr_err("diag: In %s, proc %d fail STM update %d, tried %d",
				__func__, smd_info->peripheral, wr_size,
				msg_size);
			success = 0;
		}
	} else {
		pr_err("diag: In %s, ch invalid, STM update on proc %d\n",
				__func__, smd_info->peripheral);
	}
	return success;
}

static int diag_smd_cntl_probe(struct platform_device *pdev)
{
	int r = 0;
	int index = -1;
	const char *channel_name = NULL;

	/* open control ports only on 8960 & newer targets */
	if (chk_apps_only()) {
		if (pdev->id == SMD_APPS_MODEM) {
			index = MODEM_DATA;
			channel_name = "DIAG_CNTL";
		}
		else if (pdev->id == SMD_APPS_QDSP) {
			index = LPASS_DATA;
			channel_name = "DIAG_CNTL";
		}
		else if (pdev->id == SMD_APPS_WCNSS) {
			index = WCNSS_DATA;
			channel_name = "APPS_RIVA_CTRL";
		}

		if (index != -1) {
			r = smd_named_open_on_edge(channel_name,
				pdev->id,
				&driver->smd_cntl[index].ch,
				&driver->smd_cntl[index],
				diag_smd_notify);
			driver->smd_cntl[index].ch_save =
				driver->smd_cntl[index].ch;
			diag_smd_buffer_init(&driver->smd_cntl[index]);
		}
		pr_debug("diag: In %s, open SMD CNTL port, Id = %d, r = %d\n",
			__func__, pdev->id, r);
	}

	return 0;
}

static int diagfwd_cntl_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: suspending...\n");
	return 0;
}

static int diagfwd_cntl_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: resuming...\n");
	return 0;
}

static const struct dev_pm_ops diagfwd_cntl_dev_pm_ops = {
	.runtime_suspend = diagfwd_cntl_runtime_suspend,
	.runtime_resume = diagfwd_cntl_runtime_resume,
};

static struct platform_driver msm_smd_ch1_cntl_driver = {

	.probe = diag_smd_cntl_probe,
	.driver = {
		.name = "DIAG_CNTL",
		.owner = THIS_MODULE,
		.pm   = &diagfwd_cntl_dev_pm_ops,
	},
};

static struct platform_driver diag_smd_lite_cntl_driver = {

	.probe = diag_smd_cntl_probe,
	.driver = {
		.name = "APPS_RIVA_CTRL",
		.owner = THIS_MODULE,
		.pm   = &diagfwd_cntl_dev_pm_ops,
	},
};

int diagfwd_cntl_init(void)
{
	int ret;
	int i;

	reg_dirty = 0;
	driver->polling_reg_flag = 0;
	driver->log_on_demand_support = 1;
	driver->diag_cntl_wq = create_singlethread_workqueue("diag_cntl_wq");
	if (!driver->diag_cntl_wq)
		goto err;

	for (i = 0; i < NUM_SMD_CONTROL_CHANNELS; i++) {
		ret = diag_smd_constructor(&driver->smd_cntl[i], i,
							SMD_CNTL_TYPE);
		if (ret)
			goto err;
	}

	platform_driver_register(&msm_smd_ch1_cntl_driver);
	platform_driver_register(&diag_smd_lite_cntl_driver);

	return 0;
err:
	pr_err("diag: Could not initialize diag buffers");

	for (i = 0; i < NUM_SMD_CONTROL_CHANNELS; i++)
		diag_smd_destructor(&driver->smd_cntl[i]);

	if (driver->diag_cntl_wq)
		destroy_workqueue(driver->diag_cntl_wq);
	return -ENOMEM;
}

void diagfwd_cntl_exit(void)
{
	int i;

	for (i = 0; i < NUM_SMD_CONTROL_CHANNELS; i++)
		diag_smd_destructor(&driver->smd_cntl[i]);

	destroy_workqueue(driver->diag_cntl_wq);
	destroy_workqueue(driver->diag_real_time_wq);

	platform_driver_unregister(&msm_smd_ch1_cntl_driver);
	platform_driver_unregister(&diag_smd_lite_cntl_driver);
}
