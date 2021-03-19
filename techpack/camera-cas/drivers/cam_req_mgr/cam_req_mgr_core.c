// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include "cam_req_mgr_interface.h"
#include "cam_req_mgr_util.h"
#include "cam_req_mgr_core.h"
#include "cam_req_mgr_workq.h"
#include "cam_req_mgr_debug.h"
#include "cam_trace.h"
#include "cam_debug_util.h"
#include "cam_req_mgr_dev.h"

static struct cam_req_mgr_core_device *g_crm_core_dev;
static struct cam_req_mgr_core_link g_links[MAXIMUM_LINKS_PER_SESSION];

void cam_req_mgr_core_link_reset(struct cam_req_mgr_core_link *link)
{
	link->link_hdl = 0;
	link->num_devs = 0;
	link->max_delay = CAM_PIPELINE_DELAY_0;
	link->workq = NULL;
	link->pd_mask = 0;
	link->l_dev = NULL;
	link->req.in_q = NULL;
	link->req.l_tbl = NULL;
	link->req.num_tbl = 0;
	link->watchdog = NULL;
	link->state = CAM_CRM_LINK_STATE_AVAILABLE;
	link->parent = NULL;
	link->subscribe_event = 0;
	link->trigger_mask = 0;
	link->sync_link = 0;
	link->sync_link_sof_skip = false;
	link->open_req_cnt = 0;
	link->last_flush_id = 0;
	link->initial_sync_req = -1;
	link->in_msync_mode = false;
	link->retry_cnt = 0;
	link->is_shutdown = false;
	link->initial_skip = true;
	link->sof_timestamp = 0;
	link->prev_sof_timestamp = 0;
}

void cam_req_mgr_handle_core_shutdown(void)
{
	struct cam_req_mgr_core_session *session;
	struct cam_req_mgr_core_session *tsession;
	struct cam_req_mgr_session_info ses_info;

	if (!list_empty(&g_crm_core_dev->session_head)) {
		list_for_each_entry_safe(session, tsession,
			&g_crm_core_dev->session_head, entry) {
			ses_info.session_hdl =
				session->session_hdl;
			cam_req_mgr_destroy_session(&ses_info, true);
		}
	}
}

static int __cam_req_mgr_setup_payload(struct cam_req_mgr_core_workq *workq)
{
	int32_t                  i = 0;
	int                      rc = 0;
	struct crm_task_payload *task_data = NULL;

	task_data = kcalloc(
		workq->task.num_task, sizeof(*task_data),
		GFP_KERNEL);
	if (!task_data) {
		rc = -ENOMEM;
	} else {
		for (i = 0; i < workq->task.num_task; i++)
			workq->task.pool[i].payload = &task_data[i];
	}

	return rc;
}

/**
 * __cam_req_mgr_find_pd_tbl()
 *
 * @brief    : Find pipeline delay based table pointer which matches delay
 * @tbl      : Pointer to list of request table
 * @delay    : Pipeline delay value to be searched for comparison
 *
 * @return   : pointer to request table for matching pipeline delay table.
 *
 */
static struct cam_req_mgr_req_tbl *__cam_req_mgr_find_pd_tbl(
	struct cam_req_mgr_req_tbl *tbl, int32_t delay)
{
	if (!tbl)
		return NULL;

	do {
		if (delay != tbl->pd)
			tbl = tbl->next;
		else
			return tbl;
	} while (tbl != NULL);

	return NULL;
}

/**
 * __cam_req_mgr_inc_idx()
 *
 * @brief    : Increment val passed by step size and rollover after max_val
 * @val      : value to be incremented
 * @step     : amount/step by which val is incremented
 * @max_val  : max val after which idx will roll over
 *
 */
static void __cam_req_mgr_inc_idx(int32_t *val, int32_t step, int32_t max_val)
{
	*val = (*val + step) % max_val;
}

/**
 * __cam_req_mgr_dec_idx()
 *
 * @brief    : Decrement val passed by step size and rollover after max_val
 * @val      : value to be decremented
 * @step     : amount/step by which val is decremented
 * @max_val  : after zero value will roll over to max val
 *
 */
static void __cam_req_mgr_dec_idx(int32_t *val, int32_t step, int32_t max_val)
{
	*val = *val - step;
	if (*val < 0)
		*val = max_val + (*val);
}

/**
 * __cam_req_mgr_inject_delay()
 *
 * @brief    : Check if any pd device is injecting delay
 * @tbl      : cam_req_mgr_req_tbl
 * @curr_idx : slot idx
 *
 * @return   : 0 for success, negative for failure
 */
static int __cam_req_mgr_inject_delay(
	struct cam_req_mgr_req_tbl  *tbl,
	int32_t curr_idx)
{
	struct cam_req_mgr_tbl_slot *slot = NULL;
	int rc = 0;

	while (tbl) {
		slot = &tbl->slot[curr_idx];
		if (slot->inject_delay > 0) {
			slot->inject_delay--;
			CAM_DBG(CAM_CRM,
				"Delay injected by pd %d device",
				tbl->pd);
			rc = -EAGAIN;
		}
		__cam_req_mgr_dec_idx(&curr_idx, tbl->pd_delta,
			tbl->num_slots);
		tbl = tbl->next;
	}
	return rc;
}

/**
 * __cam_req_mgr_find_dev_name()
 *
 * @brief      : Find the dev name whose req is not ready
 * @link       : link info
 * @req_id     : req_id which is not ready
 * @pd         : pipeline delay
 * @masked_val : masked value holds the bit for all devices
 *               that don't have the req_id ready for a given
 *               pipeline delay
 * @pd         : pipeline delay
 *
 */
static void __cam_req_mgr_find_dev_name(
	struct cam_req_mgr_core_link *link,
	int64_t req_id, uint32_t pd, uint32_t masked_val)
{
	int i = 0;
	struct cam_req_mgr_connected_device *dev = NULL;

	for (i = 0; i < link->num_devs; i++) {
		dev = &link->l_dev[i];
		if (dev->dev_info.p_delay == pd) {
			if (masked_val & (1 << dev->dev_bit))
				continue;

			CAM_INFO(CAM_CRM,
				"Skip Frame: req: %lld not ready on link: 0x%x for pd: %d dev: %s open_req count: %d",
				req_id, link->link_hdl, pd, dev->dev_info.name,
				link->open_req_cnt);
		}
	}
}

/**
 * __cam_req_mgr_notify_error_on_link()
 *
 * @brief : Notify userspace on exceeding max retry
 *          attempts to apply same req
 * @link  : link on which the req could not be applied
 *
 */
static int __cam_req_mgr_notify_error_on_link(
	struct cam_req_mgr_core_link    *link,
	struct cam_req_mgr_connected_device *dev)
{
	struct cam_req_mgr_core_session *session = NULL;
	struct cam_req_mgr_message       msg;
	int rc = 0, pd;

	session = (struct cam_req_mgr_core_session *)link->parent;

	pd = dev->dev_info.p_delay;
	if (pd >= CAM_PIPELINE_DELAY_MAX) {
		CAM_ERR(CAM_CRM, "pd : %d is more than expected", pd);
		return -EINVAL;
	}

	CAM_ERR(CAM_CRM,
		"Notifying userspace to trigger recovery on link 0x%x for session %d",
		link->link_hdl, session->session_hdl);

	memset(&msg, 0, sizeof(msg));

	msg.session_hdl = session->session_hdl;
	msg.u.err_msg.error_type = CAM_REQ_MGR_ERROR_TYPE_RECOVERY;
	msg.u.err_msg.request_id =
		link->req.apply_data[pd].req_id;
	msg.u.err_msg.link_hdl   = link->link_hdl;

	CAM_DBG(CAM_CRM, "Failed for device: %s while applying request: %lld",
		dev->dev_info.name, link->req.apply_data[pd].req_id);

	rc = cam_req_mgr_notify_message(&msg,
		V4L_EVENT_CAM_REQ_MGR_ERROR,
		V4L_EVENT_CAM_REQ_MGR_EVENT);

	if (rc)
		CAM_ERR(CAM_CRM,
			"Error in notifying recovery for session %d link 0x%x rc %d",
			session->session_hdl, link->link_hdl, rc);

	return rc;
}

/**
 * __cam_req_mgr_traverse()
 *
 * @brief    : Traverse through pd tables, it will internally cover all linked
 *             pd tables. Each pd table visited will check if idx passed to its
 *             in ready state. If ready means all devices linked to the pd table
 *             have this request id packet ready. Then it calls subsequent pd
 *             tbl with new idx. New idx value takes into account the delta
 *             between current pd table and next one.
 * @traverse_data: contains all the info to traverse through pd tables
 *
 * @return: 0 for success, negative for failure
 *
 */
static int __cam_req_mgr_traverse(struct cam_req_mgr_traverse *traverse_data)
{
	int                          rc = 0;
	int32_t                      next_idx = traverse_data->idx;
	int32_t                      curr_idx = traverse_data->idx;
	struct cam_req_mgr_req_tbl  *tbl;
	struct cam_req_mgr_apply    *apply_data;
	struct cam_req_mgr_tbl_slot *slot = NULL;

	if (!traverse_data->tbl || !traverse_data->apply_data) {
		CAM_ERR(CAM_CRM, "NULL pointer %pK %pK",
			traverse_data->tbl, traverse_data->apply_data);
		traverse_data->result = 0;
		return -EINVAL;
	}

	tbl = traverse_data->tbl;
	apply_data = traverse_data->apply_data;
	slot = &tbl->slot[curr_idx];
	CAM_DBG(CAM_CRM,
		"Enter pd %d idx %d state %d skip %d status %d skip_idx %d",
		tbl->pd, curr_idx, tbl->slot[curr_idx].state,
		tbl->skip_traverse, traverse_data->in_q->slot[curr_idx].status,
		traverse_data->in_q->slot[curr_idx].skip_idx);

	/* Check if req is ready or in skip mode or pd tbl is in skip mode */
	if (tbl->slot[curr_idx].state == CRM_REQ_STATE_READY ||
		traverse_data->in_q->slot[curr_idx].skip_idx == 1 ||
		tbl->skip_traverse > 0) {
		if (tbl->next) {
			__cam_req_mgr_dec_idx(&next_idx, tbl->pd_delta,
				tbl->num_slots);
			traverse_data->idx = next_idx;
			traverse_data->tbl = tbl->next;
			rc = __cam_req_mgr_traverse(traverse_data);
		}
		if (rc >= 0) {
			SET_SUCCESS_BIT(traverse_data->result, tbl->pd);

			if (traverse_data->validate_only == false) {
				apply_data[tbl->pd].pd = tbl->pd;
				apply_data[tbl->pd].req_id =
					CRM_GET_REQ_ID(
					traverse_data->in_q, curr_idx);
				apply_data[tbl->pd].idx = curr_idx;

				CAM_DBG(CAM_CRM, "req_id: %lld with pd of %d",
				apply_data[tbl->pd].req_id,
				apply_data[tbl->pd].pd);
				/*
				 * If traverse is successful decrement
				 * traverse skip
				 */
				if (tbl->skip_traverse > 0) {
					apply_data[tbl->pd].req_id = -1;
					tbl->skip_traverse--;
				}
			}
		} else {
			/* linked pd table is not ready for this traverse yet */
			return rc;
		}
	} else {
		/* This pd table is not ready to proceed with asked idx */
		traverse_data->result_data.req_id =
			CRM_GET_REQ_ID(traverse_data->in_q, curr_idx);
		traverse_data->result_data.pd = tbl->pd;
		traverse_data->result_data.masked_value =
			(tbl->dev_mask & slot->req_ready_map);
		SET_FAILURE_BIT(traverse_data->result, tbl->pd);
		return -EAGAIN;
	}

	return 0;
}

/**
 * __cam_req_mgr_in_q_skip_idx()
 *
 * @brief    : Decrement val passed by step size and rollover after max_val
 * @in_q     : input queue pointer
 * @idx      : Sets skip_idx bit of the particular slot to true so when traverse
 *             happens for this idx, no req will be submitted for devices
 *             handling this idx.
 *
 */
static void __cam_req_mgr_in_q_skip_idx(struct cam_req_mgr_req_queue *in_q,
	int32_t idx)
{
	in_q->slot[idx].req_id = -1;
	in_q->slot[idx].skip_idx = 1;
	in_q->slot[idx].status = CRM_SLOT_STATUS_REQ_ADDED;
	CAM_DBG(CAM_CRM, "SET IDX SKIP on slot= %d", idx);
}

/**
 * __cam_req_mgr_tbl_set_id()
 *
 * @brief    : Set unique id to table
 * @tbl      : pipeline based table which requires new id
 * @req      : pointer to request data wihch contains num_tables counter
 *
 */
static void __cam_req_mgr_tbl_set_id(struct cam_req_mgr_req_tbl *tbl,
	struct cam_req_mgr_req_data *req)
{
	if (!tbl)
		return;
	do {
		tbl->id = req->num_tbl++;
		CAM_DBG(CAM_CRM, "%d: pd %d skip_traverse %d delta %d",
			tbl->id, tbl->pd, tbl->skip_traverse,
			tbl->pd_delta);
		tbl = tbl->next;
	} while (tbl != NULL);
}

/**
 * __cam_req_mgr_tbl_set_all_skip_cnt()
 *
 * @brief    : Each pd table sets skip value based on delta between itself and
 *             max pd value. During initial streamon or bubble case this is
 *             used. That way each pd table skips required num of traverse and
 *             align themselve with req mgr connected devs.
 * @l_tbl    : iterates through list of pd tables and sets skip traverse
 *
 */
static void __cam_req_mgr_tbl_set_all_skip_cnt(
	struct cam_req_mgr_req_tbl **l_tbl)
{
	struct cam_req_mgr_req_tbl *tbl = *l_tbl;
	int32_t                     max_pd;

	if (!tbl)
		return;

	max_pd = tbl->pd;
	do {
		tbl->skip_traverse = max_pd - tbl->pd;
		CAM_DBG(CAM_CRM, "%d: pd %d skip_traverse %d delta %d",
			tbl->id, tbl->pd, tbl->skip_traverse,
			tbl->pd_delta);
		tbl = tbl->next;
	} while (tbl != NULL);
}

/**
 * __cam_req_mgr_flush_req_slot()
 *
 * @brief    : reset all the slots/pd tables when flush is
 *             invoked
 * @link     : link pointer
 *
 */
static void __cam_req_mgr_flush_req_slot(
	struct cam_req_mgr_core_link *link)
{
	int                           idx = 0;
	struct cam_req_mgr_slot      *slot;
	struct cam_req_mgr_req_tbl   *tbl;
	struct cam_req_mgr_req_queue *in_q = link->req.in_q;

	for (idx = 0; idx < in_q->num_slots; idx++) {
		slot = &in_q->slot[idx];
		tbl = link->req.l_tbl;
		CAM_DBG(CAM_CRM,
			"RESET idx: %d req_id: %lld slot->status: %d",
			idx, slot->req_id, slot->status);

		/* Reset input queue slot */
		slot->req_id = -1;
		slot->skip_idx = 1;
		slot->recover = 0;
		slot->additional_timeout = 0;
		slot->sync_mode = CAM_REQ_MGR_SYNC_MODE_NO_SYNC;
		slot->status = CRM_SLOT_STATUS_NO_REQ;

		/* Reset all pd table slot */
		while (tbl != NULL) {
			CAM_DBG(CAM_CRM, "pd: %d: idx %d state %d",
				tbl->pd, idx, tbl->slot[idx].state);
			tbl->slot[idx].req_ready_map = 0;
			tbl->slot[idx].state = CRM_REQ_STATE_EMPTY;
			tbl = tbl->next;
		}
	}

	in_q->wr_idx = 0;
	in_q->rd_idx = 0;
}

/**
 * __cam_req_mgr_reset_req_slot()
 *
 * @brief    : reset specified idx/slot in input queue as well as all pd tables
 * @link     : link pointer
 * @idx      : slot index which will be reset
 *
 */
static void __cam_req_mgr_reset_req_slot(struct cam_req_mgr_core_link *link,
	int32_t idx)
{
	struct cam_req_mgr_slot      *slot;
	struct cam_req_mgr_req_tbl   *tbl = link->req.l_tbl;
	struct cam_req_mgr_req_queue *in_q = link->req.in_q;

	slot = &in_q->slot[idx];
	CAM_DBG(CAM_CRM, "RESET: idx: %d: slot->status %d", idx, slot->status);

	/* Check if CSL has already pushed new request*/
	if (slot->status == CRM_SLOT_STATUS_REQ_ADDED)
		return;

	/* Reset input queue slot */
	slot->req_id = -1;
	slot->skip_idx = 0;
	slot->recover = 0;
	slot->additional_timeout = 0;
	slot->sync_mode = CAM_REQ_MGR_SYNC_MODE_NO_SYNC;
	slot->status = CRM_SLOT_STATUS_NO_REQ;

	/* Reset all pd table slot */
	while (tbl != NULL) {
		CAM_DBG(CAM_CRM, "pd: %d: idx %d state %d",
			tbl->pd, idx, tbl->slot[idx].state);
		tbl->slot[idx].req_ready_map = 0;
		tbl->slot[idx].state = CRM_REQ_STATE_EMPTY;
		tbl = tbl->next;
	}
}

/**
 * __cam_req_mgr_validate_crm_wd_timer()
 *
 * @brief    : Validate/modify the wd timer based on associated
 *             timeout with the request
 * @link     : link pointer
 *
 */
static void __cam_req_mgr_validate_crm_wd_timer(
	struct cam_req_mgr_core_link *link)
{
	int idx = 0;
	int next_frame_timeout = 0, current_frame_timeout = 0;
	struct cam_req_mgr_req_queue *in_q = link->req.in_q;

	idx = in_q->rd_idx;
	__cam_req_mgr_dec_idx(
		&idx, (link->max_delay - 1),
		in_q->num_slots);
	next_frame_timeout = in_q->slot[idx].additional_timeout;
	CAM_DBG(CAM_CRM,
		"rd_idx: %d idx: %d next_frame_timeout: %d ms",
		in_q->rd_idx, idx, next_frame_timeout);

	idx = in_q->rd_idx;
	__cam_req_mgr_dec_idx(
		&idx, link->max_delay,
		in_q->num_slots);
	current_frame_timeout = in_q->slot[idx].additional_timeout;
	CAM_DBG(CAM_CRM,
		"rd_idx: %d idx: %d current_frame_timeout: %d ms",
		in_q->rd_idx, idx, current_frame_timeout);
	if (link->watchdog == NULL)
		CAM_ERR(CAM_CRM, "watchdog == null,link:%p", link);
	if ((next_frame_timeout + CAM_REQ_MGR_WATCHDOG_TIMEOUT) >
		link->watchdog->expires) {
		CAM_DBG(CAM_CRM,
			"Modifying wd timer expiry from %d ms to %d ms",
			link->watchdog->expires,
			(next_frame_timeout + CAM_REQ_MGR_WATCHDOG_TIMEOUT));
		crm_timer_modify(link->watchdog,
			next_frame_timeout +
			CAM_REQ_MGR_WATCHDOG_TIMEOUT);
	} else if (current_frame_timeout) {
		CAM_DBG(CAM_CRM,
			"Reset wd timer to current frame from %d ms to %d ms",
			link->watchdog->expires,
			(current_frame_timeout + CAM_REQ_MGR_WATCHDOG_TIMEOUT));
		crm_timer_modify(link->watchdog,
			current_frame_timeout +
			CAM_REQ_MGR_WATCHDOG_TIMEOUT);
	} else if (link->watchdog->expires >
		CAM_REQ_MGR_WATCHDOG_TIMEOUT) {
		CAM_DBG(CAM_CRM,
			"Reset wd timer to default from %d ms to %d ms",
			link->watchdog->expires, CAM_REQ_MGR_WATCHDOG_TIMEOUT);
		crm_timer_modify(link->watchdog,
			CAM_REQ_MGR_WATCHDOG_TIMEOUT);
	}
}

/**
 * __cam_req_mgr_check_for_lower_pd_devices()
 *
 * @brief    : Checks if there are any devices on the link having a lesser
 *             pd than the max pd of the link
 * @link     : Pointer to link which needs to be checked
 *
 * @return   : 0 if a lower pd device is found negative otherwise
 */
static int __cam_req_mgr_check_for_lower_pd_devices(
	struct cam_req_mgr_core_link	*link)
{
	int i = 0;
	struct cam_req_mgr_connected_device *dev = NULL;

	for (i = 0; i < link->num_devs; i++) {
		dev = &link->l_dev[i];
		if (dev->dev_info.p_delay < link->max_delay)
			return 0;
	}

	return -EAGAIN;
}

/**
 * __cam_req_mgr_check_next_req_slot()
 *
 * @brief    : While streaming if input queue does not contain any pending
 *             request, req mgr still needs to submit pending request ids to
 *             devices with lower pipeline delay value.
 * @in_q     : Pointer to input queue where req mgr wil peep into
 *
 * @return   : 0 for success, negative for failure
 */
static int __cam_req_mgr_check_next_req_slot(
	struct cam_req_mgr_core_link *link)
{
	int rc = 0;
	struct cam_req_mgr_req_queue *in_q = link->req.in_q;
	int32_t idx = in_q->rd_idx;
	struct cam_req_mgr_slot *slot;

	__cam_req_mgr_inc_idx(&idx, 1, in_q->num_slots);
	slot = &in_q->slot[idx];

	CAM_DBG(CAM_CRM, "idx: %d: slot->status %d", idx, slot->status);

	/* Check if there is new req from CSL, if not complete req */
	if (slot->status == CRM_SLOT_STATUS_NO_REQ) {
		rc = __cam_req_mgr_check_for_lower_pd_devices(link);
		if (rc) {
			CAM_DBG(CAM_CRM, "No lower pd devices on link 0x%x",
				link->link_hdl);
			return rc;
		}
		__cam_req_mgr_in_q_skip_idx(in_q, idx);
		if (in_q->wr_idx != idx)
			CAM_WARN(CAM_CRM,
				"CHECK here wr %d, rd %d", in_q->wr_idx, idx);
		__cam_req_mgr_inc_idx(&in_q->wr_idx, 1, in_q->num_slots);
	}

	return rc;
}

/**
 * __cam_req_mgr_send_req()
 *
 * @brief    : send request id to be applied to each device connected on link
 * @link     : pointer to link whose input queue and req tbl are
 *             traversed through
 * @in_q     : pointer to input request queue
 *
 * @return   : 0 for success, negative for failure
 *
 */
static int __cam_req_mgr_send_req(struct cam_req_mgr_core_link *link,
	struct cam_req_mgr_req_queue *in_q, uint32_t trigger,
	struct cam_req_mgr_connected_device **failed_dev)
{
	int                                  rc = 0, pd, i, idx;
	struct cam_req_mgr_connected_device *dev = NULL;
	struct cam_req_mgr_apply_request     apply_req;
	struct cam_req_mgr_link_evt_data     evt_data;
	struct cam_req_mgr_tbl_slot          *slot = NULL;

	apply_req.link_hdl = link->link_hdl;
	apply_req.report_if_bubble = 0;

	for (i = 0; i < link->num_devs; i++) {
		dev = &link->l_dev[i];
		if (!dev)
			continue;
		pd = dev->dev_info.p_delay;
		if (pd >= CAM_PIPELINE_DELAY_MAX) {
			CAM_WARN(CAM_CRM, "pd %d greater than max",
				pd);
			continue;
		}

		idx = link->req.apply_data[pd].idx;
		slot = &dev->pd_tbl->slot[idx];
		/*
		 * Just let flash go for this request and other
		 * device get restricted
		 */

		if ((slot->skip_next_frame != true) ||
			(slot->dev_hdl != dev->dev_hdl))
			continue;

		if (!(dev->dev_info.trigger & trigger))
			continue;

		apply_req.dev_hdl = dev->dev_hdl;
		apply_req.request_id =
			link->req.apply_data[pd].req_id;
		apply_req.trigger_point = trigger;
		if (dev->ops && dev->ops->apply_req) {
			rc = dev->ops->apply_req(&apply_req);
			if (rc)
				return rc;
			CAM_DBG(CAM_REQ,
				"SEND: link_hdl: %x pd: %d req_id %lld",
				link->link_hdl, pd, apply_req.request_id);
			slot->skip_next_frame = false;
			slot->is_applied = true;
			return -EAGAIN;
		}
	}

	for (i = 0; i < link->num_devs; i++) {
		dev = &link->l_dev[i];
		if (dev) {
			pd = dev->dev_info.p_delay;
			if (pd >= CAM_PIPELINE_DELAY_MAX) {
				CAM_WARN(CAM_CRM, "pd %d greater than max",
					pd);
				continue;
			}
			if (link->req.apply_data[pd].skip_idx ||
				link->req.apply_data[pd].req_id < 0) {
				CAM_DBG(CAM_CRM, "skip %d req_id %lld",
					link->req.apply_data[pd].skip_idx,
					link->req.apply_data[pd].req_id);
				continue;
			}
			if (!(dev->dev_info.trigger & trigger))
				continue;

			apply_req.dev_hdl = dev->dev_hdl;
			apply_req.request_id =
				link->req.apply_data[pd].req_id;
			idx = link->req.apply_data[pd].idx;
			slot = &dev->pd_tbl->slot[idx];
			apply_req.report_if_bubble =
				in_q->slot[idx].recover;

			if ((slot->dev_hdl == dev->dev_hdl) &&
				(slot->is_applied == true)) {
				slot->is_applied = false;
				continue;
			}

			apply_req.trigger_point = trigger;
			CAM_DBG(CAM_REQ,
				"SEND: link_hdl: %x pd %d req_id %lld",
				link->link_hdl, pd, apply_req.request_id);
			if (dev->ops && dev->ops->apply_req) {
				rc = dev->ops->apply_req(&apply_req);
				if (rc < 0) {
					*failed_dev = dev;
					break;
				}
			}
			trace_cam_req_mgr_apply_request(link, &apply_req, dev);
		}
	}
	if (rc < 0) {
		CAM_WARN_RATE_LIMIT(CAM_CRM, "APPLY FAILED pd %d req_id %lld",
			dev->dev_info.p_delay, apply_req.request_id);
		/* Apply req failed notify already applied devs */
		for (; i >= 0; i--) {
			dev = &link->l_dev[i];
			evt_data.evt_type = CAM_REQ_MGR_LINK_EVT_ERR;
			evt_data.dev_hdl = dev->dev_hdl;
			evt_data.link_hdl =  link->link_hdl;
			evt_data.req_id = apply_req.request_id;
			evt_data.u.error = CRM_KMD_ERR_BUBBLE;
			if (dev->ops && dev->ops->process_evt)
				dev->ops->process_evt(&evt_data);
		}
	}
	return rc;
}

/**
 * __cam_req_mgr_check_link_is_ready()
 *
 * @brief    : traverse through all request tables and see if all devices are
 *             ready to apply request settings.
 * @link     : pointer to link whose input queue and req tbl are
 *             traversed through
 * @idx      : index within input request queue
 * @validate_only : Whether to validate only and/or update settings
 *
 * @return   : 0 for success, negative for failure
 *
 */
static int __cam_req_mgr_check_link_is_ready(struct cam_req_mgr_core_link *link,
	int32_t idx, bool validate_only)
{
	int                            rc;
	struct cam_req_mgr_traverse    traverse_data;
	struct cam_req_mgr_req_queue  *in_q;
	struct cam_req_mgr_apply      *apply_data;

	in_q = link->req.in_q;

	apply_data = link->req.apply_data;

	if (validate_only == false) {
		memset(apply_data, 0,
		    sizeof(struct cam_req_mgr_apply) * CAM_PIPELINE_DELAY_MAX);
	}

	traverse_data.apply_data = apply_data;
	traverse_data.idx = idx;
	traverse_data.tbl = link->req.l_tbl;
	traverse_data.in_q = in_q;
	traverse_data.result = 0;
	traverse_data.result_data.masked_value = 0;
	traverse_data.result_data.pd = 0;
	traverse_data.result_data.req_id = 0;
	traverse_data.validate_only = validate_only;
	traverse_data.open_req_cnt = link->open_req_cnt;

	/*
	 * Some no-sync mode requests are processed after link config,
	 * then process the sync mode requests after no-sync mode requests
	 * are handled, the initial_skip should be false when processing
	 * the sync mode requests.
	 */
	if (link->initial_skip) {
		CAM_DBG(CAM_CRM,
			"Set initial_skip to false for link %x",
			link->link_hdl);
		link->initial_skip = false;
	}

	/*
	 *  Traverse through all pd tables, if result is success,
	 *  apply the settings
	 */
	rc = __cam_req_mgr_traverse(&traverse_data);
	CAM_DBG(CAM_CRM,
		"SOF: idx %d result %x pd_mask %x rc %d",
		idx, traverse_data.result, link->pd_mask, rc);

	if (!rc && traverse_data.result == link->pd_mask) {
		CAM_DBG(CAM_CRM,
			"READY: link_hdl= %x idx= %d, req_id= %lld :%lld :%lld",
			link->link_hdl, idx,
			apply_data[2].req_id,
			apply_data[1].req_id,
			apply_data[0].req_id);
	} else {
		rc = -EAGAIN;
		__cam_req_mgr_find_dev_name(link,
			traverse_data.result_data.req_id,
			traverse_data.result_data.pd,
			traverse_data.result_data.masked_value);
	}

	return rc;
}

/**
 * __cam_req_mgr_find_slot_for_req()
 *
 * @brief    : Find idx from input queue at which req id is enqueued
 * @in_q     : input request queue pointer
 * @req_id   : request id which needs to be searched in input queue
 *
 * @return   : slot index where passed request id is stored, -1 for failure
 *
 */
static int32_t __cam_req_mgr_find_slot_for_req(
	struct cam_req_mgr_req_queue *in_q, int64_t req_id)
{
	int32_t                   idx, i;
	struct cam_req_mgr_slot  *slot;

	idx = in_q->rd_idx;
	for (i = 0; i < in_q->num_slots; i++) {
		slot = &in_q->slot[idx];
		if (slot->req_id == req_id) {
			CAM_DBG(CAM_CRM,
				"req: %lld found at idx: %d status: %d sync_mode: %d",
				req_id, idx, slot->status, slot->sync_mode);
			break;
		}
		__cam_req_mgr_dec_idx(&idx, 1, in_q->num_slots);
	}
	if (i >= in_q->num_slots)
		idx = -1;

	return idx;
}

/**
 * __cam_req_mgr_check_sync_for_mslave()
 *
 * @brief    : Processes requests during sync mode [master-slave]
 *             Here master corresponds to the link having a higher
 *             max_delay (pd) compared to the slave link.
 * @link     : Pointer to link whose input queue and req tbl are
 *             traversed through
 * @slot     : Pointer to the current slot being processed
 * @return   : 0 for success, negative for failure
 *
 */
static int __cam_req_mgr_check_sync_for_mslave(
	struct cam_req_mgr_core_link *link,
	struct cam_req_mgr_slot *slot)
{
	struct cam_req_mgr_core_link *sync_link = NULL;
	struct cam_req_mgr_slot      *sync_slot = NULL;
	int sync_slot_idx = 0, prev_idx, next_idx, rd_idx, sync_rd_idx, rc = 0;
	int64_t req_id = 0, sync_req_id = 0;
	int32_t sync_num_slots = 0;

	if (!link->sync_link) {
		CAM_ERR(CAM_CRM, "Sync link null");
		return -EINVAL;
	}

	sync_link = link->sync_link;
	req_id = slot->req_id;
	sync_num_slots = sync_link->req.in_q->num_slots;
	sync_rd_idx = sync_link->req.in_q->rd_idx;

	CAM_DBG(CAM_CRM,
		"link_hdl %x req %lld frame_skip_flag %d open_req_cnt:%d initial_sync_req [%lld,%lld] is_master:%d",
		link->link_hdl, req_id, link->sync_link_sof_skip,
		link->open_req_cnt, link->initial_sync_req,
		sync_link->initial_sync_req, link->is_master);

	if (sync_link->sync_link_sof_skip) {
		CAM_DBG(CAM_CRM,
			"No req applied on corresponding SOF on sync link: %x",
				sync_link->link_hdl);
		sync_link->sync_link_sof_skip = false;
		__cam_req_mgr_inject_delay(link->req.l_tbl, slot->idx);
		return -EAGAIN;
	}

	if (link->in_msync_mode &&
		sync_link->in_msync_mode &&
		(req_id - sync_link->req.in_q->slot[sync_rd_idx].req_id >
		link->max_delay - sync_link->max_delay)) {
		CAM_DBG(CAM_CRM,
			"Req: %lld on link:%x need to hold for link: %x req:%d",
			req_id,
			link->link_hdl,
			sync_link->link_hdl,
			sync_link->req.in_q->slot[sync_rd_idx].req_id);
		return -EINVAL;
	}

	if (link->is_master) {
		rc = __cam_req_mgr_inject_delay(link->req.l_tbl, slot->idx);
		if (rc) {
			CAM_DBG(CAM_CRM,
				"Skip Process Req: %lld on link: %x",
				req_id, link->link_hdl);
			link->sync_link_sof_skip = true;
			return rc;
		}

		if (sync_link->initial_skip) {
			CAM_DBG(CAM_CRM,  "Link 0x%x [slave] not streamed on",
				sync_link->link_hdl);
			return -EAGAIN;
		}

		rc = __cam_req_mgr_check_link_is_ready(link, slot->idx, true);
		if (rc) {
			CAM_DBG(CAM_CRM,
				"Req: %lld [master] not ready on link: %x, rc=%d",
				req_id, link->link_hdl, rc);
			link->sync_link_sof_skip = true;
			return rc;
		}

		prev_idx = slot->idx;
		__cam_req_mgr_dec_idx(&prev_idx,
			(link->max_delay - sync_link->max_delay),
			link->req.in_q->num_slots);

		rd_idx = sync_link->req.in_q->rd_idx;
		sync_req_id = link->req.in_q->slot[prev_idx].req_id;
		if ((sync_link->initial_sync_req != -1) &&
			(sync_link->initial_sync_req <= sync_req_id)) {
			sync_slot_idx = __cam_req_mgr_find_slot_for_req(
				sync_link->req.in_q, sync_req_id);

			if (sync_slot_idx == -1) {
				CAM_DBG(CAM_CRM,
					"Prev Req: %lld [master] not found on link: %x [slave]",
					sync_req_id, sync_link->link_hdl);
				link->sync_link_sof_skip = true;
				return -EINVAL;
			}

			if ((sync_link->req.in_q->slot[sync_slot_idx].status !=
				CRM_SLOT_STATUS_REQ_APPLIED) &&
				(((sync_slot_idx - rd_idx + sync_num_slots) %
				sync_num_slots) >= 1) &&
				(sync_link->req.in_q->slot[rd_idx].status !=
				CRM_SLOT_STATUS_REQ_APPLIED)) {
				CAM_DBG(CAM_CRM,
					"Prev Req: %lld [master] not next on link: %x [slave]",
					sync_req_id,
					sync_link->link_hdl);
				return -EINVAL;
			}

			rc = __cam_req_mgr_check_link_is_ready(sync_link,
				sync_slot_idx, true);
			if (rc &&
				(sync_link->req.in_q->slot[sync_slot_idx].status
				!= CRM_SLOT_STATUS_REQ_APPLIED)) {
				CAM_DBG(CAM_CRM,
					"Req: %lld not ready on [slave] link: %x, rc=%d",
					sync_req_id, sync_link->link_hdl, rc);
				link->sync_link_sof_skip = true;
				return rc;
			}
		}
	} else {
		if (link->initial_skip)
			link->initial_skip = false;

		rc = __cam_req_mgr_inject_delay(link->req.l_tbl, slot->idx);
		if (rc) {
			CAM_DBG(CAM_CRM,
				"Skip Process Req: %lld on link: %x",
				req_id, link->link_hdl);
			link->sync_link_sof_skip = true;
			return rc;
		}

		rc = __cam_req_mgr_check_link_is_ready(link, slot->idx, true);
		if (rc) {
			CAM_DBG(CAM_CRM,
				"Req: %lld [slave] not ready on link: %x, rc=%d",
				req_id, link->link_hdl, rc);
			link->sync_link_sof_skip = true;
			return rc;
		}

		next_idx = link->req.in_q->rd_idx;
		rd_idx = sync_link->req.in_q->rd_idx;
		__cam_req_mgr_inc_idx(&next_idx,
			(sync_link->max_delay - link->max_delay),
			link->req.in_q->num_slots);

		sync_req_id = link->req.in_q->slot[next_idx].req_id;

		if ((sync_link->initial_sync_req != -1) &&
			(sync_link->initial_sync_req <= sync_req_id)) {
			sync_slot_idx = __cam_req_mgr_find_slot_for_req(
				sync_link->req.in_q, sync_req_id);
			if (sync_slot_idx == -1) {
				CAM_DBG(CAM_CRM,
					"Next Req: %lld [slave] not found on link: %x [master]",
					sync_req_id, sync_link->link_hdl);
				link->sync_link_sof_skip = true;
				return -EINVAL;
			}

			if ((sync_link->req.in_q->slot[sync_slot_idx].status !=
				CRM_SLOT_STATUS_REQ_APPLIED) &&
				(((sync_slot_idx - rd_idx + sync_num_slots) %
				sync_num_slots) >= 1) &&
				(sync_link->req.in_q->slot[rd_idx].status !=
				CRM_SLOT_STATUS_REQ_APPLIED)) {
				CAM_DBG(CAM_CRM,
					"Next Req: %lld [slave] not next on link: %x [master]",
					sync_req_id, sync_link->link_hdl);
				return -EINVAL;
			}

			sync_slot = &sync_link->req.in_q->slot[sync_slot_idx];
			rc = __cam_req_mgr_check_link_is_ready(sync_link,
				sync_slot_idx, true);
			if (rc && (sync_slot->status !=
				CRM_SLOT_STATUS_REQ_APPLIED)) {
				CAM_DBG(CAM_CRM,
					"Next Req: %lld [slave] not ready on [master] link: %x, rc=%d",
					sync_req_id, sync_link->link_hdl, rc);
				link->sync_link_sof_skip = true;
				return rc;
			}
		}
	}

	CAM_DBG(CAM_REQ,
		"Req: %lld ready to apply on link: %x [validation successful]",
		req_id, link->link_hdl);

	/*
	 *  At this point all validation is successfully done
	 *  and we can proceed to apply the given request.
	 *  Ideally the next call should return success.
	 */
	rc = __cam_req_mgr_check_link_is_ready(link, slot->idx, false);
	if (rc)
		CAM_WARN(CAM_CRM, "Unexpected return value rc: %d", rc);

	return 0;
}


/**
 * __cam_req_mgr_check_sync_request_is_ready()
 *
 * @brief    : processes requests during sync mode
 * @link     : pointer to link whose input queue and req tbl are
 *             traversed through
 * @slot     : pointer to the current slot being processed
 * @return   : 0 for success, negative for failure
 *
 */
static int __cam_req_mgr_check_sync_req_is_ready(
	struct cam_req_mgr_core_link *link,
	struct cam_req_mgr_slot *slot)
{
	struct cam_req_mgr_core_link *sync_link = NULL;
	struct cam_req_mgr_slot *sync_rd_slot = NULL;
	int64_t req_id = 0, sync_req_id = 0;
	int sync_slot_idx = 0, sync_rd_idx = 0, rc = 0;
	int32_t sync_num_slots = 0;
	uint64_t sync_frame_duration = 0;
	uint64_t sof_timestamp_delta = 0;
	uint64_t master_slave_diff = 0;
	bool ready = true, sync_ready = true;

	if (!link->sync_link) {
		CAM_ERR(CAM_CRM, "Sync link null");
		return -EINVAL;
	}

	sync_link = link->sync_link;
	req_id = slot->req_id;
	sync_num_slots = sync_link->req.in_q->num_slots;
	sync_rd_idx = sync_link->req.in_q->rd_idx;
	sync_rd_slot = &sync_link->req.in_q->slot[sync_rd_idx];
	sync_req_id = sync_rd_slot->req_id;

	CAM_DBG(CAM_REQ,
		"link_hdl %x req %lld frame_skip_flag %d ",
		link->link_hdl, req_id, link->sync_link_sof_skip);

	if (sync_link->initial_skip) {
		link->initial_skip = false;
		__cam_req_mgr_inject_delay(link->req.l_tbl, slot->idx);
		CAM_DBG(CAM_CRM,
			"sync link %x not streamed on",
			sync_link->link_hdl);
		return -EAGAIN;
	}

	if (sync_link->prev_sof_timestamp)
		sync_frame_duration = sync_link->sof_timestamp -
			sync_link->prev_sof_timestamp;
	else
		sync_frame_duration = DEFAULT_FRAME_DURATION;

	sof_timestamp_delta =
		link->sof_timestamp >= sync_link->sof_timestamp
		? link->sof_timestamp - sync_link->sof_timestamp
		: sync_link->sof_timestamp - link->sof_timestamp;

	CAM_DBG(CAM_CRM,
		"sync link %x last frame_duration is %d ns",
		sync_link->link_hdl, sync_frame_duration);

	if (link->initial_skip) {
		link->initial_skip = false;

		if ((link->sof_timestamp > sync_link->sof_timestamp) &&
			(sync_link->sof_timestamp > 0) &&
			(link->sof_timestamp - sync_link->sof_timestamp) <
			(sync_frame_duration / 2)) {
			/*
			 * If this frame sync with the previous frame of sync
			 * link, then we need to skip this frame, since the
			 * previous frame of sync link is also skipped.
			 */
			__cam_req_mgr_inject_delay(link->req.l_tbl, slot->idx);
			CAM_DBG(CAM_CRM,
				"This frame sync with previous sync_link %x frame",
				sync_link->link_hdl);
			return -EAGAIN;
		} else if (link->sof_timestamp <= sync_link->sof_timestamp) {
			/*
			 * Sometimes, link receives the SOF event is eariler
			 * than sync link in IFE CSID side, but link's SOF
			 * event is processed later than sync link's, then
			 * we need to skip this SOF event since the sync
			 * link's SOF event is also skipped.
			 */
			__cam_req_mgr_inject_delay(link->req.l_tbl, slot->idx);
			CAM_DBG(CAM_CRM,
				"The previous frame of sync link is skipped");
			return -EAGAIN;
		}
	}

	if (sync_link->sync_link_sof_skip) {
		CAM_DBG(CAM_REQ,
			"No req applied on corresponding SOF on sync link: %x",
			sync_link->link_hdl);
		sync_link->sync_link_sof_skip = false;
		__cam_req_mgr_inject_delay(link->req.l_tbl, slot->idx);
		return -EAGAIN;
	}

	rc = __cam_req_mgr_inject_delay(link->req.l_tbl, slot->idx);
	if (rc) {
		CAM_DBG(CAM_CRM,
			"Skip Process Req: %lld on link: %x",
			req_id, link->link_hdl);
		ready = false;
	}

	sync_slot_idx = __cam_req_mgr_find_slot_for_req(
		sync_link->req.in_q, req_id);
	if (sync_slot_idx == -1) {
		CAM_DBG(CAM_CRM, "Req: %lld not found on link: %x [other link]",
			req_id, sync_link->link_hdl);
		sync_ready = false;
		return -EAGAIN;
	}

	if ((sync_link->req.in_q->slot[sync_slot_idx].status !=
		CRM_SLOT_STATUS_REQ_APPLIED) &&
		(((sync_slot_idx - sync_rd_idx + sync_num_slots) %
		sync_num_slots) >= 1) &&
		(sync_rd_slot->status !=
		CRM_SLOT_STATUS_REQ_APPLIED)) {
		CAM_DBG(CAM_CRM,
			"Req: %lld [other link] not next req to be applied on link: %x",
			req_id, sync_link->link_hdl);
		return -EAGAIN;
	}

	rc = __cam_req_mgr_check_link_is_ready(link, slot->idx, true);
	if (rc) {
		CAM_DBG(CAM_CRM,
			"Req: %lld [My link] not ready on link: %x, rc=%d",
			req_id, link->link_hdl, rc);
		ready = false;
	}

	rc = __cam_req_mgr_check_link_is_ready(sync_link, sync_slot_idx, true);
	if (rc && (sync_link->req.in_q->slot[sync_slot_idx].status !=
		CRM_SLOT_STATUS_REQ_APPLIED)) {
		CAM_DBG(CAM_CRM,
			"Req: %lld not ready on [other link] link: %x, rc=%d",
			req_id, sync_link->link_hdl, rc);
		sync_ready = false;
	}

	/*
	 * If both of them are ready or not ready, then just
	 * skip this sof and don't skip sync link next SOF.
	 */
	if (sync_ready != ready) {
		CAM_DBG(CAM_CRM,
			"Req: %lld ready %d sync_ready %d, ignore sync link next SOF",
			req_id, ready, sync_ready);

		/*
		 * Only skip the frames if current frame sync with
		 * next frame of sync link.
		 */
		if (link->sof_timestamp - sync_link->sof_timestamp >
			sync_frame_duration / 2)
			link->sync_link_sof_skip = true;
		return -EINVAL;
	} else if (ready == false) {
		CAM_DBG(CAM_CRM,
			"Req: %lld not ready on link: %x",
			req_id, link->link_hdl);
		return -EINVAL;
	}

	/*
	 * Do the self-correction when the frames are sync,
	 * we consider that the frames are synced if the
	 * difference of two SOF timestamp less than
	 * (sync_frame_duration / 5).
	 */
	master_slave_diff = sync_frame_duration;
	do_div(master_slave_diff, 5);
	if ((sync_link->sof_timestamp > 0) &&
		(sof_timestamp_delta < master_slave_diff) &&
		(sync_rd_slot->sync_mode == CAM_REQ_MGR_SYNC_MODE_SYNC)) {

		/*
		 * This means current frame should sync with next
		 * frame of sync link, then the request id of in
		 * rd slot of two links should be same.
		 */
		CAM_DBG(CAM_CRM,
			"link %x req_id %lld, sync_link %x req_id %lld",
			link->link_hdl, req_id,
			sync_link->link_hdl, sync_req_id);

		if (req_id > sync_req_id) {
			CAM_DBG(CAM_CRM,
				"link %x too quickly, skip this frame",
				link->link_hdl);
			return -EAGAIN;
		} else if (req_id < sync_req_id) {
			CAM_DBG(CAM_CRM,
				"sync link %x too quickly, skip next frame of sync link",
				sync_link->link_hdl);
			link->sync_link_sof_skip = true;
		} else if (sync_link->req.in_q->slot[sync_slot_idx].status !=
			CRM_SLOT_STATUS_REQ_APPLIED) {
			CAM_DBG(CAM_CRM,
				"link %x other not applied", link->link_hdl);
			return -EAGAIN;
		}
	}

	CAM_DBG(CAM_REQ,
		"Req: %lld ready to apply on link: %x [validation successful]",
		req_id, link->link_hdl);

	/*
	 *  At this point all validation is successfully done
	 *  and we can proceed to apply the given request.
	 *  Ideally the next call should return success.
	 */
	rc = __cam_req_mgr_check_link_is_ready(link, slot->idx, false);
	if (rc)
		CAM_WARN(CAM_CRM, "Unexpected return value rc: %d", rc);

	return 0;
}

/**
 * __cam_req_mgr_process_req()
 *
 * @brief    : processes read index in request queue and traverse through table
 * @link     : pointer to link whose input queue and req tbl are
 *             traversed through
 *
 * @return   : 0 for success, negative for failure
 *
 */
static int __cam_req_mgr_process_req(struct cam_req_mgr_core_link *link,
	struct cam_req_mgr_trigger_notify *trigger_data)
{
	int                                  rc = 0, idx;
	int                                  reset_step = 0;
	uint32_t                             trigger = trigger_data->trigger;
	struct cam_req_mgr_slot             *slot = NULL;
	struct cam_req_mgr_req_queue        *in_q;
	struct cam_req_mgr_core_session     *session;
	struct cam_req_mgr_connected_device *dev;

	in_q = link->req.in_q;
	session = (struct cam_req_mgr_core_session *)link->parent;
	mutex_lock(&session->lock);
	/*
	 * Check if new read index,
	 * - if in pending  state, traverse again to complete
	 *    transaction of this read index.
	 * - if in applied_state, somthign wrong.
	 * - if in no_req state, no new req
	 */
	CAM_DBG(CAM_REQ,
		"SOF Req[%lld] idx %d req_status %d link_hdl %x wd_timeout %d ms",
		in_q->slot[in_q->rd_idx].req_id, in_q->rd_idx,
		in_q->slot[in_q->rd_idx].status, link->link_hdl,
		in_q->slot[in_q->rd_idx].additional_timeout);

	slot = &in_q->slot[in_q->rd_idx];
	if (slot->status == CRM_SLOT_STATUS_NO_REQ) {
		CAM_DBG(CAM_CRM, "No Pending req");
		rc = 0;
		goto error;
	}

	if ((trigger != CAM_TRIGGER_POINT_SOF) &&
		(trigger != CAM_TRIGGER_POINT_EOF))
		goto error;

	if ((trigger == CAM_TRIGGER_POINT_EOF) &&
		(!(link->trigger_mask & CAM_TRIGGER_POINT_SOF))) {
		CAM_DBG(CAM_CRM, "Applying for last SOF fails");
		rc = -EINVAL;
		goto error;
	}

	if (trigger == CAM_TRIGGER_POINT_SOF) {
		/*
		 * Update the timestamp in session lock protection
		 * to avoid timing issue.
		 */
		link->prev_sof_timestamp = link->sof_timestamp;
		link->sof_timestamp = trigger_data->sof_timestamp_val;

		if (link->trigger_mask) {
			CAM_ERR_RATE_LIMIT(CAM_CRM,
				"Applying for last EOF fails");
			rc = -EINVAL;
			goto error;
		}

		if ((slot->sync_mode == CAM_REQ_MGR_SYNC_MODE_SYNC) &&
			(link->sync_link)) {
			if (link->is_master || link->sync_link->is_master) {
				if (!link->in_msync_mode) {
					CAM_DBG(CAM_CRM,
						"Settings master-slave sync mode for link 0x%x",
						link->link_hdl);
					link->in_msync_mode = true;
				}

				rc =  __cam_req_mgr_check_sync_for_mslave(
					link, slot);
			} else {
				rc = __cam_req_mgr_check_sync_req_is_ready(
					link, slot);
			}
		} else {
			if (link->in_msync_mode) {
				CAM_DBG(CAM_CRM,
					"Settings master-slave non sync mode for link 0x%x",
					link->link_hdl);
				link->in_msync_mode = false;
				link->initial_sync_req = -1;
				if (link->sync_link) {
					link->sync_link->initial_sync_req = -1;
					link->sync_link->in_msync_mode = false;
				}
			}

			rc = __cam_req_mgr_inject_delay(link->req.l_tbl,
				slot->idx);
			if (!rc)
				rc = __cam_req_mgr_check_link_is_ready(link,
					slot->idx, false);
		}

		if (rc < 0) {
			/*
			 * If traverse result is not success, then some devices
			 * are not ready with packet for the asked request id,
			 * hence try again in next sof
			 */
			slot->status = CRM_SLOT_STATUS_REQ_PENDING;
			spin_lock_bh(&link->link_state_spin_lock);
			if (link->state == CAM_CRM_LINK_STATE_ERR) {
				/*
				 * During error recovery all tables should be
				 * ready, don't expect to enter here.
				 * @TODO: gracefully handle if recovery fails.
				 */
				CAM_ERR_RATE_LIMIT(CAM_CRM,
					"FATAL recovery cant finish idx %d status %d",
					in_q->rd_idx,
					in_q->slot[in_q->rd_idx].status);
				rc = -EPERM;
			}
			spin_unlock_bh(&link->link_state_spin_lock);
			goto error;
		}
	}

	rc = __cam_req_mgr_send_req(link, link->req.in_q, trigger, &dev);
	if (rc < 0) {
		/* Apply req failed retry at next sof */
		slot->status = CRM_SLOT_STATUS_REQ_PENDING;

		link->retry_cnt++;
		if (link->retry_cnt == MAXIMUM_RETRY_ATTEMPTS) {
			CAM_DBG(CAM_CRM,
				"Max retry attempts reached on link[0x%x] for req [%lld]",
				link->link_hdl,
				in_q->slot[in_q->rd_idx].req_id);
			__cam_req_mgr_notify_error_on_link(link, dev);
			link->retry_cnt = 0;
		}
	} else {
		if (link->retry_cnt)
			link->retry_cnt = 0;

		link->trigger_mask |= trigger;

		/* Check for any long exposure settings */
		__cam_req_mgr_validate_crm_wd_timer(link);

		CAM_DBG(CAM_CRM, "Applied req[%lld] on link[%x] success",
			slot->req_id, link->link_hdl);
		spin_lock_bh(&link->link_state_spin_lock);
		if (link->state == CAM_CRM_LINK_STATE_ERR) {
			CAM_WARN(CAM_CRM, "Err recovery done idx %d",
				in_q->rd_idx);
			link->state = CAM_CRM_LINK_STATE_READY;
		}
		spin_unlock_bh(&link->link_state_spin_lock);

		if (link->sync_link_sof_skip)
			link->sync_link_sof_skip = false;

		if (link->trigger_mask == link->subscribe_event) {
			slot->status = CRM_SLOT_STATUS_REQ_APPLIED;
			link->trigger_mask = 0;
			CAM_DBG(CAM_CRM, "req %d is applied on link %x",
				slot->req_id,
				link->link_hdl);
			idx = in_q->rd_idx;
			reset_step = link->max_delay;
			if (link->sync_link) {
				if ((link->in_msync_mode) &&
					(link->sync_link->is_master))
					reset_step =
						link->sync_link->max_delay;
			}
			__cam_req_mgr_dec_idx(
				&idx, reset_step + 1,
				in_q->num_slots);
			__cam_req_mgr_reset_req_slot(link, idx);
			link->open_req_cnt--;
		}
	}

	mutex_unlock(&session->lock);
	return rc;
error:
	mutex_unlock(&session->lock);
	return rc;
}

/**
 * __cam_req_mgr_add_tbl_to_link()
 *
 * @brief    : Add table to list under link sorted by pd decremeting order
 * @l_tbl    : list of pipeline delay tables.
 * @new_tbl  : new tbl which will be appended to above list as per its pd value
 *
 */
static void __cam_req_mgr_add_tbl_to_link(struct cam_req_mgr_req_tbl **l_tbl,
	struct cam_req_mgr_req_tbl *new_tbl)
{
	struct cam_req_mgr_req_tbl *tbl;

	if (!(*l_tbl) || (*l_tbl)->pd < new_tbl->pd) {
		new_tbl->next = *l_tbl;
		if (*l_tbl) {
			new_tbl->pd_delta =
				new_tbl->pd - (*l_tbl)->pd;
		}
		*l_tbl = new_tbl;
	} else {
		tbl = *l_tbl;

		/* Reach existing  tbl which has less pd value */
		while (tbl->next != NULL &&
			new_tbl->pd < tbl->next->pd) {
			tbl = tbl->next;
		}
		if (tbl->next != NULL) {
			new_tbl->pd_delta =
				new_tbl->pd - tbl->next->pd;
		} else {
			/* This is last table in linked list*/
			new_tbl->pd_delta = 0;
		}
		new_tbl->next = tbl->next;
		tbl->next = new_tbl;
		tbl->pd_delta = tbl->pd - new_tbl->pd;
	}
	CAM_DBG(CAM_CRM, "added pd %d tbl to link delta %d", new_tbl->pd,
		new_tbl->pd_delta);
}

/**
 * __cam_req_mgr_create_pd_tbl()
 *
 * @brief    : Creates new request table for new delay value
 * @delay    : New pd table allocated will have this delay value
 *
 * @return   : pointer to newly allocated table, NULL for failure
 *
 */
static struct cam_req_mgr_req_tbl *__cam_req_mgr_create_pd_tbl(int32_t delay)
{
	struct cam_req_mgr_req_tbl *tbl =
		kzalloc(sizeof(struct cam_req_mgr_req_tbl), GFP_KERNEL);
	if (tbl != NULL) {
		tbl->num_slots = MAX_REQ_SLOTS;
		CAM_DBG(CAM_CRM, "pd= %d slots= %d", delay, tbl->num_slots);
	}

	return tbl;
}

/**
 * __cam_req_mgr_destroy_all_tbl()
 *
 * @brief   : This func will destroy all pipeline delay based req table structs
 * @l_tbl    : pointer to first table in list and it has max pd .
 *
 */
static void __cam_req_mgr_destroy_all_tbl(struct cam_req_mgr_req_tbl **l_tbl)
{
	struct cam_req_mgr_req_tbl  *tbl = *l_tbl, *temp;

	CAM_DBG(CAM_CRM, "*l_tbl %pK", tbl);
	while (tbl != NULL) {
		temp = tbl->next;
		kfree(tbl);
		tbl = temp;
	}
	*l_tbl = NULL;
}

/**
 * __cam_req_mgr_setup_in_q()
 *
 * @brief : Initialize req table data
 * @req   : request data pointer
 *
 * @return: 0 for success, negative for failure
 *
 */
static int  __cam_req_mgr_setup_in_q(struct cam_req_mgr_req_data *req)
{
	int                           i;
	struct cam_req_mgr_req_queue *in_q = req->in_q;

	if (!in_q) {
		CAM_ERR(CAM_CRM, "NULL in_q");
		return -EINVAL;
	}

	mutex_lock(&req->lock);
	in_q->num_slots = MAX_REQ_SLOTS;

	for (i = 0; i < in_q->num_slots; i++) {
		in_q->slot[i].idx = i;
		in_q->slot[i].req_id = -1;
		in_q->slot[i].skip_idx = 0;
		in_q->slot[i].status = CRM_SLOT_STATUS_NO_REQ;
	}

	in_q->wr_idx = 0;
	in_q->rd_idx = 0;
	mutex_unlock(&req->lock);

	return 0;
}

/**
 * __cam_req_mgr_reset_req_tbl()
 *
 * @brief : Initialize req table data
 * @req   : request queue pointer
 *
 * @return: 0 for success, negative for failure
 *
 */
static int __cam_req_mgr_reset_in_q(struct cam_req_mgr_req_data *req)
{
	struct cam_req_mgr_req_queue *in_q = req->in_q;

	if (!in_q) {
		CAM_ERR(CAM_CRM, "NULL in_q");
		return -EINVAL;
	}

	mutex_lock(&req->lock);
	memset(in_q->slot, 0,
		sizeof(struct cam_req_mgr_slot) * in_q->num_slots);
	in_q->num_slots = 0;

	in_q->wr_idx = 0;
	in_q->rd_idx = 0;
	mutex_unlock(&req->lock);

	return 0;
}

/**
 * __cam_req_mgr_notify_sof_freeze()
 *
 * @brief : Notify devices on link on detecting a SOF freeze
 * @link  : link on which the sof freeze was detected
 *
 */
static void __cam_req_mgr_notify_sof_freeze(
	struct cam_req_mgr_core_link *link)
{
	int                                  i = 0;
	struct cam_req_mgr_link_evt_data     evt_data;
	struct cam_req_mgr_connected_device *dev = NULL;

	for (i = 0; i < link->num_devs; i++) {
		dev = &link->l_dev[i];
		evt_data.evt_type = CAM_REQ_MGR_LINK_EVT_SOF_FREEZE;
		evt_data.dev_hdl = dev->dev_hdl;
		evt_data.link_hdl =  link->link_hdl;
		evt_data.req_id = 0;
		evt_data.u.error = CRM_KMD_ERR_FATAL;
		if (dev->ops && dev->ops->process_evt)
			dev->ops->process_evt(&evt_data);
	}
}

/**
 * __cam_req_mgr_process_sof_freeze()
 *
 * @brief : Apoptosis - Handles case when connected devices are not responding
 * @priv  : link information
 * @data  : task data
 *
 */
static int __cam_req_mgr_process_sof_freeze(void *priv, void *data)
{
	struct cam_req_mgr_core_link    *link = NULL;
	struct cam_req_mgr_core_session *session = NULL;
	struct cam_req_mgr_message       msg;
	int rc = 0;

	if (!data || !priv) {
		CAM_ERR(CAM_CRM, "input args NULL %pK %pK", data, priv);
		return -EINVAL;
	}

	link = (struct cam_req_mgr_core_link *)priv;
	session = (struct cam_req_mgr_core_session *)link->parent;

	CAM_ERR(CAM_CRM, "SOF freeze for session %d link 0x%x",
		session->session_hdl, link->link_hdl);

	__cam_req_mgr_notify_sof_freeze(link);
	memset(&msg, 0, sizeof(msg));

	msg.session_hdl = session->session_hdl;
	msg.u.err_msg.error_type = CAM_REQ_MGR_ERROR_TYPE_SOF_FREEZE;
	msg.u.err_msg.request_id = 0;
	msg.u.err_msg.link_hdl   = link->link_hdl;

	rc = cam_req_mgr_notify_message(&msg,
		V4L_EVENT_CAM_REQ_MGR_ERROR, V4L_EVENT_CAM_REQ_MGR_EVENT);

	if (rc)
		CAM_ERR(CAM_CRM,
			"Error notifying SOF freeze for session %d link 0x%x rc %d",
			session->session_hdl, link->link_hdl, rc);

	return rc;
}

/**
 * __cam_req_mgr_sof_freeze()
 *
 * @brief : Callback function for timer timeout indicating SOF freeze
 * @data  : timer pointer
 *
 */
static void __cam_req_mgr_sof_freeze(struct timer_list *timer_data)
{
	struct cam_req_mgr_timer     *timer =
		container_of(timer_data, struct cam_req_mgr_timer, sys_timer);
	struct crm_workq_task               *task = NULL;
	struct cam_req_mgr_core_link        *link = NULL;
	struct crm_task_payload             *task_data;

	if (!timer) {
		CAM_ERR(CAM_CRM, "NULL timer");
		return;
	}

	link = (struct cam_req_mgr_core_link *)timer->parent;

	if (link->watchdog->pause_timer)
		return;

	task = cam_req_mgr_workq_get_task(link->workq);
	if (!task) {
		CAM_ERR(CAM_CRM, "No empty task");
		return;
	}

	task_data = (struct crm_task_payload *)task->payload;
	task_data->type = CRM_WORKQ_TASK_NOTIFY_FREEZE;
	task->process_cb = &__cam_req_mgr_process_sof_freeze;
	cam_req_mgr_workq_enqueue_task(task, link, CRM_TASK_PRIORITY_0);
}

/**
 * __cam_req_mgr_create_subdevs()
 *
 * @brief   : Create new crm  subdev to link with realtime devices
 * @l_dev   : list of subdevs internal to crm
 * @num_dev : num of subdevs to be created for link
 *
 * @return  : pointer to allocated list of devices
 */
static int __cam_req_mgr_create_subdevs(
	struct cam_req_mgr_connected_device **l_dev, int32_t num_dev)
{
	int rc = 0;
	*l_dev = kzalloc(sizeof(struct cam_req_mgr_connected_device) *
		num_dev, GFP_KERNEL);
	if (!*l_dev)
		rc = -ENOMEM;

	return rc;
}

/**
 * __cam_req_mgr_destroy_subdev()
 *
 * @brief    : Cleans up the subdevs allocated by crm for link
 * @l_device : pointer to list of subdevs crm created
 *
 */
static void __cam_req_mgr_destroy_subdev(
	struct cam_req_mgr_connected_device *l_device)
{
	kfree(l_device);
	l_device = NULL;
}

/**
 * __cam_req_mgr_destroy_link_info()
 *
 * @brief    : Unlinks all devices on the link
 * @link     : pointer to link
 *
 * @return   : returns if unlink for any device was success or failure
 */
static int __cam_req_mgr_disconnect_link(struct cam_req_mgr_core_link *link)
{
	int32_t                                 i = 0;
	struct cam_req_mgr_connected_device    *dev;
	struct cam_req_mgr_core_dev_link_setup  link_data;
	int                                     rc = 0;

	link_data.link_enable = 0;
	link_data.link_hdl = link->link_hdl;
	link_data.crm_cb = NULL;
	link_data.subscribe_event = 0;

	/* Using device ops unlink devices */
	for (i = 0; i < link->num_devs; i++) {
		dev = &link->l_dev[i];
		if (dev == NULL)
			continue;

		link_data.dev_hdl = dev->dev_hdl;
		if (dev->ops && dev->ops->link_setup) {
			rc = dev->ops->link_setup(&link_data);
			if (rc)
				CAM_ERR(CAM_CRM,
					"Unlink failed dev name %s hdl %x",
					dev->dev_info.name,
					dev->dev_hdl);
		}
		dev->dev_hdl = 0;
		dev->parent = NULL;
		dev->ops = NULL;
	}

	return rc;
}

/**
 * __cam_req_mgr_destroy_link_info()
 *
 * @brief    : Cleans up the mem allocated while linking
 * @link     : pointer to link, mem associated with this link is freed
 */
static void __cam_req_mgr_destroy_link_info(struct cam_req_mgr_core_link *link)
{
	__cam_req_mgr_destroy_all_tbl(&link->req.l_tbl);
	__cam_req_mgr_reset_in_q(&link->req);
	link->req.num_tbl = 0;
	mutex_destroy(&link->req.lock);

	link->pd_mask = 0;
	link->num_devs = 0;
	link->max_delay = 0;
}

/**
 * __cam_req_mgr_reserve_link()
 *
 * @brief: Reserves one link data struct within session
 * @session: session identifier
 *
 * @return: pointer to link reserved
 *
 */
static struct cam_req_mgr_core_link *__cam_req_mgr_reserve_link(
	struct cam_req_mgr_core_session *session)
{
	struct cam_req_mgr_core_link *link;
	struct cam_req_mgr_req_queue *in_q;
	int i;

	if (!session || !g_crm_core_dev) {
		CAM_ERR(CAM_CRM, "NULL session/core_dev ptr");
		return NULL;
	}

	if (session->num_links >= MAXIMUM_LINKS_PER_SESSION) {
		CAM_ERR(CAM_CRM, "Reached max links %d per session limit %d",
			session->num_links, MAXIMUM_LINKS_PER_SESSION);
		return NULL;
	}
	for (i = 0; i < MAXIMUM_LINKS_PER_SESSION; i++) {
		if (!atomic_cmpxchg(&g_links[i].is_used, 0, 1)) {
			link = &g_links[i];
			CAM_DBG(CAM_CRM, "alloc link index %d", i);
			CAM_DBG(CAM_CRM, "__cam_req_mgr_reserve_link :%p",
				link);
			cam_req_mgr_core_link_reset(link);
			break;
		}
	}
	if (i == MAXIMUM_LINKS_PER_SESSION)
		return NULL;

	in_q = kzalloc(sizeof(struct cam_req_mgr_req_queue),
		GFP_KERNEL);
	if (!in_q) {
		CAM_ERR(CAM_CRM, "failed to create input queue, no mem");
		return NULL;
	}

	mutex_lock(&link->lock);
	link->num_devs = 0;
	link->max_delay = 0;
	memset(in_q->slot, 0,
		sizeof(struct cam_req_mgr_slot) * MAX_REQ_SLOTS);
	link->req.in_q = in_q;
	in_q->num_slots = 0;
	link->state = CAM_CRM_LINK_STATE_IDLE;
	link->parent = (void *)session;
	link->sync_link = NULL;
	mutex_unlock(&link->lock);

	mutex_lock(&session->lock);
	/*  Loop through and find a free index */
	for (i = 0; i < MAXIMUM_LINKS_PER_SESSION; i++) {
		if (!session->links[i]) {
			CAM_DBG(CAM_CRM,
				"Free link index %d found, num_links=%d",
				i, session->num_links);
			session->links[i] = link;
			break;
		}
	}

	if (i == MAXIMUM_LINKS_PER_SESSION) {
		CAM_ERR(CAM_CRM, "Free link index not found");
		goto error;
	}

	session->num_links++;
	CAM_DBG(CAM_CRM, "Active session links (%d)",
		session->num_links);
	mutex_unlock(&session->lock);

	return link;
error:
	mutex_unlock(&session->lock);
	kfree(in_q);
	return NULL;
}

/*
 * __cam_req_mgr_free_link()
 *
 * @brief: Frees the link and its request queue
 *
 * @link: link identifier
 *
 */
static void __cam_req_mgr_free_link(struct cam_req_mgr_core_link *link)
{
	ptrdiff_t i;
	kfree(link->req.in_q);
	link->req.in_q = NULL;
	i = link - g_links;
	CAM_DBG(CAM_CRM, "free link index %d", i);
	CAM_DBG(CAM_CRM, "__cam_req_mgr_free_link :%p", link);
	cam_req_mgr_core_link_reset(link);
	atomic_set(&g_links[i].is_used, 0);
}

/**
 * __cam_req_mgr_unreserve_link()
 *
 * @brief  : Removes the link data struct from the session and frees it
 * @session: session identifier
 * @link   : link identifier
 *
 */
static void __cam_req_mgr_unreserve_link(
	struct cam_req_mgr_core_session *session,
	struct cam_req_mgr_core_link *link)
{
	int i;

	if (!session || !link) {
		CAM_ERR(CAM_CRM, "NULL session/link ptr %pK %pK",
			session, link);
		return;
	}

	mutex_lock(&session->lock);
	if (!session->num_links) {
		CAM_WARN(CAM_CRM, "No active link or invalid state: hdl %x",
			link->link_hdl);
		mutex_unlock(&session->lock);
		return;
	}

	for (i = 0; i < MAXIMUM_LINKS_PER_SESSION; i++) {
		if (session->links[i] == link)
			session->links[i] = NULL;

		if (link->sync_link) {
			if (link->sync_link == session->links[i])
				session->links[i]->sync_link = NULL;
		}
	}

	link->sync_link = NULL;
	session->num_links--;
	CAM_DBG(CAM_CRM, "Active session links (%d)", session->num_links);
	mutex_unlock(&session->lock);
	__cam_req_mgr_free_link(link);
}

/* Workqueue context processing section */

/**
 * cam_req_mgr_process_send_req()
 *
 * @brief: This runs in workque thread context. Call core funcs to send
 *         apply request id to drivers.
 * @priv : link information.
 * @data : contains information about frame_id, link etc.
 *
 * @return: 0 on success.
 */
int cam_req_mgr_process_send_req(void *priv, void *data)
{
	int                                 rc = 0;
	struct cam_req_mgr_core_link        *link = NULL;
	struct cam_req_mgr_send_request     *send_req = NULL;
	struct cam_req_mgr_req_queue        *in_q = NULL;
	struct cam_req_mgr_connected_device *dev;

	if (!data || !priv) {
		CAM_ERR(CAM_CRM, "input args NULL %pK %pK", data, priv);
		rc = -EINVAL;
		goto end;
	}
	link = (struct cam_req_mgr_core_link *)priv;
	send_req = (struct cam_req_mgr_send_request *)data;
	in_q = send_req->in_q;

	rc = __cam_req_mgr_send_req(link, in_q, CAM_TRIGGER_POINT_SOF, &dev);
end:
	return rc;
}

/**
 * cam_req_mgr_process_flush_req()
 *
 * @brief: This runs in workque thread context. Call core funcs to check
 *         which requests need to be removed/cancelled.
 * @priv : link information.
 * @data : contains information about frame_id, link etc.
 *
 * @return: 0 on success.
 */
int cam_req_mgr_process_flush_req(void *priv, void *data)
{
	int                                  rc = 0, i = 0, idx = -1;
	struct cam_req_mgr_flush_info       *flush_info = NULL;
	struct cam_req_mgr_core_link        *link = NULL;
	struct cam_req_mgr_req_queue        *in_q = NULL;
	struct cam_req_mgr_slot             *slot = NULL;
	struct cam_req_mgr_connected_device *device = NULL;
	struct cam_req_mgr_flush_request     flush_req;
	struct crm_task_payload             *task_data = NULL;

	if (!data || !priv) {
		CAM_ERR(CAM_CRM, "input args NULL %pK %pK", data, priv);
		rc = -EINVAL;
		goto end;
	}
	link = (struct cam_req_mgr_core_link *)priv;
	task_data = (struct crm_task_payload *)data;
	flush_info  = (struct cam_req_mgr_flush_info *)&task_data->u;
	CAM_DBG(CAM_REQ, "link_hdl %x req_id %lld type %d",
		flush_info->link_hdl,
		flush_info->req_id,
		flush_info->flush_type);

	in_q = link->req.in_q;

	trace_cam_flush_req(link, flush_info);

	mutex_lock(&link->req.lock);
	if (flush_info->flush_type == CAM_REQ_MGR_FLUSH_TYPE_ALL) {
		link->last_flush_id = flush_info->req_id;
		CAM_INFO(CAM_CRM, "Last request id to flush is %lld",
			flush_info->req_id);
		__cam_req_mgr_flush_req_slot(link);
	} else if (flush_info->flush_type ==
		CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ) {
		idx = __cam_req_mgr_find_slot_for_req(in_q, flush_info->req_id);
		if (idx < 0) {
			CAM_ERR(CAM_CRM, "req_id %lld not found in input queue",
			flush_info->req_id);
		} else {
			CAM_DBG(CAM_CRM, "req_id %lld found at idx %d",
				flush_info->req_id, idx);
			slot = &in_q->slot[idx];
			if (slot->status == CRM_SLOT_STATUS_REQ_PENDING ||
				slot->status == CRM_SLOT_STATUS_REQ_APPLIED) {
				CAM_WARN(CAM_CRM,
					"req_id %lld can not be cancelled",
					flush_info->req_id);
				mutex_unlock(&link->req.lock);
				return -EINVAL;
			}
			slot->additional_timeout = 0;
			__cam_req_mgr_in_q_skip_idx(in_q, idx);
		}
	}

	for (i = 0; i < link->num_devs; i++) {
		device = &link->l_dev[i];
		flush_req.link_hdl = flush_info->link_hdl;
		flush_req.dev_hdl = device->dev_hdl;
		flush_req.req_id = flush_info->req_id;
		flush_req.type = flush_info->flush_type;
		/* @TODO: error return handling from drivers */
		if (device->ops && device->ops->flush_req)
			rc = device->ops->flush_req(&flush_req);
	}
	complete(&link->workq_comp);
	mutex_unlock(&link->req.lock);

end:
	return rc;
}

/**
 * cam_req_mgr_process_sched_req()
 *
 * @brief: This runs in workque thread context. Call core funcs to check
 *         which peding requests can be processed.
 * @priv : link information.
 * @data : contains information about frame_id, link etc.
 *
 * @return: 0 on success.
 */
int cam_req_mgr_process_sched_req(void *priv, void *data)
{
	int                               rc = 0;
	struct cam_req_mgr_sched_request *sched_req = NULL;
	struct cam_req_mgr_core_link     *link = NULL;
	struct cam_req_mgr_req_queue     *in_q = NULL;
	struct cam_req_mgr_slot          *slot = NULL;
	struct crm_task_payload          *task_data = NULL;

	if (!data || !priv) {
		CAM_ERR(CAM_CRM, "input args NULL %pK %pK", data, priv);
		rc = -EINVAL;
		goto end;
	}
	link = (struct cam_req_mgr_core_link *)priv;
	task_data = (struct crm_task_payload *)data;
	sched_req  = (struct cam_req_mgr_sched_request *)&task_data->u;
	in_q = link->req.in_q;

	CAM_DBG(CAM_CRM,
		"link_hdl %x req_id %lld at slot %d sync_mode %d is_master %d exp_timeout_val %d ms",
		sched_req->link_hdl, sched_req->req_id,
		in_q->wr_idx, sched_req->sync_mode,
		link->is_master,
		sched_req->additional_timeout);

	mutex_lock(&link->req.lock);
	slot = &in_q->slot[in_q->wr_idx];

	if (slot->status != CRM_SLOT_STATUS_NO_REQ &&
		slot->status != CRM_SLOT_STATUS_REQ_APPLIED)
		CAM_WARN(CAM_CRM, "in_q overwrite %d", slot->status);

	slot->status = CRM_SLOT_STATUS_REQ_ADDED;
	slot->req_id = sched_req->req_id;
	slot->sync_mode = sched_req->sync_mode;
	slot->skip_idx = 0;
	slot->recover = sched_req->bubble_enable;

	if (sched_req->additional_timeout < 0) {
		CAM_WARN(CAM_CRM,
			"Requested timeout is invalid [%dms]",
			sched_req->additional_timeout);
		slot->additional_timeout = 0;
	} else if (sched_req->additional_timeout >
		CAM_REQ_MGR_WATCHDOG_TIMEOUT_MAX) {
		CAM_WARN(CAM_CRM,
			"Requested timeout [%dms] max supported timeout [%dms] resetting to max",
			sched_req->additional_timeout,
			CAM_REQ_MGR_WATCHDOG_TIMEOUT_MAX);
		slot->additional_timeout = CAM_REQ_MGR_WATCHDOG_TIMEOUT_MAX;
	} else {
		slot->additional_timeout = sched_req->additional_timeout;
	}

	link->open_req_cnt++;
	__cam_req_mgr_inc_idx(&in_q->wr_idx, 1, in_q->num_slots);

	if (slot->sync_mode == CAM_REQ_MGR_SYNC_MODE_SYNC) {
		if (link->initial_sync_req == -1)
			link->initial_sync_req = slot->req_id;
	} else {
		link->initial_sync_req = -1;
		if (link->sync_link)
			link->sync_link->initial_sync_req = -1;
	}

	mutex_unlock(&link->req.lock);

end:
	return rc;
}

/**
 * cam_req_mgr_process_add_req()
 *
 * @brief: This runs in workque thread context. Call core funcs to check
 *         which peding requests can be processed.
 * @priv : link information.
 * @data : contains information about frame_id, link etc.
 *
 * @return: 0 on success.
 */
int cam_req_mgr_process_add_req(void *priv, void *data)
{
	int                                  rc = 0, i = 0, idx;
	struct cam_req_mgr_add_request      *add_req = NULL;
	struct cam_req_mgr_core_link        *link = NULL;
	struct cam_req_mgr_connected_device *device = NULL;
	struct cam_req_mgr_req_tbl          *tbl = NULL;
	struct cam_req_mgr_tbl_slot         *slot = NULL;
	struct crm_task_payload             *task_data = NULL;

	if (!data || !priv) {
		CAM_ERR(CAM_CRM, "input args NULL %pK %pK", data, priv);
		rc = -EINVAL;
		goto end;
	}

	link = (struct cam_req_mgr_core_link *)priv;
	task_data = (struct crm_task_payload *)data;
	add_req = (struct cam_req_mgr_add_request *)&task_data->u;

	for (i = 0; i < link->num_devs; i++) {
		device = &link->l_dev[i];
		if (device->dev_hdl == add_req->dev_hdl) {
			tbl = device->pd_tbl;
			break;
		}
	}
	if (!tbl) {
		CAM_ERR_RATE_LIMIT(CAM_CRM, "dev_hdl not found %x, %x %x",
			add_req->dev_hdl,
			link->l_dev[0].dev_hdl,
			link->l_dev[1].dev_hdl);
		rc = -EINVAL;
		goto end;
	}
	/*
	 * Go through request table and add
	 * request id to proper table
	 * 1. find req slot in in_q matching req_id.sent by dev
	 * 2. goto table of this device based on p_delay
	 * 3. mark req_ready_map with this dev_bit.
	 */

	mutex_lock(&link->req.lock);
	idx = __cam_req_mgr_find_slot_for_req(link->req.in_q, add_req->req_id);
	if (idx < 0) {
		CAM_ERR(CAM_CRM,
			"req %lld not found in in_q for dev %s on link 0x%x",
			add_req->req_id, device->dev_info.name, link->link_hdl);
		rc = -EBADSLT;
		mutex_unlock(&link->req.lock);
		goto end;
	}

	slot = &tbl->slot[idx];
	slot->is_applied = false;
	if ((add_req->skip_before_applying & 0xFF) > slot->inject_delay) {
		slot->inject_delay = (add_req->skip_before_applying & 0xFF);
		slot->dev_hdl = add_req->dev_hdl;
		if (add_req->skip_before_applying & SKIP_NEXT_FRAME)
			slot->skip_next_frame = true;
		CAM_DBG(CAM_CRM, "Req_id %llu injecting delay %llu",
			add_req->req_id,
			(add_req->skip_before_applying & 0xFF));
	}

	if (slot->state != CRM_REQ_STATE_PENDING &&
		slot->state != CRM_REQ_STATE_EMPTY) {
		CAM_WARN(CAM_CRM,
			"Unexpected state %d for slot %d map %x for dev %s on link 0x%x",
			slot->state, idx, slot->req_ready_map,
			device->dev_info.name, link->link_hdl);
	}

	slot->state = CRM_REQ_STATE_PENDING;
	slot->req_ready_map |= (1 << device->dev_bit);

	CAM_DBG(CAM_CRM, "idx %d dev_hdl %x req_id %lld pd %d ready_map %x",
		idx, add_req->dev_hdl, add_req->req_id, tbl->pd,
		slot->req_ready_map);

	trace_cam_req_mgr_add_req(link, idx, add_req, tbl, device);

	if (slot->req_ready_map == tbl->dev_mask) {
		CAM_DBG(CAM_REQ,
			"link 0x%x idx %d req_id %lld pd %d SLOT READY",
			link->link_hdl, idx, add_req->req_id, tbl->pd);
		slot->state = CRM_REQ_STATE_READY;
	}
	mutex_unlock(&link->req.lock);

end:
	return rc;
}

/**
 * cam_req_mgr_process_error()
 *
 * @brief: This runs in workque thread context. bubble /err recovery.
 * @priv : link information.
 * @data : contains information about frame_id, link etc.
 *
 * @return: 0 on success.
 */
int cam_req_mgr_process_error(void *priv, void *data)
{
	int                                  rc = 0, idx = -1, i;
	struct cam_req_mgr_error_notify     *err_info = NULL;
	struct cam_req_mgr_core_link        *link = NULL;
	struct cam_req_mgr_req_queue        *in_q = NULL;
	struct cam_req_mgr_slot             *slot = NULL;
	struct cam_req_mgr_connected_device *device = NULL;
	struct cam_req_mgr_link_evt_data     evt_data;
	struct crm_task_payload             *task_data = NULL;

	if (!data || !priv) {
		CAM_ERR(CAM_CRM, "input args NULL %pK %pK", data, priv);
		rc = -EINVAL;
		goto end;
	}
	link = (struct cam_req_mgr_core_link *)priv;
	task_data = (struct crm_task_payload *)data;
	err_info  = (struct cam_req_mgr_error_notify *)&task_data->u;
	CAM_DBG(CAM_CRM, "link_hdl %x req_id %lld error %d",
		err_info->link_hdl,
		err_info->req_id,
		err_info->error);

	in_q = link->req.in_q;

	mutex_lock(&link->req.lock);
	if (err_info->error == CRM_KMD_ERR_BUBBLE) {
		idx = __cam_req_mgr_find_slot_for_req(in_q, err_info->req_id);
		if (idx < 0) {
			CAM_ERR_RATE_LIMIT(CAM_CRM,
				"req_id %lld not found in input queue",
				err_info->req_id);
		} else {
			CAM_DBG(CAM_CRM, "req_id %lld found at idx %d",
				err_info->req_id, idx);
			slot = &in_q->slot[idx];
			if (!slot->recover) {
				CAM_WARN(CAM_CRM,
					"err recovery disabled req_id %lld",
					err_info->req_id);
				mutex_unlock(&link->req.lock);
				return 0;
			} else if (slot->status != CRM_SLOT_STATUS_REQ_PENDING
			&& slot->status != CRM_SLOT_STATUS_REQ_APPLIED) {
				CAM_WARN(CAM_CRM,
					"req_id %lld can not be recovered %d",
					err_info->req_id, slot->status);
				mutex_unlock(&link->req.lock);
				return -EINVAL;
			}
			/* Notify all devices in the link about error */
			for (i = 0; i < link->num_devs; i++) {
				device = &link->l_dev[i];
				if (device != NULL) {
					evt_data.dev_hdl = device->dev_hdl;
					evt_data.evt_type =
						CAM_REQ_MGR_LINK_EVT_ERR;
					evt_data.link_hdl =  link->link_hdl;
					evt_data.req_id = err_info->req_id;
					evt_data.u.error = err_info->error;
					if (device->ops &&
						device->ops->process_evt)
						rc = device->ops->process_evt(
							&evt_data);
				}
			}
			/* Bring processing pointer to bubbled req id */
			__cam_req_mgr_tbl_set_all_skip_cnt(&link->req.l_tbl);
			in_q->rd_idx = idx;
			in_q->slot[idx].status = CRM_SLOT_STATUS_REQ_ADDED;
			spin_lock_bh(&link->link_state_spin_lock);
			link->state = CAM_CRM_LINK_STATE_ERR;
			spin_unlock_bh(&link->link_state_spin_lock);
			link->open_req_cnt++;
		}
	}
	mutex_unlock(&link->req.lock);

end:
	return rc;
}

/**
 * cam_req_mgr_process_trigger()
 *
 * @brief: This runs in workque thread context. Call core funcs to check
 *         which peding requests can be processed.
 * @priv : link information.
 * @data : contains information about frame_id, link etc.
 *
 * @return: 0 on success.
 */
static int cam_req_mgr_process_trigger(void *priv, void *data)
{
	int                                  rc = 0;
	struct cam_req_mgr_trigger_notify   *trigger_data = NULL;
	struct cam_req_mgr_core_link        *link = NULL;
	struct cam_req_mgr_req_queue        *in_q = NULL;
	struct crm_task_payload             *task_data = NULL;

	if (!data || !priv) {
		CAM_ERR(CAM_CRM, "input args NULL %pK %pK", data, priv);
		rc = -EINVAL;
		goto end;
	}
	link = (struct cam_req_mgr_core_link *)priv;
	task_data = (struct crm_task_payload *)data;
	trigger_data = (struct cam_req_mgr_trigger_notify *)&task_data->u;

	CAM_DBG(CAM_REQ, "link_hdl %x frame_id %lld, trigger %x\n",
		trigger_data->link_hdl,
		trigger_data->frame_id,
		trigger_data->trigger);

	in_q = link->req.in_q;

	mutex_lock(&link->req.lock);
	/*
	 * Check if current read index is in applied state, if yes make it free
	 *    and increment read index to next slot.
	 */
	CAM_DBG(CAM_CRM, "link_hdl %x curent idx %d req_status %d",
		link->link_hdl, in_q->rd_idx, in_q->slot[in_q->rd_idx].status);

	spin_lock_bh(&link->link_state_spin_lock);
	if (link->state == CAM_CRM_LINK_STATE_ERR)
		CAM_WARN(CAM_CRM, "Error recovery idx %d status %d",
			in_q->rd_idx,
			in_q->slot[in_q->rd_idx].status);

	spin_unlock_bh(&link->link_state_spin_lock);

	if (in_q->slot[in_q->rd_idx].status == CRM_SLOT_STATUS_REQ_APPLIED) {
		/*
		 * Do NOT reset req q slot data here, it can not be done
		 * here because we need to preserve the data to handle bubble.
		 *
		 * Check if any new req is pending in slot, if not finish the
		 * lower pipeline delay device with available req ids.
		 */
		CAM_DBG(CAM_CRM, "link[%x] Req[%lld] invalidating slot",
			link->link_hdl, in_q->slot[in_q->rd_idx].req_id);
		rc = __cam_req_mgr_check_next_req_slot(link);
		if (rc) {
			CAM_DBG(CAM_REQ,
				"No pending req to apply to lower pd devices");
			rc = 0;
			goto release_lock;
		}
		__cam_req_mgr_inc_idx(&in_q->rd_idx, 1, in_q->num_slots);
	}

	rc = __cam_req_mgr_process_req(link, trigger_data);

release_lock:
	mutex_unlock(&link->req.lock);
end:
	return rc;
}

/**
 * __cam_req_mgr_dev_handle_to_name()
 *
 * @brief    : Finds device name based on the device handle
 * @dev_hdl  : Device handle whose name is to be found
 * @link     : Link on which the device is connected
 * @return   : String containing the device name
 *
 */
static const char *__cam_req_mgr_dev_handle_to_name(
	int32_t dev_hdl, struct cam_req_mgr_core_link *link)
{
	struct cam_req_mgr_connected_device *dev = NULL;
	int i = 0;

	for (i = 0; i < link->num_devs; i++) {
		dev = &link->l_dev[i];

		if (dev_hdl == dev->dev_hdl)
			return dev->dev_info.name;
	}

	return "Invalid dev_hdl";
}

/* Linked devices' Callback section */

/**
 * cam_req_mgr_cb_add_req()
 *
 * @brief    : Drivers call this function to notify new packet is available.
 * @add_req  : Information about new request available at a device.
 *
 * @return   : 0 on success, negative in case of failure
 *
 */
static int cam_req_mgr_cb_add_req(struct cam_req_mgr_add_request *add_req)
{
	int                             rc = 0, idx;
	struct crm_workq_task          *task = NULL;
	struct cam_req_mgr_core_link   *link = NULL;
	struct cam_req_mgr_add_request *dev_req;
	struct crm_task_payload        *task_data;

	if (!add_req) {
		CAM_ERR(CAM_CRM, "sof_data is NULL");
		return -EINVAL;
	}

	link = (struct cam_req_mgr_core_link *)
		cam_get_device_priv(add_req->link_hdl);

	if (!link) {
		CAM_DBG(CAM_CRM, "link ptr NULL %x", add_req->link_hdl);
		return -EINVAL;
	}

	CAM_DBG(CAM_REQ, "dev name %s dev_hdl %d dev req %lld",
		__cam_req_mgr_dev_handle_to_name(add_req->dev_hdl, link),
		add_req->dev_hdl, add_req->req_id);

	mutex_lock(&link->lock);
	spin_lock_bh(&link->link_state_spin_lock);
	if (link->state < CAM_CRM_LINK_STATE_READY) {
		CAM_WARN(CAM_CRM, "invalid link state:%d", link->state);
		rc = -EPERM;
		spin_unlock_bh(&link->link_state_spin_lock);
		goto end;
	}
	spin_unlock_bh(&link->link_state_spin_lock);

	/* Validate if req id is present in input queue */
	idx = __cam_req_mgr_find_slot_for_req(link->req.in_q, add_req->req_id);
	if (idx < 0) {
		CAM_ERR(CAM_CRM, "req %lld not found in in_q", add_req->req_id);
		rc = -ENOENT;
		goto end;
	}

	task = cam_req_mgr_workq_get_task(link->workq);
	if (!task) {
		CAM_ERR_RATE_LIMIT(CAM_CRM, "no empty task dev %x req %lld",
			add_req->dev_hdl, add_req->req_id);
		rc = -EBUSY;
		goto end;
	}

	task_data = (struct crm_task_payload *)task->payload;
	task_data->type = CRM_WORKQ_TASK_DEV_ADD_REQ;
	dev_req = (struct cam_req_mgr_add_request *)&task_data->u;
	dev_req->req_id = add_req->req_id;
	dev_req->link_hdl = add_req->link_hdl;
	dev_req->dev_hdl = add_req->dev_hdl;
	dev_req->skip_before_applying = add_req->skip_before_applying;
	task->process_cb = &cam_req_mgr_process_add_req;
	rc = cam_req_mgr_workq_enqueue_task(task, link, CRM_TASK_PRIORITY_0);
	CAM_DBG(CAM_CRM, "X: dev %x dev req %lld",
		add_req->dev_hdl, add_req->req_id);

end:
	mutex_unlock(&link->lock);
	return rc;
}

/**
 * cam_req_mgr_cb_notify_err()
 *
 * @brief    : Error received from device, sends bubble recovery
 * @err_info : contains information about error occurred like bubble/overflow
 *
 * @return   : 0 on success, negative in case of failure
 *
 */
static int cam_req_mgr_cb_notify_err(
	struct cam_req_mgr_error_notify *err_info)
{
	int                              rc = 0;
	struct crm_workq_task           *task = NULL;
	struct cam_req_mgr_core_link    *link = NULL;
	struct cam_req_mgr_error_notify *notify_err;
	struct crm_task_payload         *task_data;

	if (!err_info) {
		CAM_ERR(CAM_CRM, "err_info is NULL");
		rc = -EINVAL;
		goto end;
	}

	link = (struct cam_req_mgr_core_link *)
		cam_get_device_priv(err_info->link_hdl);
	if (!link) {
		CAM_DBG(CAM_CRM, "link ptr NULL %x", err_info->link_hdl);
		rc = -EINVAL;
		goto end;
	}

	spin_lock_bh(&link->link_state_spin_lock);
	if (link->state != CAM_CRM_LINK_STATE_READY) {
		CAM_WARN(CAM_CRM, "invalid link state:%d", link->state);
		spin_unlock_bh(&link->link_state_spin_lock);
		rc = -EPERM;
		goto end;
	}
	crm_timer_reset(link->watchdog);
	spin_unlock_bh(&link->link_state_spin_lock);

	task = cam_req_mgr_workq_get_task(link->workq);
	if (!task) {
		CAM_ERR(CAM_CRM, "no empty task req_id %lld", err_info->req_id);
		rc = -EBUSY;
		goto end;
	}

	task_data = (struct crm_task_payload *)task->payload;
	task_data->type = CRM_WORKQ_TASK_NOTIFY_ERR;
	notify_err = (struct cam_req_mgr_error_notify *)&task_data->u;
	notify_err->req_id = err_info->req_id;
	notify_err->link_hdl = err_info->link_hdl;
	notify_err->dev_hdl = err_info->dev_hdl;
	notify_err->error = err_info->error;
	task->process_cb = &cam_req_mgr_process_error;
	rc = cam_req_mgr_workq_enqueue_task(task, link, CRM_TASK_PRIORITY_0);

end:
	return rc;
}

/**
 * cam_req_mgr_cb_notify_timer()
 *
 * @brief      : Notify SOF timer to pause after flush
 * @timer_data : contains information about frame_id, link etc.
 *
 * @return  : 0 on success
 *
 */
static int cam_req_mgr_cb_notify_timer(
	struct cam_req_mgr_timer_notify *timer_data)
{
	int                              rc = 0;
	struct cam_req_mgr_core_link    *link = NULL;

	if (!timer_data) {
		CAM_ERR(CAM_CRM, "timer data  is NULL");
		rc = -EINVAL;
		goto end;
	}

	link = (struct cam_req_mgr_core_link *)
		cam_get_device_priv(timer_data->link_hdl);
	if (!link) {
		CAM_DBG(CAM_CRM, "link ptr NULL %x", timer_data->link_hdl);
		rc = -EINVAL;
		goto end;
	}

	spin_lock_bh(&link->link_state_spin_lock);
	if (link->state < CAM_CRM_LINK_STATE_READY) {
		CAM_WARN(CAM_CRM, "invalid link state:%d", link->state);
		spin_unlock_bh(&link->link_state_spin_lock);
		rc = -EPERM;
		goto end;
	}
	spin_unlock_bh(&link->link_state_spin_lock);


	if (!timer_data->state)
		link->watchdog->pause_timer = true;

end:
	return rc;
}

/**
 * cam_req_mgr_cb_notify_trigger()
 *
 * @brief   : SOF received from device, sends trigger through workqueue
 * @sof_data: contains information about frame_id, link etc.
 *
 * @return  : 0 on success
 *
 */
static int cam_req_mgr_cb_notify_trigger(
	struct cam_req_mgr_trigger_notify *trigger_data)
{
	int                              rc = 0;
	struct crm_workq_task           *task = NULL;
	struct cam_req_mgr_core_link    *link = NULL;
	struct cam_req_mgr_trigger_notify   *notify_trigger;
	struct crm_task_payload         *task_data;

	if (!trigger_data) {
		CAM_ERR(CAM_CRM, "sof_data is NULL");
		rc = -EINVAL;
		goto end;
	}

	link = (struct cam_req_mgr_core_link *)
		cam_get_device_priv(trigger_data->link_hdl);
	if (!link) {
		CAM_DBG(CAM_CRM, "link ptr NULL %x", trigger_data->link_hdl);
		rc = -EINVAL;
		goto end;
	}

	spin_lock_bh(&link->link_state_spin_lock);
	if (link->state < CAM_CRM_LINK_STATE_READY) {
		CAM_WARN(CAM_CRM, "invalid link state:%d", link->state);
		spin_unlock_bh(&link->link_state_spin_lock);
		rc = -EPERM;
		goto end;
	}

	if (link->watchdog->pause_timer)
		link->watchdog->pause_timer = false;

	crm_timer_reset(link->watchdog);
	spin_unlock_bh(&link->link_state_spin_lock);

	task = cam_req_mgr_workq_get_task(link->workq);
	if (!task) {
		CAM_ERR(CAM_CRM, "no empty task frame %lld",
			trigger_data->frame_id);
		rc = -EBUSY;
		goto end;
	}
	task_data = (struct crm_task_payload *)task->payload;
	task_data->type = CRM_WORKQ_TASK_NOTIFY_SOF;
	notify_trigger = (struct cam_req_mgr_trigger_notify *)&task_data->u;
	notify_trigger->frame_id = trigger_data->frame_id;
	notify_trigger->link_hdl = trigger_data->link_hdl;
	notify_trigger->dev_hdl = trigger_data->dev_hdl;
	notify_trigger->trigger = trigger_data->trigger;
	notify_trigger->sof_timestamp_val = trigger_data->sof_timestamp_val;
	task->process_cb = &cam_req_mgr_process_trigger;
	rc = cam_req_mgr_workq_enqueue_task(task, link, CRM_TASK_PRIORITY_0);

end:
	return rc;
}

static struct cam_req_mgr_crm_cb cam_req_mgr_ops = {
	.notify_trigger = cam_req_mgr_cb_notify_trigger,
	.notify_err     = cam_req_mgr_cb_notify_err,
	.add_req        = cam_req_mgr_cb_add_req,
	.notify_timer   = cam_req_mgr_cb_notify_timer,
};

/**
 * __cam_req_mgr_setup_link_info()
 *
 * @brief     : Sets up input queue, create pd based tables, communicate with
 *              devs connected on this link and setup communication.
 * @link      : pointer to link to setup
 * @link_info : link_info coming from CSL to prepare link
 *
 * @return    : 0 on success, negative in case of failure
 *
 */
static int __cam_req_mgr_setup_link_info(struct cam_req_mgr_core_link *link,
	struct cam_req_mgr_ver_info *link_info)
{
	int                                     rc = 0, i = 0, num_devices = 0;
	struct cam_req_mgr_core_dev_link_setup  link_data;
	struct cam_req_mgr_connected_device    *dev;
	struct cam_req_mgr_req_tbl             *pd_tbl;
	enum cam_pipeline_delay                 max_delay;
	uint32_t                                subscribe_event = 0;
	if (link_info->version == VERSION_1) {
		if (link_info->u.link_info_v1.num_devices >
			CAM_REQ_MGR_MAX_HANDLES)
			return -EPERM;
		}
	else if (link_info->version == VERSION_2) {
		if (link_info->u.link_info_v2.num_devices >
			CAM_REQ_MGR_MAX_HANDLES_V2)
			return -EPERM;
		}
	mutex_init(&link->req.lock);
	CAM_DBG(CAM_CRM, "LOCK_DBG in_q lock %pK", &link->req.lock);
	link->req.num_tbl = 0;

	rc = __cam_req_mgr_setup_in_q(&link->req);
	if (rc < 0)
		return rc;

	max_delay = CAM_PIPELINE_DELAY_0;
	if (link_info->version == VERSION_1)
		num_devices = link_info->u.link_info_v1.num_devices;
	else if (link_info->version == VERSION_2)
		num_devices = link_info->u.link_info_v2.num_devices;
	for (i = 0; i < num_devices; i++) {
		dev = &link->l_dev[i];
		/* Using dev hdl, get ops ptr to communicate with device */
		if (link_info->version == VERSION_1)
			dev->ops = (struct cam_req_mgr_kmd_ops *)
					cam_get_device_ops(
					link_info->u.link_info_v1.dev_hdls[i]);
		else if (link_info->version == VERSION_2)
			dev->ops = (struct cam_req_mgr_kmd_ops *)
					cam_get_device_ops(
					link_info->u.link_info_v2.dev_hdls[i]);
		if (!dev->ops ||
			!dev->ops->get_dev_info ||
			!dev->ops->link_setup) {
			CAM_ERR(CAM_CRM, "FATAL: device ops NULL");
			rc = -ENXIO;
			goto error;
		}
		if (link_info->version == VERSION_1)
			dev->dev_hdl = link_info->u.link_info_v1.dev_hdls[i];
		else if (link_info->version == VERSION_2)
			dev->dev_hdl = link_info->u.link_info_v2.dev_hdls[i];
		dev->parent = (void *)link;
		dev->dev_info.dev_hdl = dev->dev_hdl;
		rc = dev->ops->get_dev_info(&dev->dev_info);

		trace_cam_req_mgr_connect_device(link, &dev->dev_info);
		if (link_info->version == VERSION_1)
			CAM_DBG(CAM_CRM,
				"%x: connected: %s, id %d, delay %d, trigger %x",
				link_info->u.link_info_v1.session_hdl,
				dev->dev_info.name,
				dev->dev_info.dev_id, dev->dev_info.p_delay,
				dev->dev_info.trigger);
		else if (link_info->version == VERSION_2)
			CAM_DBG(CAM_CRM,
				"%x: connected: %s, id %d, delay %d, trigger %x",
				link_info->u.link_info_v2.session_hdl,
				dev->dev_info.name,
				dev->dev_info.dev_id, dev->dev_info.p_delay,
				dev->dev_info.trigger);
		if (rc < 0 ||
			dev->dev_info.p_delay >=
			CAM_PIPELINE_DELAY_MAX ||
			dev->dev_info.p_delay <
			CAM_PIPELINE_DELAY_0) {
			CAM_ERR(CAM_CRM, "get device info failed");
			goto error;
		} else {
			if (link_info->version == VERSION_1) {
				CAM_DBG(CAM_CRM, "%x: connected: %s, delay %d",
					link_info->u.link_info_v1.session_hdl,
					dev->dev_info.name,
					dev->dev_info.p_delay);
				}
			else if (link_info->version == VERSION_2) {
				CAM_DBG(CAM_CRM, "%x: connected: %s, delay %d",
					link_info->u.link_info_v2.session_hdl,
					dev->dev_info.name,
					dev->dev_info.p_delay);
				}
			if (dev->dev_info.p_delay > max_delay)
				max_delay = dev->dev_info.p_delay;

			subscribe_event |= (uint32_t)dev->dev_info.trigger;
		}
	}

	link->subscribe_event = subscribe_event;
	link_data.link_enable = 1;
	link_data.link_hdl = link->link_hdl;
	link_data.crm_cb = &cam_req_mgr_ops;
	link_data.max_delay = max_delay;
	link_data.subscribe_event = subscribe_event;

	for (i = 0; i < num_devices; i++) {
		dev = &link->l_dev[i];

		link_data.dev_hdl = dev->dev_hdl;
		/*
		 * For unique pipeline delay table create request
		 * tracking table
		 */
		if (link->pd_mask & (1 << dev->dev_info.p_delay)) {
			pd_tbl = __cam_req_mgr_find_pd_tbl(link->req.l_tbl,
				dev->dev_info.p_delay);
			if (!pd_tbl) {
				CAM_ERR(CAM_CRM, "pd %d tbl not found",
					dev->dev_info.p_delay);
				rc = -ENXIO;
				goto error;
			}
		} else {
			pd_tbl = __cam_req_mgr_create_pd_tbl(
				dev->dev_info.p_delay);
			if (pd_tbl == NULL) {
				CAM_ERR(CAM_CRM, "create new pd tbl failed");
				rc = -ENXIO;
				goto error;
			}
			pd_tbl->pd = dev->dev_info.p_delay;
			link->pd_mask |= (1 << pd_tbl->pd);
			/*
			 * Add table to list and also sort list
			 * from max pd to lowest
			 */
			__cam_req_mgr_add_tbl_to_link(&link->req.l_tbl, pd_tbl);
		}
		dev->dev_bit = pd_tbl->dev_count++;
		dev->pd_tbl = pd_tbl;
		pd_tbl->dev_mask |= (1 << dev->dev_bit);
		CAM_DBG(CAM_CRM, "dev_bit %u name %s pd %u mask %d",
			dev->dev_bit, dev->dev_info.name, pd_tbl->pd,
			pd_tbl->dev_mask);
		/* Communicate with dev to establish the link */
		dev->ops->link_setup(&link_data);

		if (link->max_delay < dev->dev_info.p_delay)
			link->max_delay = dev->dev_info.p_delay;
	}
	link->num_devs = num_devices;

	/* Assign id for pd tables */
	__cam_req_mgr_tbl_set_id(link->req.l_tbl, &link->req);

	/* At start, expect max pd devices, all are in skip state */
	__cam_req_mgr_tbl_set_all_skip_cnt(&link->req.l_tbl);

	return 0;

error:
	__cam_req_mgr_destroy_link_info(link);
	return rc;
}

/* IOCTLs handling section */
int cam_req_mgr_create_session(
	struct cam_req_mgr_session_info *ses_info)
{
	int                              rc = 0;
	int32_t                          session_hdl;
	struct cam_req_mgr_core_session *cam_session = NULL;

	if (!ses_info) {
		CAM_DBG(CAM_CRM, "NULL session info pointer");
		return -EINVAL;
	}
	mutex_lock(&g_crm_core_dev->crm_lock);
	cam_session = kzalloc(sizeof(*cam_session),
		GFP_KERNEL);
	if (!cam_session) {
		rc = -ENOMEM;
		goto end;
	}

	session_hdl = cam_create_session_hdl((void *)cam_session);
	if (session_hdl < 0) {
		CAM_ERR(CAM_CRM, "unable to create session_hdl = %x",
			session_hdl);
		rc = session_hdl;
		kfree(cam_session);
		goto end;
	}
	ses_info->session_hdl = session_hdl;

	mutex_init(&cam_session->lock);
	CAM_DBG(CAM_CRM, "LOCK_DBG session lock %pK", &cam_session->lock);

	mutex_lock(&cam_session->lock);
	cam_session->session_hdl = session_hdl;
	cam_session->num_links = 0;
	cam_session->sync_mode = CAM_REQ_MGR_SYNC_MODE_NO_SYNC;
	list_add(&cam_session->entry, &g_crm_core_dev->session_head);
	mutex_unlock(&cam_session->lock);
end:
	mutex_unlock(&g_crm_core_dev->crm_lock);
	return rc;
}

/**
 * __cam_req_mgr_unlink()
 *
 * @brief : Unlink devices on a link structure from the session
 * @link  : Pointer to the link structure
 *
 * @return: 0 for success, negative for failure
 *
 */
static int __cam_req_mgr_unlink(struct cam_req_mgr_core_link *link)
{
	int rc;

	spin_lock_bh(&link->link_state_spin_lock);
	link->state = CAM_CRM_LINK_STATE_IDLE;
	spin_unlock_bh(&link->link_state_spin_lock);

	if (!link->is_shutdown) {
		rc = __cam_req_mgr_disconnect_link(link);
		if (rc)
			CAM_ERR(CAM_CORE,
				"Unlink for all devices was not successful");
	}

	mutex_lock(&link->lock);
	/* Destroy workq of link */
	cam_req_mgr_workq_destroy(&link->workq);
	/* Destroy timer of link */
	crm_timer_exit(&link->watchdog);

	/* Cleanup request tables and unlink devices */
	__cam_req_mgr_destroy_link_info(link);

	/* Free memory holding data of linked devs */
	__cam_req_mgr_destroy_subdev(link->l_dev);

	/* Destroy the link handle */
	rc = cam_destroy_device_hdl(link->link_hdl);
	if (rc < 0) {
		CAM_ERR(CAM_CRM, "error destroying link hdl %x rc %d",
			link->link_hdl, rc);
	} else
		link->link_hdl = -1;

	mutex_unlock(&link->lock);
	return rc;
}

int cam_req_mgr_destroy_session(
		struct cam_req_mgr_session_info *ses_info,
		bool is_shutdown)
{
	int rc;
	int i;
	struct cam_req_mgr_core_session *cam_session = NULL;
	struct cam_req_mgr_core_link *link;

	if (!ses_info) {
		CAM_DBG(CAM_CRM, "NULL session info pointer");
		return -EINVAL;
	}

	mutex_lock(&g_crm_core_dev->crm_lock);
	cam_session = (struct cam_req_mgr_core_session *)
		cam_get_device_priv(ses_info->session_hdl);
	if (!cam_session) {
		CAM_ERR(CAM_CRM, "failed to get session priv");
		rc = -ENOENT;
		goto end;

	}
	if (cam_session->num_links) {
		CAM_DBG(CAM_CRM, "destroy session %x num_active_links %d",
			ses_info->session_hdl,
			cam_session->num_links);

		for (i = 0; i < MAXIMUM_LINKS_PER_SESSION; i++) {
			link = cam_session->links[i];

			if (!link)
				continue;

			/* Ignore return value since session is going away */
			link->is_shutdown = is_shutdown;
			__cam_req_mgr_unlink(link);
			__cam_req_mgr_free_link(link);
		}
	}
	list_del(&cam_session->entry);
	mutex_destroy(&cam_session->lock);
	kfree(cam_session);

	rc = cam_destroy_session_hdl(ses_info->session_hdl);
	if (rc < 0)
		CAM_ERR(CAM_CRM, "unable to destroy session_hdl = %x rc %d",
			ses_info->session_hdl, rc);

end:
	mutex_unlock(&g_crm_core_dev->crm_lock);
	return rc;
}

int cam_req_mgr_link(struct cam_req_mgr_ver_info *link_info)
{
	int                                     rc = 0;
	int                                     wq_flag = 0;
	char                                    buf[128];
	struct cam_create_dev_hdl               root_dev;
	struct cam_req_mgr_core_session        *cam_session;
	struct cam_req_mgr_core_link           *link;

	if (!link_info) {
		CAM_DBG(CAM_CRM, "NULL pointer");
		return -EINVAL;
	}
	if (link_info->u.link_info_v1.num_devices > CAM_REQ_MGR_MAX_HANDLES) {
		CAM_ERR(CAM_CRM, "Invalid num devices %d",
			link_info->u.link_info_v1.num_devices);
		return -EINVAL;
	}

	mutex_lock(&g_crm_core_dev->crm_lock);

	/* session hdl's priv data is cam session struct */
	cam_session = (struct cam_req_mgr_core_session *)
		cam_get_device_priv(link_info->u.link_info_v1.session_hdl);
	if (!cam_session) {
		CAM_DBG(CAM_CRM, "NULL pointer");
		mutex_unlock(&g_crm_core_dev->crm_lock);
		return -EINVAL;
	}

	/* Allocate link struct and map it with session's request queue */
	link = __cam_req_mgr_reserve_link(cam_session);
	if (!link) {
		CAM_ERR(CAM_CRM, "failed to reserve new link");
		mutex_unlock(&g_crm_core_dev->crm_lock);
		return -EINVAL;
	}
	CAM_DBG(CAM_CRM, "link reserved %pK %x", link, link->link_hdl);

	memset(&root_dev, 0, sizeof(struct cam_create_dev_hdl));
	root_dev.session_hdl = link_info->u.link_info_v1.session_hdl;
	root_dev.priv = (void *)link;

	mutex_lock(&link->lock);
	/* Create unique dev handle for link */
	link->link_hdl = cam_create_device_hdl(&root_dev);
	if (link->link_hdl < 0) {
		CAM_ERR(CAM_CRM,
			"Insufficient memory to create new device handle");
		rc = link->link_hdl;
		goto link_hdl_fail;
	}
	link_info->u.link_info_v1.link_hdl = link->link_hdl;
	link->last_flush_id = 0;

	/* Allocate memory to hold data of all linked devs */
	rc = __cam_req_mgr_create_subdevs(&link->l_dev,
		link_info->u.link_info_v1.num_devices);
	if (rc < 0) {
		CAM_ERR(CAM_CRM,
			"Insufficient memory to create new crm subdevs");
		goto create_subdev_failed;
	}

	/* Using device ops query connected devs, prepare request tables */
	rc = __cam_req_mgr_setup_link_info(link, link_info);
	if (rc < 0)
		goto setup_failed;

	spin_lock_bh(&link->link_state_spin_lock);
	link->state = CAM_CRM_LINK_STATE_READY;
	spin_unlock_bh(&link->link_state_spin_lock);

	/* Create worker for current link */
	snprintf(buf, sizeof(buf), "%x-%x",
		link_info->u.link_info_v1.session_hdl, link->link_hdl);
	wq_flag = CAM_WORKQ_FLAG_HIGH_PRIORITY | CAM_WORKQ_FLAG_SERIAL;
	rc = cam_req_mgr_workq_create(buf, CRM_WORKQ_NUM_TASKS,
		&link->workq, CRM_WORKQ_USAGE_NON_IRQ, wq_flag);
	if (rc < 0) {
		CAM_ERR(CAM_CRM, "FATAL: unable to create worker");
		__cam_req_mgr_destroy_link_info(link);
		goto setup_failed;
	}

	/* Assign payload to workqueue tasks */
	rc = __cam_req_mgr_setup_payload(link->workq);
	if (rc < 0) {
		__cam_req_mgr_destroy_link_info(link);
		cam_req_mgr_workq_destroy(&link->workq);
		goto setup_failed;
	}

	mutex_unlock(&link->lock);
	mutex_unlock(&g_crm_core_dev->crm_lock);
	return rc;
setup_failed:
	__cam_req_mgr_destroy_subdev(link->l_dev);
create_subdev_failed:
	cam_destroy_device_hdl(link->link_hdl);
	link_info->u.link_info_v1.link_hdl = -1;
link_hdl_fail:
	mutex_unlock(&link->lock);
	__cam_req_mgr_unreserve_link(cam_session, link);
	mutex_unlock(&g_crm_core_dev->crm_lock);
	return rc;
}

int cam_req_mgr_link_v2(struct cam_req_mgr_ver_info *link_info)
{
	int                                     rc = 0;
	int                                     wq_flag = 0;
	char                                    buf[128];
	struct cam_create_dev_hdl               root_dev;
	struct cam_req_mgr_core_session        *cam_session;
	struct cam_req_mgr_core_link           *link;

	if (!link_info) {
		CAM_DBG(CAM_CRM, "NULL pointer");
		return -EINVAL;
	}
	if (link_info->u.link_info_v2.num_devices >
		CAM_REQ_MGR_MAX_HANDLES_V2) {
		CAM_ERR(CAM_CRM, "Invalid num devices %d",
			link_info->u.link_info_v2.num_devices);
		return -EINVAL;
	}

	mutex_lock(&g_crm_core_dev->crm_lock);

	/* session hdl's priv data is cam session struct */
	cam_session = (struct cam_req_mgr_core_session *)
		cam_get_device_priv(link_info->u.link_info_v2.session_hdl);
	if (!cam_session) {
		CAM_DBG(CAM_CRM, "NULL pointer");
		mutex_unlock(&g_crm_core_dev->crm_lock);
		return -EINVAL;
	}

	/* Allocate link struct and map it with session's request queue */
	link = __cam_req_mgr_reserve_link(cam_session);
	if (!link) {
		CAM_ERR(CAM_CRM, "failed to reserve new link");
		mutex_unlock(&g_crm_core_dev->crm_lock);
		return -EINVAL;
	}
	CAM_DBG(CAM_CRM, "link reserved %pK %x", link, link->link_hdl);

	memset(&root_dev, 0, sizeof(struct cam_create_dev_hdl));
	root_dev.session_hdl = link_info->u.link_info_v2.session_hdl;
	root_dev.priv = (void *)link;

	mutex_lock(&link->lock);
	/* Create unique dev handle for link */
	link->link_hdl = cam_create_device_hdl(&root_dev);
	if (link->link_hdl < 0) {
		CAM_ERR(CAM_CRM,
			"Insufficient memory to create new device handle");
		rc = link->link_hdl;
		goto link_hdl_fail;
	}
	link_info->u.link_info_v2.link_hdl = link->link_hdl;
	link->last_flush_id = 0;

	/* Allocate memory to hold data of all linked devs */
	rc = __cam_req_mgr_create_subdevs(&link->l_dev,
		link_info->u.link_info_v2.num_devices);
	if (rc < 0) {
		CAM_ERR(CAM_CRM,
			"Insufficient memory to create new crm subdevs");
		goto create_subdev_failed;
	}

	/* Using device ops query connected devs, prepare request tables */
	rc = __cam_req_mgr_setup_link_info(link, link_info);
	if (rc < 0)
		goto setup_failed;

	spin_lock_bh(&link->link_state_spin_lock);
	link->state = CAM_CRM_LINK_STATE_READY;
	spin_unlock_bh(&link->link_state_spin_lock);

	/* Create worker for current link */
	snprintf(buf, sizeof(buf), "%x-%x",
		link_info->u.link_info_v2.session_hdl, link->link_hdl);
	wq_flag = CAM_WORKQ_FLAG_HIGH_PRIORITY | CAM_WORKQ_FLAG_SERIAL;
	rc = cam_req_mgr_workq_create(buf, CRM_WORKQ_NUM_TASKS,
		&link->workq, CRM_WORKQ_USAGE_NON_IRQ, wq_flag);
	if (rc < 0) {
		CAM_ERR(CAM_CRM, "FATAL: unable to create worker");
		__cam_req_mgr_destroy_link_info(link);
		goto setup_failed;
	}

	/* Assign payload to workqueue tasks */
	rc = __cam_req_mgr_setup_payload(link->workq);
	if (rc < 0) {
		__cam_req_mgr_destroy_link_info(link);
		cam_req_mgr_workq_destroy(&link->workq);
		goto setup_failed;
	}

	mutex_unlock(&link->lock);
	mutex_unlock(&g_crm_core_dev->crm_lock);
	return rc;
setup_failed:
	__cam_req_mgr_destroy_subdev(link->l_dev);
create_subdev_failed:
	cam_destroy_device_hdl(link->link_hdl);
	link_info->u.link_info_v2.link_hdl = -1;
link_hdl_fail:
	mutex_unlock(&link->lock);
	__cam_req_mgr_unreserve_link(cam_session, link);
	mutex_unlock(&g_crm_core_dev->crm_lock);
	return rc;
}


int cam_req_mgr_unlink(struct cam_req_mgr_unlink_info *unlink_info)
{
	int                              rc = 0;
	struct cam_req_mgr_core_session *cam_session;
	struct cam_req_mgr_core_link    *link;

	if (!unlink_info) {
		CAM_ERR(CAM_CRM, "NULL pointer");
		return -EINVAL;
	}

	mutex_lock(&g_crm_core_dev->crm_lock);
	CAM_DBG(CAM_CRM, "link_hdl %x", unlink_info->link_hdl);

	/* session hdl's priv data is cam session struct */
	cam_session = (struct cam_req_mgr_core_session *)
		cam_get_device_priv(unlink_info->session_hdl);
	if (!cam_session) {
		CAM_ERR(CAM_CRM, "NULL pointer");
		mutex_unlock(&g_crm_core_dev->crm_lock);
		return -EINVAL;
	}

	/* link hdl's priv data is core_link struct */
	link = cam_get_device_priv(unlink_info->link_hdl);
	if (!link) {
		CAM_ERR(CAM_CRM, "NULL pointer");
		rc = -EINVAL;
		goto done;
	}

	rc = __cam_req_mgr_unlink(link);

	/* Free curent link and put back into session's free pool of links */
	__cam_req_mgr_unreserve_link(cam_session, link);

done:
	mutex_unlock(&g_crm_core_dev->crm_lock);
	return rc;
}

int cam_req_mgr_schedule_request(
			struct cam_req_mgr_sched_request *sched_req)
{
	int                               rc = 0;
	struct cam_req_mgr_core_link     *link = NULL;
	struct cam_req_mgr_core_session  *session = NULL;
	struct cam_req_mgr_sched_request *sched;
	struct crm_task_payload           task_data;

	if (!sched_req) {
		CAM_ERR(CAM_CRM, "csl_req is NULL");
		return -EINVAL;
	}

	mutex_lock(&g_crm_core_dev->crm_lock);
	link = (struct cam_req_mgr_core_link *)
		cam_get_device_priv(sched_req->link_hdl);
	if (!link) {
		CAM_DBG(CAM_CRM, "link ptr NULL %x", sched_req->link_hdl);
		rc = -EINVAL;
		goto end;
	}

	session = (struct cam_req_mgr_core_session *)link->parent;
	if (!session) {
		CAM_WARN(CAM_CRM, "session ptr NULL %x", sched_req->link_hdl);
		rc = -EINVAL;
		goto end;
	}

	if (sched_req->req_id <= link->last_flush_id) {
		CAM_INFO(CAM_CRM,
			"request %lld is flushed, last_flush_id to flush %d",
			sched_req->req_id, link->last_flush_id);
		rc = -EBADR;
		goto end;
	}

	if (sched_req->req_id > link->last_flush_id)
		link->last_flush_id = 0;

	CAM_DBG(CAM_CRM, "link 0x%x req %lld, sync_mode %d",
		sched_req->link_hdl, sched_req->req_id, sched_req->sync_mode);

	task_data.type = CRM_WORKQ_TASK_SCHED_REQ;
	sched = (struct cam_req_mgr_sched_request *)&task_data.u;
	sched->req_id = sched_req->req_id;
	sched->sync_mode = sched_req->sync_mode;
	sched->link_hdl = sched_req->link_hdl;
	sched->additional_timeout = sched_req->additional_timeout;
	if (session->force_err_recovery == AUTO_RECOVERY) {
		sched->bubble_enable = sched_req->bubble_enable;
	} else {
		sched->bubble_enable =
		(session->force_err_recovery == FORCE_ENABLE_RECOVERY) ? 1 : 0;
	}

	rc = cam_req_mgr_process_sched_req(link, &task_data);

	CAM_DBG(CAM_REQ, "Open req %lld on link 0x%x with sync_mode %d",
		sched_req->req_id, sched_req->link_hdl, sched_req->sync_mode);
end:
	mutex_unlock(&g_crm_core_dev->crm_lock);
	return rc;
}

/**
 * __cam_req_mgr_set_master_link()
 *
 * @brief    : Each links sets its max pd delay based on the devices on the
 *             link. The link with higher pd is assigned master.
 * @link1    : One of the sync links
 * @link2    : The other sync link
 */
static void __cam_req_mgr_set_master_link(
	struct cam_req_mgr_core_link *link1,
	struct cam_req_mgr_core_link *link2)
{

	if (link1->max_delay > link2->max_delay) {
		link1->is_master = true;
		link2->initial_skip = true;
	} else if (link2->max_delay > link1->max_delay) {
		link2->is_master = true;
		link1->initial_skip = true;
	}

	CAM_DBG(CAM_CRM,
		"link_hdl1[0x%x] is_master [%u] link_hdl2[0x%x] is_master[%u]",
		link1->link_hdl, link1->is_master,
		link2->link_hdl, link2->is_master);
}

int cam_req_mgr_sync_config(
	struct cam_req_mgr_sync_mode *sync_info)
{
	int                              rc = 0;
	struct cam_req_mgr_core_session *cam_session;
	struct cam_req_mgr_core_link    *link1 = NULL;
	struct cam_req_mgr_core_link    *link2 = NULL;

	if (!sync_info) {
		CAM_ERR(CAM_CRM, "NULL pointer");
		return -EINVAL;
	}

	if ((sync_info->num_links < 0) ||
		(sync_info->num_links >
		MAX_LINKS_PER_SESSION)) {
		CAM_ERR(CAM_CRM, "Invalid num links %d", sync_info->num_links);
		return -EINVAL;
	}

	if ((sync_info->sync_mode != CAM_REQ_MGR_SYNC_MODE_SYNC) &&
		(sync_info->sync_mode != CAM_REQ_MGR_SYNC_MODE_NO_SYNC)) {
		CAM_ERR(CAM_CRM, "Invalid sync mode %d", sync_info->sync_mode);
		return -EINVAL;
	}

	if ((!sync_info->link_hdls[0]) || (!sync_info->link_hdls[1])) {
		CAM_WARN(CAM_CRM, "Invalid link handles 0x%x 0x%x",
			sync_info->link_hdls[0], sync_info->link_hdls[1]);
		return -EINVAL;
	}

	mutex_lock(&g_crm_core_dev->crm_lock);
	/* session hdl's priv data is cam session struct */
	cam_session = (struct cam_req_mgr_core_session *)
		cam_get_device_priv(sync_info->session_hdl);
	if (!cam_session) {
		CAM_ERR(CAM_CRM, "NULL pointer");
		mutex_unlock(&g_crm_core_dev->crm_lock);
		return -EINVAL;
	}

	mutex_lock(&cam_session->lock);

	CAM_DBG(CAM_CRM, "link handles %x %x",
		sync_info->link_hdls[0], sync_info->link_hdls[1]);

	/* only two links existing per session in dual cam use case*/
	link1 = cam_get_device_priv(sync_info->link_hdls[0]);
	if (!link1) {
		CAM_ERR(CAM_CRM, "link1 NULL pointer");
		rc = -EINVAL;
		goto done;
	}

	link2 = cam_get_device_priv(sync_info->link_hdls[1]);
	if (!link2) {
		CAM_ERR(CAM_CRM, "link2 NULL pointer");
		rc = -EINVAL;
		goto done;
	}

	link1->sync_link_sof_skip = false;
	link1->sync_link = NULL;

	link2->sync_link_sof_skip = false;
	link2->sync_link = NULL;

	link1->is_master = false;
	link2->is_master = false;

	link1->in_msync_mode = false;
	link2->in_msync_mode = false;
	link1->initial_sync_req = -1;
	link2->initial_sync_req = -1;

	if (sync_info->sync_mode == CAM_REQ_MGR_SYNC_MODE_SYNC) {
		link1->sync_link = link2;
		link2->sync_link = link1;
		__cam_req_mgr_set_master_link(link1, link2);
	} else {
		/*
		 * Reset below info after the mode is configured
		 * to NO-SYNC mode since they may be overridden
		 * if the sync config is invoked after SOF comes.
		 */
		link1->initial_skip = true;
		link2->initial_skip = true;
		link1->sof_timestamp = 0;
		link2->sof_timestamp = 0;
	}

	cam_session->sync_mode = sync_info->sync_mode;
	CAM_DBG(CAM_REQ,
		"Sync config on link1 0x%x & link2 0x%x with sync_mode %d",
		link1->link_hdl, link2->link_hdl, cam_session->sync_mode);

done:
	mutex_unlock(&cam_session->lock);
	mutex_unlock(&g_crm_core_dev->crm_lock);
	return rc;
}

int cam_req_mgr_flush_requests(
	struct cam_req_mgr_flush_info *flush_info)
{
	int                               rc = 0;
	struct crm_workq_task            *task = NULL;
	struct cam_req_mgr_core_link     *link = NULL;
	struct cam_req_mgr_flush_info    *flush;
	struct crm_task_payload          *task_data;
	struct cam_req_mgr_core_session  *session = NULL;

	if (!flush_info) {
		CAM_ERR(CAM_CRM, "flush req is NULL");
		return -EFAULT;
	}
	if (flush_info->flush_type >= CAM_REQ_MGR_FLUSH_TYPE_MAX) {
		CAM_ERR(CAM_CRM, "incorrect flush type %x",
			flush_info->flush_type);
		return -EINVAL;
	}

	mutex_lock(&g_crm_core_dev->crm_lock);
	/* session hdl's priv data is cam session struct */
	session = (struct cam_req_mgr_core_session *)
		cam_get_device_priv(flush_info->session_hdl);
	if (!session) {
		CAM_ERR(CAM_CRM, "Invalid session %x", flush_info->session_hdl);
		rc = -EINVAL;
		goto end;
	}
	if (session->num_links <= 0) {
		CAM_WARN(CAM_CRM, "No active links in session %x",
		flush_info->session_hdl);
		goto end;
	}

	link = (struct cam_req_mgr_core_link *)
		cam_get_device_priv(flush_info->link_hdl);
	if (!link) {
		CAM_DBG(CAM_CRM, "link ptr NULL %x", flush_info->link_hdl);
		rc = -EINVAL;
		goto end;
	}

	task = cam_req_mgr_workq_get_task(link->workq);
	if (!task) {
		rc = -ENOMEM;
		goto end;
	}

	task_data = (struct crm_task_payload *)task->payload;
	task_data->type = CRM_WORKQ_TASK_FLUSH_REQ;
	flush = (struct cam_req_mgr_flush_info *)&task_data->u;
	flush->req_id = flush_info->req_id;
	flush->link_hdl = flush_info->link_hdl;
	flush->flush_type = flush_info->flush_type;
	task->process_cb = &cam_req_mgr_process_flush_req;
	init_completion(&link->workq_comp);
	rc = cam_req_mgr_workq_enqueue_task(task, link, CRM_TASK_PRIORITY_0);

	/* Blocking call */
	rc = wait_for_completion_timeout(
		&link->workq_comp,
		msecs_to_jiffies(CAM_REQ_MGR_SCHED_REQ_TIMEOUT));
end:
	mutex_unlock(&g_crm_core_dev->crm_lock);
	return rc;
}

int cam_req_mgr_link_control(struct cam_req_mgr_link_control *control)
{
	int                               rc = 0;
	int                               i, j;
	struct cam_req_mgr_core_link     *link = NULL;

	struct cam_req_mgr_connected_device *dev = NULL;
	struct cam_req_mgr_link_evt_data     evt_data;

	if (!control) {
		CAM_ERR(CAM_CRM, "Control command is NULL");
		rc = -EINVAL;
		goto end;
	}

	if ((control->num_links <= 0) ||
		(control->num_links > MAX_LINKS_PER_SESSION)) {
		CAM_ERR(CAM_CRM, "Invalid number of links %d",
			control->num_links);
		rc = -EINVAL;
		goto end;
	}

	mutex_lock(&g_crm_core_dev->crm_lock);
	for (i = 0; i < control->num_links; i++) {
		link = (struct cam_req_mgr_core_link *)
			cam_get_device_priv(control->link_hdls[i]);
		if (!link) {
			CAM_ERR(CAM_CRM, "Link(%d) is NULL on session 0x%x",
				i, control->session_hdl);
			rc = -EINVAL;
			break;
		}

		mutex_lock(&link->lock);
		if (control->ops == CAM_REQ_MGR_LINK_ACTIVATE) {
			/* Start SOF watchdog timer */
			rc = crm_timer_init(&link->watchdog,
				CAM_REQ_MGR_WATCHDOG_TIMEOUT, link,
				&__cam_req_mgr_sof_freeze);
			if (rc < 0) {
				CAM_ERR(CAM_CRM,
					"SOF timer start fails: link=0x%x",
					link->link_hdl);
				rc = -EFAULT;
			}
			/* notify nodes */
			for (j = 0; j < link->num_devs; j++) {
				dev = &link->l_dev[j];
				evt_data.evt_type = CAM_REQ_MGR_LINK_EVT_RESUME;
				evt_data.link_hdl =  link->link_hdl;
				evt_data.dev_hdl = dev->dev_hdl;
				evt_data.req_id = 0;
				if (dev->ops && dev->ops->process_evt)
					dev->ops->process_evt(&evt_data);
			}
		} else if (control->ops == CAM_REQ_MGR_LINK_DEACTIVATE) {
			/* Destroy SOF watchdog timer */
			spin_lock_bh(&link->link_state_spin_lock);
			crm_timer_exit(&link->watchdog);
			spin_unlock_bh(&link->link_state_spin_lock);
			/* notify nodes */
			for (j = 0; j < link->num_devs; j++) {
				dev = &link->l_dev[j];
				evt_data.evt_type = CAM_REQ_MGR_LINK_EVT_PAUSE;
				evt_data.link_hdl =  link->link_hdl;
				evt_data.dev_hdl = dev->dev_hdl;
				evt_data.req_id = 0;
				if (dev->ops && dev->ops->process_evt)
					dev->ops->process_evt(&evt_data);
			}
		} else {
			CAM_ERR(CAM_CRM, "Invalid link control command");
			rc = -EINVAL;
		}
		mutex_unlock(&link->lock);
	}
	mutex_unlock(&g_crm_core_dev->crm_lock);
end:
	return rc;
}


int cam_req_mgr_core_device_init(void)
{
	int i;
	CAM_DBG(CAM_CRM, "Enter g_crm_core_dev %pK", g_crm_core_dev);

	if (g_crm_core_dev) {
		CAM_WARN(CAM_CRM, "core device is already initialized");
		return 0;
	}
	g_crm_core_dev = kzalloc(sizeof(*g_crm_core_dev),
		GFP_KERNEL);
	if (!g_crm_core_dev)
		return -ENOMEM;

	CAM_DBG(CAM_CRM, "g_crm_core_dev %pK", g_crm_core_dev);
	INIT_LIST_HEAD(&g_crm_core_dev->session_head);
	mutex_init(&g_crm_core_dev->crm_lock);
	cam_req_mgr_debug_register(g_crm_core_dev);

	for (i = 0; i < MAXIMUM_LINKS_PER_SESSION; i++) {
		mutex_init(&g_links[i].lock);
		spin_lock_init(&g_links[i].link_state_spin_lock);
		atomic_set(&g_links[i].is_used, 0);
		CAM_DBG(CAM_CRM, "cam_req_mgr_core_device_init: %p",&g_links[i]);
		cam_req_mgr_core_link_reset(&g_links[i]);
	}
	return 0;
}

int cam_req_mgr_core_device_deinit(void)
{
	if (!g_crm_core_dev) {
		CAM_ERR(CAM_CRM, "NULL pointer");
		return -EINVAL;
	}

	CAM_DBG(CAM_CRM, "g_crm_core_dev %pK", g_crm_core_dev);
	mutex_destroy(&g_crm_core_dev->crm_lock);
	kfree(g_crm_core_dev);
	g_crm_core_dev = NULL;

	return 0;
}
