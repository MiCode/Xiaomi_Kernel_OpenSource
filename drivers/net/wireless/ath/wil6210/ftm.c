/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/etherdevice.h>
#include <net/netlink.h>
#include "wil6210.h"
#include "ftm.h"
#include "wmi.h"

/* FTM session ID we use with FW */
#define WIL_FTM_FW_SESSION_ID		1

/* fixed spare allocation we reserve in NL messages we allocate */
#define WIL_FTM_NL_EXTRA_ALLOC		32

/* approx maximum length for FTM_MEAS_RESULT NL80211 event */
#define WIL_FTM_MEAS_RESULT_MAX_LENGTH	2048

/* timeout for waiting for standalone AOA measurement, milliseconds */
#define WIL_AOA_MEASUREMENT_TIMEOUT	1000

/* maximum number of allowed FTM measurements per burst */
#define WIL_FTM_MAX_MEAS_PER_BURST	31

/* initial token to use on non-secure FTM measurement */
#define WIL_TOF_FTM_DEFAULT_INITIAL_TOKEN	2

#define WIL_TOF_FTM_MAX_LCI_LENGTH		(240)
#define WIL_TOF_FTM_MAX_LCR_LENGTH		(240)

static const struct
nla_policy wil_nl80211_loc_policy[QCA_WLAN_VENDOR_ATTR_LOC_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_FTM_SESSION_COOKIE] = { .type = NLA_U64 },
	[QCA_WLAN_VENDOR_ATTR_LOC_CAPA] = { .type = NLA_NESTED },
	[QCA_WLAN_VENDOR_ATTR_FTM_MEAS_PEERS] = { .type = NLA_NESTED },
	[QCA_WLAN_VENDOR_ATTR_FTM_MEAS_PEER_RESULTS] = { .type = NLA_NESTED },
	[QCA_WLAN_VENDOR_ATTR_FTM_RESPONDER_ENABLE] = { .type = NLA_FLAG },
	[QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_FTM_INITIAL_TOKEN] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_AOA_TYPE] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_LOC_ANTENNA_ARRAY_MASK] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_FREQ] = { .type = NLA_U32 },
};

static const struct
nla_policy wil_nl80211_ftm_peer_policy[
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_FTM_PEER_MAC_ADDR] = { .len = ETH_ALEN },
	[QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAGS] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_PARAMS] = { .type = NLA_NESTED },
	[QCA_WLAN_VENDOR_ATTR_FTM_PEER_SECURE_TOKEN_ID] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_FTM_PEER_FREQ] = { .type = NLA_U32 },
};

static const struct
nla_policy wil_nl80211_ftm_meas_param_policy[
	QCA_WLAN_VENDOR_ATTR_FTM_PARAM_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_FTM_PARAM_MEAS_PER_BURST] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_FTM_PARAM_NUM_BURSTS_EXP] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_FTM_PARAM_BURST_DURATION] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_FTM_PARAM_BURST_PERIOD] = { .type = NLA_U16 },
};

static u8 wil_ftm_get_channel(struct wil6210_priv *wil,
			      const u8 *mac_addr, u32 freq)
{
	struct wiphy *wiphy = wil_to_wiphy(wil);
	struct cfg80211_bss *bss;
	struct ieee80211_channel *chan;
	u8 channel;

	if (freq) {
		chan = ieee80211_get_channel(wiphy, freq);
		if (!chan) {
			wil_err(wil, "invalid freq: %d\n", freq);
			return 0;
		}
		channel = chan->hw_value;
	} else {
		bss = cfg80211_get_bss(wiphy, NULL, mac_addr,
				       NULL, 0, IEEE80211_BSS_TYPE_ANY,
				       IEEE80211_PRIVACY_ANY);
		if (!bss) {
			wil_err(wil, "Unable to find BSS\n");
			return 0;
		}
		channel = bss->channel->hw_value;
		cfg80211_put_bss(wiphy, bss);
	}

	wil_dbg_misc(wil, "target %pM at channel %d\n", mac_addr, channel);
	return channel;
}

static int wil_ftm_parse_meas_params(struct wil6210_priv *wil,
				     struct nlattr *attr,
				     struct wil_ftm_meas_params *params)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_FTM_PARAM_MAX + 1];
	int rc;

	if (!attr) {
		/* temporary defaults for one-shot measurement */
		params->meas_per_burst = 1;
		params->burst_period = 5; /* 500 milliseconds */
		return 0;
	}
	rc = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_FTM_PARAM_MAX,
			      attr, wil_nl80211_ftm_meas_param_policy);
	if (rc) {
		wil_err(wil, "invalid measurement params\n");
		return rc;
	}
	if (tb[QCA_WLAN_VENDOR_ATTR_FTM_PARAM_MEAS_PER_BURST])
		params->meas_per_burst = nla_get_u8(
			tb[QCA_WLAN_VENDOR_ATTR_FTM_PARAM_MEAS_PER_BURST]);
	if (tb[QCA_WLAN_VENDOR_ATTR_FTM_PARAM_NUM_BURSTS_EXP])
		params->num_of_bursts_exp = nla_get_u8(
			tb[QCA_WLAN_VENDOR_ATTR_FTM_PARAM_NUM_BURSTS_EXP]);
	if (tb[QCA_WLAN_VENDOR_ATTR_FTM_PARAM_BURST_DURATION])
		params->burst_duration = nla_get_u8(
			tb[QCA_WLAN_VENDOR_ATTR_FTM_PARAM_BURST_DURATION]);
	if (tb[QCA_WLAN_VENDOR_ATTR_FTM_PARAM_BURST_PERIOD])
		params->burst_period = nla_get_u16(
			tb[QCA_WLAN_VENDOR_ATTR_FTM_PARAM_BURST_PERIOD]);
	return 0;
}

static int wil_ftm_validate_meas_params(struct wil6210_priv *wil,
					struct wil_ftm_meas_params *params)
{
	/* temporary allow only single-burst */
	if (params->meas_per_burst > WIL_FTM_MAX_MEAS_PER_BURST ||
	    params->num_of_bursts_exp != 0) {
		wil_err(wil, "invalid measurement params\n");
		return -EINVAL;
	}

	return 0;
}

static int wil_ftm_append_meas_params(struct wil6210_priv *wil,
				      struct sk_buff *msg,
				      struct wil_ftm_meas_params *params)
{
	struct nlattr *nl_p;

	nl_p = nla_nest_start(
		msg, QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_MEAS_PARAMS);
	if (!nl_p)
		goto out_put_failure;
	if (nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_FTM_PARAM_MEAS_PER_BURST,
		       params->meas_per_burst) ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_FTM_PARAM_NUM_BURSTS_EXP,
		       params->num_of_bursts_exp) ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_FTM_PARAM_BURST_DURATION,
		       params->burst_duration) ||
	    nla_put_u16(msg, QCA_WLAN_VENDOR_ATTR_FTM_PARAM_BURST_PERIOD,
			params->burst_period))
		goto out_put_failure;
	nla_nest_end(msg, nl_p);
	return 0;
out_put_failure:
	return -ENOBUFS;
}

static int wil_ftm_append_peer_meas_res(struct wil6210_priv *wil,
					struct sk_buff *msg,
					struct wil_ftm_peer_meas_res *res)
{
	struct nlattr *nl_mres, *nl_f;
	int i;

	if (nla_put(msg, QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_MAC_ADDR,
		    ETH_ALEN, res->mac_addr) ||
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_FLAGS,
			res->flags) ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS,
		       res->status))
		goto out_put_failure;
	if (res->status == QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS_FAILED &&
	    nla_put_u8(msg,
		       QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_VALUE_SECONDS,
		       res->value_seconds))
		goto out_put_failure;
	if (res->has_params &&
	    wil_ftm_append_meas_params(wil, msg, &res->params))
		goto out_put_failure;
	nl_mres = nla_nest_start(msg, QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_MEAS);
	if (!nl_mres)
		goto out_put_failure;
	for (i = 0; i < res->n_meas; i++) {
		nl_f = nla_nest_start(msg, i);
		if (!nl_f)
			goto out_put_failure;
		if (nla_put_u64_64bit(msg, QCA_WLAN_VENDOR_ATTR_FTM_MEAS_T1,
				      res->meas[i].t1,
				      QCA_WLAN_VENDOR_ATTR_FTM_MEAS_PAD) ||
		    nla_put_u64_64bit(msg, QCA_WLAN_VENDOR_ATTR_FTM_MEAS_T2,
				      res->meas[i].t2,
				      QCA_WLAN_VENDOR_ATTR_FTM_MEAS_PAD) ||
		    nla_put_u64_64bit(msg, QCA_WLAN_VENDOR_ATTR_FTM_MEAS_T3,
				      res->meas[i].t3,
				      QCA_WLAN_VENDOR_ATTR_FTM_MEAS_PAD) ||
		    nla_put_u64_64bit(msg, QCA_WLAN_VENDOR_ATTR_FTM_MEAS_T4,
				      res->meas[i].t4,
				      QCA_WLAN_VENDOR_ATTR_FTM_MEAS_PAD))
			goto out_put_failure;
		nla_nest_end(msg, nl_f);
	}
	nla_nest_end(msg, nl_mres);
	return 0;
out_put_failure:
	wil_err(wil, "fail to append peer result\n");
	return -ENOBUFS;
}

static void wil_ftm_send_meas_result(struct wil6210_priv *wil,
				     struct wil_ftm_peer_meas_res *res)
{
	struct sk_buff *vendor_event = NULL;
	struct nlattr *nl_res;
	int rc = 0;

	wil_dbg_misc(wil, "sending %d results for peer %pM\n",
		     res->n_meas, res->mac_addr);

	vendor_event = cfg80211_vendor_event_alloc(
				wil_to_wiphy(wil),
				wil->wdev,
				WIL_FTM_MEAS_RESULT_MAX_LENGTH,
				QCA_NL80211_VENDOR_EVENT_FTM_MEAS_RESULT_INDEX,
				GFP_KERNEL);
	if (!vendor_event) {
		wil_err(wil, "fail to allocate measurement result\n");
		rc = -ENOMEM;
		goto out;
	}

	if (nla_put_u64_64bit(
		vendor_event, QCA_WLAN_VENDOR_ATTR_FTM_SESSION_COOKIE,
		wil->ftm.session_cookie, QCA_WLAN_VENDOR_ATTR_PAD)) {
		rc = -ENOBUFS;
		goto out;
	}

	nl_res = nla_nest_start(vendor_event,
				QCA_WLAN_VENDOR_ATTR_FTM_MEAS_PEER_RESULTS);
	if (!nl_res) {
		rc = -ENOBUFS;
		goto out;
	}

	rc = wil_ftm_append_peer_meas_res(wil, vendor_event, res);
	if (rc)
		goto out;

	nla_nest_end(vendor_event, nl_res);
	cfg80211_vendor_event(vendor_event, GFP_KERNEL);
	vendor_event = NULL;
out:
	if (vendor_event)
		kfree_skb(vendor_event);
	if (rc)
		wil_err(wil, "send peer result failed, err %d\n", rc);
}

static void wil_ftm_send_peer_res(struct wil6210_priv *wil)
{
	if (!wil->ftm.has_ftm_res || !wil->ftm.ftm_res)
		return;

	wil_ftm_send_meas_result(wil, wil->ftm.ftm_res);
	wil->ftm.has_ftm_res = 0;
	wil->ftm.ftm_res->n_meas = 0;
}

static void wil_aoa_measurement_timeout(struct work_struct *work)
{
	struct wil_ftm_priv *ftm = container_of(work, struct wil_ftm_priv,
						aoa_timeout_work);
	struct wil6210_priv *wil = container_of(ftm, struct wil6210_priv, ftm);
	struct wil_aoa_meas_result res;

	wil_dbg_misc(wil, "AOA measurement timeout\n");

	memset(&res, 0, sizeof(res));
	ether_addr_copy(res.mac_addr, wil->ftm.aoa_peer_mac_addr);
	res.type = wil->ftm.aoa_type;
	res.status = QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS_ABORTED;
	wil_aoa_cfg80211_meas_result(wil, &res);
}

static int
wil_ftm_cfg80211_start_session(struct wil6210_priv *wil,
			       struct wil_ftm_session_request *request)
{
	int rc = 0;
	bool has_lci = false, has_lcr = false;
	u8 max_meas = 0, channel, *ptr;
	u32 i, cmd_len;
	struct wmi_tof_session_start_cmd *cmd;

	mutex_lock(&wil->ftm.lock);
	if (wil->ftm.session_started) {
		wil_err(wil, "FTM session already running\n");
		rc = -EAGAIN;
		goto out;
	}

	for (i = 0; i < request->n_peers; i++) {
		if (request->peers[i].flags &
		    QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_LCI)
			has_lci = true;
		if (request->peers[i].flags &
		    QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_LCR)
			has_lcr = true;
		max_meas = max(max_meas,
			       request->peers[i].params.meas_per_burst);
	}

	wil->ftm.ftm_res = kzalloc(sizeof(*wil->ftm.ftm_res) +
		      max_meas * sizeof(struct wil_ftm_peer_meas) +
		      (has_lci ? WIL_TOF_FTM_MAX_LCI_LENGTH : 0) +
		      (has_lcr ? WIL_TOF_FTM_MAX_LCR_LENGTH : 0), GFP_KERNEL);
	if (!wil->ftm.ftm_res) {
		rc = -ENOMEM;
		goto out;
	}
	ptr = (u8 *)wil->ftm.ftm_res;
	ptr += sizeof(struct wil_ftm_peer_meas_res) +
	       max_meas * sizeof(struct wil_ftm_peer_meas);
	if (has_lci) {
		wil->ftm.ftm_res->lci = ptr;
		ptr += WIL_TOF_FTM_MAX_LCI_LENGTH;
	}
	if (has_lcr)
		wil->ftm.ftm_res->lcr = ptr;
	wil->ftm.max_ftm_meas = max_meas;

	cmd_len = sizeof(struct wmi_tof_session_start_cmd) +
		  request->n_peers * sizeof(struct wmi_ftm_dest_info);
	cmd = kzalloc(cmd_len, GFP_KERNEL);
	if (!cmd) {
		rc = -ENOMEM;
		goto out_ftm_res;
	}

	cmd->session_id = cpu_to_le32(WIL_FTM_FW_SESSION_ID);
	cmd->num_of_dest = cpu_to_le16(request->n_peers);
	for (i = 0; i < request->n_peers; i++) {
		ether_addr_copy(cmd->ftm_dest_info[i].dst_mac,
				request->peers[i].mac_addr);
		channel = wil_ftm_get_channel(wil, request->peers[i].mac_addr,
					      request->peers[i].freq);
		if (!channel) {
			wil_err(wil, "can't find FTM target at index %d\n", i);
			rc = -EINVAL;
			goto out_cmd;
		}
		cmd->ftm_dest_info[i].channel = channel - 1;
		if (request->peers[i].flags &
		    QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_SECURE) {
			cmd->ftm_dest_info[i].flags |=
				WMI_TOF_SESSION_START_FLAG_SECURED;
			cmd->ftm_dest_info[i].initial_token =
				request->peers[i].secure_token_id;
		} else {
			cmd->ftm_dest_info[i].initial_token =
				WIL_TOF_FTM_DEFAULT_INITIAL_TOKEN;
		}
		if (request->peers[i].flags &
		    QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_ASAP)
			cmd->ftm_dest_info[i].flags |=
				WMI_TOF_SESSION_START_FLAG_ASAP;
		if (request->peers[i].flags &
		    QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_LCI)
			cmd->ftm_dest_info[i].flags |=
				WMI_TOF_SESSION_START_FLAG_LCI_REQ;
		if (request->peers[i].flags &
		    QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_LCR)
			cmd->ftm_dest_info[i].flags |=
				WMI_TOF_SESSION_START_FLAG_LCR_REQ;
		cmd->ftm_dest_info[i].num_of_ftm_per_burst =
			request->peers[i].params.meas_per_burst;
		cmd->ftm_dest_info[i].num_of_bursts_exp =
			request->peers[i].params.num_of_bursts_exp;
		cmd->ftm_dest_info[i].burst_duration =
			request->peers[i].params.burst_duration;
		cmd->ftm_dest_info[i].burst_period =
			cpu_to_le16(request->peers[i].params.burst_period);
	}

	rc = wmi_send(wil, WMI_TOF_SESSION_START_CMDID, cmd, cmd_len);

	if (!rc) {
		wil->ftm.session_cookie = request->session_cookie;
		wil->ftm.session_started = 1;
	}
out_cmd:
	kfree(cmd);
out_ftm_res:
	if (rc) {
		kfree(wil->ftm.ftm_res);
		wil->ftm.ftm_res = NULL;
	}
out:
	mutex_unlock(&wil->ftm.lock);
	return rc;
}

static void
wil_ftm_cfg80211_session_ended(struct wil6210_priv *wil, u32 status)
{
	struct sk_buff *vendor_event = NULL;

	mutex_lock(&wil->ftm.lock);

	if (!wil->ftm.session_started) {
		wil_dbg_misc(wil, "FTM session not started, ignoring event\n");
		goto out;
	}

	/* finish the session */
	wil_dbg_misc(wil, "finishing FTM session\n");

	/* send left-over results if any */
	wil_ftm_send_peer_res(wil);

	wil->ftm.session_started = 0;
	kfree(wil->ftm.ftm_res);
	wil->ftm.ftm_res = NULL;

	vendor_event = cfg80211_vendor_event_alloc(
		wil_to_wiphy(wil),
		wil->wdev,
		WIL_FTM_NL_EXTRA_ALLOC,
		QCA_NL80211_VENDOR_EVENT_FTM_SESSION_DONE_INDEX,
		GFP_KERNEL);
	if (!vendor_event)
		goto out;

	if (nla_put_u64_64bit(vendor_event,
			      QCA_WLAN_VENDOR_ATTR_FTM_SESSION_COOKIE,
			      wil->ftm.session_cookie,
			      QCA_WLAN_VENDOR_ATTR_PAD) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS, status)) {
		wil_err(wil, "failed to fill session done event\n");
		goto out;
	}
	cfg80211_vendor_event(vendor_event, GFP_KERNEL);
	vendor_event = NULL;
out:
	kfree_skb(vendor_event);
	mutex_unlock(&wil->ftm.lock);
}

static void wil_aoa_timer_fn(ulong x)
{
	struct wil6210_priv *wil = (void *)x;

	wil_dbg_misc(wil, "AOA timer\n");
	schedule_work(&wil->ftm.aoa_timeout_work);
}

static int
wil_aoa_cfg80211_start_measurement(struct wil6210_priv *wil,
				   struct wil_aoa_meas_request *request)
{
	int rc = 0;
	struct wmi_aoa_meas_cmd cmd;
	u8 channel;

	mutex_lock(&wil->ftm.lock);

	if (wil->ftm.aoa_started) {
		wil_err(wil, "AOA measurement already running\n");
		rc = -EAGAIN;
		goto out;
	}
	if (request->type >= QCA_WLAN_VENDOR_ATTR_AOA_TYPE_MAX) {
		wil_err(wil, "invalid AOA type: %d\n", request->type);
		rc = -EINVAL;
		goto out;
	}

	channel = wil_ftm_get_channel(wil, request->mac_addr, request->freq);
	if (!channel) {
		rc = -EINVAL;
		goto out;
	}

	memset(&cmd, 0, sizeof(cmd));
	ether_addr_copy(cmd.mac_addr, request->mac_addr);
	cmd.channel = channel - 1;
	cmd.aoa_meas_type = request->type;

	rc = wmi_send(wil, WMI_AOA_MEAS_CMDID, &cmd, sizeof(cmd));
	if (rc)
		goto out;

	ether_addr_copy(wil->ftm.aoa_peer_mac_addr, request->mac_addr);
	mod_timer(&wil->ftm.aoa_timer,
		  jiffies + msecs_to_jiffies(WIL_AOA_MEASUREMENT_TIMEOUT));
	wil->ftm.aoa_started = 1;
out:
	mutex_unlock(&wil->ftm.lock);
	return rc;
}

void wil_aoa_cfg80211_meas_result(struct wil6210_priv *wil,
				  struct wil_aoa_meas_result *result)
{
	struct sk_buff *vendor_event = NULL;

	mutex_lock(&wil->ftm.lock);

	if (!wil->ftm.aoa_started) {
		wil_info(wil, "AOA not started, not sending result\n");
		goto out;
	}

	wil_dbg_misc(wil, "sending AOA measurement result\n");

	vendor_event = cfg80211_vendor_event_alloc(
				wil_to_wiphy(wil),
				wil->wdev,
				result->length + WIL_FTM_NL_EXTRA_ALLOC,
				QCA_NL80211_VENDOR_EVENT_AOA_MEAS_RESULT_INDEX,
				GFP_KERNEL);
	if (!vendor_event) {
		wil_err(wil, "fail to allocate measurement result\n");
		goto out;
	}

	if (nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_MAC_ADDR,
		    ETH_ALEN, result->mac_addr) ||
	    nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_AOA_TYPE,
			result->type) ||
	    nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS,
			result->status) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LOC_ANTENNA_ARRAY_MASK,
			result->antenna_array_mask)) {
		wil_err(wil, "failed to fill vendor event\n");
		goto out;
	}

	if (result->length > 0 &&
	    nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_AOA_MEAS_RESULT,
		    result->length, result->data)) {
		wil_err(wil, "failed to fill vendor event with AOA data\n");
		goto out;
	}

	cfg80211_vendor_event(vendor_event, GFP_KERNEL);

	del_timer_sync(&wil->ftm.aoa_timer);
	wil->ftm.aoa_started = 0;
out:
	mutex_unlock(&wil->ftm.lock);
}

void wil_ftm_evt_session_ended(struct wil6210_priv *wil,
			       struct wmi_tof_session_end_event *evt)
{
	u32 status;

	switch (evt->status) {
	case WMI_TOF_SESSION_END_NO_ERROR:
		status = QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS_OK;
		break;
	case WMI_TOF_SESSION_END_PARAMS_ERROR:
		status = QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS_INVALID;
		break;
	case WMI_TOF_SESSION_END_FAIL:
		status = QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS_FAILED;
		break;
	case WMI_TOF_SESSION_END_ABORTED:
		status = QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS_ABORTED;
		break;
	default:
		status = QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS_FAILED;
		break;
	}

	wil_ftm_cfg80211_session_ended(wil, status);
}

void wil_ftm_evt_per_dest_res(struct wil6210_priv *wil,
			      struct wmi_tof_ftm_per_dest_res_event *evt)
{
	u32 i, index;
	__le64 tmp = 0;
	u8 n_meas;

	mutex_lock(&wil->ftm.lock);

	if (!wil->ftm.session_started || !wil->ftm.ftm_res) {
		wil_dbg_misc(wil, "Session not running, ignoring res event\n");
		goto out;
	}
	if (wil->ftm.has_ftm_res &&
	    !ether_addr_equal(evt->dst_mac, wil->ftm.ftm_res->mac_addr)) {
		wil_dbg_misc(wil,
			     "Results for previous peer not properly terminated\n");
		wil_ftm_send_peer_res(wil);
	}

	if (!wil->ftm.has_ftm_res) {
		ether_addr_copy(wil->ftm.ftm_res->mac_addr, evt->dst_mac);
		wil->ftm.has_ftm_res = 1;
	}

	n_meas = evt->actual_ftm_per_burst;
	switch (evt->status) {
	case WMI_PER_DEST_RES_NO_ERROR:
		wil->ftm.ftm_res->status =
			QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS_OK;
		break;
	case WMI_PER_DEST_RES_TX_RX_FAIL:
		/* FW reports corrupted results here, discard. */
		n_meas = 0;
		wil->ftm.ftm_res->status =
			QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS_OK;
		break;
	case WMI_PER_DEST_RES_PARAM_DONT_MATCH:
		wil->ftm.ftm_res->status =
			QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS_INVALID;
		break;
	default:
		wil_err(wil, "unexpected status %d\n", evt->status);
		wil->ftm.ftm_res->status =
			QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS_INVALID;
		break;
	}

	for (i = 0; i < n_meas; i++) {
		index = wil->ftm.ftm_res->n_meas;
		if (index >= wil->ftm.max_ftm_meas) {
			wil_dbg_misc(wil, "Too many measurements, some lost\n");
			break;
		}
		memcpy(&tmp, evt->responder_ftm_res[i].t1,
		       sizeof(evt->responder_ftm_res[i].t1));
		wil->ftm.ftm_res->meas[index].t1 = le64_to_cpu(tmp);
		memcpy(&tmp, evt->responder_ftm_res[i].t2,
		       sizeof(evt->responder_ftm_res[i].t2));
		wil->ftm.ftm_res->meas[index].t2 = le64_to_cpu(tmp);
		memcpy(&tmp, evt->responder_ftm_res[i].t3,
		       sizeof(evt->responder_ftm_res[i].t3));
		wil->ftm.ftm_res->meas[index].t3 = le64_to_cpu(tmp);
		memcpy(&tmp, evt->responder_ftm_res[i].t4,
		       sizeof(evt->responder_ftm_res[i].t4));
		wil->ftm.ftm_res->meas[index].t4 = le64_to_cpu(tmp);
		wil->ftm.ftm_res->n_meas++;
	}

	if (evt->flags & WMI_PER_DEST_RES_BURST_REPORT_END)
		wil_ftm_send_peer_res(wil);
out:
	mutex_unlock(&wil->ftm.lock);
}

void wil_aoa_evt_meas(struct wil6210_priv *wil,
		      struct wmi_aoa_meas_event *evt,
		      int len)
{
	int data_len = len - offsetof(struct wmi_aoa_meas_event, meas_data);
	struct wil_aoa_meas_result *res;

	data_len = min_t(int, le16_to_cpu(evt->length), data_len);

	res = kmalloc(sizeof(*res) + data_len, GFP_KERNEL);
	if (!res)
		return;

	ether_addr_copy(res->mac_addr, evt->mac_addr);
	res->type = evt->aoa_meas_type;
	res->antenna_array_mask = le32_to_cpu(evt->meas_rf_mask);
	res->status = evt->meas_status;
	res->length = data_len;
	memcpy(res->data, evt->meas_data, data_len);

	wil_dbg_misc(wil, "AOA result status %d type %d mask %d length %d\n",
		     res->status, res->type,
		     res->antenna_array_mask, res->length);

	wil_aoa_cfg80211_meas_result(wil, res);
	kfree(res);
}

int wil_ftm_get_capabilities(struct wiphy *wiphy, struct wireless_dev *wdev,
			     const void *data, int data_len)
{
	struct wil6210_priv *wil = wiphy_to_wil(wiphy);
	struct sk_buff *skb;
	struct nlattr *attr;

	if (!test_bit(WMI_FW_CAPABILITY_FTM, wil->fw_capabilities))
		return -ENOTSUPP;

	/* we should get the capabilities from the FW. for now,
	 * report dummy capabilities for one shot measurement
	 */
	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 128);
	if (!skb)
		return -ENOMEM;
	attr = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_LOC_CAPA);
	if (!attr ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAGS,
			QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAG_FTM_RESPONDER |
			QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAG_FTM_INITIATOR |
			QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAG_ASAP |
			QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAG_AOA) ||
	    nla_put_u16(skb, QCA_WLAN_VENDOR_ATTR_FTM_CAPA_MAX_NUM_SESSIONS,
			1) ||
	    nla_put_u16(skb, QCA_WLAN_VENDOR_ATTR_FTM_CAPA_MAX_NUM_PEERS, 1) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_FTM_CAPA_MAX_NUM_BURSTS_EXP,
		       0) ||
	    nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_FTM_CAPA_MAX_MEAS_PER_BURST,
		       4) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_AOA_CAPA_SUPPORTED_TYPES,
			BIT(QCA_WLAN_VENDOR_ATTR_AOA_TYPE_TOP_CIR_PHASE))) {
		wil_err(wil, "fail to fill get_capabilities reply\n");
		kfree_skb(skb);
		return -ENOMEM;
	}
	nla_nest_end(skb, attr);

	return cfg80211_vendor_cmd_reply(skb);
}

int wil_ftm_start_session(struct wiphy *wiphy, struct wireless_dev *wdev,
			  const void *data, int data_len)
{
	struct wil6210_priv *wil = wiphy_to_wil(wiphy);
	struct wil_ftm_session_request *request;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_LOC_MAX + 1];
	struct nlattr *tb2[QCA_WLAN_VENDOR_ATTR_FTM_PEER_MAX + 1];
	struct nlattr *peer;
	int rc, n_peers = 0, index = 0, tmp;

	if (!test_bit(WMI_FW_CAPABILITY_FTM, wil->fw_capabilities))
		return -ENOTSUPP;

	rc = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_LOC_MAX, data, data_len,
		       wil_nl80211_loc_policy);
	if (rc) {
		wil_err(wil, "Invalid ATTR\n");
		return rc;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_FTM_MEAS_PEERS]) {
		wil_err(wil, "no peers specified\n");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_FTM_SESSION_COOKIE]) {
		wil_err(wil, "session cookie not specified\n");
		return -EINVAL;
	}

	nla_for_each_nested(peer, tb[QCA_WLAN_VENDOR_ATTR_FTM_MEAS_PEERS],
			    tmp)
		n_peers++;

	if (!n_peers) {
		wil_err(wil, "empty peer list\n");
		return -EINVAL;
	}

	/* for now only allow measurement for a single peer */
	if (n_peers != 1) {
		wil_err(wil, "only single peer allowed\n");
		return -EINVAL;
	}

	request = kzalloc(sizeof(*request) +
			  n_peers * sizeof(struct wil_ftm_meas_peer_info),
			  GFP_KERNEL);
	if (!request)
		return -ENOMEM;

	request->session_cookie =
		nla_get_u64(tb[QCA_WLAN_VENDOR_ATTR_FTM_SESSION_COOKIE]);
	request->n_peers = n_peers;
	nla_for_each_nested(peer, tb[QCA_WLAN_VENDOR_ATTR_FTM_MEAS_PEERS],
			    tmp) {
		rc = nla_parse_nested(tb2, QCA_WLAN_VENDOR_ATTR_FTM_PEER_MAX,
				      peer, wil_nl80211_ftm_peer_policy);
		if (rc) {
			wil_err(wil, "Invalid peer ATTR\n");
			goto out;
		}
		if (!tb2[QCA_WLAN_VENDOR_ATTR_FTM_PEER_MAC_ADDR] ||
		    nla_len(tb2[QCA_WLAN_VENDOR_ATTR_FTM_PEER_MAC_ADDR])
			    != ETH_ALEN) {
			wil_err(wil, "Peer MAC address missing or invalid\n");
			rc = -EINVAL;
			goto out;
		}
		memcpy(request->peers[index].mac_addr,
		       nla_data(tb2[QCA_WLAN_VENDOR_ATTR_FTM_PEER_MAC_ADDR]),
		       ETH_ALEN);
		if (tb2[QCA_WLAN_VENDOR_ATTR_FTM_PEER_FREQ])
			request->peers[index].freq = nla_get_u32(
				tb2[QCA_WLAN_VENDOR_ATTR_FTM_PEER_FREQ]);
		if (tb2[QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAGS])
			request->peers[index].flags = nla_get_u32(
				tb2[QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAGS]);
		if (tb2[QCA_WLAN_VENDOR_ATTR_FTM_PEER_SECURE_TOKEN_ID])
			request->peers[index].secure_token_id = nla_get_u8(
			   tb2[QCA_WLAN_VENDOR_ATTR_FTM_PEER_SECURE_TOKEN_ID]);
		rc = wil_ftm_parse_meas_params(
			wil,
			tb2[QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_PARAMS],
			&request->peers[index].params);
		if (!rc)
			rc = wil_ftm_validate_meas_params(
				wil, &request->peers[index].params);
		if (rc)
			goto out;
		index++;
	}

	rc = wil_ftm_cfg80211_start_session(wil, request);
out:
	kfree(request);
	return rc;
}

int wil_ftm_abort_session(struct wiphy *wiphy, struct wireless_dev *wdev,
			  const void *data, int data_len)
{
	struct wil6210_priv *wil = wiphy_to_wil(wiphy);

	wil_dbg_misc(wil, "stub\n");
	return -ENOTSUPP;
}

int wil_ftm_configure_responder(struct wiphy *wiphy, struct wireless_dev *wdev,
				const void *data, int data_len)
{
	struct wil6210_priv *wil = wiphy_to_wil(wiphy);

	wil_dbg_misc(wil, "stub\n");
	return -ENOTSUPP;
}

int wil_aoa_start_measurement(struct wiphy *wiphy, struct wireless_dev *wdev,
			      const void *data, int data_len)
{
	struct wil6210_priv *wil = wiphy_to_wil(wiphy);
	struct wil_aoa_meas_request request;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_LOC_MAX + 1];
	int rc;

	if (!test_bit(WMI_FW_CAPABILITY_FTM, wil->fw_capabilities))
		return -ENOTSUPP;

	wil_dbg_misc(wil, "AOA start measurement\n");

	rc = nla_parse(tb, QCA_WLAN_VENDOR_ATTR_LOC_MAX, data, data_len,
		       wil_nl80211_loc_policy);
	if (rc) {
		wil_err(wil, "Invalid ATTR\n");
		return rc;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_MAC_ADDR] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_AOA_TYPE]) {
		wil_err(wil, "Must specify MAC address and type\n");
		return -EINVAL;
	}

	memset(&request, 0, sizeof(request));
	ether_addr_copy(request.mac_addr,
			nla_data(tb[QCA_WLAN_VENDOR_ATTR_MAC_ADDR]));
	request.type = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_AOA_TYPE]);
	if (tb[QCA_WLAN_VENDOR_ATTR_FREQ])
		request.freq = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_FREQ]);

	rc = wil_aoa_cfg80211_start_measurement(wil, &request);
	return rc;
}

int wil_aoa_abort_measurement(struct wiphy *wiphy, struct wireless_dev *wdev,
			      const void *data, int data_len)
{
	struct wil6210_priv *wil = wiphy_to_wil(wiphy);

	wil_dbg_misc(wil, "stub\n");
	return -ENOTSUPP;
}

void wil_ftm_init(struct wil6210_priv *wil)
{
	mutex_init(&wil->ftm.lock);
	setup_timer(&wil->ftm.aoa_timer, wil_aoa_timer_fn, (ulong)wil);
	INIT_WORK(&wil->ftm.aoa_timeout_work, wil_aoa_measurement_timeout);
}

void wil_ftm_deinit(struct wil6210_priv *wil)
{
	del_timer_sync(&wil->ftm.aoa_timer);
	cancel_work_sync(&wil->ftm.aoa_timeout_work);
	kfree(wil->ftm.ftm_res);
}

void wil_ftm_stop_operations(struct wil6210_priv *wil)
{
	wil_ftm_cfg80211_session_ended(
		wil, QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS_ABORTED);
}
