// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */
#include <linux/kthread.h>
#include "ccci_bm.h"
#include "port_proxy.h"
#include "port_udc.h"
#include "port_smem.h"

extern atomic_t udc_status;

#define MAX_QUEUE_LENGTH 16

#define Min(a, b) (a < b ? a : b)
#if (MD_GENERATION >= 6295)
#define MAX_PACKET_SIZE 1277936 /* (2.4375*1024*1024 -32)/2 */
#define COMP_DATA_OFFSET (0xC00000 + 32)
#define RW_INDEX_OFFSET 0xC00000
#define UDC_UNCMP_CACHE_BUF_SZ 0x64000 /* 400k */
#define rslt_entry 2048
#elif (MD_GENERATION == 6293)
#define MAX_PACKET_SIZE 1048576 /* 2*1024*1024/2 */
#define COMP_DATA_OFFSET 0x570000
#define RW_INDEX_OFFSET 0x568000
#define UDC_UNCMP_CACHE_BUF_SZ 0xC000 /* 48k */
#define rslt_entry 1024
#endif
/*kernel 4.14 diff kernel 4.19

In kernel4.19:
GEN93 : MT6761 MT6765
GEN95 : MT6779

#define MAX_PACKET_SIZE 2555872 // 2.4375*1024*1024 -32
#define COMP_DATA_OFFSET (0x500000+32)
#define RW_INDEX_OFFSET 0x500000
#define UDC_UNCMP_CACHE_BUF_SZ 0xC000 // 48k

In kernel4.14:
#if (MD_GENERATION >= 6295)
#define MAX_PACKET_SIZE 1277936 // (2.4375*1024*1024 -32)/2
#define COMP_DATA_OFFSET (0xC00000 + 32)
#define RW_INDEX_OFFSET 0xC00000
#define UDC_UNCMP_CACHE_BUF_SZ 0x64000 // 400k
#define rslt_entry 2048
#elif (MD_GENERATION == 6293)
#define MAX_PACKET_SIZE 1048576 // 2*1024*1024/2
#define COMP_DATA_OFFSET 0x570000
#define RW_INDEX_OFFSET 0x568000
#define UDC_UNCMP_CACHE_BUF_SZ 0xC000 // 48k
#define rslt_entry 1024
#endif
*/

struct ap_md_rw_index *rw_index;
unsigned char *comp_data_buf_base, *uncomp_data_buf_base;
unsigned char *uncomp_cache_data_base;
struct udc_comp_req_t *req_des_0_base, *req_des_1_base;
struct udc_comp_rslt_t *rslt_des_0_base, *rslt_des_1_base;
static unsigned int total_comp_size;

void set_udc_status(struct sk_buff *skb)
{
	struct ccci_udc_cmd_rsp_t *udc_cmd_rsp =
		(struct ccci_udc_cmd_rsp_t *)skb->data;

	if (udc_cmd_rsp->udc_inst_id == 0 &&
		udc_cmd_rsp->udc_cmd == UDC_CMD_KICK)
		atomic_set(&udc_status, UDC_HighKick);
	if (udc_cmd_rsp->udc_cmd == UDC_CMD_DEACTV)
		atomic_set(&udc_status, UDC_DEACTV);
	if (udc_cmd_rsp->udc_cmd == UDC_CMD_DISC)
		atomic_set(&udc_status, UDC_DISCARD);
}

int udc_resp_msg_to_md(struct port_t *port,
	struct sk_buff *skb, int handle_udc_ret)
{
	int md_id = port->md_id;
	int data_len, ret;
	struct ccci_udc_cmd_rsp_t *udc_cmd_rsp =
		(struct ccci_udc_cmd_rsp_t *)skb->data;

	/* write back to modem */
	/* update message */
	udc_cmd_rsp->udc_cmd |= UDC_API_RESP_ID;
	data_len = sizeof(*udc_cmd_rsp);
	if (handle_udc_ret < 0) {
		udc_cmd_rsp->rslt = UDC_CMD_RSLT_ERROR;
		CCCI_NORMAL_LOG(md_id, UDC,
			"rsp ins%d cmd:0x%x,rslt:%d\n",
			udc_cmd_rsp->udc_inst_id,
			udc_cmd_rsp->udc_cmd, udc_cmd_rsp->rslt);
	} else
		udc_cmd_rsp->rslt = UDC_CMD_RSLT_OK;

	/* resize skb */
	CCCI_DEBUG_LOG(md_id, UDC,
		"data_len:%d,skb->len:%d\n", data_len, skb->len);
	if (data_len > skb->len)
		skb_put(skb, data_len - skb->len);
	else if (data_len < skb->len)
		skb_trim(skb, data_len);
	/* update CCCI header */
	udc_cmd_rsp->header.channel = CCCI_UDC_TX;
	udc_cmd_rsp->header.data[1] = data_len;
	CCCI_DEBUG_LOG(md_id, UDC,
		"Write %d/%d, %08X, %08X, %08X, %08X, op_id=0x%x\n",
		skb->len, data_len, udc_cmd_rsp->header.data[0],
		udc_cmd_rsp->header.data[1], udc_cmd_rsp->header.channel,
		udc_cmd_rsp->header.reserved, udc_cmd_rsp->udc_cmd);
	/* switch to Tx request */
	ret = port_send_skb_to_md(port, skb, 1);

	CCCI_DEBUG_LOG(md_id, UDC,
		"send_skb_to_md:%d,rsp ins%d cmd:0x%x,rslt:%d\n",
		ret, udc_cmd_rsp->udc_inst_id,
		udc_cmd_rsp->udc_cmd, udc_cmd_rsp->rslt);

	return ret;
}

void udc_cmd_check(struct port_t *port,
	struct sk_buff **skb_tmp, struct sk_buff **skb,
	u32 inst_id, struct udc_state_ctl *ctl)
{
	int md_id = port->md_id;
	unsigned long flags;
	struct ccci_udc_deactv_param_t *ccci_udc_deactv;
	struct ccci_udc_actv_param_t *ccci_udc_actv;
	int skb_len, skb_tmp_len, skb_tmp1_len;
	struct sk_buff *skb_tmp1 = NULL;
	u32 ins_id_tmp;

	ctl->last_state = ctl->curr_state;
	ctl->curr_state = atomic_read(&udc_status);

	switch (ctl->curr_state) {
	case UDC_DEACTV_DONE:
	case UDC_DISC_DONE:
		atomic_set(&udc_status, UDC_IDLE);
		ctl->curr_state = UDC_IDLE;
		break;
	case UDC_IDLE:
	case UDC_HandleHighKick:
	case UDC_KICKDEACTV:
		break;
	case UDC_HighKick:
	{
		if (inst_id == 0) {
			atomic_set(&udc_status, UDC_IDLE);
			ctl->curr_state = UDC_IDLE;
			break;
		}
		CCCI_DEBUG_LOG(md_id, UDC,
			"high prio kick0 come in\n");
		if (!skb_queue_empty(&port->rx_skb_list)) {
			skb_len = (*skb)->len;
			skb_tmp_len = (*skb_tmp)->len;
			 /* resize skb */
			 CCCI_DEBUG_LOG(md_id, UDC,
				"skb_len:%d,skb_tmp_len:%d\n",
				skb_len, skb_tmp_len);
			if (skb_len > skb_tmp_len)
				skb_put(*skb_tmp, skb_len - skb_tmp_len);
			else if (skb_len < skb_tmp_len)
				skb_trim(*skb_tmp, skb_len);

			/* backup skb to skb_tmp */
			skb_copy_to_linear_data(*skb_tmp,
				(*skb)->data, skb_len);
			spin_lock_irqsave(&port->rx_skb_list.lock, flags);
			*skb = __skb_dequeue(&port->rx_skb_list);
			spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
			ctl->last_state = ctl->curr_state;
			atomic_set(&udc_status, UDC_HandleHighKick);
			ctl->curr_state = UDC_HandleHighKick;
		} else
			goto err;
		break;
	}
	case UDC_DEACTV:
	{
		if (!skb_queue_empty(&port->rx_skb_list)) {
			if (ctl->last_state == UDC_HandleHighKick) {
				skb_tmp1 = ccci_alloc_skb(
					sizeof(*ccci_udc_actv), 1, 1);
				if (unlikely(!skb_tmp1)) {
					CCCI_ERROR_LOG(md_id, UDC,
						"alloc skb_tmp1 fail\n");
					return;
				}
				/* backup skb_tmp to skb_tmp1 */
				skb_copy_to_linear_data(skb_tmp1,
					(*skb_tmp)->data, (*skb_tmp)->len);
				skb_put(skb_tmp1, (*skb_tmp)->len);
			}
			skb_len = (*skb)->len;
			skb_tmp_len = (*skb_tmp)->len;
			/* resize skb */
			CCCI_DEBUG_LOG(md_id, UDC,
				"skb_len:%d,skb_tmp_len:%d\n",
				skb_len, skb_tmp_len);
			if (skb_len > skb_tmp_len)
				skb_put(*skb_tmp, skb_len - skb_tmp_len);
			else if (skb_len < skb_tmp_len)
				skb_trim(*skb_tmp, skb_len);

			/* backup skb to skb_tmp */
			skb_copy_to_linear_data(*skb_tmp,
				(*skb)->data, skb_len);
			spin_lock_irqsave(&port->rx_skb_list.lock, flags);
			/* dequeue */
			*skb = __skb_dequeue(&port->rx_skb_list);
			if ((*skb) == NULL) {
				CCCI_ERROR_LOG(md_id, UDC,
					"%s:__skb_dequeue fail\n", __func__);
				spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
				return;
			}
			spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
			ccci_udc_deactv
				= (struct ccci_udc_deactv_param_t *)
				(*skb)->data;
			ins_id_tmp = ccci_udc_deactv->udc_inst_id;
			if (ctl->last_state == UDC_HandleHighKick) {
				if (ins_id_tmp == 0) {
					skb_tmp_len = (*skb_tmp)->len;
					skb_tmp1_len = skb_tmp1->len;
					if (skb_tmp_len > skb_tmp1_len)
						skb_put(*skb_tmp,
						skb_tmp_len - skb_tmp1_len);
					else if (skb_tmp_len < skb_tmp1_len)
						skb_trim(*skb_tmp, skb_tmp_len);

					skb_copy_to_linear_data(*skb_tmp,
						skb_tmp1->data, skb_tmp1_len);
				}
				ccci_free_skb(skb_tmp1);
				ctl->last_state = ctl->curr_state;
				atomic_set(&udc_status, UDC_KICKDEACTV);
				ctl->curr_state = UDC_KICKDEACTV;
			} else {
				if (ins_id_tmp == inst_id) {
					ctl->last_state = ctl->curr_state;
					atomic_set(&udc_status,
						UDC_DEACTV_DONE);
					ctl->curr_state = UDC_DEACTV_DONE;
				} else {
					ctl->last_state = ctl->curr_state;
					atomic_set(&udc_status, UDC_KICKDEACTV);
					ctl->curr_state = UDC_KICKDEACTV;
				}
			}
		} else
			goto err;
		break;
	}
	case UDC_DISCARD:
	{
		if (!skb_queue_empty(&port->rx_skb_list)) {
			skb_len = (*skb)->len;
			skb_tmp_len = (*skb_tmp)->len;
			/* resize skb */
			CCCI_DEBUG_LOG(md_id, UDC,
				"skb_len:%d,skb_tmp_len:%d\n",
				skb_len, skb_tmp_len);
			if (skb_len > skb_tmp_len)
				skb_put(*skb_tmp, skb_len - skb_tmp_len);
			else if (skb_len < skb_tmp_len)
				skb_trim(*skb_tmp, skb_len);

			/* backup skb to skb_tmp */
			skb_copy_to_linear_data(*skb_tmp,
				(*skb)->data, skb_len);
			spin_lock_irqsave(&port->rx_skb_list.lock, flags);
			/* dequeue */
			*skb = __skb_dequeue(&port->rx_skb_list);
			spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);

			ctl->last_state = ctl->curr_state;
			atomic_set(&udc_status, UDC_DISC_DONE);
			ctl->curr_state = UDC_DISC_DONE;
		} else
			goto err;
		break;
	}
	default:
		CCCI_ERROR_LOG(md_id, UDC,
			"[Error]Unknown UDC STATUS (0x%08X)\n",
			atomic_read(&udc_status));
		break;
	}
	if (ctl->last_state != ctl->curr_state)
		CCCI_NORMAL_LOG(md_id, UDC,
			"udc_status:from %d to %d by %s\n",
			ctl->last_state, ctl->curr_state, __func__);
	return;
err:
	CCCI_ERROR_LOG(md_id, UDC,
		"udc_status%d:skb list is empty\n", ctl->curr_state);
	atomic_set(&udc_status, UDC_IDLE);
	ctl->curr_state = UDC_IDLE;
}

int udc_actv_handler(struct z_stream_s *zcpr, enum udc_dict_opt_e dic_option,
	unsigned int buffer_size, u32 inst_id)
{
	int ret = 0;
	static struct udc_private_data my_param0, my_param1;

	if (inst_id == 0)
		ret = udc_init(zcpr, &my_param0);
	else if (inst_id == 1)
		ret = udc_init(zcpr, &my_param1);

	ret |= deflateInit2_cb(zcpr, Z_DEFAULT_COMPRESSION,
		Z_DEFLATED, buffer_size, 8, Z_FIXED);

	if (ret < 0) {
		CCCI_ERROR_LOG(-1, UDC,
			"ins%d deflateInit2 fail ret:%d\n", inst_id, ret);
		return ret;
	}
	if (dic_option == UDC_DICT_STD_FOR_SIP) {
		ret |= deflateSetDictionary_cb(zcpr,
			get_dictionary_content(dic_option),
			UDC_DICTIONARY_LENGTH);
		if (ret < 0) {
			CCCI_ERROR_LOG(-1, UDC,
				"ins%d deflateSetDictionary fail ret:%d\n",
				inst_id, ret);
		}
	}

	return ret;
}

/* phase out:when ap recevice deflateEnd cmd, call deflateEnd_cb directly */
int udc_deactv_handler(struct z_stream_s *zcpr, u32 inst_id)
{
	int deflate_end_flag = 0;
	struct udc_comp_req_t *req_des;
	struct udc_comp_rslt_t *rslt_des;
	unsigned int ap_read = 0, ap_write = 0, md_write = 0, md_read = 0;
	struct udc_comp_req_t *req_des_base = NULL;
	struct udc_comp_rslt_t *rslt_des_base = NULL;

	if (inst_id == 0) {
		req_des_base = req_des_0_base;
		rslt_des_base = rslt_des_0_base;
		ap_read = rw_index->md_des_ins0.read;
		md_write = rw_index->md_des_ins0.write;
		md_read = rw_index->ap_resp_ins0.read;
	} else if (inst_id == 1) {
		req_des_base = req_des_1_base;
		rslt_des_base = rslt_des_1_base;
		ap_read = rw_index->md_des_ins1.read;
		md_write = rw_index->md_des_ins1.write;
		md_read = rw_index->ap_resp_ins1.read;
	}

	while (ap_read != md_write) {
		if (inst_id == 0) {
			ap_read = rw_index->md_des_ins0.read;
			ap_write = rw_index->ap_resp_ins0.write;
			md_write = rw_index->md_des_ins0.write;
			md_read = rw_index->ap_resp_ins0.read;
		} else if (inst_id == 1) {
			ap_read = rw_index->md_des_ins1.read;
			ap_write = rw_index->ap_resp_ins1.write;
			md_write = rw_index->md_des_ins1.write;
			md_read = rw_index->ap_resp_ins1.read;
		}

		/* req_des table is only 4kb */
		req_des = req_des_base + ap_read;
		/* md_write must be <=511 */
		ap_read = (ap_read + 1) % 512;
		/* dump req_des */
		CCCI_NORMAL_LOG(-1, UDC,
			"deactv req%d:sdu_idx(%d),sit_type(%d),rst(%d),ap_r(%d),md_w(%d),req_des(%p)\n",
			inst_id, req_des->sdu_idx, req_des->sit_type,
			req_des->rst, ap_read, md_write, req_des);
		if (req_des->con == 0) {
			if ((ap_write+1) == md_read) {
				CCCI_ERROR_LOG(-1, UDC,
					"%s:cmp_rslt table is full\n",
					__func__);
				CCCI_ERROR_LOG(-1, UDC,
					"%s:ins%d md r:%d,md w:%d,ap r:%d,ap w:%d\n",
					__func__, inst_id, ap_read,
					md_write, md_read, ap_write);
				break;
			}
			rslt_des = rslt_des_base + ap_write;
			rslt_des->sdu_idx = req_des->sdu_idx;
			rslt_des->sit_type = req_des->sit_type;
			rslt_des->udc = 0;

			ap_write = (ap_write + 1) % 512;
			/* insure sequential execution */
			mb();
			if (inst_id == 0) {
				rw_index->md_des_ins0.read = ap_read;
				rw_index->ap_resp_ins0.write = ap_write;
			} else if (inst_id == 1) {
				rw_index->md_des_ins1.read = ap_read;
				rw_index->ap_resp_ins1.write = ap_write;
			}
			CCCI_NORMAL_LOG(-1, UDC,
				"deactv rslt%d:sdu_idx(%d),sit_type(%d),udc(%d),rst(%d)\n",
				inst_id, rslt_des->sdu_idx, rslt_des->sit_type,
				rslt_des->udc, rslt_des->rst);
		} else {
			if (inst_id == 0)
				rw_index->md_des_ins0.read = ap_read;
			else if (inst_id == 1)
				rw_index->md_des_ins1.read = ap_read;
		}
	}
	deflate_end_flag = deflateEnd_cb(zcpr);
	udc_deinit(zcpr);

	CCCI_NORMAL_LOG(-1, UDC, "deflateEnd_ins%d,ret:%d\n",
		inst_id, deflate_end_flag);
	if (deflate_end_flag < 0) {
		/* the continuous input is unprocessed,
		 *it maybe return -3
		 */
		if (deflate_end_flag == -3)
			return 0;
		else
			return deflate_end_flag;
	}
	return deflate_end_flag;
}

static void ccci_udc_req_data_dump(u32 inst_id,
	unsigned char *uncomp_data, unsigned int uncomp_len)
{
#ifdef UDC_DATA_DUMP
	int j = 0;

	CCCI_HISTORY_TAG_LOG(-1, UDC,
		"req%d:uncomp_data:%p\n", inst_id, uncomp_data);
	for (j = 0; j < 16; j++) {
		if (j % 16 == 0)
			CCCI_HISTORY_LOG(-1, UDC, "%04X:", j);
		CCCI_MEM_LOG(-1, UDC, " %02X", uncomp_data[j]);
		if (j == 15)
			CCCI_HISTORY_LOG(-1, UDC, "\n");
	}
#endif
}

static void ccci_udc_rslt_data_dump(u32 inst_id,
	unsigned char *comp_data, unsigned int comp_len)
{
#ifdef UDC_DATA_DUMP
	unsigned int ap_read, ap_write, md_read, md_write;
	unsigned int j = 0;

	if (inst_id == 0) {
		ap_read = rw_index->md_des_ins0.read;
		ap_write = rw_index->ap_resp_ins0.write;
		md_read = rw_index->ap_resp_ins0.read;
		md_write = rw_index->md_des_ins0.write;
	} else if (inst_id == 1) {
		ap_read = rw_index->md_des_ins1.read;
		ap_write = rw_index->ap_resp_ins1.write;
		md_read = rw_index->ap_resp_ins1.read;
		md_write = rw_index->md_des_ins1.write;
	}

	CCCI_HISTORY_TAG_LOG(-1, UDC,
		"rst%d:comp_len:%d,comp_data:%p\n",
		inst_id, comp_len, comp_data);
	CCCI_HISTORY_TAG_LOG(-1, UDC,
		"ins%d:md r:%d,md w:%d,ap r:%d,ap w:%d\n",
		inst_id, ap_read, md_write, md_read, ap_write);

	for (j = 0; j < comp_len; j++) {
		if (j % 16 == 0) {
			if (j > 0)
				CCCI_HISTORY_LOG(-1, UDC, "\n%04X:", j);
			else
				CCCI_HISTORY_LOG(-1, UDC, "%04X:", j);
		}
		CCCI_HISTORY_LOG(-1, UDC, " %02X", *(comp_data + j));
		if (j == (comp_len - 1))
			CCCI_HISTORY_LOG(-1, UDC, "\n");
	}
#endif
}

/* <0:full 0:not full */
static int check_cmp_buf(u32 inst_id,
	int max_output_size)
{
	unsigned int ap_read = 0, ap_write = 0, md_read = 0, md_read_len = 0;
	struct udc_comp_req_t *req_des, *req_des_base = NULL;
	struct udc_comp_rslt_t *rslt_des, *rslt_des_base = NULL;

	if (inst_id == 0) {
		req_des_base = req_des_0_base;
		rslt_des_base = rslt_des_0_base;
		ap_read = rw_index->md_des_ins0.read;
		ap_write = rw_index->ap_resp_ins0.write;
		md_read = rw_index->ap_resp_ins0.read;
	} else if (inst_id == 1) {
		req_des_base = req_des_1_base;
		rslt_des_base = rslt_des_1_base;
		ap_read = rw_index->md_des_ins1.read;
		ap_write = rw_index->ap_resp_ins1.write;
		md_read = rw_index->ap_resp_ins1.read;
	} else {
		CCCI_ERROR_LOG(-1, UDC,
			"inst_id is error,rslt_des_base maybe null\n");
		return -1;
	}

	md_read_len = (rslt_des_base + md_read)->cmp_addr
		+ (rslt_des_base + md_read)->cmp_len;
	if (total_comp_size < md_read_len) {
		if ((total_comp_size + max_output_size)
				>= md_read_len) {
			CCCI_NORMAL_LOG(-1, UDC,
				"%s:ins%d cmp_buf full,ap_w:%d,ap_r:%d\n",
				__func__, inst_id, ap_write, md_read);
			CCCI_NORMAL_LOG(-1, UDC,
				"(total_comp_size:%d+max_output_size:%d)>md_read_len:%d\n",
				total_comp_size, max_output_size, md_read_len);
			req_des = req_des_base + ap_read;
			rslt_des = rslt_des_base + ap_write;

			rslt_des->sdu_idx = req_des->sdu_idx;
			rslt_des->sit_type = req_des->sit_type;
			rslt_des->udc = 0;

			/* insure sequential execution */
			mb();
			/* update ap write index */
			ap_write = (ap_write + 1) % 512;
			if (inst_id == 0)
				rw_index->ap_resp_ins0.write = ap_write;
			else if (inst_id == 1)
				rw_index->ap_resp_ins1.write = ap_write;
			return -CMP_BUF_FULL;
		}
	}
	return 0;
}

static int cal_udc_param(struct z_stream_s *zcpr, u32 inst_id,
	int *max_output_size, int *udc_chksum)
{
	struct udc_comp_req_t *req_des_tmp = NULL, *req_des_base = NULL;
	unsigned int ap_read = 0, md_write = 0;
	unsigned int uncomp_len_total = 0;
	int j = 0;

	if (inst_id == 0) {
		req_des_base = req_des_0_base;
		ap_read = rw_index->md_des_ins0.read;
		md_write = rw_index->md_des_ins0.write;
	} else if (inst_id == 1) {
		req_des_base = req_des_1_base;
		ap_read = rw_index->md_des_ins1.read;
		md_write = rw_index->md_des_ins1.write;
	} else {
		CCCI_ERROR_LOG(-1, UDC,
			"inst_id is error\n");
		return -1;
	}

	if (*max_output_size == 0) {
		req_des_tmp = req_des_base + ap_read;
		if (!req_des_tmp) {
			CCCI_ERROR_LOG(-1, UDC,
				"%s:req_des_base&ap_read is null\n",
				__func__);
			return -1;
		}
		if (req_des_tmp->con == 0)
			*max_output_size = deflateBound_cb(zcpr,
			req_des_tmp->seg_len);
		else if (req_des_tmp->con == 1) {
			for (j = 0; ap_read != md_write; j++) {
				req_des_tmp = req_des_base + (ap_read+j)%512;
				uncomp_len_total += req_des_tmp->seg_len;
				if (req_des_tmp->con == 0)
					break;
			}
			*max_output_size =
				deflateBound_cb(zcpr, uncomp_len_total);
			CCCI_DEBUG_LOG(-1, UDC,
				"req_des%d:deflateBound uncomp_len_total:%d,packet_count:%d\n",
				inst_id, uncomp_len_total, j+1);
			/* packet_count > 2*/
			if (j > 1)
				CCCI_ERROR_LOG(-1, UDC,
					"req_des%d:packet_count:%d,md_r:%d,md_w:%d\n",
					inst_id, j+1, ap_read, md_write);
		}
		/* calc chksum before call deflate */
		*udc_chksum = udc_chksum_cb(zcpr);
		CCCI_DEBUG_LOG(-1, UDC,
			"ins%d:max_output_size:%d udc_chksum:%d\n",
			inst_id, *max_output_size, *udc_chksum);
	}

	return 0;
}

int udc_deflate(struct z_stream_s *zcpr, u32 inst_id, u32 con,
	unsigned char *uncomp_data, unsigned int uncomp_len,
	unsigned char *comp_data, unsigned int remain_len)
{
	unsigned long bytes_processed, prev_bytes_processed;
	int deflate_st;
	int comp_len = 0;

	(*zcpr).next_in = uncomp_data;
	(*zcpr).next_out = comp_data;
	(*zcpr).avail_in = uncomp_len;
	(*zcpr).avail_out = remain_len;

	prev_bytes_processed = (*zcpr).total_in;
	deflate_st = deflate_cb(zcpr,
		con ? Z_NO_FLUSH : Z_SYNC_FLUSH);
	if (deflate_st < 0) {
		CCCI_ERROR_LOG(-1, UDC,
			"ins%d:deflate_st:%d\n", inst_id, deflate_st);
		if (deflate_st == Z_BUF_ERROR)
			CCCI_ERROR_LOG(-1, UDC,
				"ins%d zcpr.avail_in:%d,zcpr.avail_out:%d\n",
				inst_id, (*zcpr).avail_in, (*zcpr).avail_out);
		return deflate_st;
	}
	if ((*zcpr).avail_in > 0)
		CCCI_ERROR_LOG(-1, UDC,
			"%d input bytes not processed!\n",
			(*zcpr).avail_in);
	bytes_processed = (*zcpr).total_in - prev_bytes_processed;
	if (bytes_processed != uncomp_len)
		CCCI_ERROR_LOG(-1, UDC,
			"input %d bytes, only process %lu bytes\n",
			uncomp_len, bytes_processed);
	comp_len = udc_GetCmpLen_cb(zcpr, comp_data, (*zcpr).next_out);
	total_comp_size += (*zcpr).next_out - comp_data;
	CCCI_DEBUG_LOG(-1, UDC,
		"ins%d total_comp_size:%d\n",
		inst_id, total_comp_size);

	return comp_len;
}

int udc_kick_handler(struct port_t *port, struct z_stream_s *zcpr,
	u32 inst_id, unsigned char **comp_data)
{
	int md_id = port->md_id;
	int ret = 0;
	static int max_output_size;
	int max_packet_size = MAX_PACKET_SIZE;
	static unsigned int udc_chksum;
	static unsigned int is_rst;
	unsigned int ap_read = 0, ap_write = 0, md_read = 0, md_write = 0;
	struct udc_comp_req_t *req_des = NULL, *req_des_base = NULL;
	struct udc_comp_rslt_t *rslt_des = NULL, *rslt_des_base = NULL;
	unsigned int uncomp_len, comp_len = 0;
	unsigned int remain_len;
	unsigned char *uncomp_data = NULL;
	/* reserved 8k for reduce memcpy op */
	unsigned int rsvd_len = MAX_PACKET_SIZE - 8*1024;

	if (inst_id == 0) {
		req_des_base = req_des_0_base;
		rslt_des_base = rslt_des_0_base;
		ap_read = rw_index->md_des_ins0.read;
		ap_write = rw_index->ap_resp_ins0.write;
		md_read = rw_index->ap_resp_ins0.read;
		md_write = rw_index->md_des_ins0.write;
	} else if (inst_id == 1) {
		req_des_base = req_des_1_base;
		rslt_des_base = rslt_des_1_base;
		ap_read = rw_index->md_des_ins1.read;
		ap_write = rw_index->ap_resp_ins1.write;
		md_read = rw_index->ap_resp_ins1.read;
		md_write = rw_index->md_des_ins1.write;
	} else {
		CCCI_ERROR_LOG(md_id, UDC,
			"inst_id is error\n");
		return -1;
	}

	/* check if cmp_rslt table is full */
	if ((ap_write+1) == md_read) {
		CCCI_ERROR_LOG(md_id, UDC,
			"cmp_rslt table is full\n");
		CCCI_ERROR_LOG(md_id, UDC,
			"ins%d:md r:%d,md w:%d,ap r:%d,ap w:%d\n",
			inst_id, ap_read, md_write, md_read, ap_write);
		return -CMP_RSLT_FULL;
	}
	/* req_des table is only 4kb */
	req_des = req_des_base + ap_read;
	if (req_des == NULL) {
		CCCI_ERROR_LOG(md_id, UDC, "invalid req_des");
		return -CMP_INST_ID_ERR;
	}
	/* dump req_des */
	CCCI_NORMAL_LOG(md_id, UDC,
		"req%d:sdu_idx(%d),buf_type(%d),seg_len(%d),phy_offset(%#x)\n",
		inst_id, req_des->sdu_idx, req_des->buf_type,
		req_des->seg_len, req_des->seg_phy_addr);
	uncomp_len = req_des->seg_len;
	if (req_des->buf_type == 0)
		uncomp_data = (unsigned char *)
			((unsigned long)uncomp_data_buf_base +
			req_des->seg_phy_addr);
	else if (req_des->buf_type == 1)
		uncomp_data = (unsigned char *)
			((unsigned long)uncomp_cache_data_base +
			req_des->seg_phy_addr);

	ccci_udc_req_data_dump(inst_id, uncomp_data, uncomp_len);

	if (req_des->rst == 1) {
		is_rst = 1;
		CCCI_NORMAL_LOG(md_id, UDC,
			"kick req%d:rst(%d),sdu_idx(%d),md r(%d)\n",
			inst_id, req_des->rst, req_des->sdu_idx, ap_read);
		deflateReset_cb(zcpr);
	}

	/* check max_output_size&udc_chksum */
	cal_udc_param(zcpr, inst_id, &max_output_size, &udc_chksum);

	/* md_write must be <=511 */
	ap_read = (ap_read + 1) % 512;

	if (inst_id == 0)
		rw_index->md_des_ins0.read = ap_read;
	else if (inst_id == 1)
		rw_index->md_des_ins1.read = ap_read;

	/* deinit comp_data to reduce memcpy */
	if (total_comp_size >= rsvd_len ||
		(total_comp_size + max_output_size) > max_packet_size) {
		CCCI_NORMAL_LOG(md_id, UDC,
			"ins%d total_cmp_size:%d,deflateBound:%d,rsvd_len:%d\n",
			inst_id, total_comp_size, max_output_size, rsvd_len);
		*comp_data = comp_data_buf_base;
		total_comp_size = 0;
	}
	/* cal md_read_len for check if cmp_buf is full or not */
	ret = check_cmp_buf(inst_id, max_output_size);
	if (ret == -CMP_BUF_FULL)
		return ret;
	remain_len = Min(max_output_size,
		max_packet_size - total_comp_size);

	ret = udc_deflate(zcpr, inst_id, req_des->con,
		uncomp_data, uncomp_len, *comp_data, remain_len);
	if (ret < 0)
		return ret;
	comp_len += ret;

	if (req_des->con == 0) {
		rslt_des = rslt_des_base + ap_write;
		rslt_des->sdu_idx = req_des->sdu_idx;
		rslt_des->sit_type = req_des->sit_type;
		rslt_des->udc = 1;
		rslt_des->rst = is_rst;
		rslt_des->cksm = udc_chksum;
		rslt_des->cmp_addr = *comp_data - comp_data_buf_base;
		rslt_des->cmp_len = comp_len;

		CCCI_NORMAL_LOG(md_id, UDC,
			"rslt%d:sdu_idx(%d),rst(%d),comp_len(%d),offset(%d),chsm(%d),ap_write(%d)\n",
			inst_id, rslt_des->sdu_idx, rslt_des->rst,
			comp_len, rslt_des->cmp_addr, rslt_des->cksm, ap_write);

		if (comp_len == 0) {
			/* if no check comp_len,ke will happen */
			CCCI_ERROR_LOG(md_id, UDC,
				"kick%d comp_len = 0\n", inst_id);
			return -CMP_ZERO_LEN;
		}

		comp_len = 0;
		is_rst = 0;
		max_output_size = 0;
		/* update ap write index */
		ap_write = (ap_write + 1) % 512;
		/* insure sequential execution */
		mb();
		if (inst_id == 0)
			rw_index->ap_resp_ins0.write = ap_write;
		else if (inst_id == 1)
			rw_index->ap_resp_ins1.write = ap_write;
		ccci_udc_rslt_data_dump(inst_id,
			*comp_data, rslt_des->cmp_len);
	}
	*comp_data = (*zcpr).next_out;

	return ret;
}

int udc_restore_skb(struct port_t *port,
	struct udc_state_ctl *ctl,
	struct sk_buff **skb_tmp, struct sk_buff **skb)
{
	struct ccci_udc_actv_param_t *ccci_udc_actv;
	int ret = 0;
	int md_id = port->md_id;

	ctl->last_state = ctl->curr_state;
	/* ctl->curr_state = atomic_read(&udc_status); */

	switch (ctl->curr_state) {
	case UDC_HandleHighKick:
	case UDC_DISC_DONE:
	case UDC_KICKDEACTV:
	case UDC_DEACTV_DONE:
	{
		*skb = ccci_alloc_skb(sizeof(*ccci_udc_actv), 1, 1);
		if (unlikely(!(*skb))) {
			CCCI_ERROR_LOG(md_id, UDC,
				"%s:alloc skb fail\n", __func__);
			return ret;
		}

		skb_copy_to_linear_data(*skb,
			(*skb_tmp)->data, (*skb_tmp)->len);
		skb_put(*skb, (*skb_tmp)->len);

		ctl->last_state = ctl->curr_state;
		atomic_set(&udc_status, UDC_IDLE);
		ctl->curr_state = UDC_IDLE;
		ret = 1;
		break;
	}
	case UDC_IDLE:
	case UDC_HighKick:
	case UDC_DISCARD:
	case UDC_DEACTV:
		break;
	default:
		CCCI_ERROR_LOG(md_id, UDC,
			"[Error]%s:Unknown UDC STATUS (0x%08X)\n",
			__func__, atomic_read(&udc_status));
		break;
	}
	if (ctl->last_state != ctl->curr_state)
		CCCI_NORMAL_LOG(md_id, UDC,
			"udc_status:from %d to %d by %s\n",
			ctl->last_state, ctl->curr_state, __func__);
	return ret;
}

void udc_cmd_handler(struct port_t *port, struct sk_buff *skb)
{
	int md_id = port->md_id;
	struct ccci_smem_region *region;
	int ret = 0;
	unsigned int udc_cmd = 0;
	struct udc_state_ctl *ctl;
	static unsigned char *comp_data;
	struct ccci_udc_deactv_param_t *ccci_udc_deactv;
	struct ccci_udc_disc_param_t *ccci_udc_disc;
	struct sk_buff *skb_tmp;
	struct ccci_udc_actv_param_t *ccci_udc_actv;
	static struct z_stream_s zcpr0, zcpr1;
	int deflate_end_flag = 0;
	unsigned int md_write = 0, ap_read = 0;

	skb_tmp = ccci_alloc_skb(sizeof(*ccci_udc_actv), 1, 1);
	if (!skb_tmp) {
		CCCI_ERROR_LOG(md_id, UDC,
			"%s:alloc skb_tmp fail\n", __func__);
		return;
	}

	ctl = kzalloc(sizeof(struct udc_state_ctl), GFP_KERNEL);
	if (ctl == NULL) {
		CCCI_ERROR_LOG(md_id, UDC,
			"%s:kzalloc ctl fail\n", __func__);
		return;
	}

	ccci_udc_actv = (struct ccci_udc_actv_param_t *)skb->data;
	udc_cmd = ccci_udc_actv->udc_cmd;
	CCCI_DEBUG_LOG(md_id, UDC,
		"%s++ udc_cmd:%d\n", __func__, udc_cmd);

	switch (udc_cmd) {
	case UDC_CMD_ACTV:
	{
		unsigned int buffer_size = ccci_udc_actv->buf_sz;
		enum udc_dict_opt_e dic_option = ccci_udc_actv->dict_opt;
		unsigned int inst_id  = ccci_udc_actv->udc_inst_id;

		CCCI_NORMAL_LOG(md_id, UDC,
			"udc_actv ins%d:cmd:%d,buf_sz:%d,dict_opt:%d\n",
			inst_id, udc_cmd, buffer_size, dic_option);

		if (inst_id == 0)
			ret = udc_actv_handler(&zcpr0, dic_option,
					buffer_size, inst_id);
		else if (inst_id == 1)
			ret = udc_actv_handler(&zcpr1, dic_option,
					buffer_size, inst_id);
		if (ret < 0)
			goto end;
		/* get sharememory info */
		region = ccci_md_get_smem_by_user_id(md_id,
					SMEM_USER_RAW_UDC_DATA);
		if (region) {
			uncomp_data_buf_base = (unsigned char *)
				region->base_ap_view_vir;
			comp_data_buf_base = (unsigned char *)
				(region->base_ap_view_vir + COMP_DATA_OFFSET);
			rw_index = (struct ap_md_rw_index *)
				(region->base_ap_view_vir + 0x500000);
			comp_data = comp_data_buf_base;
			CCCI_NORMAL_LOG(md_id, UDC,
				"base_md_view_phy:0x%lx,base_ap_view_phy:0x%lx\n",
				(unsigned long)region->base_md_view_phy,
				(unsigned long)region->base_ap_view_phy);
			CCCI_NORMAL_LOG(md_id, UDC,
				"uncomp_base:%p,comp_base:%p\n",
				uncomp_data_buf_base, comp_data_buf_base);
		} else
			CCCI_ERROR_LOG(md_id, UDC,
				"can not find region:SMEM_USER_RAW_UDC_DATA\n");

		region = ccci_md_get_smem_by_user_id(md_id,
					SMEM_USER_RAW_UDC_DESCTAB);
		if (region) {
			uncomp_cache_data_base =
				(unsigned char *)region->base_ap_view_vir;
			/* cmp_req and cmp_rslt offset:48k */
			req_des_0_base = (struct udc_comp_req_t *)
				(region->base_ap_view_vir + UDC_UNCMP_CACHE_BUF_SZ);
			rslt_des_0_base = (struct udc_comp_rslt_t *)
				(region->base_ap_view_vir + UDC_UNCMP_CACHE_BUF_SZ + 0x1000);
			req_des_1_base = (struct udc_comp_req_t *)
				(region->base_ap_view_vir + UDC_UNCMP_CACHE_BUF_SZ + 0x2000);
			rslt_des_1_base = (struct udc_comp_rslt_t *)
				(region->base_ap_view_vir + UDC_UNCMP_CACHE_BUF_SZ + 0x3000);
			CCCI_NORMAL_LOG(md_id, UDC, "uncomp_cache_base:%p\n",
				uncomp_cache_data_base);
		} else
			CCCI_ERROR_LOG(md_id, UDC,
				"can not find region:SMEM_USER_RAW_UDC_DESCTAB\n");
		break;
	}
	case UDC_CMD_DEACTV:
	{
		unsigned int inst_id;

deactive_exit:
		ccci_udc_deactv =
			(struct ccci_udc_deactv_param_t *)skb->data;

		udc_cmd = ccci_udc_deactv->udc_cmd;
		inst_id = ccci_udc_deactv->udc_inst_id;
		CCCI_NORMAL_LOG(md_id, UDC,
			"deactv ins%d:udc_cmd:%d\n",
			inst_id, udc_cmd);

		if (inst_id == 0) {
			/* ret = udc_deactv_handler(&zcpr0, inst_id); */
			deflate_end_flag = deflateEnd_cb(&zcpr0);
			udc_deinit(&zcpr0);

		} else if (inst_id == 1) {
			/* ret = udc_deactv_handler(&zcpr1, inst_id); */
			deflate_end_flag = deflateEnd_cb(&zcpr1);
			udc_deinit(&zcpr1);
		}

		/* the continuous input is unprocessed, it maybe return -3 */
		if (deflate_end_flag < 0 && deflate_end_flag != -3) {
			ret = deflate_end_flag;
			CCCI_ERROR_LOG(md_id, UDC, "deflateEnd_ins%d,ret:%d\n",
				inst_id, deflate_end_flag);
		}

		ctl->curr_state = atomic_read(&udc_status);
		if (ctl->curr_state == UDC_DEACTV ||
			ctl->curr_state == UDC_DEACTV_DONE) {
			atomic_set(&udc_status, UDC_IDLE);
			ctl->last_state = ctl->curr_state;
			ctl->curr_state = UDC_IDLE;
		}
		break;
	}
	case UDC_CMD_DISC:
	{
		unsigned int inst_id;
		unsigned int new_req_r;

discard_req:
		ccci_udc_disc =
			(struct ccci_udc_disc_param_t *)skb->data;
		udc_cmd = ccci_udc_disc->udc_cmd;
		new_req_r = ccci_udc_disc->new_req_r;
		inst_id = ccci_udc_disc->udc_inst_id;
		CCCI_NORMAL_LOG(md_id, UDC,
			"disc ins%d:udc_cmd:%d,new_req_r:%d\n",
			inst_id, udc_cmd, new_req_r);

		if (inst_id == 0) {
			ap_read = rw_index->md_des_ins0.read;
			rw_index->md_des_ins0.read =
				ccci_udc_disc->new_req_r;
			CCCI_NORMAL_LOG(md_id, UDC,
				"ins%d update ap read:from %d to %d\n",
				inst_id, ap_read, rw_index->md_des_ins0.read);
		} else if (inst_id == 1) {
			ap_read = rw_index->md_des_ins1.read;
			rw_index->md_des_ins1.read =
				ccci_udc_disc->new_req_r;
			CCCI_NORMAL_LOG(md_id, UDC,
				"ins%d update ap read:from %d to %d\n",
				inst_id, ap_read, rw_index->md_des_ins1.read);
		}
		ctl->curr_state = atomic_read(&udc_status);
		if (ctl->curr_state == UDC_DISCARD) {
			atomic_set(&udc_status, UDC_IDLE);
			ctl->last_state = ctl->curr_state;
			ctl->curr_state = UDC_IDLE;
		}
		break;
	}
	case UDC_CMD_KICK:
	{
		unsigned int inst_id, exp_timer;
		struct udc_comp_req_t *req_des, *req_des_base;
		struct ccci_udc_kick_param_t *ccci_udc_kick;

retry_kick:
		ccci_udc_kick =
			(struct ccci_udc_kick_param_t *)skb->data;
		inst_id = ccci_udc_kick->udc_inst_id;
		udc_cmd = ccci_udc_kick->udc_cmd;
		/* to do exp_timer does not work now */
		exp_timer = ccci_udc_kick->exp_tmr;

		CCCI_NORMAL_LOG(md_id, UDC,
			"kick ins%d:udc_cmd:%d,exp_timer:%d\n",
			inst_id, udc_cmd, exp_timer);
		if (inst_id == 0) {
			req_des_base = req_des_0_base;
			ap_read = rw_index->md_des_ins0.read;
			md_write = rw_index->md_des_ins0.write;
		} else if (inst_id == 1) {
			req_des_base = req_des_1_base;
			ap_read = rw_index->md_des_ins1.read;
			md_write = rw_index->md_des_ins1.write;
		}
		ctl->curr_state = atomic_read(&udc_status);

		while (ap_read != md_write) {
			if (inst_id == 0) {
				req_des = req_des_base + ap_read;
				ret = udc_kick_handler(port, &zcpr0,
						inst_id, &comp_data);
				if (ret < 0) {
					CCCI_ERROR_LOG(port->md_id, UDC,
					"udc kick fail ret:%d!!\n", ret);
					goto end;
				}
				if (req_des->con == 0) {
					udc_cmd_check(port, &skb_tmp,
						&skb, inst_id, ctl);
					if (ctl->curr_state ==
						UDC_DEACTV_DONE ||
						ctl->curr_state ==
						UDC_KICKDEACTV) {
						CCCI_NORMAL_LOG(md_id, UDC,
						"ins%d:goto deactive_exit\n",
						inst_id);
						goto deactive_exit;
					} else if (ctl->curr_state ==
						UDC_DISC_DONE) {
						CCCI_NORMAL_LOG(md_id, UDC,
						"ins%d:goto discard_req\n",
						inst_id);
						goto discard_req;
					}
				}
				/* insure sequential execution */
				/* mb(); */
				ap_read = rw_index->md_des_ins0.read;
				md_write = rw_index->md_des_ins0.write;
			} else if (inst_id == 1) {
				req_des = req_des_base + ap_read;
				ret = udc_kick_handler(port, &zcpr1,
						inst_id, &comp_data);
				if (ret < 0) {
					CCCI_ERROR_LOG(port->md_id, UDC,
					"udc kick fail ret:%d!!\n", ret);
					goto end;
				}
				if (req_des->con == 0) {
					udc_cmd_check(port, &skb_tmp,
						&skb, inst_id, ctl);
					if (ctl->curr_state ==
						UDC_HandleHighKick) {
						CCCI_NORMAL_LOG(md_id, UDC,
						"ins%d:goto retry_kick\n",
						inst_id);
						goto retry_kick;
					} else if (ctl->curr_state ==
						UDC_DEACTV_DONE ||
						ctl->curr_state ==
						UDC_KICKDEACTV) {
						CCCI_NORMAL_LOG(md_id, UDC,
						"ins%d:goto deactive_exit\n",
						inst_id);
						goto deactive_exit;
					} else if (ctl->curr_state ==
						UDC_DISC_DONE) {
						CCCI_NORMAL_LOG(md_id, UDC,
						"ins%d:goto discard_req\n",
						inst_id);
						goto discard_req;
					}
				}

				/* insure sequential execution */
				/* mb(); */
				ap_read = rw_index->md_des_ins1.read;
				md_write = rw_index->md_des_ins1.write;
			}
		}
		break;
	}
	default:
		CCCI_ERROR_LOG(md_id, UDC,
			"[Error]Unknown Operation ID (0x%08X)\n",
			ccci_udc_actv->udc_cmd);
		break;
	}
end:
	/* resp_to_md */
	ret = udc_resp_msg_to_md(port, skb, ret);
	if (ret < 0)
		CCCI_ERROR_LOG(port->md_id, UDC,
			"send udc msg to md fail ret:%d!!\n", ret);
	CCCI_DEBUG_LOG(md_id, UDC,
		"%s-- udc_cmd:%d\n", __func__, udc_cmd);
	/* dump read write index */
	CCCI_NORMAL_LOG(md_id, UDC,
		"ins0:md rw:%d %d,ap rw:%d %d,ins1:md rw:%d %d,ap rw:%d %d\n",
		rw_index->md_des_ins0.read, rw_index->md_des_ins0.write,
		rw_index->ap_resp_ins0.read, rw_index->ap_resp_ins0.write,
		rw_index->md_des_ins1.read, rw_index->md_des_ins1.write,
		rw_index->ap_resp_ins1.read, rw_index->ap_resp_ins1.write);
	if (udc_restore_skb(port, ctl, &skb_tmp, &skb)) {
		CCCI_NORMAL_LOG(md_id, UDC,
			"restore_skb:goto retry_kick\n");
		goto retry_kick;
	}
	ccci_free_skb(skb_tmp);
	kfree(ctl);
}

static int port_udc_init(struct port_t *port)
{
	CCCI_DEBUG_LOG(port->md_id, PORT,
		"kernel port %s is initializing\n", port->name);
	port->skb_handler = &udc_cmd_handler;
	port->private_data = kthread_run(port_kthread_handler,
		port, "%s", port->name);
	port->rx_length_th = MAX_QUEUE_LENGTH;
	port->skb_from_pool = 1;
	return 0;
}

struct port_ops ccci_udc_port_ops = {
	.init = &port_udc_init,
	.recv_skb = &port_recv_skb,
};

