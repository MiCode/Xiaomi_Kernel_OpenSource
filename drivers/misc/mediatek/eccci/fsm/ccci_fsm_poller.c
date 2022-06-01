// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include "ccci_fsm_internal.h"

#define POLLING_INTERVAL_TIME 15000
#define POLLING_TIMEOUT 15
#define FORCE_ASSERT_TIMEOUT 15000

static int fsm_get_no_response_assert_type(struct ccci_fsm_poller *poller_ctl)
{
	unsigned long long traffic_info[3] = {0};
	u64 latest_isr_time = 0;
	u64 latest_q0_isr_time = 0;
	u64 latest_q0_rx_time = 0;
	unsigned long long last_poll_time = 0;
	int md_id = 0, ret = 0;
	unsigned long rem_nsec0 = 0, rem_nsec1 = 0, rem_nsec2 = 0, rem_nsec3 = 0;

	if (!poller_ctl)
		return MD_FORCE_ASSERT_BY_MD_NO_RESPONSE;

	ret = ccci_hif_dump_status(1 << MD1_NORMAL_HIF, DUMP_FLAG_GET_TRAFFIC,
			traffic_info, sizeof(traffic_info));
	if (!ret) {
		latest_isr_time = traffic_info[0];
		latest_q0_isr_time = traffic_info[1];
		latest_q0_rx_time = traffic_info[2];
	}
	last_poll_time = poller_ctl->latest_poll_start_time;
	md_id = poller_ctl->md_id;

	rem_nsec0 = (last_poll_time == 0 ?
		0 : do_div(last_poll_time, NSEC_PER_SEC));
	rem_nsec1 = (latest_isr_time == 0 ?
		0 : do_div(latest_isr_time, NSEC_PER_SEC));
	rem_nsec2 = (latest_q0_isr_time == 0 ?
		0 : do_div(latest_q0_isr_time, NSEC_PER_SEC));
	rem_nsec3 = (latest_q0_rx_time == 0 ?
		0 : do_div(latest_q0_rx_time, NSEC_PER_SEC));

	CCCI_ERROR_LOG(md_id, FSM,
		"polling: start=%llu.%06lu, isr=%llu.%06lu,q0_isr=%llu.%06lu, q0_rx=%llu.%06lu\n",
		last_poll_time, rem_nsec0 / 1000,
		latest_isr_time, rem_nsec1 / 1000,
		latest_q0_isr_time, rem_nsec2 / 1000,
		latest_q0_rx_time, rem_nsec3 / 1000);
	/* Check whether ap received polling queue irq, after polling start */
	/* send status > last q0 isr time */
	if (poller_ctl->latest_poll_start_time > traffic_info[1]) {
		/* send status < last isr time */
		if (poller_ctl->latest_poll_start_time < traffic_info[0])
			CCCI_ERROR_LOG(md_id, FSM,
				"After polling start, have isr but no polling isr, maybe md no response\n");
		else {
			CCCI_ERROR_LOG(md_id, FSM,
				"After polling start, no any irq, check ap irq status and md side send or no\n");
		}
		return MD_FORCE_ASSERT_BY_MD_NO_RESPONSE;
	}
	 /* send status > last wq time, < last isr time */
	if (poller_ctl->latest_poll_start_time > traffic_info[2]) {
		CCCI_ERROR_LOG(md_id, FSM,
		"no work after poll but isr, rx queue maybe blocked\n");
		return MD_FORCE_ASSERT_BY_AP_Q0_BLOCKED;
	}

	CCCI_ERROR_LOG(md_id, FSM,
	"AP polling isr & rx queue & kthread normally after polling start, MD may not response\n");
	return MD_FORCE_ASSERT_BY_MD_NO_RESPONSE;
}

static int fsm_poll_main(void *data)
{
	struct ccci_fsm_poller *poller_ctl = (struct ccci_fsm_poller *)data;
	struct ccci_fsm_ctl *ctl = container_of(poller_ctl,
		struct ccci_fsm_ctl, poller_ctl);
	int ret, assert_md_type, count;
	enum MD_STATE md_state;

	while (1) {
		md_state = ccci_fsm_get_md_state(poller_ctl->md_id);
		if (md_state != READY
			||
			ccci_port_get_critical_user(poller_ctl->md_id,
				CRIT_USR_MDLOG) != 1)
			goto next;
		poller_ctl->poller_state = FSM_POLLER_WAITING_RESPONSE;
		poller_ctl->latest_poll_start_time = local_clock();
		ret = ccci_port_send_msg_to_md(poller_ctl->md_id,
				CCCI_STATUS_TX, 0, 0, 1);
		CCCI_NORMAL_LOG(poller_ctl->md_id, FSM,
				"poll MD status send msg %d\n", ret);
		ret = wait_event_timeout(poller_ctl->status_rx_wq,
		poller_ctl->poller_state == FSM_POLLER_RECEIVED_RESPONSE,
		POLLING_TIMEOUT * HZ);
		CCCI_NORMAL_LOG(poller_ctl->md_id, FSM,
			"poll MD status wait done %d\n", ret);
		if (!ret) { /* timeout */
			md_state = ccci_fsm_get_md_state(poller_ctl->md_id);
			if (md_state == READY) {
				CCCI_ERROR_LOG(poller_ctl->md_id, FSM,
					"poll MD status timeout, force assert\n");
				assert_md_type =
				fsm_get_no_response_assert_type(poller_ctl);
				if (assert_md_type
					== MD_FORCE_ASSERT_BY_MD_NO_RESPONSE)
					ccci_md_dump_info(poller_ctl->md_id,
						DUMP_FLAG_IRQ_STATUS, NULL, 0);
				ccci_md_dump_info(poller_ctl->md_id,
					DUMP_FLAG_QUEUE_0, NULL, 0);
				ccci_md_force_assert(poller_ctl->md_id,
					assert_md_type, NULL, 0);

				count = 0;
				while (count < FORCE_ASSERT_TIMEOUT / 200) {
					if (ccci_fsm_get_md_state(
						poller_ctl->md_id)
							== EXCEPTION) {
						count = 0;
						break;
					}
					count++;
					msleep(200);
				}
				if (count) {
					CCCI_ERROR_LOG(poller_ctl->md_id, FSM,
						"MD long time no response\n");
					ccci_md_dump_info(poller_ctl->md_id,
						DUMP_FLAG_QUEUE_0, NULL, 0);
					fsm_append_command(ctl,
						CCCI_COMMAND_MD_HANG, 0);
				}
			}
		}
next:
		msleep(POLLING_INTERVAL_TIME);
	}
	return 0;
}


int fsm_poller_init(struct ccci_fsm_poller *poller_ctl)
{
	struct ccci_fsm_ctl *ctl = container_of(poller_ctl,
		struct ccci_fsm_ctl, poller_ctl);

	init_waitqueue_head(&poller_ctl->status_rx_wq);
	poller_ctl->md_id = ctl->md_id;
	poller_ctl->poll_thread = kthread_run(fsm_poll_main, poller_ctl,
		"ccci_poll%d", poller_ctl->md_id + 1);
	return 0;
}


int ccci_fsm_recv_status_packet(int md_id, struct sk_buff *skb)
{
	struct ccci_fsm_ctl *ctl = fsm_get_entity_by_md_id(md_id);
	struct ccci_fsm_poller *poller_ctl;

	if (!ctl)
		return -CCCI_ERR_INVALID_PARAM;
	poller_ctl = &ctl->poller_ctl;

	CCCI_NORMAL_LOG(poller_ctl->md_id, FSM,
		"received MD status response %x\n", *(((u32 *)skb->data) + 2));
	/*
	 * ccci_util_cmpt_mem_dump(poller_ctl->md_id,
	 * CCCI_DUMP_REPEAT, skb->data, skb->len);
	 */
	poller_ctl->poller_state = FSM_POLLER_RECEIVED_RESPONSE;
	wake_up(&poller_ctl->status_rx_wq);
	ccci_free_skb(skb);
	return 0;
}

