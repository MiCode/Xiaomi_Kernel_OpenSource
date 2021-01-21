// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2015-2019, 2021, The Linux Foundation. All rights reserved.
 */
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/ratelimit.h>
#include <linux/workqueue.h>
#include <linux/diagchar.h>
#include <linux/of.h>
#include <linux/kmemleak.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include "diagchar.h"
#include "diagchar_hdlc.h"
#include "diagfwd_peripheral.h"
#include "diagfwd_cntl.h"
#include "diag_masks.h"
#include "diag_dci.h"
#include "diagfwd.h"
#include "diagfwd_socket.h"
#include "diag_mux.h"
#include "diag_ipc_logging.h"
#include "diagfwd_rpmsg.h"
#include "diag_memorydevice.h"

struct data_header {
	uint8_t control_char;
	uint8_t version;
	uint16_t length;
};

static struct diagfwd_info *early_init_info[NUM_TRANSPORT];

static void diagfwd_queue_read(struct diagfwd_info *fwd_info);
static void diagfwd_buffers_exit(struct diagfwd_info *fwd_info);
static void diagfwd_cntl_open(struct diagfwd_info *fwd_info);
static void diagfwd_cntl_close(struct diagfwd_info *fwd_info);
static void diagfwd_dci_open(struct diagfwd_info *fwd_info);
static void diagfwd_dci_close(struct diagfwd_info *fwd_info);
static void diagfwd_data_read_untag_done(struct diagfwd_info *fwd_info,
				   unsigned char *buf, int len);
static void diagfwd_data_read_done(struct diagfwd_info *fwd_info,
				   unsigned char *buf, int len);
static void diagfwd_cntl_read_done(struct diagfwd_info *fwd_info,
				   unsigned char *buf, int len);
static void diagfwd_dci_read_done(struct diagfwd_info *fwd_info,
				  unsigned char *buf, int len);
struct diagfwd_info peripheral_info[NUM_TYPES][NUM_PERIPHERALS];

static struct diag_channel_ops data_ch_ops = {
	.open = NULL,
	.close = NULL,
	.read_done = diagfwd_data_read_untag_done
};

static struct diag_channel_ops cntl_ch_ops = {
	.open = diagfwd_cntl_open,
	.close = diagfwd_cntl_close,
	.read_done = diagfwd_cntl_read_done
};

static struct diag_channel_ops dci_ch_ops = {
	.open = diagfwd_dci_open,
	.close = diagfwd_dci_close,
	.read_done = diagfwd_dci_read_done
};

static void diagfwd_cntl_open(struct diagfwd_info *fwd_info)
{
	if (!fwd_info)
		return;
	diag_cntl_channel_open(fwd_info);
}

static void diagfwd_cntl_close(struct diagfwd_info *fwd_info)
{
	if (!fwd_info)
		return;
	diag_cntl_channel_close(fwd_info);
}

static void diagfwd_dci_open(struct diagfwd_info *fwd_info)
{
	if (!fwd_info)
		return;

	diag_dci_notify_client(PERIPHERAL_MASK(fwd_info->peripheral),
			       DIAG_STATUS_OPEN, DCI_LOCAL_PROC);
}

static void diagfwd_dci_close(struct diagfwd_info *fwd_info)
{
	if (!fwd_info)
		return;

	diag_dci_notify_client(PERIPHERAL_MASK(fwd_info->peripheral),
			       DIAG_STATUS_CLOSED, DCI_LOCAL_PROC);
}

static int diag_add_hdlc_encoding(unsigned char *dest_buf, int *dest_len,
				  unsigned char *buf, int len)
{
	struct diag_send_desc_type send = { NULL, NULL, DIAG_STATE_START, 0 };
	struct diag_hdlc_dest_type enc = { NULL, NULL, 0 };
	struct data_header *header;
	int header_size = sizeof(struct data_header);
	uint8_t *end_control_char = NULL;
	uint8_t *payload = NULL;
	uint8_t *temp_buf = NULL;
	uint8_t *temp_encode_buf = NULL;
	int src_pkt_len;
	int encoded_pkt_length;
	int max_size;
	int total_processed = 0;
	int bytes_remaining;
	int err = 0;
	uint8_t loop_count = 0;

	if (!dest_buf || !dest_len || !buf)
		return -EIO;

	temp_buf = buf;
	temp_encode_buf = dest_buf;
	bytes_remaining = *dest_len;

	while (total_processed < len) {
		loop_count++;
		header = (struct data_header *)temp_buf;
		/* Perform initial error checking */
		if (header->control_char != CONTROL_CHAR ||
		    header->version != 1) {
			err = -EINVAL;
			break;
		}

		if (header->length >= bytes_remaining)
			break;

		payload = temp_buf + header_size;
		end_control_char = payload + header->length;
		if (*end_control_char != CONTROL_CHAR) {
			err = -EINVAL;
			break;
		}

		max_size = 2 * header->length + 3;
		if (bytes_remaining < max_size) {
			err = -EINVAL;
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

	*dest_len = (int)(temp_encode_buf - dest_buf);

	return err;
}

static int check_bufsize_for_encoding(struct diagfwd_buf_t *buf, uint32_t len)
{
	int i, ctx = 0;
	uint32_t max_size = 0;
	unsigned long flags;
	unsigned char *temp_buf = NULL;
	struct diag_md_info *ch = NULL;

	if (!buf || len == 0)
		return -EINVAL;

	max_size = (2 * len) + 3;
	if (max_size > PERIPHERAL_BUF_SZ) {
		if (max_size > MAX_PERIPHERAL_HDLC_BUF_SZ) {
			pr_err("diag: In %s, max_size is going beyond limit %d\n",
			       __func__, max_size);
			max_size = MAX_PERIPHERAL_HDLC_BUF_SZ;
		}

		mutex_lock(&driver->md_session_lock);
		if (buf->len < max_size) {
			if (driver->logging_mode[DIAG_LOCAL_PROC] ==
				DIAG_MEMORY_DEVICE_MODE ||
				driver->logging_mode[DIAG_LOCAL_PROC] ==
							DIAG_MULTI_MODE) {
				ch = &diag_md[DIAG_LOCAL_PROC];
				if (!ch || !ch->md_info_inited) {
					mutex_unlock(&driver->md_session_lock);
					return -EINVAL;
				}
				spin_lock_irqsave(&ch->lock, flags);
				for (i = 0; i < ch->num_tbl_entries; i++) {
					if (ch->tbl[i].buf == buf->data) {
						ctx = ch->tbl[i].ctx;
						ch->tbl[i].buf = NULL;
						ch->tbl[i].len = 0;
						ch->tbl[i].ctx = 0;
						DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
						"Flushed mdlog table entries before reallocating data buffer, p:%d, t:%d\n",
						GET_BUF_PERIPHERAL(ctx),
						GET_BUF_TYPE(ctx));
						break;
					}
				}
				spin_unlock_irqrestore(&ch->lock, flags);
			}
			temp_buf = krealloc(buf->data, max_size +
						APF_DIAG_PADDING,
					    GFP_KERNEL);
			if (!temp_buf) {
				mutex_unlock(&driver->md_session_lock);
				return -ENOMEM;
			}
			DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"Reallocated data buffer: %pK with size: %d\n",
			temp_buf, max_size);
			buf->data = temp_buf;
			buf->len = max_size;
		}
		mutex_unlock(&driver->md_session_lock);
	}

	return buf->len;
}

/*
 * diag_md_get_peripheral(int ctxt)
 *
 * Context(ctxt) contains peripheral, channel type, buffer num and diag_id
 * The function decodes the ctxt, checks for the active user pd session
 * using diag_id and returns peripheral if not active or the PD if active.
 *
 */
int diag_md_get_peripheral(int ctxt)
{
	uint8_t diag_id = 0, i = 0, pd = 0;
	int type = 0, peripheral = -EINVAL;
	int index = 0;
	struct diagfwd_info *fwd_info = NULL;

	peripheral = GET_BUF_PERIPHERAL(ctxt);

	/* Check for peripheral value within bounds
	 * of peripherals and UPD combined.
	 */
	if (peripheral < 0 || peripheral > NUM_MD_SESSIONS)
		return -EINVAL;

	if (peripheral == APPS_DATA)
		return peripheral;

	/* With peripheral value bound checked
	 * return user pd value.
	 */
	if (peripheral > NUM_PERIPHERALS)
		return peripheral;

	type = GET_BUF_TYPE(ctxt);
	if (type < 0 || type >= NUM_TYPES)
		return -EINVAL;

	fwd_info = &peripheral_info[type][peripheral];
	if (!fwd_info)
		return -EINVAL;

	diag_id = GET_PD_CTXT(ctxt);

	if (driver->num_pd_session &&
		driver->feature[peripheral].untag_header) {
		if (diag_id == fwd_info->root_diag_id.diagid_val) {
			if (peripheral != fwd_info->root_diag_id.pd)
				peripheral = -EINVAL;
		} else {
			for (i = 0; i <= (fwd_info->num_pd - 2); i++) {
				if (diag_id ==
					fwd_info->upd_diag_id[i].diagid_val) {
					pd = fwd_info->upd_diag_id[i].pd;
					index = pd - UPD_WLAN;
					if ((index >= 0 && index < NUM_UPD) &&
					driver->pd_logging_mode[index]) {
						peripheral = pd;
						break;
					}
				}
			}
		}
	}
	return peripheral;
}

static void diagfwd_data_process_done(struct diagfwd_info *fwd_info,
				   struct diagfwd_buf_t *buf, int len)
{
	int err = 0;
	int write_len = 0, peripheral = 0;
	unsigned char *write_buf = NULL;
	uint8_t hdlc_disabled = 0;

	if (!fwd_info || !buf || len <= 0) {
		diag_ws_release();
		return;
	}

	switch (fwd_info->type) {
	case TYPE_DATA:
	case TYPE_CMD:
		break;
	default:
		pr_err_ratelimited("diag: In %s, invalid type %d for peripheral %d\n",
			__func__, fwd_info->type,
			fwd_info->peripheral);
		diag_ws_release();
		return;
	}

	mutex_lock(&driver->hdlc_disable_mutex);
	mutex_lock(&fwd_info->data_mutex);

	peripheral =
		diag_md_get_peripheral(buf->ctxt);
	if (peripheral < 0) {
		pr_err("diag:%s:%d invalid peripheral = %d\n",
			__func__, __LINE__, peripheral);
		mutex_unlock(&fwd_info->data_mutex);
		mutex_unlock(&driver->hdlc_disable_mutex);
		diag_ws_release();
		return;
	}

	hdlc_disabled = driver->p_hdlc_disabled[peripheral];

	if (hdlc_disabled) {
		/* The data is raw and and on APPS side HDLC is disabled */
		if (!buf) {
			pr_err("diag: In %s, no match for non encode buffer %pK, peripheral %d, type: %d\n",
			       __func__, buf, fwd_info->peripheral,
			       fwd_info->type);
			goto end;
		}
		if (len > PERIPHERAL_BUF_SZ) {
			pr_err("diag: In %s, Incoming buffer too large %d, peripheral %d, type: %d\n",
			       __func__, len, fwd_info->peripheral,
			       fwd_info->type);
			goto end;
		}
		write_len = len;
		if (write_len <= 0)
			goto end;
		write_buf = buf->data_raw;
	} else {
		if (!buf) {
			pr_err("diag: In %s, no match for non encode buffer %pK, peripheral %d, type: %d\n",
				__func__, buf, fwd_info->peripheral,
				fwd_info->type);
			goto end;
		}

		write_len = check_bufsize_for_encoding(buf, len);
		if (write_len <= 0) {
			pr_err("diag: error in checking buf for encoding\n");
			goto end;
		}
		write_buf = buf->data;
		err = diag_add_hdlc_encoding(write_buf, &write_len,
			buf->data_raw, len);
		if (err) {
			pr_err("diag: error in adding hdlc encoding\n");
			goto end;
		}
	}
	mutex_unlock(&fwd_info->data_mutex);
	mutex_unlock(&driver->hdlc_disable_mutex);

	if (write_len > 0) {
		err = diag_mux_write(DIAG_LOCAL_PROC, write_buf, write_len,
				     buf->ctxt);
		if (err) {
			pr_err_ratelimited("diag: In %s, unable to write to mux error: %d\n",
					   __func__, err);
			goto end_write;
		}
	}

	diagfwd_queue_read(fwd_info);
	return;

end:
	mutex_unlock(&fwd_info->data_mutex);
	mutex_unlock(&driver->hdlc_disable_mutex);
end_write:
	diag_ws_release();
	if (buf) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		"Marking buffer as free p: %d, t: %d, buf_num: %d\n",
			fwd_info->peripheral, fwd_info->type,
			GET_BUF_NUM(buf->ctxt));
		diagfwd_write_done(fwd_info->peripheral, fwd_info->type,
				   GET_BUF_NUM(buf->ctxt));
	}
	diagfwd_queue_read(fwd_info);
}

/*
 * diagfwd_data_read_untag_done(struct diagfwd_info *fwd_info,
 *			   unsigned char *buf, int len)
 *
 * Data received from the peripheral can contain data from core and user PD
 * The function segregates the data depending on the diag_id in the header
 * of the packet chunk and copies to PD specific buffers.
 * Sets the context for the buffers using diag_id and process it later for
 * splitting the stream based on active PD logging.
 *
 */

static void diagfwd_data_read_untag_done(struct diagfwd_info *fwd_info,
				   unsigned char *buf, int len)
{
	int i = 0;
	int len_cpd = 0;
	int ctxt_cpd = 0;
	int len_upd[MAX_PERIPHERAL_UPD] = {0};
	int ctxt_upd[MAX_PERIPHERAL_UPD] = {0};
	int packet_len = 0, processed = 0;
	unsigned char *temp_buf_main = NULL;
	unsigned char *temp_buf_cpd = NULL;
	unsigned char *temp_buf_upd[MAX_PERIPHERAL_UPD] = {NULL};
	struct diagfwd_buf_t *temp_fwdinfo_cpd = NULL;
	struct diagfwd_buf_t *temp_fwdinfo_upd = NULL;
	int flag_buf_1 = 0, flag_buf_2 = 0;
	uint8_t peripheral, temp_diagid_val;
	unsigned char *buf_offset = NULL;

	if (!fwd_info || !buf || len <= 0) {
		diag_ws_release();
		return;
	}

	switch (fwd_info->type) {
	case TYPE_DATA:
	case TYPE_CMD:
		break;
	default:
		pr_err_ratelimited("diag: In %s, invalid type %d for peripheral %d\n",
				   __func__, fwd_info->type,
				   fwd_info->peripheral);
		diag_ws_release();
		return;
	}
	peripheral = fwd_info->peripheral;
	if (peripheral >= NUM_PERIPHERALS)
		return;

	if (driver->feature[peripheral].encode_hdlc &&
		driver->feature[peripheral].untag_header &&
		driver->peripheral_untag[peripheral]) {
		temp_buf_cpd = buf;
		temp_buf_main = buf;
		if (fwd_info->buf_1 &&
			fwd_info->buf_1->data_raw == buf) {
			flag_buf_1 = 1;
			temp_fwdinfo_cpd = fwd_info->buf_1;
			if (fwd_info->type == TYPE_DATA) {
				for (i = 0; (i <= (fwd_info->num_pd - 2)) &&
					fwd_info->buf_upd[i][0]; i++)
					temp_buf_upd[i] =
					fwd_info->buf_upd[i][0]->data_raw;
			}
		} else if (fwd_info->buf_2 &&
					fwd_info->buf_2->data_raw == buf) {
			flag_buf_2 = 1;
			temp_fwdinfo_cpd = fwd_info->buf_2;
			if (fwd_info->type == TYPE_DATA) {
				for (i = 0; (i <= (fwd_info->num_pd - 2)) &&
					fwd_info->buf_upd[i][1]; i++)
					temp_buf_upd[i] =
					fwd_info->buf_upd[i][1]->data_raw;
			}
		} else {
			pr_err("diag: In %s, no match for buffer %pK, peripheral %d, type: %d\n",
			       __func__, buf, peripheral,
			       fwd_info->type);
			goto end;
		}

		while (processed < len) {
			/* Debug log to check diag_id header validity*/
			pr_debug("diag_fr:untagged packet buf contents: %02x %02x %02x %02x\n",
			 *temp_buf_main, *(temp_buf_main+1),
			 *(temp_buf_main+2), *(temp_buf_main+3));

			/* Debug log ONLY for CMD channel*/
			if (fwd_info->type == TYPE_CMD) {
				/* buf_offset taking into account
				 * diag_id header and non-hdlc header
				 */
				buf_offset = temp_buf_main + 8;

				DIAG_LOG(DIAG_DEBUG_CMD_INFO,
				"diag: cmd rsp (%02x %02x %02x %02x) received from peripheral: %d\n",
				*(buf_offset), *(buf_offset+1),
				*(buf_offset+2), *(buf_offset+3), peripheral);
			}

			packet_len =
				*(uint16_t *) (temp_buf_main + 2);
			if (packet_len > PERIPHERAL_BUF_SZ)
				goto end;
			if ((*temp_buf_main) ==
				fwd_info->root_diag_id.diagid_val) {
				ctxt_cpd =
					fwd_info->root_diag_id.diagid_val;
				len_cpd += packet_len;
				if (temp_buf_cpd) {
					memcpy(temp_buf_cpd,
					(temp_buf_main + 4), packet_len);
					temp_buf_cpd += packet_len;
				}
			} else {
				for (i = 0; i <= (fwd_info->num_pd - 2); i++) {
					temp_diagid_val =
					fwd_info->upd_diag_id[i].diagid_val;
					if (((*temp_buf_main) ==
						temp_diagid_val)
						&& temp_buf_upd[i]) {
						ctxt_upd[i] = temp_diagid_val;
						memcpy(temp_buf_upd[i],
						(temp_buf_main + 4),
						packet_len);
						temp_buf_upd[i] += packet_len;
						len_upd[i] += packet_len;
					}
				}
			}
			len = len - 4;
			temp_buf_main += (packet_len + 4);
			processed += packet_len;
		}

		if (flag_buf_1) {
			fwd_info->cpd_len_1 = len_cpd;
			for (i = 0; i <= (fwd_info->num_pd - 2); i++)
				if (fwd_info->type == TYPE_DATA)
					fwd_info->upd_len[i][0] = len_upd[i];
		} else if (flag_buf_2) {
			fwd_info->cpd_len_2 = len_cpd;
			for (i = 0; i <= (fwd_info->num_pd - 2); i++)
				if (fwd_info->type == TYPE_DATA)
					fwd_info->upd_len[i][1] = len_upd[i];
		}

		for (i = 0; i <= (fwd_info->num_pd - 2); i++) {
			if (fwd_info->type == TYPE_DATA && len_upd[i]) {
				if (flag_buf_1)
					temp_fwdinfo_upd =
						fwd_info->buf_upd[i][0];
				else
					temp_fwdinfo_upd =
						fwd_info->buf_upd[i][1];
				if (!temp_fwdinfo_upd)
					break;
				temp_fwdinfo_upd->ctxt &= 0x00FFFFFF;
				temp_fwdinfo_upd->ctxt |=
					(SET_PD_CTXT(ctxt_upd[i]));
				atomic_set(&temp_fwdinfo_upd->in_busy, 1);
				diagfwd_data_process_done(fwd_info,
					temp_fwdinfo_upd, len_upd[i]);
			} else {
				if (flag_buf_1)
					fwd_info->upd_len[i][0] = 0;
				if (flag_buf_2)
					fwd_info->upd_len[i][1] = 0;
			}
		}

		if (len_cpd) {
			temp_fwdinfo_cpd->ctxt &= 0x00FFFFFF;
			temp_fwdinfo_cpd->ctxt |= (SET_PD_CTXT(ctxt_cpd));
			diagfwd_data_process_done(fwd_info,
				temp_fwdinfo_cpd, len_cpd);
		} else {
			if (flag_buf_1)
				fwd_info->cpd_len_1 = 0;
			if (flag_buf_2)
				fwd_info->cpd_len_2 = 0;
		}
	} else {
		diagfwd_data_read_done(fwd_info, buf, len);
	}
	return;
end:
	diag_ws_release();
	if (temp_fwdinfo_cpd) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		"Marking buffer as free p: %d, t: %d, buf_num: %d\n",
			fwd_info->peripheral, fwd_info->type,
			GET_BUF_NUM(temp_fwdinfo_cpd->ctxt));
		diagfwd_write_done(fwd_info->peripheral, fwd_info->type,
				   GET_BUF_NUM(temp_fwdinfo_cpd->ctxt));
	}
	diagfwd_queue_read(fwd_info);
}

static void diagfwd_data_read_done(struct diagfwd_info *fwd_info,
				   unsigned char *buf, int len)
{
	int err = 0;
	int write_len = 0;
	unsigned char *write_buf = NULL, *buf_offset = NULL;
	struct diagfwd_buf_t *temp_buf = NULL;
	uint8_t hdlc_disabled = 0;

	if (!fwd_info || !buf || len <= 0) {
		diag_ws_release();
		return;
	}

	switch (fwd_info->type) {
	case TYPE_DATA:
	case TYPE_CMD:
		break;
	default:
		pr_err_ratelimited("diag: In %s, invalid type %d for peripheral %d\n",
				   __func__, fwd_info->type,
				   fwd_info->peripheral);
		diag_ws_release();
		return;
	}

	mutex_lock(&driver->hdlc_disable_mutex);
	mutex_lock(&fwd_info->data_mutex);

	hdlc_disabled = driver->p_hdlc_disabled[fwd_info->peripheral];

	if (fwd_info->type == TYPE_CMD) {
		/*buf_offset taking into account non-hdlc header */
		buf_offset = buf + 4;

		DIAG_LOG(DIAG_DEBUG_CMD_INFO,
		"diag: cmd rsp(%02x %02x %02x %02x) received from peripheral: %d\n",
		*(buf_offset), *(buf_offset+1), *(buf_offset+2),
		*(buf_offset+3), fwd_info->peripheral);
	}

	if (!driver->feature[fwd_info->peripheral].encode_hdlc) {
		if (fwd_info->buf_1 && fwd_info->buf_1->data == buf) {
			temp_buf = fwd_info->buf_1;
			write_buf = fwd_info->buf_1->data;
		} else if (fwd_info->buf_2 && fwd_info->buf_2->data == buf) {
			temp_buf = fwd_info->buf_2;
			write_buf = fwd_info->buf_2->data;
		} else {
			pr_err("diag: In %s, no match for buffer %pK, peripheral %d, type: %d\n",
			       __func__, buf, fwd_info->peripheral,
			       fwd_info->type);
			goto end;
		}
		write_len = len;
	} else if (hdlc_disabled) {
		/* The data is raw and and on APPS side HDLC is disabled */
		if (fwd_info->buf_1 && fwd_info->buf_1->data_raw == buf) {
			temp_buf = fwd_info->buf_1;
		} else if (fwd_info->buf_2 &&
			   fwd_info->buf_2->data_raw == buf) {
			temp_buf = fwd_info->buf_2;
		} else {
			pr_err("diag: In %s, no match for non encode buffer %pK, peripheral %d, type: %d\n",
			       __func__, buf, fwd_info->peripheral,
			       fwd_info->type);
			goto end;
		}
		if (len > PERIPHERAL_BUF_SZ) {
			pr_err("diag: In %s, Incoming buffer too large %d, peripheral %d, type: %d\n",
			       __func__, len, fwd_info->peripheral,
			       fwd_info->type);
			goto end;
		}
		write_len = len;
		write_buf = buf;
	} else {
		if (fwd_info->buf_1 && fwd_info->buf_1->data_raw == buf) {
			temp_buf = fwd_info->buf_1;
		} else if (fwd_info->buf_2 &&
			   fwd_info->buf_2->data_raw == buf) {
			temp_buf = fwd_info->buf_2;
		} else {
			pr_err("diag: In %s, no match for non encode buffer %pK, peripheral %d, type: %d\n",
				__func__, buf, fwd_info->peripheral,
				fwd_info->type);
			goto end;
		}
		write_len = check_bufsize_for_encoding(temp_buf, len);
		if (write_len <= 0) {
			pr_err("diag: error in checking buf for encoding\n");
			goto end;
		}
		write_buf = temp_buf->data;
		err = diag_add_hdlc_encoding(write_buf, &write_len, buf, len);
		if (err) {
			pr_err("diag: error in adding hdlc encoding\n");
			goto end;
		}
	}

	mutex_unlock(&fwd_info->data_mutex);
	mutex_unlock(&driver->hdlc_disable_mutex);

	if (write_len > 0) {
		err = diag_mux_write(DIAG_LOCAL_PROC, write_buf, write_len,
				     temp_buf->ctxt);
		if (err) {
			pr_err_ratelimited("diag: In %s, unable to write to mux error: %d\n",
					   __func__, err);
			goto end_write;
		}
	}
	diagfwd_queue_read(fwd_info);
	return;

end:
	mutex_unlock(&fwd_info->data_mutex);
	mutex_unlock(&driver->hdlc_disable_mutex);
end_write:
	diag_ws_release();
	if (temp_buf) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		"Marking buffer as free p: %d, t: %d, buf_num: %d\n",
			fwd_info->peripheral, fwd_info->type,
			GET_BUF_NUM(temp_buf->ctxt));
		diagfwd_write_done(fwd_info->peripheral, fwd_info->type,
				   GET_BUF_NUM(temp_buf->ctxt));
	}
	diagfwd_queue_read(fwd_info);
}

static void diagfwd_cntl_read_done(struct diagfwd_info *fwd_info,
				   unsigned char *buf, int len)
{
	if (!fwd_info) {
		diag_ws_release();
		return;
	}

	if (fwd_info->type != TYPE_CNTL) {
		pr_err("diag: In %s, invalid type %d for peripheral %d\n",
		       __func__, fwd_info->type, fwd_info->peripheral);
		diag_ws_release();
		return;
	}

	diag_ws_on_read(DIAG_WS_MUX, len);
	diag_cntl_process_read_data(fwd_info, buf, len);
	/*
	 * Control packets are not consumed by the clients. Mimic
	 * consumption by setting and clearing the wakeup source copy_count
	 * explicitly.
	 */
	diag_ws_on_copy_fail(DIAG_WS_MUX);
	/* Reset the buffer in_busy value after processing the data */
	if (fwd_info->buf_1) {
		atomic_set(&fwd_info->buf_1->in_busy, 0);
		fwd_info->buffer_status[BUF_1_INDEX] = 0;
	}

	diagfwd_queue_read(fwd_info);
	diagfwd_queue_read(&peripheral_info[TYPE_DATA][fwd_info->peripheral]);
	diagfwd_queue_read(&peripheral_info[TYPE_CMD][fwd_info->peripheral]);
}

static void diagfwd_dci_read_done(struct diagfwd_info *fwd_info,
				  unsigned char *buf, int len)
{
	if (!fwd_info)
		return;

	switch (fwd_info->type) {
	case TYPE_DCI:
	case TYPE_DCI_CMD:
		break;
	default:
		pr_err("diag: In %s, invalid type %d for peripheral %d\n",
		       __func__, fwd_info->type, fwd_info->peripheral);
		return;
	}

	diag_dci_process_peripheral_data(fwd_info, (void *)buf, len);
	/* Reset the buffer in_busy value after processing the data */
	if (fwd_info->buf_1) {
		atomic_set(&fwd_info->buf_1->in_busy, 0);
		fwd_info->buffer_status[BUF_1_INDEX] = 0;
	}

	diagfwd_queue_read(fwd_info);
}

static void diagfwd_reset_buffers(struct diagfwd_info *fwd_info,
				  unsigned char *buf)
{
	if (!fwd_info || !buf)
		return;

	if (!driver->feature[fwd_info->peripheral].encode_hdlc) {
		if (fwd_info->buf_1 && fwd_info->buf_1->data == buf) {
			atomic_set(&fwd_info->buf_1->in_busy, 0);
			fwd_info->buffer_status[BUF_1_INDEX] = 0;
		} else if (fwd_info->buf_2 && fwd_info->buf_2->data == buf) {
			atomic_set(&fwd_info->buf_2->in_busy, 0);
			fwd_info->buffer_status[BUF_2_INDEX] = 0;
		}
	} else {
		if (fwd_info->buf_1 && fwd_info->buf_1->data_raw == buf) {
			atomic_set(&fwd_info->buf_1->in_busy, 0);
			fwd_info->buffer_status[BUF_1_INDEX] = 0;
		} else if (fwd_info->buf_2 &&
			fwd_info->buf_2->data_raw == buf) {
			atomic_set(&fwd_info->buf_2->in_busy, 0);
			fwd_info->buffer_status[BUF_2_INDEX] = 0;
		}
	}
	if (fwd_info->buf_1 && !atomic_read(&(fwd_info->buf_1->in_busy))) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		"Buffer 1 for core PD is marked free, p: %d, t: %d\n",
			fwd_info->peripheral, fwd_info->type);
	}
	if (fwd_info->buf_2 && !atomic_read(&(fwd_info->buf_2->in_busy))) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		"Buffer 2 for core PD is marked free, p: %d, t: %d\n",
			fwd_info->peripheral, fwd_info->type);
	}
}

int diagfwd_peripheral_init(void)
{
	uint8_t peripheral;
	uint8_t transport;
	uint8_t type;
	int i = 0;
	struct diagfwd_info *fwd_info = NULL;

	for (transport = 0; transport < NUM_TRANSPORT; transport++) {
		early_init_info[transport] = kzalloc(
				sizeof(struct diagfwd_info) * NUM_PERIPHERALS,
				GFP_KERNEL);
		if (!early_init_info[transport])
			return -ENOMEM;
		kmemleak_not_leak(early_init_info[transport]);
	}

	for (peripheral = 0; peripheral < NUM_PERIPHERALS; peripheral++) {
		for (transport = 0; transport < NUM_TRANSPORT; transport++) {
			fwd_info = &early_init_info[transport][peripheral];
			fwd_info->peripheral = peripheral;
			fwd_info->type = TYPE_CNTL;
			fwd_info->transport = transport;
			fwd_info->ctxt = NULL;
			fwd_info->p_ops = NULL;
			fwd_info->ch_open = 0;
			fwd_info->inited = 1;
			fwd_info->read_bytes = 0;
			fwd_info->write_bytes = 0;
			fwd_info->cpd_len_1 = 0;
			fwd_info->cpd_len_2 = 0;
			fwd_info->num_pd = 0;
			fwd_info->root_diag_id.diagid_val = 0;
			mutex_init(&fwd_info->buf_mutex);
			mutex_init(&fwd_info->data_mutex);

			for (i = 0; i < MAX_PERIPHERAL_UPD; i++) {
				fwd_info->upd_diag_id[i].diagid_val = 0;
				fwd_info->upd_len[i][0] = 0;
				fwd_info->upd_len[i][1] = 0;
				fwd_info->buf_upd[i][0] = NULL;
				fwd_info->buf_upd[i][1] = NULL;
			}
		}
	}

	for (peripheral = 0; peripheral < NUM_PERIPHERALS; peripheral++) {
		for (type = 0; type < NUM_TYPES; type++) {
			fwd_info = &peripheral_info[type][peripheral];
			fwd_info->peripheral = peripheral;
			fwd_info->type = type;
			fwd_info->ctxt = NULL;
			fwd_info->p_ops = NULL;
			fwd_info->ch_open = 0;
			fwd_info->read_bytes = 0;
			fwd_info->write_bytes = 0;
			fwd_info->num_pd = 0;
			fwd_info->cpd_len_1 = 0;
			fwd_info->cpd_len_2 = 0;
			fwd_info->root_diag_id.diagid_val = 0;
			mutex_init(&fwd_info->buf_mutex);
			mutex_init(&fwd_info->data_mutex);

			for (i = 0; i < MAX_PERIPHERAL_UPD; i++) {
				fwd_info->upd_diag_id[i].diagid_val = 0;
				fwd_info->upd_len[i][0] = 0;
				fwd_info->upd_len[i][1] = 0;
				fwd_info->buf_upd[i][0] = NULL;
				fwd_info->buf_upd[i][1] = NULL;
			}
			/*
			 * This state shouldn't be set for Control channels
			 * during initialization. This is set when the feature
			 * mask is received for the first time.
			 */
			if (type != TYPE_CNTL)
				fwd_info->inited = 1;
		}
		driver->diagfwd_data[peripheral] =
			&peripheral_info[TYPE_DATA][peripheral];
		driver->diagfwd_cntl[peripheral] =
			&peripheral_info[TYPE_CNTL][peripheral];
		driver->diagfwd_dci[peripheral] =
			&peripheral_info[TYPE_DCI][peripheral];
		driver->diagfwd_cmd[peripheral] =
			&peripheral_info[TYPE_CMD][peripheral];
		driver->diagfwd_dci_cmd[peripheral] =
			&peripheral_info[TYPE_DCI_CMD][peripheral];
	}

	if (driver->supports_sockets)
		diag_socket_init();
	diag_rpmsg_init();

	return 0;
}

void diagfwd_peripheral_exit(void)
{
	uint8_t peripheral;
	uint8_t type;
	struct diagfwd_info *fwd_info = NULL;
	int transport = 0;

	diag_socket_exit();

	for (peripheral = 0; peripheral < NUM_PERIPHERALS; peripheral++) {
		for (type = 0; type < NUM_TYPES; type++) {
			fwd_info = &peripheral_info[type][peripheral];
			fwd_info->ctxt = NULL;
			fwd_info->p_ops = NULL;
			fwd_info->ch_open = 0;
			diagfwd_buffers_exit(fwd_info);
		}
	}

	for (peripheral = 0; peripheral < NUM_PERIPHERALS; peripheral++) {
		driver->diagfwd_data[peripheral] = NULL;
		driver->diagfwd_cntl[peripheral] = NULL;
		driver->diagfwd_dci[peripheral] = NULL;
		driver->diagfwd_cmd[peripheral] = NULL;
		driver->diagfwd_dci_cmd[peripheral] = NULL;
	}

	for (transport = 0; transport < NUM_TRANSPORT; transport++) {
		kfree(early_init_info[transport]);
		early_init_info[transport] = NULL;
	}
}

int diagfwd_cntl_register(uint8_t transport, uint8_t peripheral, void *ctxt,
			  struct diag_peripheral_ops *ops,
			  struct diagfwd_info **fwd_ctxt)
{
	struct diagfwd_info *fwd_info = NULL;

	if (!ctxt || !ops)
		return -EIO;

	if (transport >= NUM_TRANSPORT || peripheral >= NUM_PERIPHERALS)
		return -EINVAL;

	fwd_info = &early_init_info[transport][peripheral];
	*fwd_ctxt = &early_init_info[transport][peripheral];
	fwd_info->ctxt = ctxt;
	fwd_info->p_ops = ops;
	fwd_info->c_ops = &cntl_ch_ops;

	return 0;
}

int diagfwd_register(uint8_t transport, uint8_t peripheral, uint8_t type,
		     void *ctxt, struct diag_peripheral_ops *ops,
		     struct diagfwd_info **fwd_ctxt)
{
	struct diagfwd_info *fwd_info = NULL;

	if (peripheral >= NUM_PERIPHERALS || type >= NUM_TYPES ||
	    !ctxt || !ops || transport >= NUM_TRANSPORT) {
		pr_err("diag: In %s, returning error\n", __func__);
		return -EIO;
	}

	fwd_info = &peripheral_info[type][peripheral];
	*fwd_ctxt = &peripheral_info[type][peripheral];
	fwd_info->ctxt = ctxt;
	fwd_info->p_ops = ops;
	fwd_info->transport = transport;
	fwd_info->ch_open = 0;

	switch (type) {
	case TYPE_DATA:
	case TYPE_CMD:
		fwd_info->c_ops = &data_ch_ops;
		break;
	case TYPE_DCI:
	case TYPE_DCI_CMD:
		fwd_info->c_ops = &dci_ch_ops;
		break;
	default:
		pr_err("diag: In %s, invalid type: %d\n", __func__, type);
		return -EINVAL;
	}

	if (atomic_read(&fwd_info->opened) &&
	    fwd_info->p_ops && fwd_info->p_ops->open) {
		/*
		 * The registration can happen late, like in the case of
		 * sockets. fwd_info->opened reflects diag_state. Propagate the
		 * state to the peipherals.
		 */
		fwd_info->p_ops->open(fwd_info->ctxt);
	}

	return 0;
}

void diagfwd_deregister(uint8_t peripheral, uint8_t type, void *ctxt)
{
	struct diagfwd_info *fwd_info = NULL;

	if (peripheral >= NUM_PERIPHERALS || type >= NUM_TYPES || !ctxt)
		return;

	fwd_info = &peripheral_info[type][peripheral];
	if (fwd_info->ctxt != ctxt) {
		pr_err("diag: In %s, unable to find a match for p: %d t: %d\n",
		       __func__, peripheral, type);
		return;
	}
	fwd_info->ctxt = NULL;
	fwd_info->p_ops = NULL;
	fwd_info->ch_open = 0;
	diagfwd_buffers_exit(fwd_info);

	switch (type) {
	case TYPE_DATA:
		driver->diagfwd_data[peripheral] = NULL;
		break;
	case TYPE_CNTL:
		driver->diagfwd_cntl[peripheral] = NULL;
		break;
	case TYPE_DCI:
		driver->diagfwd_dci[peripheral] = NULL;
		break;
	case TYPE_CMD:
		driver->diagfwd_cmd[peripheral] = NULL;
		break;
	case TYPE_DCI_CMD:
		driver->diagfwd_dci_cmd[peripheral] = NULL;
		break;
	}
}

void diagfwd_close_transport(uint8_t transport, uint8_t peripheral)
{
	struct diagfwd_info *fwd_info = NULL;
	struct diagfwd_info *dest_info = NULL;
	int (*init_fn)(uint8_t) = NULL;
	void (*invalidate_fn)(void *, struct diagfwd_info *) = NULL;
	int (*check_channel_state)(void *) = NULL;
	uint8_t transport_open = 0;

	if (peripheral >= NUM_PERIPHERALS)
		return;

	switch (transport) {
	case TRANSPORT_RPMSG:
		transport_open = TRANSPORT_SOCKET;
		init_fn = diag_socket_init_peripheral;
		invalidate_fn = diag_socket_invalidate;
		check_channel_state = diag_socket_check_state;
		break;
	case TRANSPORT_SOCKET:
		transport_open = TRANSPORT_RPMSG;
		init_fn = diag_rpmsg_init_peripheral;
		invalidate_fn = diag_rpmsg_invalidate;
		check_channel_state = diag_rpmsg_check_state;
		break;
	default:
		return;

	}

	mutex_lock(&driver->diagfwd_channel_mutex[peripheral]);
	fwd_info = &early_init_info[transport][peripheral];
	mutex_unlock(&driver->diagfwd_channel_mutex[peripheral]);
	if (fwd_info->p_ops && fwd_info->p_ops->close)
		fwd_info->p_ops->close(fwd_info->ctxt);
	mutex_lock(&driver->diagfwd_channel_mutex[peripheral]);
	fwd_info = &early_init_info[transport_open][peripheral];
	dest_info = &peripheral_info[TYPE_CNTL][peripheral];
	dest_info->inited = 1;
	dest_info->ctxt = fwd_info->ctxt;
	dest_info->p_ops = fwd_info->p_ops;
	dest_info->c_ops = fwd_info->c_ops;
	dest_info->ch_open = fwd_info->ch_open;
	dest_info->read_bytes = fwd_info->read_bytes;
	dest_info->write_bytes = fwd_info->write_bytes;
	dest_info->inited = fwd_info->inited;
	dest_info->buf_1 = fwd_info->buf_1;
	dest_info->buf_2 = fwd_info->buf_2;
	dest_info->transport = fwd_info->transport;
	invalidate_fn(dest_info->ctxt, dest_info);
	if (!check_channel_state(dest_info->ctxt))
		diagfwd_late_open(dest_info);

	/*
	 *	Open control channel to update masks after buffers are
	 *	initialized for peripherals that have transport other than
	 *	GLINK. GLINK supported peripheral mask update will
	 *	happen after glink buffers are initialized.
	 */
	diagfwd_cntl_open(dest_info);
	init_fn(peripheral);
	mutex_unlock(&driver->diagfwd_channel_mutex[peripheral]);
	diagfwd_queue_read(&peripheral_info[TYPE_DATA][peripheral]);
	diagfwd_queue_read(&peripheral_info[TYPE_CMD][peripheral]);
}

int diagfwd_write(uint8_t peripheral, uint8_t type, void *buf, int len)
{
	struct diagfwd_info *fwd_info = NULL;
	int err = 0;
	uint8_t retry_count = 0;
	uint8_t max_retries = 3;
	unsigned char *temp_buf = NULL;

	if (peripheral >= NUM_PERIPHERALS || type >= NUM_TYPES || !buf)
		return -EINVAL;

	if (type == TYPE_CMD || type == TYPE_DCI_CMD) {
		if (!driver->feature[peripheral].rcvd_feature_mask ||
			!driver->feature[peripheral].sent_feature_mask) {
			pr_debug_ratelimited("diag: In %s, feature mask for peripheral: %d not received or sent yet\n",
					     __func__, peripheral);
			return 0;
		}
		if (!driver->feature[peripheral].separate_cmd_rsp)
			type = (type == TYPE_CMD) ? TYPE_DATA : TYPE_DCI;
	}

	fwd_info = &peripheral_info[type][peripheral];
	if (!fwd_info->inited || !atomic_read(&fwd_info->opened))
		return -ENODEV;

	if (type == TYPE_CMD) {
		if (driver->feature[peripheral].diag_id_support)
			if (!fwd_info->root_diag_id.diagid_val ||
				(!driver->diag_id_sent[peripheral])) {
				DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
					 "diag: diag_id is not assigned yet\n");
			return 0;
		}
	}

	if (!(fwd_info->p_ops && fwd_info->p_ops->write && fwd_info->ctxt))
		return -EIO;

	if (type == TYPE_CMD) {
		temp_buf = (unsigned char *)(buf);
		/* Only raw bytes is sent to peripheral,
		 * HDLC/NON-HDLC need not be considered
		 */
		DIAG_LOG(DIAG_DEBUG_CMD_INFO,
		"diag: cmd (%02x %02x %02x %02x) ready to be written to p: %d\n",
		*(temp_buf), *(temp_buf+1), *(temp_buf+2), *(temp_buf+3),
		peripheral);
	}

	while (retry_count < max_retries) {
		err = 0;
		err = fwd_info->p_ops->write(fwd_info->ctxt, buf, len);
		if (err && err != -ENODEV) {
			usleep_range(100000, 101000);
			retry_count++;
			continue;
		}
		break;
	}

	if (!err)
		fwd_info->write_bytes += len;

	return err;
}

static void __diag_fwd_open(struct diagfwd_info *fwd_info)
{
	if (!fwd_info)
		return;

	atomic_set(&fwd_info->opened, 1);
	if (!fwd_info->inited)
		return;

	/*
	 * Logging mode here is reflecting previous mode
	 * status and will be updated to new mode later.
	 *
	 * Keeping the buffers busy for Memory Device and Multi Mode.
	 */

	if (driver->logging_mode[DIAG_LOCAL_PROC] != DIAG_USB_MODE) {
		if (fwd_info->buf_1) {
			atomic_set(&fwd_info->buf_1->in_busy, 0);
			fwd_info->buffer_status[BUF_1_INDEX] = 0;
			DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"Buffer 1 for core PD is marked free, p: %d, t: %d\n",
			fwd_info->peripheral, fwd_info->type);
		}
		if (fwd_info->buf_2) {
			atomic_set(&fwd_info->buf_2->in_busy, 0);
			fwd_info->buffer_status[BUF_2_INDEX] = 0;
			DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"Buffer 2 for core PD is marked free, p: %d, t: %d\n",
			fwd_info->peripheral, fwd_info->type);
		}
	}

	if (fwd_info->p_ops && fwd_info->p_ops->open)
		fwd_info->p_ops->open(fwd_info->ctxt);

	diagfwd_queue_read(fwd_info);
}

void diagfwd_early_open(uint8_t peripheral)
{
	uint8_t transport = 0;
	struct diagfwd_info *fwd_info = NULL;

	if (peripheral >= NUM_PERIPHERALS)
		return;

	for (transport = 0; transport < NUM_TRANSPORT; transport++) {
		fwd_info = &early_init_info[transport][peripheral];
		__diag_fwd_open(fwd_info);
	}
}

void diagfwd_open(uint8_t peripheral, uint8_t type)
{
	struct diagfwd_info *fwd_info = NULL;

	if (peripheral >= NUM_PERIPHERALS || type >= NUM_TYPES)
		return;

	fwd_info = &peripheral_info[type][peripheral];
	__diag_fwd_open(fwd_info);
}

void diagfwd_late_open(struct diagfwd_info *fwd_info)
{
	__diag_fwd_open(fwd_info);
}

void diagfwd_close(uint8_t peripheral, uint8_t type)
{
	struct diagfwd_info *fwd_info = NULL;

	if (peripheral >= NUM_PERIPHERALS || type >= NUM_TYPES)
		return;

	fwd_info = &peripheral_info[type][peripheral];
	atomic_set(&fwd_info->opened, 0);
	if (!fwd_info->inited)
		return;

	if (fwd_info->p_ops && fwd_info->p_ops->close)
		fwd_info->p_ops->close(fwd_info->ctxt);

	if (fwd_info->buf_1)
		atomic_set(&fwd_info->buf_1->in_busy, 1);
	/*
	 * Only Data channels have two buffers. Set both the buffers
	 * to busy on close.
	 */
	if (fwd_info->buf_2)
		atomic_set(&fwd_info->buf_2->in_busy, 1);
}

int diagfwd_channel_open(struct diagfwd_info *fwd_info)
{

	if (!fwd_info)
		return -EIO;

	if (!fwd_info->inited) {
		pr_debug("diag: In %s, channel is not inited, p: %d, t: %d\n",
			 __func__, fwd_info->peripheral, fwd_info->type);
		return -EINVAL;
	}

	if (fwd_info->ch_open) {
		pr_debug("diag: In %s, channel is already open, p: %d, t: %d\n",
			 __func__, fwd_info->peripheral, fwd_info->type);
		return 0;
	}
	mutex_lock(&driver->diagfwd_channel_mutex[fwd_info->peripheral]);
	fwd_info->ch_open = 1;
	diagfwd_buffers_init(fwd_info);

	if (fwd_info && fwd_info->c_ops && fwd_info->c_ops->open)
		fwd_info->c_ops->open(fwd_info);

	diagfwd_queue_read(fwd_info);
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "p: %d t: %d considered opened\n",
		 fwd_info->peripheral, fwd_info->type);

	if (atomic_read(&fwd_info->opened)) {
		if (fwd_info->p_ops && fwd_info->p_ops->open)
			fwd_info->p_ops->open(fwd_info->ctxt);
	}
	mutex_unlock(&driver->diagfwd_channel_mutex[fwd_info->peripheral]);
	return 0;
}

int diagfwd_channel_close(struct diagfwd_info *fwd_info)
{
	if (!fwd_info)
		return -EIO;

	mutex_lock(&driver->diagfwd_channel_mutex[fwd_info->peripheral]);
	fwd_info->ch_open = 0;
	if (fwd_info && fwd_info->c_ops && fwd_info->c_ops->close)
		fwd_info->c_ops->close(fwd_info);

	if (fwd_info->buf_1 && fwd_info->buf_1->data) {
		atomic_set(&fwd_info->buf_1->in_busy, 0);
		fwd_info->buffer_status[BUF_1_INDEX] = 0;
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		"Buffer 1 for core PD is marked free, p: %d, t: %d\n",
			fwd_info->peripheral, fwd_info->type);
	}
	if (fwd_info->buf_2 && fwd_info->buf_2->data) {
		atomic_set(&fwd_info->buf_2->in_busy, 0);
		fwd_info->buffer_status[BUF_2_INDEX] = 0;
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		"Buffer 2 for core PD is marked free, p: %d, t: %d\n",
				fwd_info->peripheral, fwd_info->type);
	}

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "p: %d t: %d considered closed\n",
		 fwd_info->peripheral, fwd_info->type);
	mutex_unlock(&driver->diagfwd_channel_mutex[fwd_info->peripheral]);
	return 0;
}

int diagfwd_channel_read_done(struct diagfwd_info *fwd_info,
			      unsigned char *buf, uint32_t len)
{
	if (!fwd_info) {
		diag_ws_release();
		return -EIO;
	}

	/*
	 * Diag peripheral layers should send len as 0 if there is any error
	 * in reading data from the transport. Use this information to reset the
	 * in_busy flags. No need to queue read in this case.
	 */
	if (len == 0) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		"Read Length is 0, resetting the diag buffers p: %d, t: %d\n",
			fwd_info->peripheral, fwd_info->type);
		diagfwd_reset_buffers(fwd_info, buf);
		diag_ws_release();
		return 0;
	}

	if (fwd_info && fwd_info->c_ops && fwd_info->c_ops->read_done)
		fwd_info->c_ops->read_done(fwd_info, buf, len);
	fwd_info->read_bytes += len;

	return 0;
}

void diagfwd_write_done(uint8_t peripheral, uint8_t type, int buf_num)
{
	int i = 0, upd_valid_len = 0;
	struct diagfwd_info *fwd_info = NULL;

	if (peripheral >= NUM_PERIPHERALS || type >= NUM_TYPES)
		return;

	fwd_info = &peripheral_info[type][peripheral];
	if (!fwd_info)
		return;

	if (buf_num == 1 && fwd_info->buf_1) {
		/*
		 * Core PD buffer data is processed and
		 * length in the buffer is marked zero.
		 *
		 * Check if the user PD buffer contains any
		 * data before freeing core PD buffer.
		 */
		fwd_info->cpd_len_1 = 0;
		for (i = 0; i <= (fwd_info->num_pd - 2); i++) {
			if (fwd_info->upd_len[i][0]) {
				upd_valid_len = 1;
				break;
			}
		}
		/*
		 * Do not free the core pd buffer if valid data
		 * is present in any user PD buffer.
		 */
		if (!upd_valid_len) {
			atomic_set(&fwd_info->buf_1->in_busy, 0);
			fwd_info->buffer_status[BUF_1_INDEX] = 0;
			DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"Buffer 1 for core PD is marked free, p: %d, t: %d, buf_num: %d\n",
				fwd_info->peripheral, fwd_info->type, buf_num);
			rpmsg_mark_buffers_free(peripheral, type, buf_num);
		}
	} else if (buf_num == 2 && fwd_info->buf_2) {
		/*
		 * Core PD buffer data is processed and
		 * length in the buffer is marked zero.
		 *
		 * Check if the user PD buffer contains any
		 * data before freeing core PD buffer.
		 */
		fwd_info->cpd_len_2 = 0;
		for (i = 0; i <= (fwd_info->num_pd - 2); i++) {
			if (fwd_info->upd_len[i][1]) {
				upd_valid_len = 1;
				break;
			}
		}
		/*
		 * Do not free the core pd buffer if valid data
		 * is present in any user PD buffer
		 */
		if (!upd_valid_len) {
			atomic_set(&fwd_info->buf_2->in_busy, 0);
			fwd_info->buffer_status[BUF_2_INDEX] = 0;
			DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"Buffer 2 for core PD is marked free, p: %d, t: %d, buf_num: %d\n",
				fwd_info->peripheral, fwd_info->type, buf_num);
			rpmsg_mark_buffers_free(peripheral, type, buf_num);
		}
	} else if (buf_num >= 3 && (buf_num % 2)) {
		/*
		 * Go through each User PD buffer, validate the
		 * request for freeing the buffer by validating
		 * the buffer number.
		 *
		 */
		for (i = 0; i <= (fwd_info->num_pd - 2); i++) {
			if (fwd_info->buf_upd[i][0] &&
				(buf_num == ((2 * i) + 3))) {
				/* Buffer 1 for ith user PD is freed */
				atomic_set(&fwd_info->buf_upd[i][0]->in_busy,
					0);
				fwd_info->upd_len[i][0] = 0;
			}
			/*
			 * Check if there is any data in user PD buffer other
			 * than buffer requested for freeing.
			 *
			 */
			if (fwd_info->upd_len[i][0])
				upd_valid_len = 1;
		}
		/*
		 * Mark the core pd buffer free if there is no
		 * data present in core PD buffer and other User PD buffer.
		 *
		 */
		if (!upd_valid_len && !fwd_info->cpd_len_1) {
			atomic_set(&fwd_info->buf_1->in_busy, 0);
			fwd_info->buffer_status[BUF_1_INDEX] = 0;
			DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
				"Buffer 1 for core PD is marked free, p: %d, t: %d, buf_num: %d\n",
				fwd_info->peripheral, fwd_info->type, buf_num);
			rpmsg_mark_buffers_free(peripheral, type, 1);
		}
	} else if (buf_num >= 4 && !(buf_num % 2)) {
		/*
		 * Go through each User PD buffer, validate the
		 * request for freeing the buffer by validating
		 * the buffer number.
		 *
		 */
		for (i = 0; i <= (fwd_info->num_pd - 2); i++) {
			if (fwd_info->buf_upd[i][1] &&
				(buf_num == ((2 * i) + 4))) {
				/* Buffer 2 for ith user PD is freed */
				atomic_set(&fwd_info->buf_upd[i][1]->in_busy,
					0);
				fwd_info->upd_len[i][1] = 0;
			}
			/*
			 * Check if there is any data in user PD buffer other
			 * than buffer requested for freeing.
			 *
			 */
			if (fwd_info->upd_len[i][1])
				upd_valid_len = 1;
		}
		/*
		 * Mark the core pd buffer free if there is no
		 * data present in core PD buffer and other User PD buffer.
		 *
		 */
		if (!upd_valid_len && !fwd_info->cpd_len_2) {
			atomic_set(&fwd_info->buf_2->in_busy, 0);
			fwd_info->buffer_status[BUF_2_INDEX] = 0;
			DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"Buffer 2 for core PD is marked free, p: %d, t: %d, buf_num: %d\n",
			fwd_info->peripheral, fwd_info->type, buf_num);
			rpmsg_mark_buffers_free(peripheral, type, 2);
		}
	} else
		pr_err("diag: In %s, invalid buf_num %d\n", __func__, buf_num);

	diagfwd_queue_read(fwd_info);
}

void diagfwd_channel_read(struct diagfwd_info *fwd_info)
{
	int err = 0;
	uint32_t read_len = 0;
	unsigned char *read_buf = NULL;
	struct diagfwd_buf_t *temp_buf = NULL;

	if (!fwd_info) {
		diag_ws_release();
		return;
	}

	if (!fwd_info->inited || !atomic_read(&fwd_info->opened)) {
		pr_debug("diag: In %s, p: %d, t: %d, inited: %d, opened: %d  ch_open: %d\n",
			 __func__, fwd_info->peripheral, fwd_info->type,
			 fwd_info->inited, atomic_read(&fwd_info->opened),
			 fwd_info->ch_open);
		diag_ws_release();
		return;
	}

	if (fwd_info->buf_1 && !atomic_read(&fwd_info->buf_1->in_busy)) {
		if (driver->feature[fwd_info->peripheral].encode_hdlc &&
		    (fwd_info->type == TYPE_DATA ||
		     fwd_info->type == TYPE_CMD)) {
			read_buf = fwd_info->buf_1->data_raw;
			read_len = fwd_info->buf_1->len_raw;
		} else {
			read_buf = fwd_info->buf_1->data;
			read_len = fwd_info->buf_1->len;
		}
		if (read_buf) {
			temp_buf = fwd_info->buf_1;
			atomic_set(&temp_buf->in_busy, 1);
		}
	} else if (fwd_info->buf_2 && !atomic_read(&fwd_info->buf_2->in_busy)) {
		if (driver->feature[fwd_info->peripheral].encode_hdlc &&
		    (fwd_info->type == TYPE_DATA ||
		     fwd_info->type == TYPE_CMD)) {
			read_buf = fwd_info->buf_2->data_raw;
			read_len = fwd_info->buf_2->len_raw;
		} else {
			read_buf = fwd_info->buf_2->data;
			read_len = fwd_info->buf_2->len;
		}
		if (read_buf) {
			temp_buf = fwd_info->buf_2;
			atomic_set(&temp_buf->in_busy, 1);
		}
	} else {
		pr_debug("diag: In %s, both buffers are empty for p: %d, t: %d\n",
			 __func__, fwd_info->peripheral, fwd_info->type);
	}

	if (!read_buf) {
		diag_ws_release();
		return;
	}

	if (!(fwd_info->p_ops && fwd_info->p_ops->read && fwd_info->ctxt))
		goto fail_return;

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "issued a read p: %d t: %d buf: %pK\n",
		 fwd_info->peripheral, fwd_info->type, read_buf);
	err = fwd_info->p_ops->read(fwd_info->ctxt, read_buf, read_len);
	if (err)
		goto fail_return;

	return;

fail_return:
	diag_ws_release();
	atomic_set(&temp_buf->in_busy, 0);
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
	"Buffer for core PD is marked free, p: %d, t: %d, buf_num: %d\n",
		fwd_info->peripheral, fwd_info->type,
		GET_BUF_NUM(temp_buf->ctxt));
}

static void diagfwd_queue_read(struct diagfwd_info *fwd_info)
{
	if (!fwd_info)
		return;

	if (!fwd_info->inited || !atomic_read(&fwd_info->opened)) {
		pr_debug("diag: In %s, p: %d, t: %d, inited: %d, opened: %d  ch_open: %d\n",
			 __func__, fwd_info->peripheral, fwd_info->type,
			 fwd_info->inited, atomic_read(&fwd_info->opened),
			 fwd_info->ch_open);
		return;
	}

	/*
	 * Don't queue a read on the data and command channels before receiving
	 * the feature mask from the peripheral. We won't know which buffer to
	 * use - HDLC or non HDLC buffer for reading.
	 */
	if ((!driver->feature[fwd_info->peripheral].rcvd_feature_mask) &&
	    (fwd_info->type != TYPE_CNTL)) {
		return;
	}

	if (fwd_info->p_ops && fwd_info->p_ops->queue_read && fwd_info->ctxt)
		fwd_info->p_ops->queue_read(fwd_info->ctxt);
}

static int diagfwd_buffers_allocate(struct diagfwd_info *fwd_info)
{
	int i, j;

	for (i = 0; ((fwd_info->num_pd > 1) &&
		(i <= (fwd_info->num_pd - 2))); i++) {
		for (j = 0; j < NUM_WRITE_BUFFERS; j++) {
			if (!fwd_info->buf_upd[i][j]) {
				fwd_info->buf_upd[i][j] =
					kzalloc(sizeof(struct diagfwd_buf_t),
						  GFP_KERNEL);
				if (ZERO_OR_NULL_PTR(fwd_info->buf_upd[i][j]))
					return -ENOMEM;
				kmemleak_not_leak(fwd_info->buf_upd[i][j]);
			}

			if (fwd_info->buf_upd[i][j] &&
				!fwd_info->buf_upd[i][j]->data) {
				fwd_info->buf_upd[i][j]->data =
					kzalloc(PERIPHERAL_BUF_SZ +
						APF_DIAG_PADDING,
						GFP_KERNEL);
				if (ZERO_OR_NULL_PTR(
					fwd_info->buf_upd[i][j]->data))
					return -ENOMEM;
				fwd_info->buf_upd[i][j]->len =
					PERIPHERAL_BUF_SZ;
				kmemleak_not_leak(
					fwd_info->buf_upd[i][j]->data);
				fwd_info->buf_upd[i][j]->ctxt =
					SET_BUF_CTXT(fwd_info->peripheral,
					fwd_info->type, ((2 * i) + (j + 3)));
			}

			if (driver->supports_apps_hdlc_encoding) {
				if (fwd_info->buf_upd[i][j] &&
					!fwd_info->buf_upd[i][j]->data_raw) {
					fwd_info->buf_upd[i][j]->data_raw =
						kzalloc(PERIPHERAL_BUF_SZ +
							APF_DIAG_PADDING,
							GFP_KERNEL);
					if (ZERO_OR_NULL_PTR(
					fwd_info->buf_upd[i][j]->data_raw))
						return -ENOMEM;
					fwd_info->buf_upd[i][j]->len_raw =
						PERIPHERAL_BUF_SZ;
					kmemleak_not_leak(
					fwd_info->buf_upd[i][j]->data_raw);
				}
			}
		}
	}
	return 0;
}

void diagfwd_buffers_init(struct diagfwd_info *fwd_info)
{
	int ret = 0;
	unsigned char *temp_char_buf;

	if (!fwd_info)
		return;

	if (!fwd_info->inited) {
		pr_err("diag: In %s, channel not inited, p: %d, t: %d\n",
		       __func__, fwd_info->peripheral, fwd_info->type);
		return;
	}

	mutex_lock(&fwd_info->buf_mutex);

	if (!fwd_info->buf_1) {
		fwd_info->buf_1 = kzalloc(sizeof(struct diagfwd_buf_t),
					  GFP_KERNEL);
		if (ZERO_OR_NULL_PTR(fwd_info->buf_1))
			goto err;
		kmemleak_not_leak(fwd_info->buf_1);
	}

	if (!fwd_info->buf_1->data) {
		fwd_info->buf_1->data = kzalloc(PERIPHERAL_BUF_SZ +
					APF_DIAG_PADDING,
					GFP_KERNEL);
		if (ZERO_OR_NULL_PTR(fwd_info->buf_1->data))
			goto err;
		fwd_info->buf_1->len = PERIPHERAL_BUF_SZ;
		kmemleak_not_leak(fwd_info->buf_1->data);
		fwd_info->buf_1->ctxt = SET_BUF_CTXT(fwd_info->peripheral,
						     fwd_info->type, 1);
	}

	if (fwd_info->type == TYPE_DATA) {
		if (!fwd_info->buf_2) {
			fwd_info->buf_2 = kzalloc(sizeof(struct diagfwd_buf_t),
					      GFP_KERNEL);
			if (ZERO_OR_NULL_PTR(fwd_info->buf_2))
				goto err;
			kmemleak_not_leak(fwd_info->buf_2);
		}

		if (!fwd_info->buf_2->data) {
			fwd_info->buf_2->data = kzalloc(PERIPHERAL_BUF_SZ +
							APF_DIAG_PADDING,
						    GFP_KERNEL);
			if (ZERO_OR_NULL_PTR(fwd_info->buf_2->data))
				goto err;
			fwd_info->buf_2->len = PERIPHERAL_BUF_SZ;
			kmemleak_not_leak(fwd_info->buf_2->data);
			fwd_info->buf_2->ctxt = SET_BUF_CTXT(
							fwd_info->peripheral,
							fwd_info->type, 2);
		}

		if (driver->feature[fwd_info->peripheral].untag_header) {
			ret = diagfwd_buffers_allocate(fwd_info);
			if (ret)
				goto err;
		}

		if (driver->supports_apps_hdlc_encoding) {
			/* In support of hdlc encoding */
			if (!fwd_info->buf_1->data_raw) {
				fwd_info->buf_1->data_raw =
					kzalloc(PERIPHERAL_BUF_SZ +
						APF_DIAG_PADDING,
						GFP_KERNEL);
				temp_char_buf =
					fwd_info->buf_1->data_raw;
				if (ZERO_OR_NULL_PTR(temp_char_buf))
					goto err;
				fwd_info->buf_1->len_raw =
					PERIPHERAL_BUF_SZ;
				kmemleak_not_leak(temp_char_buf);
			}

			if (!fwd_info->buf_2->data_raw) {
				fwd_info->buf_2->data_raw =
					kzalloc(PERIPHERAL_BUF_SZ +
						APF_DIAG_PADDING,
						GFP_KERNEL);
				temp_char_buf =
					fwd_info->buf_2->data_raw;
				if (ZERO_OR_NULL_PTR(temp_char_buf))
					goto err;
				fwd_info->buf_2->len_raw =
					PERIPHERAL_BUF_SZ;
				kmemleak_not_leak(temp_char_buf);
			}
		}
	}

	if (fwd_info->type == TYPE_CMD &&
		driver->supports_apps_hdlc_encoding) {
		/* In support of hdlc encoding */
		if (!fwd_info->buf_1->data_raw) {
			fwd_info->buf_1->data_raw = kzalloc(PERIPHERAL_BUF_SZ +
						APF_DIAG_PADDING,
							GFP_KERNEL);
			temp_char_buf =
				fwd_info->buf_1->data_raw;
			if (ZERO_OR_NULL_PTR(temp_char_buf))
				goto err;
			fwd_info->buf_1->len_raw = PERIPHERAL_BUF_SZ;
			kmemleak_not_leak(temp_char_buf);
		}
	}

	mutex_unlock(&fwd_info->buf_mutex);
	return;

err:
	mutex_unlock(&fwd_info->buf_mutex);
	diagfwd_buffers_exit(fwd_info);
}

static void diagfwd_buffers_exit(struct diagfwd_info *fwd_info)
{
	int i = 0;

	if (!fwd_info)
		return;

	mutex_lock(&fwd_info->buf_mutex);
	if (fwd_info->buf_1) {
		kfree(fwd_info->buf_1->data);
		fwd_info->buf_1->data = NULL;
		kfree(fwd_info->buf_1->data_raw);
		fwd_info->buf_1->data_raw = NULL;
		kfree(fwd_info->buf_1);
		fwd_info->buf_1 = NULL;
	}
	if (fwd_info->buf_2) {
		kfree(fwd_info->buf_2->data);
		fwd_info->buf_2->data = NULL;
		kfree(fwd_info->buf_2->data_raw);
		fwd_info->buf_2->data_raw = NULL;
		kfree(fwd_info->buf_2);
		fwd_info->buf_2 = NULL;
	}
	for (i = 0; i <= (fwd_info->num_pd - 2); i++) {
		if (fwd_info->buf_upd[i][0]) {
			kfree(fwd_info->buf_upd[i][0]->data);
			fwd_info->buf_upd[i][0]->data = NULL;
			kfree(fwd_info->buf_upd[i][0]->data_raw);
			fwd_info->buf_upd[i][0]->data_raw = NULL;
			kfree(fwd_info->buf_upd[i][0]);
			fwd_info->buf_upd[i][0] = NULL;
		}
		if (fwd_info->buf_upd[i][1]) {
			kfree(fwd_info->buf_upd[i][1]->data);
			fwd_info->buf_upd[i][1]->data = NULL;
			kfree(fwd_info->buf_upd[i][1]->data_raw);
			fwd_info->buf_upd[i][1]->data_raw = NULL;
			kfree(fwd_info->buf_upd[i][1]);
			fwd_info->buf_upd[i][1] = NULL;
		}
	}
	mutex_unlock(&fwd_info->buf_mutex);
}

