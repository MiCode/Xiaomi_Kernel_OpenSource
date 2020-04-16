/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef PD_DPM_PRV_H_INCLUDED
#define PD_DPM_PRV_H_INCLUDED

#include <linux/of.h>
#include <linux/device.h>

#define SVID_DATA_LOCAL_MODE(svid_data, n)	\
		(svid_data->local_mode.mode_vdo[n])

#define SVID_DATA_REMOTE_MODE(svid_data, n) \
		(svid_data->remote_mode.mode_vdo[n])

#define SVID_DATA_DFP_GET_ACTIVE_MODE(svid_data)\
	SVID_DATA_REMOTE_MODE(svid_data, svid_data->active_mode-1)

#define SVID_DATA_UFP_GET_ACTIVE_MODE(svid_data)\
	SVID_DATA_LOCAL_MODE(svid_data, svid_data->active_mode-1)

extern int dpm_check_supported_modes(void);

struct svdm_svid_ops {
	const char *name;
	uint16_t svid;

	bool (*dfp_inform_id)(struct pd_port *pd_port,
		struct svdm_svid_data *svid_data, bool ack);
	bool (*dfp_inform_svids)(struct pd_port *pd_port,
		struct svdm_svid_data *svid_data, bool ack);
	bool (*dfp_inform_modes)(struct pd_port *pd_port,
		struct svdm_svid_data *svid_data, bool ack);

	bool (*dfp_inform_enter_mode)(struct pd_port *pd_port,
		struct svdm_svid_data *svid_data, uint8_t ops, bool ack);
	bool (*dfp_inform_exit_mode)(struct pd_port *pd_port,
		struct svdm_svid_data *svid_data, uint8_t ops);

	bool (*dfp_inform_attention)(struct pd_port *pd_port,
		struct svdm_svid_data *svid_data);

	void (*ufp_request_enter_mode)(struct pd_port *pd_port,
		struct svdm_svid_data *svid_data, uint8_t ops);
	void (*ufp_request_exit_mode)(struct pd_port *pd_port,
		struct svdm_svid_data *svid_data, uint8_t ops);

	bool (*notify_pe_startup)(struct pd_port *pd_port,
		struct svdm_svid_data *svid_data);
	int (*notify_pe_ready)(struct pd_port *pd_port,
		struct svdm_svid_data *svid_data);
	bool (*notify_pe_shutdown)(struct pd_port *pd_port,
		struct svdm_svid_data *svid_data);

#ifdef CONFIG_USB_PD_CUSTOM_VDM
	bool (*dfp_notify_uvdm)(struct pd_port *pd_port,
		struct svdm_svid_data *svid_data, bool ack);

	bool (*ufp_notify_uvdm)(struct pd_port *pd_port,
		struct svdm_svid_data *svid_data);
#endif

	bool (*reset_state)(struct pd_port *pd_port,
		struct svdm_svid_data *svid_data);

	bool (*parse_svid_data)(struct pd_port *pd_port,
		struct svdm_svid_data *svid_data);
};

static inline bool dpm_check_data_msg_event(
	struct pd_port *pd_port, uint8_t msg)
{
	return pd_event_data_msg_match(
		pd_get_curr_pd_event(pd_port), msg);
}

#ifdef CONFIG_USB_PD_REV30
static inline bool dpm_check_ext_msg_event(
	struct pd_port *pd_port, uint8_t msg)
{
	return pd_event_ext_msg_match(
		pd_get_curr_pd_event(pd_port), msg);
}
#endif	/* CONFIG_USB_PD_REV30 */

static inline uint8_t dpm_vdm_get_ops(struct pd_port *pd_port)
{
	return pd_port->curr_vdm_ops;
}

static inline uint16_t dpm_vdm_get_svid(struct pd_port *pd_port)
{
	return pd_port->curr_vdm_svid;
}

static inline int dpm_vdm_reply_svdm_request(
		struct pd_port *pd_port, bool ack)
{
	return pd_reply_svdm_request_simply(
		pd_port, ack ? CMDT_RSP_ACK : CMDT_RSP_NAK);
}

static inline int dpm_vdm_reply_svdm_nak(struct pd_port *pd_port)
{
	return pd_reply_svdm_request_simply(pd_port, CMDT_RSP_NAK);
}

enum {
	GOOD_PW_NONE = 0,	/* both no GP */
	GOOD_PW_PARTNER,	/* partner has GP */
	GOOD_PW_LOCAL,		/* local has GP */
	GOOD_PW_BOTH,		/* both have GPs */
};

int dpm_check_good_power(struct pd_port *pd_port);

/* SVDM */

extern struct svdm_svid_data *
	dpm_get_svdm_svid_data(struct pd_port *pd_port, uint16_t svid);

extern bool svdm_reset_state(struct pd_port *pd_port);
extern bool svdm_notify_pe_startup(struct pd_port *pd_port);

static inline int svdm_notify_pe_ready(struct pd_port *pd_port)
{
	int i, ret;
	struct svdm_svid_data *svid_data;

	for (i = 0; i < pd_port->svid_data_cnt; i++) {
		svid_data = &pd_port->svid_data[i];
		if (svid_data->ops && svid_data->ops->notify_pe_ready) {
			ret = svid_data->ops->notify_pe_ready(
						pd_port, svid_data);
			if (ret != 0)
				return ret;
		}
	}

	return 0;
}

static inline bool svdm_notify_pe_shutdown(
	struct pd_port *pd_port)
{
	int i;
	struct svdm_svid_data *svid_data;

	for (i = 0; i < pd_port->svid_data_cnt; i++) {
		svid_data = &pd_port->svid_data[i];
		if (svid_data->ops && svid_data->ops->notify_pe_shutdown) {
			svid_data->ops->notify_pe_shutdown(
				pd_port, svid_data);
		}
	}

	return 0;
}

static inline bool svdm_dfp_inform_id(struct pd_port *pd_port, bool ack)
{
	int i;
	struct svdm_svid_data *svid_data;

	for (i = 0; i < pd_port->svid_data_cnt; i++) {
		svid_data = &pd_port->svid_data[i];
		if (svid_data->ops && svid_data->ops->dfp_inform_id)
			svid_data->ops->dfp_inform_id(pd_port, svid_data, ack);
	}

	return true;
}

static inline bool svdm_dfp_inform_svids(struct pd_port *pd_port, bool ack)
{
	int i;
	struct svdm_svid_data *svid_data;

	for (i = 0; i < pd_port->svid_data_cnt; i++) {
		svid_data = &pd_port->svid_data[i];
		if (svid_data->ops && svid_data->ops->dfp_inform_svids)
			svid_data->ops->dfp_inform_svids(
						pd_port, svid_data, ack);
	}

	return true;
}

static inline bool svdm_dfp_inform_modes(
		struct pd_port *pd_port, uint16_t svid, bool ack)
{
	struct svdm_svid_data *svid_data;

	svid_data = dpm_get_svdm_svid_data(pd_port, svid);
	if (svid_data == NULL)
		return false;

	if (svid_data->ops && svid_data->ops->dfp_inform_modes)
		svid_data->ops->dfp_inform_modes(pd_port, svid_data, ack);

	return true;
}

static inline bool svdm_dfp_inform_enter_mode(
	struct pd_port *pd_port, uint16_t svid, uint8_t ops, bool ack)
{
	struct svdm_svid_data *svid_data;

	svid_data = dpm_get_svdm_svid_data(pd_port, svid);
	if (svid_data == NULL)
		return false;

	if (svid_data->ops && svid_data->ops->dfp_inform_enter_mode)
		svid_data->ops->dfp_inform_enter_mode(
						pd_port, svid_data, ops, ack);

	return true;
}

static inline bool svdm_dfp_inform_exit_mode(
	struct pd_port *pd_port, uint16_t svid, uint8_t ops)
{
	struct svdm_svid_data *svid_data;

	svid_data = dpm_get_svdm_svid_data(pd_port, svid);
	if (svid_data == NULL)
		return false;

	if (svid_data->ops && svid_data->ops->dfp_inform_exit_mode)
		svid_data->ops->dfp_inform_exit_mode(pd_port, svid_data, ops);

	return true;
}

static inline bool svdm_dfp_inform_attention(
	struct pd_port *pd_port, uint16_t svid)
{
	struct svdm_svid_data *svid_data;

	svid_data = dpm_get_svdm_svid_data(pd_port, svid);
	if (svid_data == NULL)
		return false;

	if (svid_data->ops && svid_data->ops->dfp_inform_attention)
		svid_data->ops->dfp_inform_attention(pd_port, svid_data);

	return true;
}

static inline bool svdm_ufp_request_enter_mode(
	struct pd_port *pd_port, uint16_t svid, uint8_t ops)
{
	struct svdm_svid_data *svid_data;

	svid_data = dpm_get_svdm_svid_data(pd_port, svid);
	if (svid_data == NULL)
		return false;

	if (svid_data->ops && svid_data->ops->ufp_request_enter_mode)
		svid_data->ops->ufp_request_enter_mode(pd_port, svid_data, ops);

	return true;
}

static inline bool svdm_ufp_request_exit_mode(
	struct pd_port *pd_port, uint16_t svid, uint8_t ops)
{
	struct svdm_svid_data *svid_data;

	svid_data = dpm_get_svdm_svid_data(pd_port, svid);
	if (svid_data == NULL)
		return false;

	if (svid_data->ops && svid_data->ops->ufp_request_exit_mode)
		svid_data->ops->ufp_request_exit_mode(pd_port, svid_data, ops);

	return true;
}

#endif /* PD_DPM_PRV_H_INCLUDED */
