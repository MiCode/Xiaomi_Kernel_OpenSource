/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

#include "fm_typedef.h"
#include "fm_dbg.h"
#include "fm_err.h"
#include "fm_stdlib.h"
#include "fm_link.h"
#include "fm_reg_utils.h"

static struct fm_link_event *link_event;

static struct fm_trace_fifo_t *cmd_fifo;

static struct fm_trace_fifo_t *evt_fifo;

signed int fm_link_setup(void *data)
{
	signed int ret = 0;

	link_event = fm_zalloc(sizeof(struct fm_link_event));
	if (!link_event) {
		WCN_DBG(FM_ALT | LINK, "fm_zalloc(fm_link_event) -ENOMEM\n");
		return -1;
	}

	link_event->ln_event = fm_flag_event_create("ln_evt");

	if (!link_event->ln_event) {
		WCN_DBG(FM_ALT | LINK, "create mt6620_ln_event failed\n");
		fm_free(link_event);
		return -1;
	}

	fm_flag_event_get(link_event->ln_event);

	WCN_DBG(FM_NTC | LINK, "fm link setup\n");

	cmd_fifo = fm_trace_fifo_create("cmd_fifo");
	if (!cmd_fifo) {
		WCN_DBG(FM_ALT | LINK, "create cmd_fifo failed\n");
		ret = -1;
		goto failed;
	}

	evt_fifo = fm_trace_fifo_create("evt_fifo");
	if (!evt_fifo) {
		WCN_DBG(FM_ALT | LINK, "create evt_fifo failed\n");
		ret = -1;
		goto failed;
	}

	if (fm_wcn_ops.ei.wmt_msgcb_reg)
		fm_wcn_ops.ei.wmt_msgcb_reg(data);

	return 0;

failed:
	fm_trace_fifo_release(evt_fifo);
	fm_trace_fifo_release(cmd_fifo);
	if (link_event) {
		fm_flag_event_put(link_event->ln_event);
		fm_free(link_event);
	}

	return ret;
}

signed int fm_link_release(void)
{

	fm_trace_fifo_release(evt_fifo);
	fm_trace_fifo_release(cmd_fifo);
	if (link_event) {
		fm_flag_event_put(link_event->ln_event);
		fm_free(link_event);
	}
	WCN_DBG(FM_NTC | LINK, "fm link release\n");
	return 0;
}

/*
 * fm_ctrl_rx
 * the low level func to read a rigister
 * @addr - rigister address
 * @val - the pointer of target buf
 * If success, return 0; else error code
 */
signed int fm_ctrl_rx(unsigned char addr, unsigned short *val)
{
	return 0;
}

/*
 * fm_ctrl_tx
 * the low level func to write a rigister
 * @addr - rigister address
 * @val - value will be writed in the rigister
 * If success, return 0; else error code
 */
signed int fm_ctrl_tx(unsigned char addr, unsigned short val)
{
	return 0;
}

/*
 * fm_cmd_tx() - send cmd to FM firmware and wait event
 * @buf - send buffer
 * @len - the length of cmd
 * @mask - the event flag mask
 * @	cnt - the retry conter
 * @timeout - timeout per cmd
 * Return 0, if success; error code, if failed
 */
signed int fm_cmd_tx(unsigned char *buf, unsigned short len, signed int mask, signed int cnt, signed int timeout,
		 signed int (*callback)(struct fm_res_ctx *result))
{
	signed int ret_time = 0;
	struct task_struct *task = current;
	struct fm_trace_t trace;

	if ((buf == NULL) || (len < 0) || (mask == 0)
	    || (cnt > SW_RETRY_CNT_MAX) || (timeout > SW_WAIT_TIMEOUT_MAX)) {
		WCN_DBG(FM_ERR | LINK, "cmd tx, invalid para\n");
		return -FM_EPARA;
	}

	FM_EVENT_CLR(link_event->ln_event, mask);

#ifdef FM_TRACE_ENABLE
	trace.type = buf[0];
	trace.opcode = buf[1];
	trace.len = len - 4;
	trace.tid = (signed int) task->pid;
	fm_memset(trace.pkt, 0, FM_TRACE_PKT_SIZE);
	fm_memcpy(trace.pkt, &buf[4], (trace.len > FM_TRACE_PKT_SIZE) ? FM_TRACE_PKT_SIZE : trace.len);
#endif


#ifdef FM_TRACE_ENABLE
	if (true == FM_TRACE_FULL(cmd_fifo))
		FM_TRACE_OUT(cmd_fifo, NULL);

	FM_TRACE_IN(cmd_fifo, &trace);
#endif

	/* send cmd to FM firmware */
	if (fm_wcn_ops.ei.stp_send_data)
		ret_time = fm_wcn_ops.ei.stp_send_data(buf, len);
	if (ret_time <= 0) {
		WCN_DBG(FM_EMG | LINK, "send data over stp failed[%d]\n", ret_time);
		return -FM_ELINK;
	}
	/* wait the response form FM firmware */
	ret_time = FM_EVENT_WAIT_TIMEOUT(link_event->ln_event, mask, timeout);

	if (!ret_time) {
		if (cnt-- > 0) {
			WCN_DBG(FM_WAR | LINK, "wait event timeout, [retry_cnt=%d], pid=%d\n", cnt, task->pid);
			fm_print_cmd_fifo();
			fm_print_evt_fifo();
		} else
			WCN_DBG(FM_ALT | LINK, "fatal error, SW retry failed, reset HW\n");

		return -FM_EFW;
	}

	FM_EVENT_CLR(link_event->ln_event, mask);

	if (callback)
		callback(&link_event->result);

	return 0;
}

signed int fm_event_parser(signed int(*rds_parser) (struct rds_rx_t *, signed int))
{
	signed int len;
	signed int i = 0;
	unsigned char opcode = 0;
	unsigned short length = 0;
	unsigned char ch;
	unsigned char rx_buf[RX_BUF_SIZE + 10] = { 0 };	/* the 10 bytes are protect gaps */
	unsigned int rx_len = 0;

	static enum fm_task_parser_state state = FM_TASK_RX_PARSER_PKT_TYPE;
	struct fm_trace_t trace;
	struct task_struct *task = current;

	if (fm_wcn_ops.ei.stp_recv_data)
		len = fm_wcn_ops.ei.stp_recv_data(rx_buf, RX_BUF_SIZE);
	WCN_DBG_LIMITED(FM_DBG | LINK, "[len=%d],[CMD=0x%02x 0x%02x 0x%02x 0x%02x]\n", len, rx_buf[0],
		rx_buf[1], rx_buf[2], rx_buf[3]);

	while (i < len) {
		ch = rx_buf[i];

		switch (state) {
		case FM_TASK_RX_PARSER_PKT_TYPE:

			if (ch == FM_TASK_EVENT_PKT_TYPE) {
				if ((i + 5) < RX_BUF_SIZE) {
					if (i != 0) {
						WCN_DBG(FM_DBG | LINK,
							"0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
							rx_buf[i], rx_buf[i + 1], rx_buf[i + 2],
							rx_buf[i + 3], rx_buf[i + 4], rx_buf[i + 5]);
					}
				} else {
					WCN_DBG(FM_DBG | LINK, "0x%02x 0x%02x\n", rx_buf[i], rx_buf[i + 1]);
				}

				state = FM_TASK_RX_PARSER_OPCODE;
			} else {
				WCN_DBG(FM_ALT | LINK, "event pkt type error (rx_buf[%d] = 0x%02x)\n", i, ch);
			}

			i++;
			break;

		case FM_TASK_RX_PARSER_OPCODE:
			i++;
			opcode = ch;
			state = FM_TASK_RX_PARSER_PKT_LEN_1;
			break;

		case FM_TASK_RX_PARSER_PKT_LEN_1:
			i++;
			length = ch;
			state = FM_TASK_RX_PARSER_PKT_LEN_2;
			break;

		case FM_TASK_RX_PARSER_PKT_LEN_2:
			i++;
			length |= (unsigned short) (ch << 0x8);

#ifdef FM_TRACE_ENABLE
			trace.type = FM_TASK_EVENT_PKT_TYPE;
			trace.opcode = opcode;
			trace.len = length;
			trace.tid = (signed int) task->pid;
			fm_memset(trace.pkt, 0, FM_TRACE_PKT_SIZE);
			rx_len = (length > FM_TRACE_PKT_SIZE) ? FM_TRACE_PKT_SIZE : length;
			rx_len = (rx_len > (sizeof(rx_buf) - i)) ? sizeof(rx_buf) - i : rx_len;
			fm_memcpy(trace.pkt, &rx_buf[i], rx_len);

			if (true == FM_TRACE_FULL(evt_fifo))
				FM_TRACE_OUT(evt_fifo, NULL);

			FM_TRACE_IN(evt_fifo, &trace);
#endif
			if (length > 0) {
				state = FM_TASK_RX_PARSER_PKT_PAYLOAD;
			} else if (opcode == CSPI_WRITE_OPCODE) {
				state = FM_TASK_RX_PARSER_PKT_TYPE;
				FM_EVENT_SEND(link_event->ln_event, FLAG_CSPI_WRITE);
			} else {
				state = FM_TASK_RX_PARSER_PKT_TYPE;
				FM_EVENT_SEND(link_event->ln_event, (1 << opcode));
			}

			break;

		case FM_TASK_RX_PARSER_PKT_PAYLOAD:

			switch (opcode) {
			case FM_TUNE_OPCODE:

				if ((length == 1) && (rx_buf[i] == 1))
					FM_EVENT_SEND(link_event->ln_event, FLAG_TUNE_DONE);

				break;

			case FM_SOFT_MUTE_TUNE_OPCODE:

				if (length >= 2) {
					fm_memcpy(link_event->result.cqi, &rx_buf[i],
						  (length > FM_CQI_BUF_SIZE) ? FM_CQI_BUF_SIZE : length);
					FM_EVENT_SEND(link_event->ln_event, FLAG_SM_TUNE);
				}
				break;

			case FM_SEEK_OPCODE:

				if ((i + 1) < RX_BUF_SIZE)
					link_event->result.seek_result = rx_buf[i] + (rx_buf[i + 1] << 8);

				FM_EVENT_SEND(link_event->ln_event, FLAG_SEEK_DONE);
				break;

			case FM_SCAN_OPCODE:

				/* check if the result data is long enough */
				if ((RX_BUF_SIZE - i) < (sizeof(unsigned short) * FM_SCANTBL_SIZE)) {
					WCN_DBG(FM_ALT | LINK,
						"FM_SCAN_OPCODE err, [tblsize=%d],[bufsize=%d]\n",
						(unsigned int)(sizeof(unsigned short) * FM_SCANTBL_SIZE),
						(unsigned int)(RX_BUF_SIZE - i));
					FM_EVENT_SEND(link_event->ln_event, FLAG_SCAN_DONE);
					return 0;
				} else if ((length >= FM_CQI_BUF_SIZE)
					   && ((RX_BUF_SIZE - i) >= FM_CQI_BUF_SIZE)) {
					fm_memcpy(link_event->result.cqi, &rx_buf[i], FM_CQI_BUF_SIZE);
					FM_EVENT_SEND(link_event->ln_event, FLAG_CQI_DONE);
				} else {
					fm_memcpy(link_event->result.scan_result, &rx_buf[i],
						  sizeof(unsigned short) * FM_SCANTBL_SIZE);
					FM_EVENT_SEND(link_event->ln_event, FLAG_SCAN_DONE);
				}

				break;

			case FSPI_READ_OPCODE:

				if ((i + 1) < RX_BUF_SIZE)
					link_event->result.fspi_rd = (rx_buf[i] + (rx_buf[i + 1] << 8));

				FM_EVENT_SEND(link_event->ln_event, (1 << opcode));
				break;
			case CSPI_READ_OPCODE:
				{
					if ((i + 1) < RX_BUF_SIZE) {
						link_event->result.cspi_rd =
						    (rx_buf[i] + (rx_buf[i + 1] << 8) +
						     (rx_buf[i + 2] << 16) + (rx_buf[i + 3] << 24));
					}

					FM_EVENT_SEND(link_event->ln_event, FLAG_CSPI_READ);
					break;
				}
			case FM_HOST_READ_OPCODE:
				{
					if ((i + 1) < RX_BUF_SIZE) {
						link_event->result.cspi_rd =
						    (rx_buf[i] + (rx_buf[i + 1] << 8) +
						     (rx_buf[i + 2] << 16) + (rx_buf[i + 3] << 24));
					}

					FM_EVENT_SEND(link_event->ln_event, (1 << opcode));
					break;
				}
			case FM_WRITE_PMIC_CR_OPCODE:
			case FM_MODIFY_PMIC_CR_OPCODE:
				{
					link_event->result.pmic_result[0] = rx_buf[i];

					FM_EVENT_SEND(link_event->ln_event, FLAG_PMIC_MODIFY);
					break;
				}
			case FM_READ_PMIC_CR_OPCODE:
				{
					fm_memcpy(&link_event->result.pmic_result[0], &rx_buf[i], length);
					FM_EVENT_SEND(link_event->ln_event, FLAG_PMIC_READ);
					break;
				}
			case RDS_RX_DATA_OPCODE:

				/* check if the rds data is long enough */
				if ((RX_BUF_SIZE - i) < length) {
					WCN_DBG(FM_ALT | LINK,
						"RDS RX err, [rxlen=%d],[bufsize=%d]\n",
						(signed int) length, (RX_BUF_SIZE - i));
					FM_EVENT_SEND(link_event->ln_event, (1 << opcode));
					break;
				}

				if (length > sizeof(struct rds_rx_t)) {
					WCN_DBG(FM_ALT | LINK,
						"RDS RX len out of range, [rxlen=%d],[needmaxlen=%zd]\n",
						(signed int) length, (sizeof(struct rds_rx_t)));
					FM_EVENT_SEND(link_event->ln_event, (1 << opcode));
					break;
				}
				/* copy rds data to rds buf */
				fm_memcpy(&link_event->result.rds_rx_result, &rx_buf[i], length);

				/*Handle the RDS data that we get */
				if (rds_parser)
					rds_parser(&link_event->result.rds_rx_result, length);
				else
					WCN_DBG(FM_WAR | LINK, "no method to parse RDS data\n");

				FM_EVENT_SEND(link_event->ln_event, (1 << opcode));
				break;

			default:
				FM_EVENT_SEND(link_event->ln_event, (1 << opcode));
				break;
			}

			state = FM_TASK_RX_PARSER_PKT_TYPE;
			i += length;
			break;

		default:
			break;
		}
	}

	return 0;
}

bool fm_wait_stc_done(unsigned int sec)
{
	return true;
}

signed int fm_force_active_event(unsigned int mask)
{
	unsigned int flag;

	flag = FM_EVENT_GET(link_event->ln_event);
	WCN_DBG(FM_WAR | LINK, "before force active event, [flag=0x%08x]\n", flag);
	flag = FM_EVENT_SEND(link_event->ln_event, mask);
	WCN_DBG(FM_WAR | LINK, "after force active event, [flag=0x%08x]\n", flag);

	return 0;
}

extern signed int fm_print_cmd_fifo(void)
{
#ifdef FM_TRACE_ENABLE
	struct fm_trace_t trace;
	signed int i = 0;

	fm_memset(&trace, 0, sizeof(struct fm_trace_t));

	while (false == FM_TRACE_EMPTY(cmd_fifo)) {
		fm_memset(trace.pkt, 0, FM_TRACE_PKT_SIZE);
		FM_TRACE_OUT(cmd_fifo, &trace);
		WCN_DBG_LIMITED(FM_ALT | LINK, "trace, type %d, op %d, len %d, tid %d, time %d\n",
			trace.type, trace.opcode, trace.len, trace.tid, jiffies_to_msecs(abs(trace.time)));
		i = 0;
		while ((trace.len > 0) && (i < trace.len) && (i < (FM_TRACE_PKT_SIZE - 8))) {
			WCN_DBG_LIMITED(FM_ALT | LINK, "trace, %02x %02x %02x %02x %02x %02x %02x %02x\n",
				trace.pkt[i], trace.pkt[i + 1], trace.pkt[i + 2], trace.pkt[i + 3],
				trace.pkt[i + 4], trace.pkt[i + 5], trace.pkt[i + 6], trace.pkt[i + 7]);
			i += 8;
		}
		WCN_DBG_LIMITED(FM_ALT | LINK, "trace\n");
	}
#endif

	return 0;
}

extern signed int fm_print_evt_fifo(void)
{
#ifdef FM_TRACE_ENABLE
	struct fm_trace_t trace;
	signed int i = 0;

	fm_memset(&trace, 0, sizeof(struct fm_trace_t));

	while (false == FM_TRACE_EMPTY(evt_fifo)) {
		fm_memset(trace.pkt, 0, FM_TRACE_PKT_SIZE);
		FM_TRACE_OUT(evt_fifo, &trace);
		WCN_DBG_LIMITED(FM_ALT | LINK, "%s: op %d, len %d, %d\n", evt_fifo->name, trace.opcode,
			trace.len, jiffies_to_msecs(abs(trace.time)));
		i = 0;
		while ((trace.len > 0) && (i < trace.len) && (i < (FM_TRACE_PKT_SIZE - 8))) {
			WCN_DBG_LIMITED(FM_ALT | LINK, "%s: %02x %02x %02x %02x %02x %02x %02x %02x\n",
				evt_fifo->name, trace.pkt[i], trace.pkt[i + 1], trace.pkt[i + 2],
				trace.pkt[i + 3], trace.pkt[i + 4], trace.pkt[i + 5],
				trace.pkt[i + 6], trace.pkt[i + 7]);
			i += 8;
		}
		WCN_DBG_LIMITED(FM_ALT | LINK, "%s\n", evt_fifo->name);
	}
#endif

	return 0;
}

signed int fm_trace_in(struct fm_trace_fifo_t *thiz, struct fm_trace_t *new_tra)
{
	if (new_tra == NULL) {
		WCN_DBG(FM_ERR | LINK, "%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (thiz->len < thiz->size) {
		fm_memcpy(&(thiz->trace[thiz->in]), new_tra, sizeof(struct fm_trace_t));
		thiz->trace[thiz->in].time = jiffies;
		thiz->in = (thiz->in + 1) % thiz->size;
		thiz->len++;
		/* WCN_DBG(FM_DBG | RDSC, "add a new tra[len=%d]\n", thiz->len); */
	} else {
		WCN_DBG(FM_WAR | LINK, "tra buf is full\n");
		return -FM_ENOMEM;
	}

	return 0;
}

signed int fm_trace_out(struct fm_trace_fifo_t *thiz, struct fm_trace_t *dst_tra)
{
	if (thiz->len > 0) {
		if (dst_tra) {
			fm_memcpy(dst_tra, &(thiz->trace[thiz->out]), sizeof(struct fm_trace_t));
			fm_memset(&(thiz->trace[thiz->out]), 0, sizeof(struct fm_trace_t));
		}
		thiz->out = (thiz->out + 1) % thiz->size;
		thiz->len--;
		/* WCN_DBG(FM_DBG | LINK, "del a tra[len=%d]\n", thiz->len); */
	} else {
		WCN_DBG(FM_WAR | LINK, "tra buf is empty\n");
	}

	return 0;
}

bool fm_trace_is_full(struct fm_trace_fifo_t *thiz)
{
	return (thiz->len == thiz->size) ? true : false;
}

bool fm_trace_is_empty(struct fm_trace_fifo_t *thiz)
{
	return (thiz->len == 0) ? true : false;
}

struct fm_trace_fifo_t *fm_trace_fifo_create(const signed char *name)
{
	struct fm_trace_fifo_t *tmp;

	tmp = fm_zalloc(sizeof(struct fm_trace_fifo_t));
	if (!tmp) {
		WCN_DBG(FM_ALT | MAIN, "fm_zalloc(fm_trace_fifo) -ENOMEM\n");
		return NULL;
	}

	fm_memcpy(tmp->name, name, (strlen(name) + 1));
	tmp->size = FM_TRACE_FIFO_SIZE;
	tmp->in = 0;
	tmp->out = 0;
	tmp->len = 0;
	tmp->trace_in = fm_trace_in;
	tmp->trace_out = fm_trace_out;
	tmp->is_full = fm_trace_is_full;
	tmp->is_empty = fm_trace_is_empty;

	WCN_DBG(FM_NTC | LINK, "%s created\n", tmp->name);

	return tmp;
}

signed int fm_trace_fifo_release(struct fm_trace_fifo_t *fifo)
{
	if (fifo) {
		WCN_DBG(FM_NTC | LINK, "%s released\n", fifo->name);
		fm_free(fifo);
	}

	return 0;
}
