// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Communications Inc.
 *
 * Filename : sdiohal_tx.c
 * Abstract : This file is a implementation for wcn sdio hal function
 */
#include <linux/sched/clock.h>
#include "sdiohal.h"

int sdiohal_tx_data_list_send(struct sdiohal_list_t *data_list)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct mbuf_t *mbuf_node;
	unsigned int i;
	int ret;

	sdiohal_sdma_enter();
	sdiohal_list_check(data_list, __func__, SDIOHAL_WRITE);
	mbuf_node = data_list->mbuf_head;
	for (i = 0; i < data_list->node_num;
		i++, mbuf_node = mbuf_node->next) {
		if (p_data->send_buf.used_len + sizeof(struct bus_puh_t) +
		    SDIOHAL_ALIGN_4BYTE(mbuf_node->len) >
		    SDIOHAL_TX_SENDBUF_LEN) {
			sdiohal_tx_set_eof(&p_data->send_buf, p_data->eof_buf);
			ret = sdiohal_sdio_pt_write(p_data->send_buf.buf,
						    SDIOHAL_ALIGN_BLK(
						    p_data->send_buf.used_len));
			if (ret)
				pr_err("err 1,type:%d subtype:%d num:%d\n",
				       data_list->type, data_list->subtype,
				       data_list->node_num);
			p_data->send_buf.used_len = 0;
		}
		sdiohal_tx_packer(&p_data->send_buf, data_list, mbuf_node);
	}
	sdiohal_tx_set_eof(&p_data->send_buf, p_data->eof_buf);

	sdiohal_tx_list_denq(data_list);
	ret = sdiohal_sdio_pt_write(p_data->send_buf.buf,
				    SDIOHAL_ALIGN_BLK(
				    p_data->send_buf.used_len));
	if (ret)
		pr_err("err 2,type:%d subtype:%d num:%d\n",
		       data_list->type, data_list->subtype,
		       data_list->node_num);

	p_data->send_buf.used_len = 0;
	sdiohal_sdma_leave();

	return ret;
}

int sdiohal_tx_thread(void *data)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct sdiohal_list_t data_list;
	struct sched_param param;
	struct timespec64 tm_begin, tm_end;
	static long time_total_ns;
	static int times_count;
	int ret;

	set_user_nice(current, -20);
	param.sched_priority = SDIO_TX_TASK_PRIO;
	sched_setscheduler(current, SCHED_FIFO, &param);

	while (1) {
		/* Wait the semaphore */
		sdiohal_tx_down();
		if (p_data->exit_flag)
			break;

		tm_enter_tx_thread = local_clock();
		ktime_get_real_ts64(&p_data->tm_end_sch);
		sdiohal_pr_perf("tx sch time:%ld\n",
				(long)(timespec_to_ns(&p_data->tm_end_sch) -
				timespec_to_ns(&p_data->tm_begin_sch)));

		sdiohal_lock_tx_ws();
		sdiohal_resume_check();

		/* wakeup cp */
		sdiohal_cp_tx_wakeup(PACKER_TX);

		while (!sdiohal_is_tx_list_empty()) {
			ktime_get_real_ts64(&tm_begin);

			sdiohal_tx_find_data_list(&data_list);
			p_data->data_list_tx = &data_list;

			if (p_data->adma_tx_enable) {
				ret = sdiohal_adma_pt_write(&data_list);
				if (ret)
					pr_err("sdio adma write fail ret:%d\n",
					       ret);
				sdiohal_tx_list_denq(&data_list);
			} else
				sdiohal_tx_data_list_send(&data_list);
			p_data->data_list_tx = NULL;

			ktime_get_real_ts64(&tm_end);
			time_total_ns += timespec64_to_ns(&tm_end) -
					timespec64_to_ns(&tm_begin);
			times_count++;
			if (!(times_count % PERFORMANCE_COUNT)) {
				sdiohal_pr_perf("tx list avg time:%ld\n",
						(time_total_ns /
						PERFORMANCE_COUNT));
				time_total_ns = 0;
				times_count = 0;
			}
		}

		tm_exit_tx_thread = local_clock();
		sdiohal_cp_tx_sleep(PACKER_TX);
		sdiohal_unlock_tx_ws();
	}

	return 0;
}

int sdiohal_remove_datalist_invalid_data(struct mchn_ops_t *ops, struct sdiohal_list_t *data_list)
{
	unsigned int type, subtype;
	struct mbuf_t *mbuf_node, *mbuf_node_next;
	int inout = 1;
	int del_node_num = 0;
	int node_num = data_list->node_num;
	struct bus_puh_t *puh = NULL;

	sdiohal_channel_to_hwtype(inout, ops->channel, &type, &subtype);
	mbuf_node = data_list->mbuf_head;
	mbuf_node_next = mbuf_node;
	do {
		puh = (struct bus_puh_t *)mbuf_node_next->buf;
		if (puh->subtype == subtype) {
			del_node_num++;
			if (mbuf_node_next == data_list->mbuf_head) {
				data_list->mbuf_head = mbuf_node->next;
				mbuf_node = data_list->mbuf_head;
				mbuf_node_next = mbuf_node;
			} else {
				mbuf_node_next = mbuf_node_next->next;
				mbuf_node->next = mbuf_node_next;
			}
			data_list->node_num--;
		} else {
			mbuf_node = mbuf_node_next;
			mbuf_node_next = mbuf_node_next->next;
		}
		node_num--;
	} while (node_num && mbuf_node_next);
	data_list->mbuf_tail = mbuf_node;

	return del_node_num;
}
