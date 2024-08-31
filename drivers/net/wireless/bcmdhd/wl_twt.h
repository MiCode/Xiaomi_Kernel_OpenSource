/*
 * Target Wake Time header
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
#ifndef _wl_twt_h_
#define _wl_twt_h_

#include <typedefs.h>
#include <bcmendian.h>
#include <net/cfg80211.h>
#include <dhd_dbg.h>
#include <dhd_linux.h>
#include <802.11ah.h>
#include "dhd_linux_priv.h"
#include "wlioctl.h"
#include "wldev_common.h"
#include "wl_cfg80211.h"
#include "wl_cfgvendor.h"
#include "bcmutils.h"
#include "ifx_nl80211.h"

enum wl_twt_session_state {
	TWT_SESSION_SETUP_COMPLETE,
	TWT_SESSION_TEARDOWN_COMPLETE
};

typedef struct wl_twt_param {
	uint8 twt_oper;
	enum ifx_twt_param_nego_type negotiation_type;
	enum ifx_twt_oper_setup_cmd_type setup_cmd;
	uint8 dialog_token;
	uint64 twt;
	uint64 twt_offset;
	uint8 min_twt;
	uint8 exponent;
	uint16 mantissa;
	uint8 requestor;
	uint8 trigger;
	uint8 implicit;
	uint8 flow_type;
	uint8 flow_id;
	uint8 bcast_twt_id;
	uint8 protection;
	uint8 twt_channel;
	uint8 twt_info_frame_disabled;
	uint8 min_twt_unit;
	uint8 teardown_all_twt;
} wl_twt_param_t;

typedef struct wl_twt_session {
	uint8 ifidx;
	uint8 state;
	wl_twt_param_t twt_param;
	struct list_head list;
} wl_twt_session_t;

typedef struct wl_twt_ctx {
	dhd_pub_t *dhd;
	struct list_head twt_session_list;
} wl_twt_ctx_t;

/* Nominal Minimum Wake Duration derivation from Wake Duration */
inline void
wl_twt_wake_dur_to_min_twt(uint32 wake_dur, uint8 *min_twt, uint8 *min_twt_unit);
/* Wake Interval Mantissa & Exponent derivation from Wake Interval */
inline void
wl_twt_uint32_to_float(uint32 val, uint8 *exp, uint16 *mant);
int wl_twt_cleanup_session_records(dhd_pub_t *dhd, u8 ifidx);
int wl_twt_oper(struct net_device *pri_ndev,
		struct wireless_dev *wdev, wl_twt_param_t twt_param);
int wl_twt_event(dhd_pub_t *dhd, wl_event_msg_t *event, void *event_data);
int wl_twt_init(dhd_pub_t *dhd);
int wl_twt_deinit(dhd_pub_t *dhd);

#endif /* _wl_twt_h_ */
