/* Copyright (c) 2008-2016, The Linux Foundation. All rights reserved.
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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/ratelimit.h>
#include <linux/workqueue.h>
#include <linux/pm_runtime.h>
#include <linux/diagchar.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/of.h>
#include <linux/kmemleak.h>
#ifdef CONFIG_DIAG_OVER_USB
#include <linux/usb/usbdiag.h>
#endif
#include <soc/qcom/socinfo.h>
#include <soc/qcom/smd.h>
#include <soc/qcom/restart.h>
#include "diagmem.h"
#include "diagchar.h"
#include "diagfwd.h"
#include "diagfwd_cntl.h"
#include "diagchar_hdlc.h"
#include "diag_dci.h"
#include "diag_masks.h"
#include "diag_usb.h"
#include "diag_mux.h"

#define STM_CMD_VERSION_OFFSET	4
#define STM_CMD_MASK_OFFSET	5
#define STM_CMD_DATA_OFFSET	6
#define STM_CMD_NUM_BYTES	7

#define STM_RSP_SUPPORTED_INDEX		7
#define STM_RSP_SMD_STATUS_INDEX	8
#define STM_RSP_NUM_BYTES		9

#define SMD_DRAIN_BUF_SIZE 4096

int diag_debug_buf_idx;
unsigned char diag_debug_buf[1024];
/* Number of entries in table of buffers */
struct diag_master_table entry;
int wrap_enabled;
uint16_t wrap_count;

#define DIAG_NUM_COMMON_CMD	1
static uint8_t common_cmds[DIAG_NUM_COMMON_CMD] = {
	DIAG_CMD_LOG_ON_DMND
};

/* Determine if this device uses a device tree */
#ifdef CONFIG_OF
static int has_device_tree(void)
{
	struct device_node *node;

	node = of_find_node_by_path("/");
	if (node) {
		of_node_put(node);
		return 1;
	}
	return 0;
}
#else
static int has_device_tree(void)
{
	return 0;
}
#endif

int chk_config_get_id(void)
{
	switch (socinfo_get_msm_cpu()) {
	case MSM_CPU_8X60:
		return APQ8060_TOOLS_ID;
	case MSM_CPU_8960:
	case MSM_CPU_8960AB:
		return AO8960_TOOLS_ID;
	case MSM_CPU_8064:
	case MSM_CPU_8064AB:
	case MSM_CPU_8064AA:
		return APQ8064_TOOLS_ID;
	case MSM_CPU_8930:
	case MSM_CPU_8930AA:
	case MSM_CPU_8930AB:
		return MSM8930_TOOLS_ID;
	case MSM_CPU_8974:
		return MSM8974_TOOLS_ID;
	case MSM_CPU_8625:
		return MSM8625_TOOLS_ID;
	case MSM_CPU_8084:
		return APQ8084_TOOLS_ID;
	case MSM_CPU_8916:
		return MSM8916_TOOLS_ID;
	case MSM_CPU_8939:
		return MSM8939_TOOLS_ID;
	case MSM_CPU_8994:
		return MSM8994_TOOLS_ID;
	case MSM_CPU_8226:
		return APQ8026_TOOLS_ID;
	case MSM_CPU_8909:
		return MSM8909_TOOLS_ID;
	case MSM_CPU_8992:
		return MSM8992_TOOLS_ID;
	case MSM_CPU_TELLURIUM:
		return MSMTELLURIUM_TOOLS_ID;
	case MSM_CPU_8929:
		return MSM8929_TOOLS_ID;
	default:
		if (driver->use_device_tree) {
			if (machine_is_msm8974())
				return MSM8974_TOOLS_ID;
			else if (machine_is_apq8074())
				return APQ8074_TOOLS_ID;
			else
				return 0;
		} else {
			return 0;
		}
	}
}

/*
 * This will return TRUE for targets which support apps only mode and hence SSR.
 * This applies to 8960 and newer targets.
 */
int chk_apps_only(void)
{
	if (driver->use_device_tree)
		return 1;

	switch (socinfo_get_msm_cpu()) {
	case MSM_CPU_8960:
	case MSM_CPU_8960AB:
	case MSM_CPU_8064:
	case MSM_CPU_8064AB:
	case MSM_CPU_8064AA:
	case MSM_CPU_8930:
	case MSM_CPU_8930AA:
	case MSM_CPU_8930AB:
	case MSM_CPU_8627:
	case MSM_CPU_9615:
	case MSM_CPU_8974:
		return 1;
	default:
		return 0;
	}
}

/*
 * This will return TRUE for targets which support apps as master.
 * Thus, SW DLOAD and Mode Reset are supported on apps processor.
 * This applies to 8960 and newer targets.
 */
int chk_apps_master(void)
{
	if (driver->use_device_tree)
		return 1;
	else if (soc_class_is_msm8960() || soc_class_is_msm8930() ||
		 soc_class_is_apq8064() || cpu_is_msm9615())
		return 1;
	else
		return 0;
}

int chk_polling_response(void)
{
	if (!(driver->polling_reg_flag) && chk_apps_master())
		/*
		 * If the apps processor is master and no other processor
		 * has registered to respond for polling
		 */
		return 1;
	else if (!((driver->smd_data[MODEM_DATA].ch) &&
		 (driver->rcvd_feature_mask[MODEM_DATA])) &&
		 (chk_apps_master()))
		/*
		 * If the apps processor is not the master and the modem
		 * is not up or we did not receive the feature masks from Modem
		 */
		return 1;
	else
		return 0;
}

/*
 * This function should be called if you feel that the logging process may
 * need to be woken up. For instance, if the logging mode is MEMORY_DEVICE MODE
 * and while trying to read data from a SMD data channel there are no buffers
 * available to read the data into, then this function should be called to
 * determine if the logging process needs to be woken up.
 */
void chk_logging_wakeup(void)
{
	int i;

	/* Find the index of the logging process */
	for (i = 0; i < driver->num_clients; i++)
		if (driver->client_map[i].pid ==
			driver->logging_process_id)
			break;

	if (i < driver->num_clients) {
		/* At very high logging rates a race condition can
		 * occur where the buffers containing the data from
		 * an smd channel are all in use, but the data_ready
		 * flag is cleared. In this case, the buffers never
		 * have their data read/logged.  Detect and remedy this
		 * situation.
		 */
		if ((driver->data_ready[i] & USER_SPACE_DATA_TYPE) == 0) {
			driver->data_ready[i] |= USER_SPACE_DATA_TYPE;
			pr_debug("diag: Force wakeup of logging process\n");
			wake_up_interruptible(&driver->wait_q);
		}
	}
}
int diag_add_hdlc_encoding(struct diag_smd_info *smd_info, void *buf,
			   int total_recd, uint8_t *encode_buf,
			   int *encoded_length)
{
	struct diag_send_desc_type send = { NULL, NULL, DIAG_STATE_START, 0 };
	struct diag_hdlc_dest_type enc = { NULL, NULL, 0 };
	struct data_header {
		uint8_t control_char;
		uint8_t version;
		uint16_t length;
	};
	struct data_header *header;
	int header_size = sizeof(struct data_header);
	uint8_t *end_control_char;
	uint8_t *payload;
	uint8_t *temp_buf;
	uint8_t *temp_encode_buf;
	int src_pkt_len;
	int encoded_pkt_length;
	int max_size;
	int total_processed = 0;
	int bytes_remaining;
	int success = 1;

	temp_buf = buf;
	temp_encode_buf = encode_buf;
	bytes_remaining = *encoded_length;
	while (total_processed < total_recd) {
		header = (struct data_header *)temp_buf;
		/* Perform initial error checking */
		if (header->control_char != CONTROL_CHAR ||
			header->version != 1) {
			success = 0;
			break;
		}
		payload = temp_buf + header_size;
		end_control_char = payload + header->length;
		if (*end_control_char != CONTROL_CHAR) {
			success = 0;
			break;
		}

		max_size = 2 * header->length + 3;
		if (bytes_remaining < max_size) {
			pr_err("diag: In %s, Not enough room to encode remaining data for peripheral: %d, bytes available: %d, max_size: %d\n",
				__func__, smd_info->peripheral,
				bytes_remaining, max_size);
			success = 0;
			break;
		}

		/* Prepare for encoding the data */
		send.state = DIAG_STATE_START;
		send.pkt = payload;
		send.last = (void *)(payload + header->length - 1);
		send.terminate = 1;

		enc.dest = temp_encode_buf;
		enc.dest_last = (void *)(temp_encode_buf + max_size);
		enc.crc = 0;
		diag_hdlc_encode(&send, &enc);

		/* Prepare for next packet */
		src_pkt_len = (header_size + header->length + 1);
		total_processed += src_pkt_len;
		temp_buf += src_pkt_len;

		encoded_pkt_length = (uint8_t *)enc.dest - temp_encode_buf;
		bytes_remaining -= encoded_pkt_length;
		temp_encode_buf = enc.dest;
	}

	*encoded_length = (int)(temp_encode_buf - encode_buf);

	return success;
}

static int check_bufsize_for_encoding(struct diag_smd_info *smd_info, void *buf,
					int total_recd)
{
	int buf_size = IN_BUF_SIZE;
	int max_size = 2 * total_recd + 3;
	unsigned char *temp_buf;

	if (max_size > IN_BUF_SIZE) {
		if (max_size > MAX_IN_BUF_SIZE) {
			pr_err_ratelimited("diag: In %s, SMD sending packet of %d bytes that may expand to %d bytes, peripheral: %d\n",
				__func__, total_recd, max_size,
				smd_info->peripheral);
			max_size = MAX_IN_BUF_SIZE;
		}
		if (buf == smd_info->buf_in_1_raw) {
			/* Only realloc if we need to increase the size */
			if (smd_info->buf_in_1_size < max_size) {
				temp_buf = krealloc(smd_info->buf_in_1,
					max_size, GFP_KERNEL);
				if (temp_buf) {
					smd_info->buf_in_1 = temp_buf;
					smd_info->buf_in_1_size = max_size;
				} else {
					return -ENOMEM;
				}
			}
			buf_size = smd_info->buf_in_1_size;
		} else {
			/* Only realloc if we need to increase the size */
			if (smd_info->buf_in_2_size < max_size) {
				temp_buf = krealloc(smd_info->buf_in_2,
					max_size, GFP_KERNEL);
				if (temp_buf) {
					smd_info->buf_in_2 = temp_buf;
					smd_info->buf_in_2_size = max_size;
				} else {
					return -ENOMEM;
				}
			}
			buf_size = smd_info->buf_in_2_size;
		}
	}

	return buf_size;
}

/* Process the data read from the smd data channel */
int diag_process_smd_read_data(struct diag_smd_info *smd_info, void *buf,
			       int total_recd)
{
	int *in_busy_ptr = 0;
	int err = 0;
	int success = 0;
	int write_length = total_recd;
	int ctxt = 0;
	unsigned char *write_buf = NULL;

	unsigned long flags;

	/*
	 * Do not process data on command channel if the
	 * channel is not designated to do so
	 */
	if ((smd_info->type == SMD_CMD_TYPE) &&
		!driver->separate_cmdrsp[smd_info->peripheral]) {
		pr_debug("diag, In %s, received data on non-designated command channel: %d\n",
			__func__, smd_info->peripheral);
		return 0;
	}

	if (!smd_info->encode_hdlc) {
		/* If the data is already hdlc encoded */
		if (smd_info->buf_in_1 == buf) {
			in_busy_ptr = &smd_info->in_busy_1;
			ctxt = smd_info->buf_in_1_ctxt;
		} else if (smd_info->buf_in_2 == buf) {
			in_busy_ptr = &smd_info->in_busy_2;
			ctxt = smd_info->buf_in_2_ctxt;
		} else {
			pr_err("diag: In %s, no match for in_busy_1, peripheral: %d\n",
				__func__, smd_info->peripheral);
			return -EIO;
		}
		write_buf = buf;
		success = 1;
	} else {
		/* The data is raw and needs to be hdlc encoded */
		write_length = check_bufsize_for_encoding(smd_info, buf,
							  total_recd);
		if (write_length < 0)
			return write_length;
		if (smd_info->buf_in_1_raw == buf) {
			write_buf = smd_info->buf_in_1;
			in_busy_ptr = &smd_info->in_busy_1;
			ctxt = smd_info->buf_in_1_ctxt;
		} else if (smd_info->buf_in_2_raw == buf) {
			write_buf = smd_info->buf_in_2;
			in_busy_ptr = &smd_info->in_busy_2;
			ctxt = smd_info->buf_in_2_ctxt;
		} else {
			pr_err("diag: In %s, no match for in_busy_1, peripheral: %d\n",
				__func__, smd_info->peripheral);
			return -EIO;
		}
		success = diag_add_hdlc_encoding(smd_info, buf,
						 total_recd, write_buf,
						 &write_length);
	}

	if (!success) {
		pr_err_ratelimited("diag: smd data write unsuccessful, success: %d\n",
				   success);
		return 0;
	}

	if (write_length > 0) {
		spin_lock_irqsave(&smd_info->in_busy_lock, flags);
		*in_busy_ptr = 1;
		spin_unlock_irqrestore(&smd_info->in_busy_lock, flags);
		err = diag_mux_write(DIAG_LOCAL_PROC, write_buf, write_length,
				     ctxt);
		if (err) {
			pr_err_ratelimited("diag: In %s, diag_device_write error: %d\n",
					   __func__, err);
		}
	}

	return 0;
}

static int diag_smd_resize_buf(struct diag_smd_info *smd_info, void **buf,
			       unsigned int *buf_size,
			       unsigned int requested_size)
{
	int success = 0;
	void *temp_buf = NULL;
	unsigned int new_buf_size = requested_size;

	if (!smd_info)
		return success;

	if (requested_size <= MAX_IN_BUF_SIZE) {
		pr_debug("diag: In %s, SMD peripheral: %d sending in packets up to %d bytes\n",
			__func__, smd_info->peripheral, requested_size);
	} else {
		pr_err_ratelimited("diag: In %s, SMD peripheral: %d, Packet size sent: %d, Max size supported (%d) exceeded. Data beyond max size will be lost\n",
			__func__, smd_info->peripheral, requested_size,
			MAX_IN_BUF_SIZE);
		new_buf_size = MAX_IN_BUF_SIZE;
	}

	/* Only resize if the buffer can be increased in size */
	if (new_buf_size <= *buf_size) {
		success = 1;
		return success;
	}

	temp_buf = krealloc(*buf, new_buf_size, GFP_KERNEL);

	if (temp_buf) {
		/* Match the buffer and reset the pointer and size */
		if (smd_info->encode_hdlc) {
			/*
			 * This smd channel is supporting HDLC encoding
			 * on the apps
			 */
			void *temp_hdlc = NULL;
			if (*buf == smd_info->buf_in_1_raw) {
				smd_info->buf_in_1_raw = temp_buf;
				smd_info->buf_in_1_raw_size = new_buf_size;
				temp_hdlc = krealloc(smd_info->buf_in_1,
							MAX_IN_BUF_SIZE,
							GFP_KERNEL);
				if (temp_hdlc) {
					smd_info->buf_in_1 = temp_hdlc;
					smd_info->buf_in_1_size =
							MAX_IN_BUF_SIZE;
				}
			} else if (*buf == smd_info->buf_in_2_raw) {
				smd_info->buf_in_2_raw = temp_buf;
				smd_info->buf_in_2_raw_size = new_buf_size;
				temp_hdlc = krealloc(smd_info->buf_in_2,
							MAX_IN_BUF_SIZE,
							GFP_KERNEL);
				if (temp_hdlc) {
					smd_info->buf_in_2 = temp_hdlc;
					smd_info->buf_in_2_size =
							MAX_IN_BUF_SIZE;
				}
			}
		} else {
			if (*buf == smd_info->buf_in_1) {
				smd_info->buf_in_1 = temp_buf;
				smd_info->buf_in_1_size = new_buf_size;
			} else if (*buf == smd_info->buf_in_2) {
				smd_info->buf_in_2 = temp_buf;
				smd_info->buf_in_2_size = new_buf_size;
			}
		}
		*buf = temp_buf;
		*buf_size = new_buf_size;
		success = 1;
	} else {
		pr_err_ratelimited("diag: In %s, SMD peripheral: %d. packet size sent: %d, resize to support failed. Data beyond %d will be lost\n",
			__func__, smd_info->peripheral, requested_size,
			*buf_size);
	}

	return success;
}

void diag_smd_send_req(struct diag_smd_info *smd_info)
{
	void *buf = NULL, *temp_buf = NULL;
	int total_recd = 0, r = 0, pkt_len;
	int loop_count = 0, total_recd_partial = 0;
	int notify = 0;
	int buf_size = 0;
	int resize_success = 0;
	int buf_full = 0;

	if (!smd_info) {
		pr_err("diag: In %s, no smd info. Not able to read.\n",
			__func__);
		return;
	}

	/* Determine the buffer to read the data into. */
	if (smd_info->type == SMD_DATA_TYPE) {
		/* If the data is raw and not hdlc encoded */
		if (smd_info->encode_hdlc) {
			if (!smd_info->in_busy_1) {
				buf = smd_info->buf_in_1_raw;
				buf_size = smd_info->buf_in_1_raw_size;
			} else if (!smd_info->in_busy_2) {
				buf = smd_info->buf_in_2_raw;
				buf_size = smd_info->buf_in_2_raw_size;
			}
		} else {
			if (!smd_info->in_busy_1) {
				buf = smd_info->buf_in_1;
				buf_size = smd_info->buf_in_1_size;
			} else if (!smd_info->in_busy_2) {
				buf = smd_info->buf_in_2;
				buf_size = smd_info->buf_in_2_size;
			}
		}
	} else if (smd_info->type == SMD_CMD_TYPE) {
		/* If the data is raw and not hdlc encoded */
		if (smd_info->encode_hdlc) {
			if (!smd_info->in_busy_1) {
				buf = smd_info->buf_in_1_raw;
				buf_size = smd_info->buf_in_1_raw_size;
			}
		} else {
			if (!smd_info->in_busy_1) {
				buf = smd_info->buf_in_1;
				buf_size = smd_info->buf_in_1_size;
			}
		}
	} else if (!smd_info->in_busy_1) {
		buf = smd_info->buf_in_1;
		buf_size = smd_info->buf_in_1_size;
	}

	if (!buf)
		goto fail_return;

	if (smd_info->ch && buf) {
		int required_size = 0;
		while ((pkt_len = smd_cur_packet_size(smd_info->ch)) != 0) {
			total_recd_partial = 0;

		required_size = pkt_len + total_recd;
		if (required_size > buf_size)
			resize_success = diag_smd_resize_buf(smd_info, &buf,
						&buf_size, required_size);

		temp_buf = ((unsigned char *)buf) + total_recd;
		while (pkt_len && (pkt_len != total_recd_partial)) {
			loop_count++;
			r = smd_read_avail(smd_info->ch);
			pr_debug("diag: In %s, SMD peripheral: %d, received pkt %d %d\n",
				__func__, smd_info->peripheral, r, total_recd);
			if (!r) {
				/* Nothing to read from SMD */
				wait_event(driver->smd_wait_q,
					((smd_info->ch == 0) ||
					smd_read_avail(smd_info->ch)));
				/* If the smd channel is open */
				if (smd_info->ch) {
					pr_debug("diag: In %s, SMD peripheral: %d, return from wait_event\n",
						__func__, smd_info->peripheral);
					continue;
				} else {
					pr_debug("diag: In %s, SMD peripheral: %d, return from wait_event ch closed\n",
						__func__, smd_info->peripheral);
					goto fail_return;
				}
			}

			if (pkt_len < r) {
				pr_err("diag: In %s, SMD peripheral: %d, sending incorrect pkt\n",
					__func__, smd_info->peripheral);
				goto fail_return;
			}
			if (pkt_len > r) {
				pr_debug("diag: In %s, SMD sending partial pkt %d %d %d %d %d %d\n",
				__func__, pkt_len, r, total_recd, loop_count,
				smd_info->peripheral, smd_info->type);
			}

			/* Protect from going beyond the end of the buffer */
			if (total_recd < buf_size) {
				if (total_recd + r > buf_size) {
					r = buf_size - total_recd;
					buf_full = 1;
				}

				total_recd += r;
				total_recd_partial += r;

				/* Keep reading for complete packet */
				smd_read(smd_info->ch, temp_buf, r);
				temp_buf += r;
			} else {
				/*
				 * This block handles the very rare case of a
				 * packet that is greater in length than what
				 * we can support. In this case, we
				 * incrementally drain the remaining portion
				 * of the packet that will not fit in the
				 * buffer, so that the entire packet is read
				 * from the smd.
				 */
				int drain_bytes = (r > SMD_DRAIN_BUF_SIZE) ?
							SMD_DRAIN_BUF_SIZE : r;
				unsigned char *drain_buf = kzalloc(drain_bytes,
								GFP_KERNEL);
				if (drain_buf) {
					total_recd += drain_bytes;
					total_recd_partial += drain_bytes;
					smd_read(smd_info->ch, drain_buf,
							drain_bytes);
					kfree(drain_buf);
				} else {
					pr_err("diag: In %s, SMD peripheral: %d, unable to allocate drain buffer\n",
						__func__, smd_info->peripheral);
					break;
				}
			}
		}

		if ((smd_info->type != SMD_CNTL_TYPE &&
				smd_info->type != SMD_CMD_TYPE)
					|| buf_full)
			break;

		}

		if (total_recd > 0) {
			if (!buf) {
				pr_err("diag: In %s, SMD peripheral: %d, Out of diagmem for Modem\n",
					__func__, smd_info->peripheral);
			} else if (smd_info->process_smd_read_data) {
				/*
				 * If the buffer was totally filled, reset
				 * total_recd appropriately
				 */
				if (buf_full)
					total_recd = buf_size;

				notify = smd_info->process_smd_read_data(
						smd_info, buf, total_recd);
				/* Poll SMD channels to check for data */
				if (notify)
					diag_smd_notify(smd_info,
							SMD_EVENT_DATA);
			}
		} else {
			goto fail_return;
		}
	} else if (smd_info->ch && !buf &&
		(driver->logging_mode == MEMORY_DEVICE_MODE)) {
			chk_logging_wakeup();
	}
	return;

fail_return:
	if (smd_info->type == SMD_DCI_TYPE ||
	    smd_info->type == SMD_DCI_CMD_TYPE ||
	    driver->logging_mode == MEMORY_DEVICE_MODE)
		diag_ws_release();
	return;
}

void diag_read_smd_work_fn(struct work_struct *work)
{
	struct diag_smd_info *smd_info = container_of(work,
							struct diag_smd_info,
							diag_read_smd_work);
	diag_smd_send_req(smd_info);
}

void encode_rsp_and_send(int buf_length)
{
	struct diag_send_desc_type send = { NULL, NULL, DIAG_STATE_START, 0 };
	struct diag_hdlc_dest_type enc = { NULL, NULL, 0 };
	unsigned char *rsp_ptr = driver->encoded_rsp_buf;
	int err, retry_count = 0;
	unsigned long flags;

	if (!rsp_ptr)
		return;

	if (buf_length > APPS_BUF_SIZE || buf_length < 0) {
		pr_err("diag: In %s, invalid len %d, permissible len %d\n",
					__func__, buf_length, APPS_BUF_SIZE);
		return;
	}

	/*
	 * Keep trying till we get the buffer back. It should probably
	 * take one or two iterations. When this loops till UINT_MAX, it
	 * means we did not get a write complete for the previous
	 * response.
	 */
	while (retry_count < UINT_MAX) {
		if (!driver->rsp_buf_busy)
			break;
		/*
		 * Wait for sometime and try again. The value 10000 was chosen
		 * empirically as an optimum value for USB to complete a write
		 */
		usleep_range(10000, 10100);
		retry_count++;

		/*
		 * There can be a race conditon that clears the data ready flag
		 * for responses. Make sure we don't miss previous wakeups for
		 * draining responses when we are in Memory Device Mode.
		 */
		if (driver->logging_mode == MEMORY_DEVICE_MODE)
			chk_logging_wakeup();
	}
	if (driver->rsp_buf_busy) {
		pr_err("diag: unable to get hold of response buffer\n");
		return;
	}

	spin_lock_irqsave(&driver->rsp_buf_busy_lock, flags);
	driver->rsp_buf_busy = 1;
	spin_unlock_irqrestore(&driver->rsp_buf_busy_lock, flags);
	send.state = DIAG_STATE_START;
	send.pkt = driver->apps_rsp_buf;
	send.last = (void *)(driver->apps_rsp_buf + buf_length);
	send.terminate = 1;
	enc.dest = rsp_ptr;
	enc.dest_last = (void *)(rsp_ptr + HDLC_OUT_BUF_SIZE - 1);
	diag_hdlc_encode(&send, &enc);
	driver->encoded_rsp_len = (int)(enc.dest - (void *)rsp_ptr);
	err = diag_mux_write(DIAG_LOCAL_PROC, rsp_ptr, driver->encoded_rsp_len,
			     driver->rsp_buf_ctxt);
	if (err) {
		pr_err("diag: In %s, Unable to write to device, err: %d\n",
			__func__, err);
		spin_lock_irqsave(&driver->rsp_buf_busy_lock, flags);
		driver->rsp_buf_busy = 0;
		spin_unlock_irqrestore(&driver->rsp_buf_busy_lock, flags);
	}
	memset(driver->apps_rsp_buf, '\0', APPS_BUF_SIZE);

}

void diag_update_pkt_buffer(unsigned char *buf, int type)
{
	unsigned char *ptr = NULL;
	unsigned char *temp = buf;
	unsigned int length;
	int *in_busy = NULL;

	if (!buf) {
		pr_err("diag: Invalid buffer in %s\n", __func__);
		return;
	}

	switch (type) {
	case PKT_TYPE:
		ptr = driver->pkt_buf;
		length = driver->pkt_length;
		in_busy = &driver->in_busy_pktdata;
		break;
	case DCI_PKT_TYPE:
		ptr = driver->dci_pkt_buf;
		length = driver->dci_pkt_length;
		in_busy = &driver->in_busy_dcipktdata;
		break;
	default:
		pr_err("diag: Invalid type %d in %s\n", type, __func__);
		return;
	}

	if (!ptr || length == 0) {
		pr_err("diag: Invalid ptr %pK and length %d in %s",
						ptr, length, __func__);
		return;
	}
	mutex_lock(&driver->diagchar_mutex);
	if (CHK_OVERFLOW(ptr, ptr, ptr + PKT_SIZE, length)) {
		memcpy(ptr, temp , length);
		*in_busy = 1;
	} else {
		printk(KERN_CRIT " Not enough buffer space for PKT_RESP\n");
	}
	mutex_unlock(&driver->diagchar_mutex);
}

void diag_update_userspace_clients(unsigned int type)
{
	int i;

	mutex_lock(&driver->diagchar_mutex);
	for (i = 0; i < driver->num_clients; i++)
		if (driver->client_map[i].pid != 0)
			driver->data_ready[i] |= type;
	wake_up_interruptible(&driver->wait_q);
	mutex_unlock(&driver->diagchar_mutex);
}

void diag_update_sleeping_process(int process_id, int data_type)
{
	int i;

	mutex_lock(&driver->diagchar_mutex);
	for (i = 0; i < driver->num_clients; i++)
		if (driver->client_map[i].pid == process_id) {
			driver->data_ready[i] |= data_type;
			break;
		}
	wake_up_interruptible(&driver->wait_q);
	mutex_unlock(&driver->diagchar_mutex);
}

int diag_send_data(struct diag_master_table entry, unsigned char *buf,
					 int len, int type)
{
	int success = 1;
	int err = 0;
	driver->pkt_length = len;

	/* If the process_id corresponds to an apps process */
	if (entry.process_id != NON_APPS_PROC) {
		/* If the message is to be sent to the apps process */
		if (type != MODEM_DATA) {
			diag_update_pkt_buffer(buf, PKT_TYPE);
			diag_update_sleeping_process(entry.process_id,
							PKT_TYPE);
		}
	} else {
		if (len > 0) {
			if (entry.client_id < NUM_SMD_DATA_CHANNELS) {
				struct diag_smd_info *smd_info;
				int index = entry.client_id;
				if (!driver->rcvd_feature_mask[
					entry.client_id]) {
					pr_debug("diag: In %s, feature mask for peripheral: %d not received yet\n",
						__func__, entry.client_id);
					return 0;
				}

				smd_info = (driver->separate_cmdrsp[index] &&
						index < NUM_SMD_CMD_CHANNELS) ?
						&driver->smd_cmd[index] :
						&driver->smd_data[index];
				err = diag_smd_write(smd_info, buf, len);
				if (err) {
					pr_err_ratelimited("diag: In %s, unable to write to smd, peripheral: %d, type: %d, err: %d\n",
						__func__, smd_info->peripheral,
						smd_info->type, err);
				}
			} else {
				pr_alert("diag: In %s, incorrect channel: %d",
					__func__, entry.client_id);
				success = 0;
			}
		}
	}

	return success;
}

void diag_process_stm_mask(uint8_t cmd, uint8_t data_mask, int data_type)
{
	int status = 0;
	if (data_type >= MODEM_DATA && data_type <= SENSORS_DATA) {
		if (driver->peripheral_supports_stm[data_type]) {
			status = diag_send_stm_state(
				&driver->smd_cntl[data_type], cmd);
			if (status == 1)
				driver->stm_state[data_type] = cmd;
		}
		driver->stm_state_requested[data_type] = cmd;
	} else if (data_type == APPS_DATA) {
		driver->stm_state[data_type] = cmd;
		driver->stm_state_requested[data_type] = cmd;
	}
}

int diag_process_stm_cmd(unsigned char *buf, unsigned char *dest_buf)
{
	uint8_t version, mask, cmd;
	uint8_t rsp_supported = 0;
	uint8_t rsp_smd_status = 0;
	int i;

	if (!buf || !dest_buf) {
		pr_err("diag: Invalid pointers buf: %pK, dest_buf %pK in %s\n",
		       buf, dest_buf, __func__);
		return -EIO;
	}

	version = *(buf + STM_CMD_VERSION_OFFSET);
	mask = *(buf + STM_CMD_MASK_OFFSET);
	cmd = *(buf + STM_CMD_DATA_OFFSET);

	/*
	 * Check if command is valid. If the command is asking for
	 * status, then the processor mask field is to be ignored.
	 */
	if ((version != 2) || (cmd > STATUS_STM) ||
		((cmd != STATUS_STM) && ((mask == 0) || (0 != (mask >> 4))))) {
		/* Command is invalid. Send bad param message response */
		dest_buf[0] = BAD_PARAM_RESPONSE_MESSAGE;
		for (i = 0; i < STM_CMD_NUM_BYTES; i++)
			dest_buf[i+1] = *(buf + i);
		return STM_CMD_NUM_BYTES+1;
	} else if (cmd != STATUS_STM) {
		if (mask & DIAG_STM_MODEM)
			diag_process_stm_mask(cmd, DIAG_STM_MODEM, MODEM_DATA);

		if (mask & DIAG_STM_LPASS)
			diag_process_stm_mask(cmd, DIAG_STM_LPASS, LPASS_DATA);

		if (mask & DIAG_STM_WCNSS)
			diag_process_stm_mask(cmd, DIAG_STM_WCNSS, WCNSS_DATA);

		if (mask & DIAG_STM_SENSORS)
			diag_process_stm_mask(cmd, DIAG_STM_SENSORS,
						SENSORS_DATA);

		if (mask & DIAG_STM_APPS)
			diag_process_stm_mask(cmd, DIAG_STM_APPS, APPS_DATA);
	}

	for (i = 0; i < STM_CMD_NUM_BYTES; i++)
		dest_buf[i] = *(buf + i);

	/* Set mask denoting which peripherals support STM */
	if (driver->peripheral_supports_stm[MODEM_DATA])
		rsp_supported |= DIAG_STM_MODEM;

	if (driver->peripheral_supports_stm[LPASS_DATA])
		rsp_supported |= DIAG_STM_LPASS;

	if (driver->peripheral_supports_stm[WCNSS_DATA])
		rsp_supported |= DIAG_STM_WCNSS;

	if (driver->peripheral_supports_stm[SENSORS_DATA])
		rsp_supported |= DIAG_STM_SENSORS;

	rsp_supported |= DIAG_STM_APPS;

	/* Set mask denoting STM state/status for each peripheral/APSS */
	if (driver->stm_state[MODEM_DATA])
		rsp_smd_status |= DIAG_STM_MODEM;

	if (driver->stm_state[LPASS_DATA])
		rsp_smd_status |= DIAG_STM_LPASS;

	if (driver->stm_state[WCNSS_DATA])
		rsp_smd_status |= DIAG_STM_WCNSS;

	if (driver->stm_state[SENSORS_DATA])
		rsp_smd_status |= DIAG_STM_SENSORS;

	if (driver->stm_state[APPS_DATA])
		rsp_smd_status |= DIAG_STM_APPS;

	dest_buf[STM_RSP_SUPPORTED_INDEX] = rsp_supported;
	dest_buf[STM_RSP_SMD_STATUS_INDEX] = rsp_smd_status;

	return STM_RSP_NUM_BYTES;
}

int diag_cmd_log_on_demand(unsigned char *src_buf, int src_len,
			   unsigned char *dest_buf, int dest_len)
{
	int write_len = 0;
	struct diag_log_on_demand_rsp_t header;

	if (driver->smd_cntl[MODEM_DATA].ch && !driver->log_on_demand_support)
		return 0;

	if (!src_buf || !dest_buf || src_len <= 0 || dest_len <= 0) {
		pr_err("diag: Invalid input in %s, src_buf: %pK, src_len: %d, dest_buf: %pK, dest_len: %d",
		       __func__, src_buf, src_len, dest_buf, dest_len);
		return -EINVAL;
	}

	header.cmd_code = DIAG_CMD_LOG_ON_DMND;
	header.log_code = *(uint16_t *)(src_buf + 1);
	header.status = 1;
	memcpy(dest_buf, &header, sizeof(struct diag_log_on_demand_rsp_t));
	write_len += sizeof(struct diag_log_on_demand_rsp_t);

	return write_len;
}

int diag_cmd_get_mobile_id(unsigned char *src_buf, int src_len,
			   unsigned char *dest_buf, int dest_len)
{
	int write_len = 0;
	struct diag_pkt_header_t *header = NULL;
	struct diag_cmd_ext_mobile_rsp_t rsp;

	if (!src_buf || src_len != sizeof(*header) || !dest_buf ||
	    dest_len < sizeof(rsp))
		return -EIO;

	header = (struct diag_pkt_header_t *)src_buf;
	rsp.header.cmd_code = header->cmd_code;
	rsp.header.subsys_id = header->subsys_id;
	rsp.header.subsys_cmd_code = header->subsys_cmd_code;
	rsp.version = 1;
	rsp.padding[0] = 0;
	rsp.padding[1] = 0;
	rsp.padding[2] = 0;
	rsp.family = (uint32_t)socinfo_get_msm_cpu();

	memcpy(dest_buf, &rsp, sizeof(rsp));
	write_len += sizeof(rsp);

	return write_len;
}

int diag_check_common_cmd(struct diag_pkt_header_t *header)
{
	int i;

	if (!header)
		return -EIO;

	for (i = 0; i < DIAG_NUM_COMMON_CMD; i++) {
		if (header->cmd_code == common_cmds[i])
			return 1;
	}

	return 0;
}

int diag_process_apps_pkt(unsigned char *buf, int len)
{
	uint16_t subsys_cmd_code;
	int subsys_id;
	int packet_type = 1, i, cmd_code;
	unsigned char *temp = buf;
	int data_type;
	int mask_ret;
	int status = 0;
	int write_len = 0;

	/* Check if the command is a supported mask command */
	mask_ret = diag_process_apps_masks(buf, len);
	if (mask_ret > 0) {
		encode_rsp_and_send(mask_ret - 1);
		return 0;
	}

	/* Check for registered clients and forward packet to apropriate proc */
	cmd_code = (int)(*(char *)buf);
	temp++;
	subsys_id = (int)(*(char *)temp);
	temp++;
	subsys_cmd_code = *(uint16_t *)temp;
	temp += 2;
	data_type = APPS_DATA;
	/* Dont send any command other than mode reset */
	if (chk_apps_master() && cmd_code == MODE_CMD) {
		if (subsys_id != RESET_ID)
			data_type = MODEM_DATA;
	}

	pr_debug("diag: %d %d %d", cmd_code, subsys_id, subsys_cmd_code);
	for (i = 0; i < diag_max_reg; i++) {
		entry = driver->table[i];
		if (entry.process_id != NO_PROCESS) {
			if (entry.cmd_code == cmd_code && entry.subsys_id ==
				 subsys_id && entry.cmd_code_lo <=
							 subsys_cmd_code &&
				  entry.cmd_code_hi >= subsys_cmd_code) {
				status = diag_send_data(entry, buf, len,
								data_type);
				if (status)
					packet_type = 0;
			} else if (entry.cmd_code == 255
				  && cmd_code == 75) {
				if (entry.subsys_id ==
					subsys_id &&
				   entry.cmd_code_lo <=
					subsys_cmd_code &&
					 entry.cmd_code_hi >=
					subsys_cmd_code) {
					status = diag_send_data(entry, buf,
								len, data_type);
					if (status)
						packet_type = 0;
				}
			} else if (entry.cmd_code == 255 &&
				  entry.subsys_id == 255) {
				if (entry.cmd_code_lo <=
						 cmd_code &&
						 entry.
						cmd_code_hi >= cmd_code) {
					if (cmd_code == MODE_CMD &&
							subsys_id == RESET_ID &&
							entry.process_id ==
							NON_APPS_PROC)
						continue;
					status = diag_send_data(entry, buf, len,
								 data_type);
					if (status)
						packet_type = 0;
				}
			}
		}
	}
#if defined(CONFIG_DIAG_OVER_USB)
	/* Check for the command/respond msg for the maximum packet length */
	if ((*buf == 0x4b) && (*(buf+1) == 0x12) &&
		(*(uint16_t *)(buf+2) == 0x0055)) {
		for (i = 0; i < 4; i++)
			*(driver->apps_rsp_buf+i) = *(buf+i);
		*(uint32_t *)(driver->apps_rsp_buf+4) = PKT_SIZE;
		encode_rsp_and_send(7);
		return 0;
	} else if ((*buf == 0x4b) && (*(buf+1) == 0x12) &&
		(*(uint16_t *)(buf+2) == DIAG_DIAG_STM)) {
		len = diag_process_stm_cmd(buf, driver->apps_rsp_buf);
		if (len > 0) {
			encode_rsp_and_send(len - 1);
			return 0;
		}
		return len;
	}
	/* Check for download command */
	else if ((cpu_is_msm8x60() || chk_apps_master()) && (*buf == 0x3A)) {
		/* send response back */
		driver->apps_rsp_buf[0] = *buf;
		encode_rsp_and_send(0);
		msleep(5000);
		/* call download API */
		msm_set_restart_mode(RESTART_DLOAD);
		printk(KERN_CRIT "diag: download mode set, Rebooting SoC..\n");
		kernel_restart(NULL);
		/* Not required, represents that command isnt sent to modem */
		return 0;
	}
	/* Check for polling for Apps only DIAG */
	else if ((*buf == 0x4b) && (*(buf+1) == 0x32) &&
		(*(buf+2) == 0x03)) {
		/* If no one has registered for polling */
		if (chk_polling_response()) {
			/* Respond to polling for Apps only DIAG */
			for (i = 0; i < 3; i++)
				driver->apps_rsp_buf[i] = *(buf+i);
			for (i = 0; i < 13; i++)
				driver->apps_rsp_buf[i+3] = 0;

			encode_rsp_and_send(15);
			return 0;
		}
	}
	/* Return the Delayed Response Wrap Status */
	else if ((*buf == 0x4b) && (*(buf+1) == 0x32) &&
		(*(buf+2) == 0x04) && (*(buf+3) == 0x0)) {
		memcpy(driver->apps_rsp_buf, buf, 4);
		driver->apps_rsp_buf[4] = wrap_enabled;
		encode_rsp_and_send(4);
		return 0;
	}
	/* Wrap the Delayed Rsp ID */
	else if ((*buf == 0x4b) && (*(buf+1) == 0x32) &&
		(*(buf+2) == 0x05) && (*(buf+3) == 0x0)) {
		wrap_enabled = true;
		memcpy(driver->apps_rsp_buf, buf, 4);
		driver->apps_rsp_buf[4] = wrap_count;
		encode_rsp_and_send(5);
		return 0;
	}
	/* Log on Demand Rsp */
	else if (*buf == DIAG_CMD_LOG_ON_DMND) {
		write_len = diag_cmd_log_on_demand(buf, len,
						   driver->apps_rsp_buf,
						   APPS_BUF_SIZE);
		if (write_len > 0)
			encode_rsp_and_send(write_len - 1);
		return 0;
	}
	/* Mobile ID Rsp */
	else if ((*buf == DIAG_CMD_DIAG_SUBSYS) &&
		(*(buf+1) == DIAG_SS_PARAMS) &&
		(*(buf+2) == DIAG_EXT_MOBILE_ID) && (*(buf+3) == 0x0)) {
		write_len = diag_cmd_get_mobile_id(buf, len,
						   driver->apps_rsp_buf,
						   APPS_BUF_SIZE);
		if (write_len > 0) {
			encode_rsp_and_send(write_len - 1);
			return 0;
		}
	}
	 /*
	  * If the apps processor is master and no other
	  * processor has registered for polling command.
	  * If modem is not up and we have not received feature
	  * mask update from modem, in that case APPS should
	  * respond for 0X7C command
	  */
	else if (chk_apps_master() &&
			!(driver->polling_reg_flag) &&
			!(driver->smd_data[MODEM_DATA].ch) &&
			!(driver->rcvd_feature_mask[MODEM_DATA])) {
		/* respond to 0x0 command */
		if (*buf == 0x00) {
			for (i = 0; i < 55; i++)
				driver->apps_rsp_buf[i] = 0;

			encode_rsp_and_send(54);
			return 0;
		}
		/* respond to 0x7c command */
		else if (*buf == 0x7c) {
			driver->apps_rsp_buf[0] = 0x7c;
			for (i = 1; i < 8; i++)
				driver->apps_rsp_buf[i] = 0;
			/* Tools ID for APQ 8060 */
			*(int *)(driver->apps_rsp_buf + 8) =
							 chk_config_get_id();
			*(unsigned char *)(driver->apps_rsp_buf + 12) = '\0';
			*(unsigned char *)(driver->apps_rsp_buf + 13) = '\0';
			encode_rsp_and_send(13);
			return 0;
		}
	}
#endif
	return packet_type;
}

#ifdef CONFIG_DIAG_OVER_USB
void diag_send_error_rsp(int index)
{
	int i;

	/* -1 to accomodate the first byte 0x13 */
	if (index > APPS_BUF_SIZE-1) {
		pr_err("diag: cannot send err rsp, huge length: %d\n", index);
		return;
	}

	driver->apps_rsp_buf[0] = 0x13; /* error code 13 */
	for (i = 0; i < index; i++)
		driver->apps_rsp_buf[i+1] = *(driver->hdlc_buf+i);
	encode_rsp_and_send(index - 3);
}
#else
static inline void diag_send_error_rsp(int index) {}
#endif

void diag_process_hdlc(void *data, unsigned len)
{
	struct diag_hdlc_decode_type hdlc;
	int ret, type = 0, crc_chk = 0;
	int err = 0;

	mutex_lock(&driver->diag_hdlc_mutex);

	pr_debug("diag: HDLC decode fn, len of data  %d\n", len);
	hdlc.dest_ptr = driver->hdlc_buf;
	hdlc.dest_size = USB_MAX_OUT_BUF;
	hdlc.src_ptr = data;
	hdlc.src_size = len;
	hdlc.src_idx = 0;
	hdlc.dest_idx = 0;
	hdlc.escaping = 0;

	ret = diag_hdlc_decode(&hdlc);
	if (ret) {
		crc_chk = crc_check(hdlc.dest_ptr, hdlc.dest_idx);
		if (crc_chk) {
			/* CRC check failed. */
			pr_err_ratelimited("diag: In %s, bad CRC. Dropping packet\n",
								__func__);
			mutex_unlock(&driver->diag_hdlc_mutex);
			return;
		}
	}

	/*
	 * If the message is 3 bytes or less in length then the message is
	 * too short. A message will need 4 bytes minimum, since there are
	 * 2 bytes for the CRC and 1 byte for the ending 0x7e for the hdlc
	 * encoding
	 */
	if (hdlc.dest_idx < 4) {
		pr_err_ratelimited("diag: In %s, message is too short, len: %d, dest len: %d\n",
			__func__, len, hdlc.dest_idx);
		mutex_unlock(&driver->diag_hdlc_mutex);
		return;
	}

	if (ret) {
		type = diag_process_apps_pkt(driver->hdlc_buf,
							  hdlc.dest_idx - 3);
		if (type < 0) {
			mutex_unlock(&driver->diag_hdlc_mutex);
			return;
		}
	} else if (driver->debug_flag) {
		pr_err("diag: In %s, partial packet received, dropping packet, len: %d\n",
								__func__, len);
		print_hex_dump(KERN_DEBUG, "Dropped Packet Data: ", 16, 1,
					   DUMP_PREFIX_ADDRESS, data, len, 1);
		driver->debug_flag = 0;
	}
	/* send error responses from APPS for Central Routing */
	if (type == 1 && chk_apps_only()) {
		diag_send_error_rsp(hdlc.dest_idx);
		type = 0;
	}
	/* implies this packet is NOT meant for apps */
	if (!(driver->smd_data[MODEM_DATA].ch) && type == 1) {
		if (chk_apps_only()) {
			diag_send_error_rsp(hdlc.dest_idx);
		} else { /* APQ 8060, Let Q6 respond */
			err = diag_smd_write(&driver->smd_data[LPASS_DATA],
					     driver->hdlc_buf,
					     hdlc.dest_idx - 3);
			if (err) {
				pr_err("diag: In %s, unable to write to smd, peripheral: %d, type: %d, err: %d\n",
				       __func__, LPASS_DATA, SMD_DATA_TYPE,
				       err);
			}
		}
		type = 0;
	}

#ifdef DIAG_DEBUG
	pr_debug("diag: hdlc.dest_idx = %d", hdlc.dest_idx);
	for (i = 0; i < hdlc.dest_idx; i++)
		printk(KERN_DEBUG "\t%x", *(((unsigned char *)
							driver->hdlc_buf)+i));
#endif /* DIAG DEBUG */
	/* ignore 2 bytes for CRC, one for 7E and send */
	if ((driver->smd_data[MODEM_DATA].ch) && (ret) && (type) &&
						(hdlc.dest_idx > 3)) {
		APPEND_DEBUG('g');
		err = diag_smd_write(&driver->smd_data[MODEM_DATA],
				     driver->hdlc_buf, hdlc.dest_idx - 3);
		if (err) {
			pr_err("diag: In %s, unable to write to smd, peripheral: %d, type: %d, err: %d\n",
			       __func__, MODEM_DATA, SMD_DATA_TYPE, err);
		}
		APPEND_DEBUG('h');
#ifdef DIAG_DEBUG
		printk(KERN_INFO "writing data to SMD, pkt length %d\n", len);
		print_hex_dump(KERN_DEBUG, "Written Packet Data to SMD: ", 16,
			       1, DUMP_PREFIX_ADDRESS, data, len, 1);
#endif /* DIAG DEBUG */
	}
	mutex_unlock(&driver->diag_hdlc_mutex);
}

void diag_reset_smd_data(int queue)
{
	int i;
	unsigned long flags;

	for (i = 0; i < NUM_SMD_DATA_CHANNELS; i++) {
		spin_lock_irqsave(&driver->smd_data[i].in_busy_lock, flags);
		driver->smd_data[i].in_busy_1 = 0;
		driver->smd_data[i].in_busy_2 = 0;
		spin_unlock_irqrestore(&driver->smd_data[i].in_busy_lock,
				       flags);
		if (queue)
			/* Poll SMD data channels to check for data */
			queue_work(driver->smd_data[i].wq,
				&(driver->smd_data[i].diag_read_smd_work));
	}

	if (driver->supports_separate_cmdrsp) {
		for (i = 0; i < NUM_SMD_CMD_CHANNELS; i++) {
			spin_lock_irqsave(&driver->smd_cmd[i].in_busy_lock,
					  flags);
			driver->smd_cmd[i].in_busy_1 = 0;
			driver->smd_cmd[i].in_busy_2 = 0;
			spin_unlock_irqrestore(&driver->smd_cmd[i].in_busy_lock,
					       flags);
			if (queue)
				/* Poll SMD data channels to check for data */
				queue_work(driver->diag_wq,
					&(driver->smd_cmd[i].
						diag_read_smd_work));
		}
	}
}

static void diag_smd_reset_buf(struct diag_smd_info *smd_info, int num)
{
	unsigned long flags;
	if (!smd_info)
		return;

	spin_lock_irqsave(&smd_info->in_busy_lock, flags);
	if (num == 1)
		smd_info->in_busy_1 = 0;
	else if (num == 2)
		smd_info->in_busy_2 = 0;
	else
		pr_err_ratelimited("diag: %s invalid buf %d\n", __func__, num);
	spin_unlock_irqrestore(&smd_info->in_busy_lock, flags);

	if (smd_info->type == SMD_DATA_TYPE)
		queue_work(smd_info->wq, &(smd_info->diag_read_smd_work));
	else
		queue_work(driver->diag_wq, &(smd_info->diag_read_smd_work));

}

static int diagfwd_mux_open(int id, int mode)
{
	int i;
	unsigned long flags;

	if (driver->rsp_buf_busy) {
		/*
		 * When a client switches from callback mode to USB mode
		 * explicitly, there can be a situation when the last response
		 * is not drained to the user space application. Reset the
		 * in_busy flag in this case.
		 */
		spin_lock_irqsave(&driver->rsp_buf_busy_lock, flags);
		driver->rsp_buf_busy = 0;
		spin_unlock_irqrestore(&driver->rsp_buf_busy_lock, flags);
	}
	switch (mode) {
	case DIAG_USB_MODE:
		driver->usb_connected = 1;
		break;
	case DIAG_MEMORY_DEVICE_MODE:
		break;
	default:
		return -EINVAL;
	}

	if ((mode == DIAG_USB_MODE &&
			driver->logging_mode == MEMORY_DEVICE_MODE) ||
			(mode == DIAG_MEMORY_DEVICE_MODE &&
				driver->logging_mode == USB_MODE)) {
		/* In this case Diag shouldn't not reset the smd in_busy data.
		 * If the reset of smd in_busy values happens then this will
		 * lead to loss of data read over peripherals.
		*/
	} else {
		diag_reset_smd_data(RESET_AND_QUEUE);
	}
	for (i = 0; i < NUM_SMD_CONTROL_CHANNELS; i++) {
		/* Poll SMD CNTL channels to check for data */
		diag_smd_notify(&(driver->smd_cntl[i]), SMD_EVENT_DATA);
	}
	queue_work(driver->diag_real_time_wq, &driver->diag_real_time_work);
	return 0;
}

static int diagfwd_mux_close(int id, int mode)
{
	int i;
	unsigned long flags;
	struct diag_smd_info *smd_info = NULL;

	switch (mode) {
	case DIAG_USB_MODE:
		driver->usb_connected = 0;
		break;
	case DIAG_MEMORY_DEVICE_MODE:
		break;
	default:
		return -EINVAL;
	}

	if (driver->logging_mode == USB_MODE) {
		for (i = 0; i < NUM_SMD_DATA_CHANNELS; i++) {
			smd_info = &driver->smd_data[i];
			spin_lock_irqsave(&smd_info->in_busy_lock, flags);
			smd_info->in_busy_1 = 1;
			smd_info->in_busy_2 = 1;
			spin_unlock_irqrestore(&smd_info->in_busy_lock, flags);
		}

		if (driver->supports_separate_cmdrsp) {
			for (i = 0; i < NUM_SMD_CMD_CHANNELS; i++) {
				smd_info = &driver->smd_cmd[i];
				spin_lock_irqsave(&smd_info->in_busy_lock,
						flags);
				smd_info->in_busy_1 = 1;
				smd_info->in_busy_2 = 1;
				spin_unlock_irqrestore(&smd_info->in_busy_lock,
						flags);
			}
		}
	}
	queue_work(driver->diag_real_time_wq,
		   &driver->diag_real_time_work);
	return 0;
}

static int diagfwd_mux_read_done(unsigned char *buf, int len, int ctxt)
{
	if (!buf || len <= 0)
		return -EINVAL;

	diag_process_hdlc(buf, len);
	diag_mux_queue_read(ctxt);
	return 0;
}

static int diagfwd_mux_write_done(unsigned char *buf, int len, int buf_ctxt,
				  int ctxt)
{
	struct diag_smd_info *smd_info = NULL;
	unsigned long flags;
	int peripheral = -1;
	int type = -1;
	int num = -1;

	if (!buf || len < 0)
		return -EINVAL;

	peripheral = GET_BUF_PERIPHERAL(buf_ctxt);
	type = GET_BUF_TYPE(buf_ctxt);
	num = GET_BUF_NUM(buf_ctxt);

	switch (type) {
	case SMD_DATA_TYPE:
		if (peripheral >= 0 && peripheral < NUM_SMD_DATA_CHANNELS) {
			smd_info = &driver->smd_data[peripheral];
			diag_smd_reset_buf(smd_info, num);
		} else if (peripheral == APPS_DATA) {
			diagmem_free(driver, (unsigned char *)buf,
				     POOL_TYPE_HDLC);
		} else {
			pr_err_ratelimited("diag: Invalid peripheral %d in %s, type: %d\n",
					   peripheral, __func__, type);
		}
		break;
	case SMD_CMD_TYPE:
		if (peripheral >= 0 && peripheral < NUM_SMD_CMD_CHANNELS &&
		    driver->supports_separate_cmdrsp) {
			smd_info = &driver->smd_cmd[peripheral];
			diag_smd_reset_buf(smd_info, num);
		} else if (peripheral == APPS_DATA) {
			spin_lock_irqsave(&driver->rsp_buf_busy_lock, flags);
			driver->rsp_buf_busy = 0;
			driver->encoded_rsp_len = 0;
			spin_unlock_irqrestore(&driver->rsp_buf_busy_lock,
					       flags);
		} else {
			pr_err_ratelimited("diag: Invalid peripheral %d in %s, type: %d\n",
					   peripheral, __func__, type);
		}
		break;
	default:
		pr_err_ratelimited("diag: Incorrect data type %d, buf_ctxt: %d in %s\n",
				   type, buf_ctxt, __func__);
		break;
	}

	return 0;
}

static struct diag_mux_ops diagfwd_mux_ops = {
	.open = diagfwd_mux_open,
	.close = diagfwd_mux_close,
	.read_done = diagfwd_mux_read_done,
	.write_done = diagfwd_mux_write_done
};

void diag_smd_notify(void *ctxt, unsigned event)
{
	struct diag_smd_info *smd_info = (struct diag_smd_info *)ctxt;
	if (!smd_info)
		return;

	if (event == SMD_EVENT_CLOSE) {
		smd_info->ch = 0;
		wake_up(&driver->smd_wait_q);
		if (smd_info->type == SMD_DATA_TYPE) {
			smd_info->notify_context = event;
			queue_work(driver->diag_cntl_wq,
				 &(smd_info->diag_notify_update_smd_work));
		} else if (smd_info->type == SMD_DCI_TYPE) {
			/* Notify the clients of the close */
			diag_dci_notify_client(smd_info->peripheral_mask,
					       DIAG_STATUS_CLOSED,
					       DCI_LOCAL_PROC);
		} else if (smd_info->type == SMD_CNTL_TYPE) {
			diag_cntl_stm_notify(smd_info,
						CLEAR_PERIPHERAL_STM_STATE);
		}
		return;
	} else if (event == SMD_EVENT_OPEN) {
		if (smd_info->ch_save)
			smd_info->ch = smd_info->ch_save;

		if (smd_info->type == SMD_CNTL_TYPE) {
			smd_info->notify_context = event;
			queue_work(driver->diag_cntl_wq,
				&(smd_info->diag_notify_update_smd_work));
		} else if (smd_info->type == SMD_DCI_TYPE) {
			smd_info->notify_context = event;
			queue_work(driver->diag_dci_wq,
				&(smd_info->diag_notify_update_smd_work));
			/* Notify the clients of the open */
			diag_dci_notify_client(smd_info->peripheral_mask,
					      DIAG_STATUS_OPEN, DCI_LOCAL_PROC);
		}
	} else if (event == SMD_EVENT_DATA) {
		if ((smd_info->type == SMD_DCI_TYPE) ||
		    (smd_info->type == SMD_DCI_CMD_TYPE) ||
		    (smd_info->type == SMD_DATA_TYPE &&
		     driver->logging_mode == MEMORY_DEVICE_MODE)) {
			diag_ws_on_notify();
		}
	}

	wake_up(&driver->smd_wait_q);

	if (smd_info->type == SMD_DCI_TYPE ||
					smd_info->type == SMD_DCI_CMD_TYPE) {
		queue_work(driver->diag_dci_wq,
				&(smd_info->diag_read_smd_work));
	} else if (smd_info->type == SMD_DATA_TYPE) {
		queue_work(smd_info->wq,
				&(smd_info->diag_read_smd_work));
	} else {
		queue_work(driver->diag_wq, &(smd_info->diag_read_smd_work));
	}
}

static int diag_smd_probe(struct platform_device *pdev)
{
	int r = 0;
	int index = -1;
	const char *channel_name = NULL;

	switch (pdev->id) {
	case SMD_APPS_MODEM:
		index = MODEM_DATA;
		channel_name = "DIAG";
		break;
	case SMD_APPS_QDSP:
		index = LPASS_DATA;
		channel_name = "DIAG";
		break;
	case SMD_APPS_WCNSS:
		index = WCNSS_DATA;
		channel_name = "APPS_RIVA_DATA";
		break;
	case SMD_APPS_DSPS:
		index = SENSORS_DATA;
		channel_name = "DIAG";
		break;
	default:
		pr_debug("diag: In %s Received probe for invalid index %d",
			__func__, pdev->id);
		return 0;

	}

	r = smd_named_open_on_edge(channel_name,
				pdev->id,
				&driver->smd_data[index].ch,
				&driver->smd_data[index],
				diag_smd_notify);
	driver->smd_data[index].ch_save = driver->smd_data[index].ch;
	diag_smd_buffer_init(&driver->smd_data[index]);

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pr_debug("diag: In %s, open SMD port, Id = %d, r = %d\n",
		__func__, pdev->id, r);

	return 0;
}

static int diag_smd_cmd_probe(struct platform_device *pdev)
{
	int r = 0;
	int index = -1;
	const char *channel_name = NULL;

	if (!driver->supports_separate_cmdrsp)
		return 0;

	switch (pdev->id) {
	case SMD_APPS_MODEM:
		index = MODEM_DATA;
		break;
	case SMD_APPS_QDSP:
		index = LPASS_DATA;
		break;
	case SMD_APPS_WCNSS:
		index = WCNSS_DATA;
		break;
	case SMD_APPS_DSPS:
		index = SENSORS_DATA;
		break;
	default:
		pr_debug("diag: In %s Received probe for invalid index %d",
			__func__, pdev->id);
		return 0;

	}
	channel_name = "DIAG_CMD";
	r = smd_named_open_on_edge(channel_name,
			pdev->id,
			&driver->smd_cmd[index].ch,
			&driver->smd_cmd[index],
			diag_smd_notify);
	driver->smd_cmd[index].ch_save = driver->smd_cmd[index].ch;
	diag_smd_buffer_init(&driver->smd_cmd[index]);

	pr_debug("diag: In %s, open SMD CMD port, Id = %d, r = %d\n",
		__func__, pdev->id, r);

	return 0;
}

static int diag_smd_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: suspending...\n");
	return 0;
}

static int diag_smd_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: resuming...\n");
	return 0;
}

static const struct dev_pm_ops diag_smd_dev_pm_ops = {
	.runtime_suspend = diag_smd_runtime_suspend,
	.runtime_resume = diag_smd_runtime_resume,
};

static struct platform_driver msm_smd_ch1_driver = {

	.probe = diag_smd_probe,
	.driver = {
		.name = "DIAG",
		.owner = THIS_MODULE,
		.pm   = &diag_smd_dev_pm_ops,
	},
};

static struct platform_driver diag_smd_lite_driver = {

	.probe = diag_smd_probe,
	.driver = {
		.name = "APPS_RIVA_DATA",
		.owner = THIS_MODULE,
		.pm   = &diag_smd_dev_pm_ops,
	},
};

static struct platform_driver smd_lite_data_cmd_drivers = {

	.probe = diag_smd_cmd_probe,
	.driver = {
		.name = "DIAG_CMD",
		.owner = THIS_MODULE,
		.pm   = &diag_smd_dev_pm_ops,
	}
};

int device_supports_separate_cmdrsp(void)
{
	return driver->use_device_tree;
}

void diag_smd_destructor(struct diag_smd_info *smd_info)
{
	if (smd_info->type == SMD_DATA_TYPE)
		destroy_workqueue(smd_info->wq);

	if (smd_info->ch)
		smd_close(smd_info->ch);

	smd_info->ch = 0;
	smd_info->ch_save = 0;
	kfree(smd_info->buf_in_1);
	kfree(smd_info->buf_in_2);
	kfree(smd_info->buf_in_1_raw);
	kfree(smd_info->buf_in_2_raw);
}

void diag_smd_buffer_init(struct diag_smd_info *smd_info)
{
	if (!smd_info) {
		pr_err("diag: Invalid smd_info\n");
		return;
	}

	if (smd_info->inited) {
		pr_debug("diag: smd buffers are already initialized, peripheral: %d, type: %d\n",
			 smd_info->peripheral, smd_info->type);
		return;
	}

	if (smd_info->buf_in_1 == NULL) {
		smd_info->buf_in_1 = kzalloc(IN_BUF_SIZE, GFP_KERNEL);
		if (smd_info->buf_in_1 == NULL)
			goto err;
		smd_info->buf_in_1_size = IN_BUF_SIZE;
		kmemleak_not_leak(smd_info->buf_in_1);
	}

	/* The smd data type needs two buffers */
	if (smd_info->type == SMD_DATA_TYPE) {
		if (smd_info->buf_in_2 == NULL) {
			smd_info->buf_in_2 = kzalloc(IN_BUF_SIZE, GFP_KERNEL);
			if (smd_info->buf_in_2 == NULL)
				goto err;
			smd_info->buf_in_2_size = IN_BUF_SIZE;
			kmemleak_not_leak(smd_info->buf_in_2);
		}

		if (driver->supports_apps_hdlc_encoding) {
			/* In support of hdlc encoding */
			if (smd_info->buf_in_1_raw == NULL) {
				smd_info->buf_in_1_raw = kzalloc(IN_BUF_SIZE,
								GFP_KERNEL);
				if (smd_info->buf_in_1_raw == NULL)
					goto err;
				smd_info->buf_in_1_raw_size = IN_BUF_SIZE;
				kmemleak_not_leak(smd_info->buf_in_1_raw);
			}
			if (smd_info->buf_in_2_raw == NULL) {
				smd_info->buf_in_2_raw = kzalloc(IN_BUF_SIZE,
								GFP_KERNEL);
				if (smd_info->buf_in_2_raw == NULL)
					goto err;
				smd_info->buf_in_2_raw_size = IN_BUF_SIZE;
				kmemleak_not_leak(smd_info->buf_in_2_raw);
			}
		}
	}

	if (smd_info->type == SMD_CMD_TYPE &&
		driver->supports_apps_hdlc_encoding) {
		/* In support of hdlc encoding */
		if (smd_info->buf_in_1_raw == NULL) {
			smd_info->buf_in_1_raw = kzalloc(IN_BUF_SIZE,
								GFP_KERNEL);
			if (smd_info->buf_in_1_raw == NULL)
				goto err;
			smd_info->buf_in_1_raw_size = IN_BUF_SIZE;
			kmemleak_not_leak(smd_info->buf_in_1_raw);
		}
	}
	smd_info->inited = 1;
	return;
err:
	smd_info->inited = 0;
	kfree(smd_info->buf_in_1);
	kfree(smd_info->buf_in_2);
	kfree(smd_info->buf_in_1_raw);
	kfree(smd_info->buf_in_2_raw);
}

int diag_smd_constructor(struct diag_smd_info *smd_info, int peripheral,
			  int type)
{
	if (!smd_info)
		return -EIO;

	smd_info->peripheral = peripheral;
	smd_info->type = type;
	smd_info->encode_hdlc = 0;
	smd_info->inited = 0;
	mutex_init(&smd_info->smd_ch_mutex);
	spin_lock_init(&smd_info->in_busy_lock);

	switch (peripheral) {
	case MODEM_DATA:
		smd_info->peripheral_mask = DIAG_CON_MPSS;
		break;
	case LPASS_DATA:
		smd_info->peripheral_mask = DIAG_CON_LPASS;
		break;
	case WCNSS_DATA:
		smd_info->peripheral_mask = DIAG_CON_WCNSS;
		break;
	case SENSORS_DATA:
		smd_info->peripheral_mask = DIAG_CON_SENSORS;
		break;
	default:
		pr_err("diag: In %s, unknown peripheral, peripheral: %d\n",
			__func__, peripheral);
		goto err;
	}

	smd_info->ch = 0;
	smd_info->ch_save = 0;

	/* The smd data type needs separate work queues for reads */
	if (type == SMD_DATA_TYPE) {
		switch (peripheral) {
		case MODEM_DATA:
			smd_info->wq = create_singlethread_workqueue(
						"diag_modem_data_read_wq");
			break;
		case LPASS_DATA:
			smd_info->wq = create_singlethread_workqueue(
						"diag_lpass_data_read_wq");
			break;
		case WCNSS_DATA:
			smd_info->wq = create_singlethread_workqueue(
						"diag_wcnss_data_read_wq");
			break;
		case SENSORS_DATA:
			smd_info->wq = create_singlethread_workqueue(
						"diag_sensors_data_read_wq");
			break;
		default:
			smd_info->wq = NULL;
			break;
		}
		if (!smd_info->wq)
			goto err;
	} else {
		smd_info->wq = NULL;
	}

	INIT_WORK(&(smd_info->diag_read_smd_work), diag_read_smd_work_fn);

	/*
	 * The update function assigned to the diag_notify_update_smd_work
	 * work_struct is meant to be used for updating that is not to
	 * be done in the context of the smd notify function. The
	 * notify_context variable can be used for passing additional
	 * information to the update function.
	 */
	smd_info->notify_context = 0;
	smd_info->general_context = 0;
	switch (type) {
	case SMD_DATA_TYPE:
	case SMD_CMD_TYPE:
		INIT_WORK(&(smd_info->diag_notify_update_smd_work),
						diag_clean_reg_fn);
		INIT_WORK(&(smd_info->diag_general_smd_work),
						diag_cntl_smd_work_fn);
		break;
	case SMD_CNTL_TYPE:
		INIT_WORK(&(smd_info->diag_notify_update_smd_work),
						diag_mask_update_fn);
		INIT_WORK(&(smd_info->diag_general_smd_work),
						diag_cntl_smd_work_fn);
		break;
	case SMD_DCI_TYPE:
	case SMD_DCI_CMD_TYPE:
		INIT_WORK(&(smd_info->diag_notify_update_smd_work),
					diag_update_smd_dci_work_fn);
		INIT_WORK(&(smd_info->diag_general_smd_work),
					diag_cntl_smd_work_fn);
		break;
	default:
		pr_err("diag: In %s, unknown type, type: %d\n", __func__, type);
		goto err;
	}

	/*
	 * Set function ptr for function to call to process the data that
	 * was just read from the smd channel
	 */
	switch (type) {
	case SMD_DATA_TYPE:
	case SMD_CMD_TYPE:
		smd_info->process_smd_read_data = diag_process_smd_read_data;
		break;
	case SMD_CNTL_TYPE:
		smd_info->process_smd_read_data =
						diag_process_smd_cntl_read_data;
		break;
	case SMD_DCI_TYPE:
	case SMD_DCI_CMD_TYPE:
		smd_info->process_smd_read_data =
						diag_process_smd_dci_read_data;
		break;
	default:
		pr_err("diag: In %s, unknown type, type: %d\n", __func__, type);
		goto err;
	}

	smd_info->buf_in_1_ctxt = SET_BUF_CTXT(peripheral, smd_info->type, 1);
	smd_info->buf_in_2_ctxt = SET_BUF_CTXT(peripheral, smd_info->type, 2);

	return 0;
err:
	if (smd_info->wq)
		destroy_workqueue(smd_info->wq);

	return -ENOMEM;
}

int diag_smd_write(struct diag_smd_info *smd_info, void *buf, int len)
{
	int write_len = 0;
	int retry_count = 0;
	int max_retries = 3;

	if (!smd_info || !buf || len <= 0) {
		pr_err_ratelimited("diag: In %s, invalid params, smd_info: %pK, buf: %pK, len: %d\n",
				   __func__, smd_info, buf, len);
		return -EINVAL;
	}

	if (!smd_info->ch)
		return -ENODEV;

	do {
		mutex_lock(&smd_info->smd_ch_mutex);
		write_len = smd_write(smd_info->ch, buf, len);
		mutex_unlock(&smd_info->smd_ch_mutex);
		if (write_len == len)
			break;
		/*
		 * The channel maybe busy - the FIFO can be full. Retry after
		 * sometime. The value of 10000 was chosen emprically as the
		 * optimal value for the peripherals to read data from the SMD
		 * channel.
		 */
		usleep_range(10000, 10100);
		retry_count++;
	} while (retry_count < max_retries);

	if (write_len != len)
		return -ENOMEM;

	return 0;
}

int diagfwd_init(void)
{
	int ret;
	int i;

	wrap_enabled = 0;
	wrap_count = 0;
	diag_debug_buf_idx = 0;
	driver->use_device_tree = has_device_tree();
	for (i = 0; i < DIAG_NUM_PROC; i++)
		driver->real_time_mode[i] = 1;
	driver->supports_separate_cmdrsp = device_supports_separate_cmdrsp();
	driver->supports_apps_hdlc_encoding = 1;
	mutex_init(&driver->diag_hdlc_mutex);
	mutex_init(&driver->diag_cntl_mutex);
	mutex_init(&driver->mode_lock);
	driver->encoded_rsp_buf = kzalloc(HDLC_OUT_BUF_SIZE, GFP_KERNEL);
	if (!driver->encoded_rsp_buf)
		goto err;
	kmemleak_not_leak(driver->encoded_rsp_buf);
	driver->encoded_rsp_len = 0;
	driver->rsp_buf_busy = 0;
	spin_lock_init(&driver->rsp_buf_busy_lock);
	driver->user_space_data_busy = 0;

	for (i = 0; i < NUM_SMD_CONTROL_CHANNELS; i++) {
		driver->separate_cmdrsp[i] = 0;
		driver->peripheral_supports_stm[i] = DISABLE_STM;
		driver->rcvd_feature_mask[i] = 0;
		driver->peripheral_buffering_support[i] = 0;
		driver->buffering_mode[i].peripheral = i;
		driver->buffering_mode[i].mode = DIAG_BUFFERING_MODE_STREAMING;
		driver->buffering_mode[i].high_wm_val = DEFAULT_HIGH_WM_VAL;
		driver->buffering_mode[i].low_wm_val = DEFAULT_LOW_WM_VAL;
	}

	for (i = 0; i < NUM_STM_PROCESSORS; i++) {
		driver->stm_state_requested[i] = DISABLE_STM;
		driver->stm_state[i] = DISABLE_STM;
	}

	for (i = 0; i < NUM_SMD_DATA_CHANNELS; i++) {
		ret = diag_smd_constructor(&driver->smd_data[i], i,
							SMD_DATA_TYPE);
		if (ret)
			goto err;
	}

	if (driver->supports_separate_cmdrsp) {
		for (i = 0; i < NUM_SMD_CMD_CHANNELS; i++) {
			ret = diag_smd_constructor(&driver->smd_cmd[i], i,
								SMD_CMD_TYPE);
			if (ret)
				goto err;
		}
	}

	if (driver->hdlc_buf == NULL
	    && (driver->hdlc_buf = kzalloc(HDLC_MAX, GFP_KERNEL)) == NULL)
		goto err;
	kmemleak_not_leak(driver->hdlc_buf);
	if (driver->user_space_data_buf == NULL)
		driver->user_space_data_buf = kzalloc(USER_SPACE_DATA,
							GFP_KERNEL);
	if (driver->user_space_data_buf == NULL)
		goto err;
	kmemleak_not_leak(driver->user_space_data_buf);
	if (driver->client_map == NULL &&
	    (driver->client_map = kzalloc
	     ((driver->num_clients) * sizeof(struct diag_client_map),
		   GFP_KERNEL)) == NULL)
		goto err;
	kmemleak_not_leak(driver->client_map);
	if (driver->data_ready == NULL &&
	     (driver->data_ready = kzalloc(driver->num_clients * sizeof(int)
							, GFP_KERNEL)) == NULL)
		goto err;
	kmemleak_not_leak(driver->data_ready);
	if (driver->table == NULL &&
	     (driver->table = kzalloc(diag_max_reg*
		      sizeof(struct diag_master_table),
		       GFP_KERNEL)) == NULL)
		goto err;
	kmemleak_not_leak(driver->table);

	if (driver->pkt_buf == NULL &&
	     (driver->pkt_buf = kzalloc(PKT_SIZE,
			 GFP_KERNEL)) == NULL)
		goto err;
	kmemleak_not_leak(driver->pkt_buf);
	if (driver->dci_pkt_buf == NULL) {
		driver->dci_pkt_buf = kzalloc(PKT_SIZE, GFP_KERNEL);
		if (!driver->dci_pkt_buf)
			goto err;
	}
	kmemleak_not_leak(driver->dci_pkt_buf);
	if (driver->apps_rsp_buf == NULL) {
		driver->apps_rsp_buf = kzalloc(APPS_BUF_SIZE, GFP_KERNEL);
		if (driver->apps_rsp_buf == NULL)
			goto err;
		kmemleak_not_leak(driver->apps_rsp_buf);
	}
	driver->diag_wq = create_singlethread_workqueue("diag_wq");
	if (!driver->diag_wq)
		goto err;
	ret = diag_mux_register(DIAG_LOCAL_PROC, DIAG_LOCAL_PROC,
				&diagfwd_mux_ops);
	if (ret) {
		pr_err("diag: Unable to register with USB, err: %d\n", ret);
		goto err;
	}
	platform_driver_register(&msm_smd_ch1_driver);
	platform_driver_register(&diag_smd_lite_driver);

	if (driver->supports_separate_cmdrsp)
		platform_driver_register(&smd_lite_data_cmd_drivers);

	return 0;
err:
	pr_err("diag: In %s, couldn't initialize diag\n", __func__);

	for (i = 0; i < NUM_SMD_DATA_CHANNELS; i++)
		diag_smd_destructor(&driver->smd_data[i]);

	for (i = 0; i < NUM_SMD_CMD_CHANNELS; i++)
		diag_smd_destructor(&driver->smd_cmd[i]);

	diag_usb_exit(DIAG_USB_LOCAL);
	kfree(driver->encoded_rsp_buf);
	kfree(driver->hdlc_buf);
	kfree(driver->client_map);
	kfree(driver->data_ready);
	kfree(driver->table);
	kfree(driver->pkt_buf);
	kfree(driver->dci_pkt_buf);
	kfree(driver->apps_rsp_buf);
	kfree(driver->user_space_data_buf);
	if (driver->diag_wq)
		destroy_workqueue(driver->diag_wq);
	return -ENOMEM;
}

void diagfwd_exit(void)
{
	int i;

	for (i = 0; i < NUM_SMD_DATA_CHANNELS; i++)
		diag_smd_destructor(&driver->smd_data[i]);

	platform_driver_unregister(&msm_smd_ch1_driver);
	platform_driver_unregister(&diag_smd_lite_driver);

	if (driver->supports_separate_cmdrsp) {
		for (i = 0; i < NUM_SMD_CMD_CHANNELS; i++)
			diag_smd_destructor(&driver->smd_cmd[i]);
		platform_driver_unregister(
			&smd_lite_data_cmd_drivers);
	}

	kfree(driver->encoded_rsp_buf);
	kfree(driver->hdlc_buf);
	kfree(driver->client_map);
	kfree(driver->data_ready);
	kfree(driver->table);
	kfree(driver->pkt_buf);
	kfree(driver->dci_pkt_buf);
	kfree(driver->apps_rsp_buf);
	kfree(driver->user_space_data_buf);
	destroy_workqueue(driver->diag_wq);
}
