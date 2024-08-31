/*
 * Target Wake Time Module which is responsible for acting as an
 * interface between the userspace and firmware.
 *
 * Portions of this code are copyright (c) 2023 Cypress Semiconductor Corporation,
 * an Infineon company
 *
 * This program is the proprietary software of infineon and/or
 * its licensors, and may only be used, duplicated, modified or distributed
 * pursuant to the terms and conditions of a separate, written license
 * agreement executed between you and infineon (an "Authorized License").
 * Except as set forth in an Authorized License, infineon grants no license
 * (express or implied), right to use, or waiver of any kind with respect to
 * the Software, and infineon expressly reserves all rights in and to the
 * Software and all intellectual property rights therein.  IF YOU HAVE NO
 * AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY
 * WAY, AND SHOULD IMMEDIATELY NOTIFY INFINEON AND DISCONTINUE ALL USE OF
 * THE SOFTWARE.
 *
 * Except as expressly set forth in the Authorized License,
 *
 * 1. This program, including its structure, sequence and organization,
 * constitutes the valuable trade secrets of infineon, and you shall use
 * all reasonable efforts to protect the confidentiality thereof, and to
 * use this information only in connection with your use of infineon
 * integrated circuit products.
 *
 * 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
 * "AS IS" AND WITH ALL FAULTS AND INFINEON MAKES NO PROMISES,
 * REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR
 * OTHERWISE, WITH RESPECT TO THE SOFTWARE.  INFINEON SPECIFICALLY
 * DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
 * NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
 * ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 * CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
 * OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 * 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL
 * INFINEON OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL,
 * SPECIAL, INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR
 * IN ANY WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
 * IF INFINEON HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii)
 * ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF
 * OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY
 * NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 *
 *
 * <<Infineon-WL-IPTag/Open:>>
 *
 * $Id$
 */

#ifdef WL11AX

#include "wl_twt.h"

static DEFINE_SPINLOCK(twt_session_list_lock);

/*
 * Nominal Minimum Wake Duration derivation from Wake Duration
 */
inline void
wl_twt_wake_dur_to_min_twt(uint32 wake_dur, uint8 *min_twt, uint8 *min_twt_unit)
{
	if(*min_twt_unit == 1) {
		/*
		 * If min_twt_unit is 1, then min_twt is
		 * in units of TUs (i.e) 102400 usecs.
		 */
		*min_twt = wake_dur / 102400;
	} else if(*min_twt_unit == 0) {
		/*
		 * If min_twt_unit is 0, then min_twt is
		 * in units of 256 usecs.
		 */
		*min_twt = wake_dur / 256;
	} else {
		/* Invalid min_twt */
		*min_twt = 0;
	}
}

/*
 * Wake Duration derivation from Nominal Minimum Wake Duration
 */
static inline uint32
wl_twt_min_twt_to_wake_dur(uint8 min_twt, uint8 min_twt_unit)
{
	uint32 wake_dur;

	if (min_twt_unit == 1) {
		/*
		 * If min_twt_unit is 1, then min_twt is
		 * in units of TUs (i.e) 102400 usecs.
		 */
		wake_dur = (uint32)min_twt * 102400;
	} else if (min_twt_unit == 0) {
		/*
		 * If min_twt_unit is 0, then min_twt is
		 * in units of 256 usecs.
		 */
		wake_dur = (uint32)min_twt * 256;
	} else {
		/* Invalid min_twt */
		wake_dur = 0;
	}

	return wake_dur;
}

/*
 * Wake Interval Mantissa & Exponent derivation from Wake Interval
 */
inline void
wl_twt_uint32_to_float(uint32 val, uint8 *exp, uint16 *mant)
{
	uint8 lzs = (uint8)__builtin_clz(val); /* leading 0's */
	uint8 shift = lzs < 16 ? 16 - lzs : 0;

	*mant = (uint16)(val >> shift);
	*exp = shift;
}

/*
 * Wake Interval derivation from Wake Interval Mantissa & Exponent
 */
static inline uint32
wl_twt_float_to_uint32(uint8 exponent, uint16 mantissa)
{
	return (uint32)mantissa << exponent;
}

wl_twt_session_t* wl_twt_lookup_session_by_flow_id(struct list_head *twt_session_list,
						   uint8 flow_id)
{
	wl_twt_session_t *iter = NULL;

	list_for_each_entry(iter, twt_session_list, list) {
		if (iter->twt_param.negotiation_type != IFX_TWT_PARAM_NEGO_TYPE_ITWT)
			continue;

		if (iter->twt_param.flow_id == flow_id)
			return iter;
	}

	return NULL;
}

int wl_twt_add_session_to_list(struct list_head *twt_session_list, dhd_pub_t *dhd,
			       uint8 ifidx, wl_twt_param_t twt_param)
{
	int ret = BCME_OK;
	wl_twt_session_t *new_twt_session;

	new_twt_session = (wl_twt_session_t *)MALLOCZ(dhd->osh,
						      sizeof(wl_twt_session_t));
	if (!new_twt_session) {
		WL_ERR(("TWT: Failed to alloc memory for new session"));
		ret = BCME_NOMEM;
		goto exit;
	}

	new_twt_session->ifidx = ifidx;
	new_twt_session->twt_param = twt_param;
	new_twt_session->state = TWT_SESSION_SETUP_COMPLETE;

	spin_lock_bh(&twt_session_list_lock);
	WL_INFORM(("TWT: Adding TWT session with flow ID: %d", twt_param.flow_id));
	list_add_tail(&new_twt_session->list, twt_session_list);
exit:
	spin_unlock_bh(&twt_session_list_lock);
	return ret;
}

int wl_twt_del_session_by_flow_id(struct list_head *twt_session_list, uint8 flow_id)
{
	int ret = BCME_OK;
	wl_twt_session_t *twt_session = NULL;

	spin_lock_bh(&twt_session_list_lock);
	twt_session = wl_twt_lookup_session_by_flow_id(twt_session_list, flow_id);
	if (twt_session) {
		WL_INFORM(("TWT: Deleting TWT session with flow ID: %d", flow_id));
		list_del(&twt_session->list);
		kfree(twt_session);
	} else {
		WL_INFORM(("TWT: TWT session with flow ID: %d is not found to be deleted",
			 flow_id));
		ret = -1;
		goto exit;
	}
exit:
	spin_unlock_bh(&twt_session_list_lock);
	return ret;
}

int wl_twt_count_session(struct list_head *twt_session_list)
{
	wl_twt_session_t *twt_session = NULL;
	int ct = 0;

	list_for_each_entry(twt_session, twt_session_list, list) {
		if (twt_session->twt_param.negotiation_type ==
		    IFX_TWT_PARAM_NEGO_TYPE_ITWT)
			ct++;
	}

	return ct;
}

void wl_twt_flush_session_list(struct list_head *twt_session_list, u8 ifidx)
{
	wl_twt_session_t *entry = NULL, *next = NULL;

	spin_lock_bh(&twt_session_list_lock);
	list_for_each_entry_safe(entry, next, twt_session_list, list) {
		if ((ifidx != 0xFF) &&
		    (ifidx != entry->ifidx))
			continue;

		WL_INFORM(("TWT: Deleting TWT session with flow ID: %d",
			 entry->twt_param.flow_id));
		list_del(&entry->list);
		kfree(entry);
	}
	spin_unlock_bh(&twt_session_list_lock);
}

int wl_twt_cleanup_session_records(dhd_pub_t *dhd, u8 ifidx)
{
	wl_twt_ctx_t *twt_ctx = (wl_twt_ctx_t *)dhd->twt_ctx;
	int ret = BCME_OK;

	NULL_CHECK(twt_ctx,
		   "TWT: Failed to cleanup session records, Module not initialized",
		   ret);

	wl_twt_flush_session_list(&twt_ctx->twt_session_list, ifidx);

	return ret;
}

int wl_twt_setup(wl_twt_ctx_t *twt_ctx, struct wireless_dev *wdev,
		 wl_twt_param_t twt_param)
{
	wl_twt_setup_t val;
	s32 bw = BCME_OK;
	u8 mybuf[WLC_IOCTL_SMLEN] = {0};
	u8 resp_buf[WLC_IOCTL_SMLEN] = {0};
	uint8 *rem = mybuf;
	uint16 rem_len = sizeof(mybuf);
	wl_twt_session_t *twt_session = NULL;

	bzero(&val, sizeof(val));
	val.version = WL_TWT_SETUP_VER;
	val.length = sizeof(val.version) + sizeof(val.length);

	/* Default values, Override Below */
	val.desc.flow_flags = 0x0;
	val.desc.wake_dur = 0xFFFFFFFF;
	val.desc.wake_int = 0xFFFFFFFF;
	val.desc.wake_int_max = 0xFFFFFFFF;

	/* TWT Negotiation_type */
	val.desc.negotiation_type = (uint8)twt_param.negotiation_type;

	switch (val.desc.negotiation_type) {
	case IFX_TWT_PARAM_NEGO_TYPE_ITWT:
		/* Flow ID */
		val.desc.flow_id = twt_param.flow_id;

		if (val.desc.flow_id == 0xFF) {
			/* Let the FW choose the Flow ID */
			break;
		}

		/* Lookup the active session list for the requested flow ID */
		twt_session =
			wl_twt_lookup_session_by_flow_id(&twt_ctx->twt_session_list,
							 twt_param.flow_id);
		if (twt_session) {
			WL_ERR(("TWT: Setup REQ: flow ID: %d is already active",
				twt_param.flow_id));
			bw = BCME_ERROR;
			goto exit;
		}
		break;
	case IFX_TWT_PARAM_NEGO_TYPE_BTWT:
		/* Broadcast TWT ID */
		val.desc.bid = twt_param.bcast_twt_id;

		/* TODO: Handle the Broadcast TWT Setup REQ */

		/* FALLTHRU */
	default:
		WL_ERR(("TWT: Setup REQ: Negotiation Type %d not handled",
			twt_param.negotiation_type));
		bw = BCME_UNSUPPORTED;
		goto exit;
	}

	/* Setup command */
	val.desc.setup_cmd = twt_param.setup_cmd;

	/* Flow flags */
	val.desc.flow_flags |= ((twt_param.negotiation_type & 0x02) >> 1 ?
				WL_TWT_FLOW_FLAG_BROADCAST : 0);
	val.desc.flow_flags |= (twt_param.implicit ? WL_TWT_FLOW_FLAG_IMPLICIT : 0);
	val.desc.flow_flags |= (twt_param.flow_type ? WL_TWT_FLOW_FLAG_UNANNOUNCED : 0);
	val.desc.flow_flags |= (twt_param.trigger ? WL_TWT_FLOW_FLAG_TRIGGER : 0);
	val.desc.flow_flags |= ((twt_param.negotiation_type & 0x01) ?
				WL_TWT_FLOW_FLAG_WAKE_TBTT_NEGO : 0);
	val.desc.flow_flags |= (twt_param.requestor ? WL_TWT_FLOW_FLAG_REQUEST : 0);
	val.desc.flow_flags |= (twt_param.protection ? WL_TWT_FLOW_FLAG_PROTECT : 0);

	if (twt_param.twt) {
		/* Target Wake Time parameter */
		val.desc.wake_time_h = htod32((uint32)(twt_param.twt >> 32));
		val.desc.wake_time_l = htod32((uint32)(twt_param.twt));
		val.desc.wake_type = WL_TWT_TIME_TYPE_BSS;
	} else if (twt_param.twt_offset) {
		/* Target Wake Time offset parameter */
		val.desc.wake_time_h = htod32((uint32)(twt_param.twt_offset >> 32));
		val.desc.wake_time_l = htod32((uint32)(twt_param.twt_offset));
		val.desc.wake_type = WL_TWT_TIME_TYPE_OFFSET;
	} else {
		/* Let the FW choose the Target Wake Time */
		val.desc.wake_time_h = 0x0;
		val.desc.wake_time_l = 0x0;
		val.desc.wake_type = WL_TWT_TIME_TYPE_AUTO;
	}

	/* Wake Duration or Service Period */
	val.desc.wake_dur = htod32(wl_twt_min_twt_to_wake_dur(twt_param.min_twt,
							      twt_param.min_twt_unit));

	/* Wake Interval or Service Interval */
	val.desc.wake_int = htod32(wl_twt_float_to_uint32(twt_param.exponent,
							  twt_param.mantissa));

	bw = bcm_pack_xtlv_entry(&rem, &rem_len, WL_TWT_CMD_SETUP, sizeof(val),
				 (uint8 *)&val, BCM_XTLV_OPTION_ALIGN32);
	if (bw != BCME_OK) {
		WL_ERR(("TWT: Setup REQ: Failed to pack IOVAR, ret: %d", bw));
		goto exit;
	}

	bw = wldev_iovar_setbuf(wdev_to_ndev(wdev), "twt", mybuf,
				sizeof(mybuf) - rem_len, resp_buf,
				WLC_IOCTL_SMLEN, NULL);
	if (bw != BCME_OK) {
		WL_ERR(("TWT: Setup REQ: Failed, ret: %d", bw));
		goto exit;
	}

	WL_INFORM(("TWT: Setup REQ: Initiated\n"
		   "Setup command	: %u\n"
		   "Flow flags		: 0x %02x\n"
		   "Flow ID		: %u\n"
		   "Broadcast TWT ID	: %u\n"
		   "Wake Time H,L	: 0x %08x %08x\n"
		   "Wake Type		: %u\n"
		   "Wake Dururation	: %u usecs\n"
		   "Wake Interval	: %u usecs\n"
		   "Negotiation type	: %u\n",
		   val.desc.setup_cmd,
		   val.desc.flow_flags,
		   val.desc.flow_id,
		   val.desc.bid,
		   val.desc.wake_time_h,
		   val.desc.wake_time_l,
		   val.desc.wake_type,
		   val.desc.wake_dur,
		   val.desc.wake_int,
		   val.desc.negotiation_type));
exit:
	return bw;
}

int wl_twt_teardown(wl_twt_ctx_t *twt_ctx, struct wireless_dev *wdev,
		    wl_twt_param_t twt_param)
{
	wl_twt_teardown_t val;
	s32 bw = BCME_OK;
	u8 mybuf[WLC_IOCTL_SMLEN] = {0};
	u8 resp_buf[WLC_IOCTL_SMLEN] = {0};
	uint8 *rem = mybuf;
	uint16 rem_len = sizeof(mybuf);
	wl_twt_session_t *twt_session = NULL;

	bzero(&val, sizeof(val));
	val.version = WL_TWT_TEARDOWN_VER;
	val.length = sizeof(val.version) + sizeof(val.length);

	/* TWT Negotiation_type */
	val.teardesc.negotiation_type = (uint8)twt_param.negotiation_type;

	switch (val.teardesc.negotiation_type) {
	case IFX_TWT_PARAM_NEGO_TYPE_ITWT:
		/* Teardown all Negotiated TWT */
		val.teardesc.alltwt = twt_param.teardown_all_twt;

		if (val.teardesc.alltwt) {
			int sess_ct =
			wl_twt_count_session(&twt_ctx->twt_session_list);
			if (!sess_ct) {
				WL_ERR(("TWT: Teardown REQ: No active sessions"));
				bw = BCME_ERROR;
				goto exit;
			}
			break;
		}

		/* Flow ID */
		if (twt_param.flow_id >= 0 && twt_param.flow_id <= 0x7) {
			val.teardesc.flow_id = twt_param.flow_id;
		} else {
			WL_ERR(("TWT: Teardown REQ: flow ID: %d is invalid",
				twt_param.flow_id));
			bw = BCME_ERROR;
			goto exit;
		}

		/* Lookup the active session list for the same flow ID */
		twt_session =
			wl_twt_lookup_session_by_flow_id(&twt_ctx->twt_session_list,
							 twt_param.flow_id);
		if (!twt_session) {
			WL_ERR(("TWT: Teardown REQ: flow ID: %d is not active",
				twt_param.flow_id));
			bw = BCME_ERROR;
			goto exit;
		}
		break;
	case IFX_TWT_PARAM_NEGO_TYPE_BTWT:
		/* Broadcast TWT ID */
		val.teardesc.bid = twt_param.bcast_twt_id;

		/* TODO: Handle the Broadcast TWT Teardown REQ */

		/* FALLTHRU */
	default:
		WL_ERR(("TWT: Teardown REQ: Negotiation Type %d not handled",
			twt_param.negotiation_type));
		bw = BCME_UNSUPPORTED;
		goto exit;
	}

	bw = bcm_pack_xtlv_entry(&rem, &rem_len, WL_TWT_CMD_TEARDOWN, sizeof(val),
				 (uint8 *)&val, BCM_XTLV_OPTION_ALIGN32);
	if (bw != BCME_OK) {
		WL_ERR(("TWT: Teardown REQ: Failed to pack IOVAR, ret: %d", bw));
		goto exit;
	}

	bw = wldev_iovar_setbuf(wdev_to_ndev(wdev), "twt", mybuf,
				sizeof(mybuf) - rem_len, resp_buf,
				WLC_IOCTL_SMLEN, NULL);
	if (bw != BCME_OK) {
		WL_ERR(("TWT: Teardown REQ: Failed, ret: %d", bw));
		goto exit;
	}

	WL_INFORM(("TWT: Teardown REQ: Initiated\n"
		   "Flow ID		: %u\n"
		   "Broadcast TWT ID	: %u\n"
		   "Negotiation type	: %u\n"
		   "Teardown all TWT	: %u\n",
		   val.teardesc.flow_id,
		   val.teardesc.bid,
		   val.teardesc.negotiation_type,
		   val.teardesc.alltwt));
exit:
	return bw;
}

int wl_twt_oper(struct net_device *pri_ndev,
		struct wireless_dev *wdev, wl_twt_param_t twt_param)
{
	int ret = -1;
#ifdef WL_DHD_XR
	dhd_pub_t *dhd = (dhd_pub_t *)dhd_get_pub(wdev->netdev);
#else
	dhd_info_t *dhd_inf = *(dhd_info_t **)netdev_priv(pri_ndev);
	dhd_pub_t *dhd = &dhd_inf->pub;
#endif /* WL_DHD_XR */
	wl_twt_ctx_t *twt_ctx = NULL;

	NULL_CHECK(dhd, "dhd is NULL", ret);

	twt_ctx = (wl_twt_ctx_t *)dhd->twt_ctx;
	NULL_CHECK(twt_ctx, "TWT: REQ: Failed, Module not initialized", ret);

	switch (twt_param.twt_oper) {
		case IFX_TWT_OPER_SETUP:
			ret = wl_twt_setup(twt_ctx, wdev, twt_param);
			break;
		case IFX_TWT_OPER_TEARDOWN:
			ret = wl_twt_teardown(twt_ctx, wdev, twt_param);
			break;
		default:
			WL_ERR(("TWT: REQ: Requested operation %d not supported",
				twt_param.twt_oper));
			ret = BCME_UNSUPPORTED;
			goto exit;
	}
exit:
	return ret;
}

int wl_twt_setup_event(wl_twt_ctx_t *twt_ctx, struct wireless_dev *wdev,
		       wl_event_msg_t *event, void *event_data)
{
	wl_twt_setup_cplt_t *setup_complete;
	wl_twt_sdesc_t *setup_desc;
	wl_twt_param_t twt_param;
	int ret = BCME_OK;

	setup_complete = (wl_twt_setup_cplt_t *)event_data;
	setup_desc = (wl_twt_sdesc_t *)(event_data + sizeof(wl_twt_setup_cplt_t));

	/* TWT Negotiation_type */
	twt_param.negotiation_type = setup_desc->negotiation_type;

	switch (twt_param.negotiation_type) {
		case IFX_TWT_PARAM_NEGO_TYPE_ITWT:
			/* Flow ID */
			twt_param.flow_id = setup_desc->flow_id;
			break;
		case IFX_TWT_PARAM_NEGO_TYPE_BTWT:
			/* Broadcast TWT ID */
			twt_param.bcast_twt_id = setup_desc->bid;

			/* TODO: Handle the Broadcast TWT Setup Event */

			/* FALLTHRU */
		default:
			WL_ERR(("TWT: Setup EVT: Negotiation Type %d not handled",
				twt_param.negotiation_type));
			ret = BCME_UNSUPPORTED;
			goto exit;
	}

	/* Setup command */
	if (setup_desc->setup_cmd != TWT_SETUP_CMD_ACCEPT_TWT) {
		WL_ERR(("TWT: Setup EVT: Request not accepted by the AP"));
		ret = BCME_ERROR;
		goto exit;
	}
	twt_param.setup_cmd = setup_desc->setup_cmd;

	/* Flow flags */
	twt_param.implicit = (setup_desc->flow_flags & WL_TWT_FLOW_FLAG_IMPLICIT) ? 1 : 0;
	twt_param.flow_type = (setup_desc->flow_flags & WL_TWT_FLOW_FLAG_UNANNOUNCED) ? 1 : 0;
	twt_param.trigger = (setup_desc->flow_flags & WL_TWT_FLOW_FLAG_TRIGGER) ? 1 : 0;
	twt_param.requestor = (setup_desc->flow_flags & WL_TWT_FLOW_FLAG_REQUEST) ? 1 : 0;
	twt_param.protection = (setup_desc->flow_flags & WL_TWT_FLOW_FLAG_PROTECT) ? 1 : 0;

	/* Target Wake Time */
	twt_param.twt = dtoh64((uint64)setup_desc->wake_time_h << 32) |
			dtoh64((uint64)setup_desc->wake_time_l);

	/* Wake Duration or Service Period */
	wl_twt_wake_dur_to_min_twt(htod32(setup_desc->wake_dur),
				   &twt_param.min_twt,
				   &twt_param.min_twt_unit);

	/* Wake Interval or Service Interval */
	wl_twt_uint32_to_float(dtoh32(setup_desc->wake_int),
				      &twt_param.exponent,
				      &twt_param.mantissa);

	ret = wl_twt_add_session_to_list(&twt_ctx->twt_session_list,
					 twt_ctx->dhd, event->ifidx, twt_param);
	if (ret) {
		WL_ERR(("TWT: Setup EVT: Failed to add new session to list"));
		goto exit;
	}

	WL_INFORM(("TWT: Setup EVT: Succeeded\n"
		   "Setup command	: %u\n"
		   "Flow flags		: 0x %02x\n"
		   "Flow ID		: %u\n"
		   "Broadcast TWT ID	: %u\n"
		   "Wake Time H,L	: 0x %08x %08x\n"
		   "Wake Type		: %u\n"
		   "Wake Dururation	: %u usecs\n"
		   "Wake Interval	: %u usecs\n"
		   "Negotiation type	: %u\n",
		   setup_desc->setup_cmd,
		   setup_desc->flow_flags,
		   setup_desc->flow_id,
		   setup_desc->bid,
		   setup_desc->wake_time_h,
		   setup_desc->wake_time_l,
		   setup_desc->wake_type,
		   setup_desc->wake_dur,
		   setup_desc->wake_int,
		   setup_desc->negotiation_type));
exit:
	return ret;
}

int wl_twt_teardown_event(wl_twt_ctx_t *twt_ctx, struct wireless_dev *wdev,
			  wl_event_msg_t *event, void *event_data)
{
	dhd_pub_t *dhd = twt_ctx->dhd;
	wl_twt_teardown_cplt_t *teardown_complete;
	wl_twt_teardesc_t *teardown_desc;
	wl_twt_param_t twt_param;
	int ret = BCME_OK;
	s32 ifidx = DHD_BAD_IF;

	ifidx = dhd_net2idx(dhd->info, wdev_to_ndev(wdev));
	if (ifidx == DHD_BAD_IF) {
		ret = BCME_ERROR;
		goto exit;
	}

	teardown_complete = (wl_twt_teardown_cplt_t *)event_data;
	teardown_desc = (wl_twt_teardesc_t *)(event_data + sizeof(teardown_complete));

	/* TWT Negotiation_type */
	twt_param.negotiation_type = teardown_desc->negotiation_type;

	/* Teardown all Negotiated TWT */
	twt_param.teardown_all_twt = teardown_desc->alltwt;
	if (twt_param.teardown_all_twt) {
		wl_twt_flush_session_list(&twt_ctx->twt_session_list,
					  (u8)ifidx);
	} else {
		switch (twt_param.negotiation_type) {
			case IFX_TWT_PARAM_NEGO_TYPE_ITWT:
				/* Flow ID */
				twt_param.flow_id = teardown_desc->flow_id;
				ret = wl_twt_del_session_by_flow_id(&twt_ctx->twt_session_list,
								    twt_param.flow_id);
				if (ret) {
					WL_ERR(("TWT: Failed to del session from list"));
					goto exit;
				}
				break;
			case IFX_TWT_PARAM_NEGO_TYPE_BTWT:
				/* Broadcast TWT ID */
				twt_param.bcast_twt_id = teardown_desc->bid;
				/* TODO */
				/* FALLTHRU */
			default:
				WL_ERR(("TWT: Negotiation Type not handled\n"));
				ret = BCME_UNSUPPORTED;
				goto exit;
		}
	}

	WL_INFORM(("TWT: Teardown EVT: Succeeded\n"
		   "Flow ID		: %u\n"
		   "Broadcast TWT ID	: %u\n"
		   "Negotiation type	: %u\n"
		   "Teardown all TWT	: %u\n",
		   teardown_desc->flow_id,
		   teardown_desc->bid,
		   teardown_desc->negotiation_type,
		   teardown_desc->alltwt));

exit:
	return ret;
}

int wl_twt_event(dhd_pub_t *dhd, wl_event_msg_t *event, void *event_data)
{
	int ret = BCME_OK;
	wl_twt_ctx_t *twt_ctx = NULL;
	uint32 event_type;
	dhd_if_t *ifp = NULL;
	struct net_device *ndev;
	struct wireless_dev *wdev;

	NULL_CHECK(dhd, "TWT: EVT: Failed, dhd is NULL", ret);

	twt_ctx = (wl_twt_ctx_t *)dhd->twt_ctx;
	NULL_CHECK(twt_ctx, "TWT: EVT: Failed, Module not initialized", ret);

	ifp = dhd_get_ifp(dhd, event->ifidx);
	NULL_CHECK(ifp, "TWT: EVT: Failed, ifp is NULL", ret);

	ndev = ifp->net;
	NULL_CHECK(ndev, "TWT: EVT: Failed, ndev is NULL", ret);

	wdev = ndev_to_wdev(ndev);
	NULL_CHECK(wdev, "TWT: EVT: Failed, wdev is NULL", ret);

	NULL_CHECK(event_data, "TWT: EVT: Failed, event_data is NULL", ret);

	event_type = ntoh32_ua((void *)&event->event_type);
	switch(event_type) {
		case WLC_E_TWT_SETUP:
			ret = wl_twt_setup_event(twt_ctx, wdev, event, event_data);
			if (ret) {
				WL_ERR(("TWT: EVT: Failed to handle TWT Setup event"));
				goto exit;
			}
			break;
		case WLC_E_TWT_TEARDOWN:
			ret = wl_twt_teardown_event(twt_ctx, wdev, event, event_data);
			if (ret) {
				WL_ERR(("TWT: EVT: Failed to handle TWT Teardown event"));
				goto exit;
			}
			break;
		default:
			WL_ERR(("TWT: EVT: Received event %d not handeled",
				event_type));
			ret = BCME_UNSUPPORTED;
			goto exit;
	}

exit:
	return ret;
}

int wl_twt_init(dhd_pub_t *dhd)
{
	int ret = BCME_OK;
	wl_twt_ctx_t *twt_ctx;

	NULL_CHECK(dhd, "TWT: INIT: Failed, dhd is NULL", ret);

	if (dhd->twt_ctx) {
		WL_ERR(("TWT: INIT: Failed, Module already initialized"));
		ret = BCME_ERROR;
	        goto exit;
	}

	dhd->twt_ctx = (wl_twt_ctx_t *)MALLOCZ(dhd->osh, sizeof(wl_twt_ctx_t));
	if (dhd->twt_ctx == NULL) {
		ret = BCME_NOMEM;
	        WL_ERR(("TWT: INIT: Failed to create TWT context"));
		goto exit;
	}
	bzero(dhd->twt_ctx, sizeof(wl_twt_ctx_t));

	twt_ctx = (wl_twt_ctx_t *)dhd->twt_ctx;
	twt_ctx->dhd = dhd;

	INIT_LIST_HEAD(&twt_ctx->twt_session_list);

	WL_INFORM(("TWT: INIT: Module Initialized"));
exit:
	return ret;
}

int wl_twt_deinit(dhd_pub_t *dhd)
{
	int ret = BCME_OK;
	wl_twt_ctx_t *twt_ctx;

	NULL_CHECK(dhd, "TWT: DEINIT: Failed, dhd is NULL", ret);

	twt_ctx = dhd->twt_ctx;
	NULL_CHECK(twt_ctx,
		   "TWT: DEINIT: Failed, Module not Initialized to De-initialize", ret);

	wl_twt_flush_session_list(&twt_ctx->twt_session_list, 0xFF);

	kfree(twt_ctx);
	dhd->twt_ctx = NULL;

	WL_INFORM(("TWT: DEINIT: Module De-initialized"));

	return ret;
}

#endif /* WL11AX */
