/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/ipa.h>
#include <linux/kernel.h>
#include <linux/msm_ipa.h>
#include "ipahal_i.h"
#include "ipahal_reg.h"
#include "ipahal_reg_i.h"

static const char *ipareg_name_to_str[IPA_REG_MAX] = {
	__stringify(IPA_ROUTE),
	__stringify(IPA_IRQ_STTS_EE_n),
	__stringify(IPA_IRQ_EN_EE_n),
	__stringify(IPA_IRQ_CLR_EE_n),
	__stringify(IPA_IRQ_SUSPEND_INFO_EE_n),
	__stringify(IPA_SUSPEND_IRQ_EN_EE_n),
	__stringify(IPA_SUSPEND_IRQ_CLR_EE_n),
	__stringify(IPA_BCR),
	__stringify(IPA_ENABLED_PIPES),
	__stringify(IPA_COMP_SW_RESET),
	__stringify(IPA_VERSION),
	__stringify(IPA_TAG_TIMER),
	__stringify(IPA_COMP_HW_VERSION),
	__stringify(IPA_SPARE_REG_1),
	__stringify(IPA_SPARE_REG_2),
	__stringify(IPA_COMP_CFG),
	__stringify(IPA_STATE_AGGR_ACTIVE),
	__stringify(IPA_ENDP_INIT_HDR_n),
	__stringify(IPA_ENDP_INIT_HDR_EXT_n),
	__stringify(IPA_ENDP_INIT_AGGR_n),
	__stringify(IPA_AGGR_FORCE_CLOSE),
	__stringify(IPA_ENDP_INIT_ROUTE_n),
	__stringify(IPA_ENDP_INIT_MODE_n),
	__stringify(IPA_ENDP_INIT_NAT_n),
	__stringify(IPA_ENDP_INIT_CONN_TRACK_n),
	__stringify(IPA_ENDP_INIT_CTRL_n),
	__stringify(IPA_ENDP_INIT_CTRL_SCND_n),
	__stringify(IPA_ENDP_INIT_HOL_BLOCK_EN_n),
	__stringify(IPA_ENDP_INIT_HOL_BLOCK_TIMER_n),
	__stringify(IPA_ENDP_INIT_DEAGGR_n),
	__stringify(IPA_ENDP_INIT_SEQ_n),
	__stringify(IPA_DEBUG_CNT_REG_n),
	__stringify(IPA_ENDP_INIT_CFG_n),
	__stringify(IPA_IRQ_EE_UC_n),
	__stringify(IPA_ENDP_INIT_HDR_METADATA_MASK_n),
	__stringify(IPA_ENDP_INIT_HDR_METADATA_n),
	__stringify(IPA_ENDP_INIT_RSRC_GRP_n),
	__stringify(IPA_SHARED_MEM_SIZE),
	__stringify(IPA_SRAM_DIRECT_ACCESS_n),
	__stringify(IPA_DEBUG_CNT_CTRL_n),
	__stringify(IPA_UC_MAILBOX_m_n),
	__stringify(IPA_FILT_ROUT_HASH_FLUSH),
	__stringify(IPA_SINGLE_NDP_MODE),
	__stringify(IPA_QCNCM),
	__stringify(IPA_SYS_PKT_PROC_CNTXT_BASE),
	__stringify(IPA_LOCAL_PKT_PROC_CNTXT_BASE),
	__stringify(IPA_ENDP_STATUS_n),
	__stringify(IPA_ENDP_FILTER_ROUTER_HSH_CFG_n),
	__stringify(IPA_SRC_RSRC_GRP_01_RSRC_TYPE_n),
	__stringify(IPA_SRC_RSRC_GRP_23_RSRC_TYPE_n),
	__stringify(IPA_SRC_RSRC_GRP_45_RSRC_TYPE_n),
	__stringify(IPA_SRC_RSRC_GRP_67_RSRC_TYPE_n),
	__stringify(IPA_DST_RSRC_GRP_01_RSRC_TYPE_n),
	__stringify(IPA_DST_RSRC_GRP_23_RSRC_TYPE_n),
	__stringify(IPA_DST_RSRC_GRP_45_RSRC_TYPE_n),
	__stringify(IPA_DST_RSRC_GRP_67_RSRC_TYPE_n),
	__stringify(IPA_RX_HPS_CLIENTS_MIN_DEPTH_0),
	__stringify(IPA_RX_HPS_CLIENTS_MIN_DEPTH_1),
	__stringify(IPA_RX_HPS_CLIENTS_MAX_DEPTH_0),
	__stringify(IPA_RX_HPS_CLIENTS_MAX_DEPTH_1),
	__stringify(IPA_HPS_FTCH_ARB_QUEUE_WEIGHT),
	__stringify(IPA_QSB_MAX_WRITES),
	__stringify(IPA_QSB_MAX_READS),
	__stringify(IPA_TX_CFG),
	__stringify(IPA_IDLE_INDICATION_CFG),
	__stringify(IPA_DPS_SEQUENCER_FIRST),
	__stringify(IPA_HPS_SEQUENCER_FIRST),
	__stringify(IPA_CLKON_CFG),
	__stringify(IPA_STAT_QUOTA_BASE_n),
	__stringify(IPA_STAT_QUOTA_MASK_n),
	__stringify(IPA_STAT_TETHERING_BASE_n),
	__stringify(IPA_STAT_TETHERING_MASK_n),
	__stringify(IPA_STAT_FILTER_IPV4_BASE),
	__stringify(IPA_STAT_FILTER_IPV6_BASE),
	__stringify(IPA_STAT_ROUTER_IPV4_BASE),
	__stringify(IPA_STAT_ROUTER_IPV6_BASE),
	__stringify(IPA_STAT_FILTER_IPV4_START_ID),
	__stringify(IPA_STAT_FILTER_IPV6_START_ID),
	__stringify(IPA_STAT_ROUTER_IPV4_START_ID),
	__stringify(IPA_STAT_ROUTER_IPV6_START_ID),
	__stringify(IPA_STAT_FILTER_IPV4_END_ID),
	__stringify(IPA_STAT_FILTER_IPV6_END_ID),
	__stringify(IPA_STAT_ROUTER_IPV4_END_ID),
	__stringify(IPA_STAT_ROUTER_IPV6_END_ID),
	__stringify(IPA_STAT_DROP_CNT_BASE_n),
	__stringify(IPA_STAT_DROP_CNT_MASK_n),
};

static void ipareg_construct_dummy(enum ipahal_reg_name reg,
	const void *fields, u32 *val)
{
	IPAHAL_ERR("No construct function for %s\n",
		ipahal_reg_name_str(reg));
	WARN_ON(1);
}

static void ipareg_parse_dummy(enum ipahal_reg_name reg,
	void *fields, u32 val)
{
	IPAHAL_ERR("No parse function for %s\n",
		ipahal_reg_name_str(reg));
	WARN_ON(1);
}

static void ipareg_construct_rx_hps_clients_depth1(
	enum ipahal_reg_name reg, const void *fields, u32 *val)
{
	struct ipahal_reg_rx_hps_clients *clients =
		(struct ipahal_reg_rx_hps_clients *)fields;

	IPA_SETFIELD_IN_REG(*val, clients->client_minmax[0],
		IPA_RX_HPS_CLIENTS_MINMAX_DEPTH_X_CLIENT_n_SHFT(0),
		IPA_RX_HPS_CLIENTS_MINMAX_DEPTH_X_CLIENT_n_BMSK(0));

	IPA_SETFIELD_IN_REG(*val, clients->client_minmax[1],
		IPA_RX_HPS_CLIENTS_MINMAX_DEPTH_X_CLIENT_n_SHFT(1),
		IPA_RX_HPS_CLIENTS_MINMAX_DEPTH_X_CLIENT_n_BMSK(1));
}

static void ipareg_construct_rx_hps_clients_depth0(
	enum ipahal_reg_name reg, const void *fields, u32 *val)
{
	struct ipahal_reg_rx_hps_clients *clients =
		(struct ipahal_reg_rx_hps_clients *)fields;

	IPA_SETFIELD_IN_REG(*val, clients->client_minmax[0],
		IPA_RX_HPS_CLIENTS_MINMAX_DEPTH_X_CLIENT_n_SHFT(0),
		IPA_RX_HPS_CLIENTS_MINMAX_DEPTH_X_CLIENT_n_BMSK(0));

	IPA_SETFIELD_IN_REG(*val, clients->client_minmax[1],
		IPA_RX_HPS_CLIENTS_MINMAX_DEPTH_X_CLIENT_n_SHFT(1),
		IPA_RX_HPS_CLIENTS_MINMAX_DEPTH_X_CLIENT_n_BMSK(1));

	IPA_SETFIELD_IN_REG(*val, clients->client_minmax[2],
		IPA_RX_HPS_CLIENTS_MINMAX_DEPTH_X_CLIENT_n_SHFT(2),
		IPA_RX_HPS_CLIENTS_MINMAX_DEPTH_X_CLIENT_n_BMSK(2));

	IPA_SETFIELD_IN_REG(*val, clients->client_minmax[3],
		IPA_RX_HPS_CLIENTS_MINMAX_DEPTH_X_CLIENT_n_SHFT(3),
		IPA_RX_HPS_CLIENTS_MINMAX_DEPTH_X_CLIENT_n_BMSK(3));
}

static void ipareg_construct_rx_hps_clients_depth0_v3_5(
	enum ipahal_reg_name reg, const void *fields, u32 *val)
{
	struct ipahal_reg_rx_hps_clients *clients =
		(struct ipahal_reg_rx_hps_clients *)fields;

	IPA_SETFIELD_IN_REG(*val, clients->client_minmax[0],
		IPA_RX_HPS_CLIENTS_MINMAX_DEPTH_X_CLIENT_n_SHFT(0),
		IPA_RX_HPS_CLIENTS_MINMAX_DEPTH_X_CLIENT_n_BMSK_V3_5(0));

	IPA_SETFIELD_IN_REG(*val, clients->client_minmax[1],
		IPA_RX_HPS_CLIENTS_MINMAX_DEPTH_X_CLIENT_n_SHFT(1),
		IPA_RX_HPS_CLIENTS_MINMAX_DEPTH_X_CLIENT_n_BMSK_V3_5(1));

	IPA_SETFIELD_IN_REG(*val, clients->client_minmax[2],
		IPA_RX_HPS_CLIENTS_MINMAX_DEPTH_X_CLIENT_n_SHFT(2),
		IPA_RX_HPS_CLIENTS_MINMAX_DEPTH_X_CLIENT_n_BMSK_V3_5(2));

	IPA_SETFIELD_IN_REG(*val, clients->client_minmax[3],
		IPA_RX_HPS_CLIENTS_MINMAX_DEPTH_X_CLIENT_n_SHFT(3),
		IPA_RX_HPS_CLIENTS_MINMAX_DEPTH_X_CLIENT_n_BMSK_V3_5(3));
}

static void ipareg_construct_rsrg_grp_xy(
	enum ipahal_reg_name reg, const void *fields, u32 *val)
{
	struct ipahal_reg_rsrc_grp_cfg *grp =
		(struct ipahal_reg_rsrc_grp_cfg *)fields;

	IPA_SETFIELD_IN_REG(*val, grp->x_min,
		IPA_RSRC_GRP_XY_RSRC_TYPE_n_X_MIN_LIM_SHFT,
		IPA_RSRC_GRP_XY_RSRC_TYPE_n_X_MIN_LIM_BMSK);
	IPA_SETFIELD_IN_REG(*val, grp->x_max,
		IPA_RSRC_GRP_XY_RSRC_TYPE_n_X_MAX_LIM_SHFT,
		IPA_RSRC_GRP_XY_RSRC_TYPE_n_X_MAX_LIM_BMSK);
	IPA_SETFIELD_IN_REG(*val, grp->y_min,
		IPA_RSRC_GRP_XY_RSRC_TYPE_n_Y_MIN_LIM_SHFT,
		IPA_RSRC_GRP_XY_RSRC_TYPE_n_Y_MIN_LIM_BMSK);
	IPA_SETFIELD_IN_REG(*val, grp->y_max,
		IPA_RSRC_GRP_XY_RSRC_TYPE_n_Y_MAX_LIM_SHFT,
		IPA_RSRC_GRP_XY_RSRC_TYPE_n_Y_MAX_LIM_BMSK);
}

static void ipareg_construct_rsrg_grp_xy_v3_5(
	enum ipahal_reg_name reg, const void *fields, u32 *val)
{
	struct ipahal_reg_rsrc_grp_cfg *grp =
		(struct ipahal_reg_rsrc_grp_cfg *)fields;

	IPA_SETFIELD_IN_REG(*val, grp->x_min,
		IPA_RSRC_GRP_XY_RSRC_TYPE_n_X_MIN_LIM_SHFT_V3_5,
		IPA_RSRC_GRP_XY_RSRC_TYPE_n_X_MIN_LIM_BMSK_V3_5);
	IPA_SETFIELD_IN_REG(*val, grp->x_max,
		IPA_RSRC_GRP_XY_RSRC_TYPE_n_X_MAX_LIM_SHFT_V3_5,
		IPA_RSRC_GRP_XY_RSRC_TYPE_n_X_MAX_LIM_BMSK_V3_5);

	/* DST_23 register has only X fields at ipa V3_5 */
	if (reg == IPA_DST_RSRC_GRP_23_RSRC_TYPE_n)
		return;

	IPA_SETFIELD_IN_REG(*val, grp->y_min,
		IPA_RSRC_GRP_XY_RSRC_TYPE_n_Y_MIN_LIM_SHFT_V3_5,
		IPA_RSRC_GRP_XY_RSRC_TYPE_n_Y_MIN_LIM_BMSK_V3_5);
	IPA_SETFIELD_IN_REG(*val, grp->y_max,
		IPA_RSRC_GRP_XY_RSRC_TYPE_n_Y_MAX_LIM_SHFT_V3_5,
		IPA_RSRC_GRP_XY_RSRC_TYPE_n_Y_MAX_LIM_BMSK_V3_5);
}

static void ipareg_construct_hash_cfg_n(
	enum ipahal_reg_name reg, const void *fields, u32 *val)
{
	struct ipahal_reg_fltrt_hash_tuple *tuple =
		(struct ipahal_reg_fltrt_hash_tuple *)fields;

	IPA_SETFIELD_IN_REG(*val, tuple->flt.src_id,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_SRC_ID_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_SRC_ID_BMSK);
	IPA_SETFIELD_IN_REG(*val, tuple->flt.src_ip_addr,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_SRC_IP_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_SRC_IP_BMSK);
	IPA_SETFIELD_IN_REG(*val, tuple->flt.dst_ip_addr,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_DST_IP_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_DST_IP_BMSK);
	IPA_SETFIELD_IN_REG(*val, tuple->flt.src_port,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_SRC_PORT_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_SRC_PORT_BMSK);
	IPA_SETFIELD_IN_REG(*val, tuple->flt.dst_port,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_DST_PORT_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_DST_PORT_BMSK);
	IPA_SETFIELD_IN_REG(*val, tuple->flt.protocol,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_PROTOCOL_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_PROTOCOL_BMSK);
	IPA_SETFIELD_IN_REG(*val, tuple->flt.meta_data,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_METADATA_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_METADATA_BMSK);
	IPA_SETFIELD_IN_REG(*val, tuple->undefined1,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_UNDEFINED1_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_UNDEFINED1_BMSK);
	IPA_SETFIELD_IN_REG(*val, tuple->rt.src_id,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_SRC_ID_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_SRC_ID_BMSK);
	IPA_SETFIELD_IN_REG(*val, tuple->rt.src_ip_addr,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_SRC_IP_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_SRC_IP_BMSK);
	IPA_SETFIELD_IN_REG(*val, tuple->rt.dst_ip_addr,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_DST_IP_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_DST_IP_BMSK);
	IPA_SETFIELD_IN_REG(*val, tuple->rt.src_port,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_SRC_PORT_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_SRC_PORT_BMSK);
	IPA_SETFIELD_IN_REG(*val, tuple->rt.dst_port,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_DST_PORT_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_DST_PORT_BMSK);
	IPA_SETFIELD_IN_REG(*val, tuple->rt.protocol,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_PROTOCOL_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_PROTOCOL_BMSK);
	IPA_SETFIELD_IN_REG(*val, tuple->rt.meta_data,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_METADATA_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_METADATA_BMSK);
	IPA_SETFIELD_IN_REG(*val, tuple->undefined2,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_UNDEFINED2_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_UNDEFINED2_BMSK);
}

static void ipareg_parse_hash_cfg_n(
	enum ipahal_reg_name reg, void *fields, u32 val)
{
	struct ipahal_reg_fltrt_hash_tuple *tuple =
		(struct ipahal_reg_fltrt_hash_tuple *)fields;

	tuple->flt.src_id =
		IPA_GETFIELD_FROM_REG(val,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_SRC_ID_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_SRC_ID_BMSK);
	tuple->flt.src_ip_addr =
		IPA_GETFIELD_FROM_REG(val,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_SRC_IP_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_SRC_IP_BMSK);
	tuple->flt.dst_ip_addr =
		IPA_GETFIELD_FROM_REG(val,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_DST_IP_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_DST_IP_BMSK);
	tuple->flt.src_port =
		IPA_GETFIELD_FROM_REG(val,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_SRC_PORT_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_SRC_PORT_BMSK);
	tuple->flt.dst_port =
		IPA_GETFIELD_FROM_REG(val,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_DST_PORT_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_DST_PORT_BMSK);
	tuple->flt.protocol =
		IPA_GETFIELD_FROM_REG(val,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_PROTOCOL_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_PROTOCOL_BMSK);
	tuple->flt.meta_data =
		IPA_GETFIELD_FROM_REG(val,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_METADATA_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_FILTER_HASH_MSK_METADATA_BMSK);
	tuple->undefined1 =
		IPA_GETFIELD_FROM_REG(val,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_UNDEFINED1_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_UNDEFINED1_BMSK);
	tuple->rt.src_id =
		IPA_GETFIELD_FROM_REG(val,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_SRC_ID_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_SRC_ID_BMSK);
	tuple->rt.src_ip_addr =
		IPA_GETFIELD_FROM_REG(val,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_SRC_IP_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_SRC_IP_BMSK);
	tuple->rt.dst_ip_addr =
		IPA_GETFIELD_FROM_REG(val,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_DST_IP_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_DST_IP_BMSK);
	tuple->rt.src_port =
		IPA_GETFIELD_FROM_REG(val,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_SRC_PORT_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_SRC_PORT_BMSK);
	tuple->rt.dst_port =
		IPA_GETFIELD_FROM_REG(val,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_DST_PORT_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_DST_PORT_BMSK);
	tuple->rt.protocol =
		IPA_GETFIELD_FROM_REG(val,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_PROTOCOL_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_PROTOCOL_BMSK);
	tuple->rt.meta_data =
		IPA_GETFIELD_FROM_REG(val,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_METADATA_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_ROUTER_HASH_MSK_METADATA_BMSK);
	tuple->undefined2 =
		IPA_GETFIELD_FROM_REG(val,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_UNDEFINED2_SHFT,
		IPA_ENDP_FILTER_ROUTER_HSH_CFG_n_UNDEFINED2_BMSK);
}

static void ipareg_construct_endp_status_n(
	enum ipahal_reg_name reg, const void *fields, u32 *val)
{
	struct ipahal_reg_ep_cfg_status *ep_status =
		(struct ipahal_reg_ep_cfg_status *)fields;

	IPA_SETFIELD_IN_REG(*val, ep_status->status_en,
			IPA_ENDP_STATUS_n_STATUS_EN_SHFT,
			IPA_ENDP_STATUS_n_STATUS_EN_BMSK);

	IPA_SETFIELD_IN_REG(*val, ep_status->status_ep,
			IPA_ENDP_STATUS_n_STATUS_ENDP_SHFT,
			IPA_ENDP_STATUS_n_STATUS_ENDP_BMSK);

	IPA_SETFIELD_IN_REG(*val, ep_status->status_location,
			IPA_ENDP_STATUS_n_STATUS_LOCATION_SHFT,
			IPA_ENDP_STATUS_n_STATUS_LOCATION_BMSK);
}

static void ipareg_construct_endp_status_n_v4_0(
	enum ipahal_reg_name reg, const void *fields, u32 *val)
{
	struct ipahal_reg_ep_cfg_status *ep_status =
		(struct ipahal_reg_ep_cfg_status *)fields;

	IPA_SETFIELD_IN_REG(*val, ep_status->status_en,
			IPA_ENDP_STATUS_n_STATUS_EN_SHFT,
			IPA_ENDP_STATUS_n_STATUS_EN_BMSK);

	IPA_SETFIELD_IN_REG(*val, ep_status->status_ep,
			IPA_ENDP_STATUS_n_STATUS_ENDP_SHFT,
			IPA_ENDP_STATUS_n_STATUS_ENDP_BMSK);

	IPA_SETFIELD_IN_REG(*val, ep_status->status_location,
			IPA_ENDP_STATUS_n_STATUS_LOCATION_SHFT,
			IPA_ENDP_STATUS_n_STATUS_LOCATION_BMSK);

	IPA_SETFIELD_IN_REG(*val, ep_status->status_pkt_suppress,
			IPA_ENDP_STATUS_n_STATUS_PKT_SUPPRESS_SHFT,
			IPA_ENDP_STATUS_n_STATUS_PKT_SUPPRESS_BMSK);
}

static void ipareg_construct_qcncm(
	enum ipahal_reg_name reg, const void *fields, u32 *val)
{
	struct ipahal_reg_qcncm *qcncm =
		(struct ipahal_reg_qcncm *)fields;

	IPA_SETFIELD_IN_REG(*val, qcncm->mode_en ? 1 : 0,
		IPA_QCNCM_MODE_EN_SHFT,
		IPA_QCNCM_MODE_EN_BMSK);
	IPA_SETFIELD_IN_REG(*val, qcncm->mode_val,
		IPA_QCNCM_MODE_VAL_SHFT,
		IPA_QCNCM_MODE_VAL_BMSK);
	IPA_SETFIELD_IN_REG(*val, qcncm->undefined,
		0, IPA_QCNCM_MODE_VAL_BMSK);
}

static void ipareg_parse_qcncm(
	enum ipahal_reg_name reg, void *fields, u32 val)
{
	struct ipahal_reg_qcncm *qcncm =
		(struct ipahal_reg_qcncm *)fields;

	memset(qcncm, 0, sizeof(struct ipahal_reg_qcncm));
	qcncm->mode_en = IPA_GETFIELD_FROM_REG(val,
		IPA_QCNCM_MODE_EN_SHFT,
		IPA_QCNCM_MODE_EN_BMSK);
	qcncm->mode_val = IPA_GETFIELD_FROM_REG(val,
		IPA_QCNCM_MODE_VAL_SHFT,
		IPA_QCNCM_MODE_VAL_BMSK);
	qcncm->undefined = IPA_GETFIELD_FROM_REG(val,
		0, IPA_QCNCM_UNDEFINED1_BMSK);
	qcncm->undefined |= IPA_GETFIELD_FROM_REG(val,
		0, IPA_QCNCM_MODE_UNDEFINED2_BMSK);
}

static void ipareg_construct_single_ndp_mode(
	enum ipahal_reg_name reg, const void *fields, u32 *val)
{
	struct ipahal_reg_single_ndp_mode *mode =
		(struct ipahal_reg_single_ndp_mode *)fields;

	IPA_SETFIELD_IN_REG(*val, mode->single_ndp_en ? 1 : 0,
		IPA_SINGLE_NDP_MODE_SINGLE_NDP_EN_SHFT,
		IPA_SINGLE_NDP_MODE_SINGLE_NDP_EN_BMSK);

	IPA_SETFIELD_IN_REG(*val, mode->undefined,
		IPA_SINGLE_NDP_MODE_UNDEFINED_SHFT,
		IPA_SINGLE_NDP_MODE_UNDEFINED_BMSK);
}

static void ipareg_parse_single_ndp_mode(
	enum ipahal_reg_name reg, void *fields, u32 val)
{
	struct ipahal_reg_single_ndp_mode *mode =
		(struct ipahal_reg_single_ndp_mode *)fields;

	memset(mode, 0, sizeof(struct ipahal_reg_single_ndp_mode));
	mode->single_ndp_en = IPA_GETFIELD_FROM_REG(val,
		IPA_SINGLE_NDP_MODE_SINGLE_NDP_EN_SHFT,
		IPA_SINGLE_NDP_MODE_SINGLE_NDP_EN_BMSK);
	mode->undefined = IPA_GETFIELD_FROM_REG(val,
		IPA_SINGLE_NDP_MODE_UNDEFINED_SHFT,
		IPA_SINGLE_NDP_MODE_UNDEFINED_BMSK);
}

static void ipareg_construct_debug_cnt_ctrl_n(
	enum ipahal_reg_name reg, const void *fields, u32 *val)
{
	struct ipahal_reg_debug_cnt_ctrl *dbg_cnt_ctrl =
		(struct ipahal_reg_debug_cnt_ctrl *)fields;
	u8 type;

	IPA_SETFIELD_IN_REG(*val, dbg_cnt_ctrl->en ? 1 : 0,
		IPA_DEBUG_CNT_CTRL_n_DBG_CNT_EN_SHFT,
		IPA_DEBUG_CNT_CTRL_n_DBG_CNT_EN_BMSK);

	switch (dbg_cnt_ctrl->type) {
	case DBG_CNT_TYPE_IPV4_FLTR:
		type = 0x0;
		if (!dbg_cnt_ctrl->rule_idx_pipe_rule) {
			IPAHAL_ERR("No FLT global rules\n");
			WARN_ON(1);
		}
		break;
	case DBG_CNT_TYPE_IPV4_ROUT:
		type = 0x1;
		break;
	case DBG_CNT_TYPE_GENERAL:
		type = 0x2;
		break;
	case DBG_CNT_TYPE_IPV6_FLTR:
		type = 0x4;
		if (!dbg_cnt_ctrl->rule_idx_pipe_rule) {
			IPAHAL_ERR("No FLT global rules\n");
			WARN_ON(1);
		}
		break;
	case DBG_CNT_TYPE_IPV6_ROUT:
		type = 0x5;
		break;
	default:
		IPAHAL_ERR("Invalid dbg_cnt_ctrl type (%d) for %s\n",
			dbg_cnt_ctrl->type, ipahal_reg_name_str(reg));
		WARN_ON(1);
		return;

	};

	IPA_SETFIELD_IN_REG(*val, type,
		IPA_DEBUG_CNT_CTRL_n_DBG_CNT_TYPE_SHFT,
		IPA_DEBUG_CNT_CTRL_n_DBG_CNT_TYPE_BMSK);

	IPA_SETFIELD_IN_REG(*val, dbg_cnt_ctrl->product ? 1 : 0,
		IPA_DEBUG_CNT_CTRL_n_DBG_CNT_PRODUCT_SHFT,
		IPA_DEBUG_CNT_CTRL_n_DBG_CNT_PRODUCT_BMSK);

	IPA_SETFIELD_IN_REG(*val, dbg_cnt_ctrl->src_pipe,
		IPA_DEBUG_CNT_CTRL_n_DBG_CNT_SOURCE_PIPE_SHFT,
		IPA_DEBUG_CNT_CTRL_n_DBG_CNT_SOURCE_PIPE_BMSK);

	if (ipahal_ctx->hw_type <= IPA_HW_v3_1) {
		IPA_SETFIELD_IN_REG(*val, dbg_cnt_ctrl->rule_idx,
			IPA_DEBUG_CNT_CTRL_n_DBG_CNT_RULE_INDEX_SHFT,
			IPA_DEBUG_CNT_CTRL_n_DBG_CNT_RULE_INDEX_BMSK);
		IPA_SETFIELD_IN_REG(*val, dbg_cnt_ctrl->rule_idx_pipe_rule,
			IPA_DEBUG_CNT_CTRL_n_DBG_CNT_RULE_INDEX_PIPE_RULE_SHFT,
			IPA_DEBUG_CNT_CTRL_n_DBG_CNT_RULE_INDEX_PIPE_RULE_BMSK
			);
	} else {
		IPA_SETFIELD_IN_REG(*val, dbg_cnt_ctrl->rule_idx,
			IPA_DEBUG_CNT_CTRL_n_DBG_CNT_RULE_INDEX_SHFT,
			IPA_DEBUG_CNT_CTRL_n_DBG_CNT_RULE_INDEX_BMSK_V3_5);
	}
}

static void ipareg_parse_shared_mem_size(
	enum ipahal_reg_name reg, void *fields, u32 val)
{
	struct ipahal_reg_shared_mem_size *smem_sz =
		(struct ipahal_reg_shared_mem_size *)fields;

	memset(smem_sz, 0, sizeof(struct ipahal_reg_shared_mem_size));
	smem_sz->shared_mem_sz = IPA_GETFIELD_FROM_REG(val,
		IPA_SHARED_MEM_SIZE_SHARED_MEM_SIZE_SHFT,
		IPA_SHARED_MEM_SIZE_SHARED_MEM_SIZE_BMSK);

	smem_sz->shared_mem_baddr = IPA_GETFIELD_FROM_REG(val,
		IPA_SHARED_MEM_SIZE_SHARED_MEM_BADDR_SHFT,
		IPA_SHARED_MEM_SIZE_SHARED_MEM_BADDR_BMSK);
}

static void ipareg_construct_endp_init_rsrc_grp_n(
		enum ipahal_reg_name reg, const void *fields, u32 *val)
{
	struct ipahal_reg_endp_init_rsrc_grp *rsrc_grp =
		(struct ipahal_reg_endp_init_rsrc_grp *)fields;

	IPA_SETFIELD_IN_REG(*val, rsrc_grp->rsrc_grp,
		IPA_ENDP_INIT_RSRC_GRP_n_RSRC_GRP_SHFT,
		IPA_ENDP_INIT_RSRC_GRP_n_RSRC_GRP_BMSK);
}

static void ipareg_construct_endp_init_rsrc_grp_n_v3_5(
		enum ipahal_reg_name reg, const void *fields, u32 *val)
{
	struct ipahal_reg_endp_init_rsrc_grp *rsrc_grp =
		(struct ipahal_reg_endp_init_rsrc_grp *)fields;

	IPA_SETFIELD_IN_REG(*val, rsrc_grp->rsrc_grp,
		IPA_ENDP_INIT_RSRC_GRP_n_RSRC_GRP_SHFT_v3_5,
		IPA_ENDP_INIT_RSRC_GRP_n_RSRC_GRP_BMSK_v3_5);
}

static void ipareg_construct_endp_init_hdr_metadata_n(
		enum ipahal_reg_name reg, const void *fields, u32 *val)
{
	struct ipa_ep_cfg_metadata *metadata =
		(struct ipa_ep_cfg_metadata *)fields;

	IPA_SETFIELD_IN_REG(*val, metadata->qmap_id,
			IPA_ENDP_INIT_HDR_METADATA_n_METADATA_SHFT,
			IPA_ENDP_INIT_HDR_METADATA_n_METADATA_BMSK);
}

static void ipareg_construct_endp_init_hdr_metadata_mask_n(
		enum ipahal_reg_name reg, const void *fields, u32 *val)
{
	struct ipa_ep_cfg_metadata_mask *metadata_mask =
		(struct ipa_ep_cfg_metadata_mask *)fields;

	IPA_SETFIELD_IN_REG(*val, metadata_mask->metadata_mask,
			IPA_ENDP_INIT_HDR_METADATA_MASK_n_METADATA_MASK_SHFT,
			IPA_ENDP_INIT_HDR_METADATA_MASK_n_METADATA_MASK_BMSK);
}

static void ipareg_construct_endp_init_cfg_n(
	enum ipahal_reg_name reg, const void *fields, u32 *val)
{
	struct ipa_ep_cfg_cfg *cfg =
		(struct ipa_ep_cfg_cfg *)fields;
	u32 cs_offload_en;

	switch (cfg->cs_offload_en) {
	case IPA_DISABLE_CS_OFFLOAD:
		cs_offload_en = 0;
		break;
	case IPA_ENABLE_CS_OFFLOAD_UL:
		cs_offload_en = 1;
		break;
	case IPA_ENABLE_CS_OFFLOAD_DL:
		cs_offload_en = 2;
		break;
	default:
		IPAHAL_ERR("Invalid cs_offload_en value for %s\n",
			ipahal_reg_name_str(reg));
		WARN_ON(1);
		return;
	}

	IPA_SETFIELD_IN_REG(*val, cfg->frag_offload_en ? 1 : 0,
			IPA_ENDP_INIT_CFG_n_FRAG_OFFLOAD_EN_SHFT,
			IPA_ENDP_INIT_CFG_n_FRAG_OFFLOAD_EN_BMSK);
	IPA_SETFIELD_IN_REG(*val, cs_offload_en,
			IPA_ENDP_INIT_CFG_n_CS_OFFLOAD_EN_SHFT,
			IPA_ENDP_INIT_CFG_n_CS_OFFLOAD_EN_BMSK);
	IPA_SETFIELD_IN_REG(*val, cfg->cs_metadata_hdr_offset,
			IPA_ENDP_INIT_CFG_n_CS_METADATA_HDR_OFFSET_SHFT,
			IPA_ENDP_INIT_CFG_n_CS_METADATA_HDR_OFFSET_BMSK);
	IPA_SETFIELD_IN_REG(*val, cfg->gen_qmb_master_sel,
			IPA_ENDP_INIT_CFG_n_CS_GEN_QMB_MASTER_SEL_SHFT,
			IPA_ENDP_INIT_CFG_n_CS_GEN_QMB_MASTER_SEL_BMSK);

}

static void ipareg_construct_endp_init_deaggr_n(
		enum ipahal_reg_name reg, const void *fields, u32 *val)
{
	struct ipa_ep_cfg_deaggr *ep_deaggr =
		(struct ipa_ep_cfg_deaggr *)fields;

	IPA_SETFIELD_IN_REG(*val, ep_deaggr->deaggr_hdr_len,
		IPA_ENDP_INIT_DEAGGR_n_DEAGGR_HDR_LEN_SHFT,
		IPA_ENDP_INIT_DEAGGR_n_DEAGGR_HDR_LEN_BMSK);

	IPA_SETFIELD_IN_REG(*val, ep_deaggr->packet_offset_valid,
		IPA_ENDP_INIT_DEAGGR_n_PACKET_OFFSET_VALID_SHFT,
		IPA_ENDP_INIT_DEAGGR_n_PACKET_OFFSET_VALID_BMSK);

	IPA_SETFIELD_IN_REG(*val, ep_deaggr->packet_offset_location,
		IPA_ENDP_INIT_DEAGGR_n_PACKET_OFFSET_LOCATION_SHFT,
		IPA_ENDP_INIT_DEAGGR_n_PACKET_OFFSET_LOCATION_BMSK);

	IPA_SETFIELD_IN_REG(*val, ep_deaggr->max_packet_len,
		IPA_ENDP_INIT_DEAGGR_n_MAX_PACKET_LEN_SHFT,
		IPA_ENDP_INIT_DEAGGR_n_MAX_PACKET_LEN_BMSK);
}

static void ipareg_construct_endp_init_hol_block_en_n(
	enum ipahal_reg_name reg, const void *fields, u32 *val)
{
	struct ipa_ep_cfg_holb *ep_holb =
		(struct ipa_ep_cfg_holb *)fields;

	IPA_SETFIELD_IN_REG(*val, ep_holb->en,
		IPA_ENDP_INIT_HOL_BLOCK_EN_n_EN_SHFT,
		IPA_ENDP_INIT_HOL_BLOCK_EN_n_EN_BMSK);
}

static void ipareg_construct_endp_init_hol_block_timer_n(
	enum ipahal_reg_name reg, const void *fields, u32 *val)
{
	struct ipa_ep_cfg_holb *ep_holb =
		(struct ipa_ep_cfg_holb *)fields;

	IPA_SETFIELD_IN_REG(*val, ep_holb->tmr_val,
		IPA_ENDP_INIT_HOL_BLOCK_TIMER_n_TIMER_SHFT,
		IPA_ENDP_INIT_HOL_BLOCK_TIMER_n_TIMER_BMSK);
}

static void ipareg_construct_endp_init_ctrl_n(enum ipahal_reg_name reg,
	const void *fields, u32 *val)
{
	struct ipa_ep_cfg_ctrl *ep_ctrl =
		(struct ipa_ep_cfg_ctrl *)fields;

	IPA_SETFIELD_IN_REG(*val, ep_ctrl->ipa_ep_suspend,
		IPA_ENDP_INIT_CTRL_n_ENDP_SUSPEND_SHFT,
		IPA_ENDP_INIT_CTRL_n_ENDP_SUSPEND_BMSK);

	IPA_SETFIELD_IN_REG(*val, ep_ctrl->ipa_ep_delay,
		IPA_ENDP_INIT_CTRL_n_ENDP_DELAY_SHFT,
		IPA_ENDP_INIT_CTRL_n_ENDP_DELAY_BMSK);
}

static void ipareg_parse_endp_init_ctrl_n(enum ipahal_reg_name reg,
	void *fields, u32 val)
{
	struct ipa_ep_cfg_ctrl *ep_ctrl =
		(struct ipa_ep_cfg_ctrl *)fields;

	ep_ctrl->ipa_ep_suspend =
		((val & IPA_ENDP_INIT_CTRL_n_ENDP_SUSPEND_BMSK) >>
			IPA_ENDP_INIT_CTRL_n_ENDP_SUSPEND_SHFT);

	ep_ctrl->ipa_ep_delay =
		((val & IPA_ENDP_INIT_CTRL_n_ENDP_DELAY_BMSK) >>
		IPA_ENDP_INIT_CTRL_n_ENDP_DELAY_SHFT);
}

static void ipareg_construct_endp_init_ctrl_n_v4_0(enum ipahal_reg_name reg,
	const void *fields, u32 *val)
{
	struct ipa_ep_cfg_ctrl *ep_ctrl =
		(struct ipa_ep_cfg_ctrl *)fields;

	WARN_ON(ep_ctrl->ipa_ep_suspend);

	IPA_SETFIELD_IN_REG(*val, ep_ctrl->ipa_ep_delay,
		IPA_ENDP_INIT_CTRL_n_ENDP_DELAY_SHFT,
		IPA_ENDP_INIT_CTRL_n_ENDP_DELAY_BMSK);
}

static void ipareg_construct_endp_init_ctrl_scnd_n(enum ipahal_reg_name reg,
	const void *fields, u32 *val)
{
	struct ipahal_ep_cfg_ctrl_scnd *ep_ctrl_scnd =
		(struct ipahal_ep_cfg_ctrl_scnd *)fields;

	IPA_SETFIELD_IN_REG(*val, ep_ctrl_scnd->endp_delay,
		IPA_ENDP_INIT_CTRL_SCND_n_ENDP_DELAY_SHFT,
		IPA_ENDP_INIT_CTRL_SCND_n_ENDP_DELAY_BMSK);
}

static void ipareg_construct_endp_init_nat_n(enum ipahal_reg_name reg,
		const void *fields, u32 *val)
{
	struct ipa_ep_cfg_nat *ep_nat =
		(struct ipa_ep_cfg_nat *)fields;

	IPA_SETFIELD_IN_REG(*val, ep_nat->nat_en,
		IPA_ENDP_INIT_NAT_n_NAT_EN_SHFT,
		IPA_ENDP_INIT_NAT_n_NAT_EN_BMSK);
}

static void ipareg_construct_endp_init_conn_track_n(enum ipahal_reg_name reg,
	const void *fields, u32 *val)
{
	struct ipa_ep_cfg_conn_track *ep_ipv6ct =
		(struct ipa_ep_cfg_conn_track *)fields;

	IPA_SETFIELD_IN_REG(*val, ep_ipv6ct->conn_track_en,
		IPA_ENDP_INIT_CONN_TRACK_n_CONN_TRACK_EN_SHFT,
		IPA_ENDP_INIT_CONN_TRACK_n_CONN_TRACK_EN_BMSK);
}

static void ipareg_construct_endp_init_mode_n(enum ipahal_reg_name reg,
		const void *fields, u32 *val)
{
	struct ipahal_reg_endp_init_mode *init_mode =
		(struct ipahal_reg_endp_init_mode *)fields;

	IPA_SETFIELD_IN_REG(*val, init_mode->ep_mode.mode,
		IPA_ENDP_INIT_MODE_n_MODE_SHFT,
		IPA_ENDP_INIT_MODE_n_MODE_BMSK);

	IPA_SETFIELD_IN_REG(*val, init_mode->dst_pipe_number,
		IPA_ENDP_INIT_MODE_n_DEST_PIPE_INDEX_SHFT,
		IPA_ENDP_INIT_MODE_n_DEST_PIPE_INDEX_BMSK);
}

static void ipareg_construct_endp_init_route_n(enum ipahal_reg_name reg,
	const void *fields, u32 *val)
{
	struct ipahal_reg_endp_init_route *ep_init_rt =
		(struct ipahal_reg_endp_init_route *)fields;

	IPA_SETFIELD_IN_REG(*val, ep_init_rt->route_table_index,
		IPA_ENDP_INIT_ROUTE_n_ROUTE_TABLE_INDEX_SHFT,
		IPA_ENDP_INIT_ROUTE_n_ROUTE_TABLE_INDEX_BMSK);

}

static void ipareg_parse_endp_init_aggr_n(enum ipahal_reg_name reg,
	void *fields, u32 val)
{
	struct ipa_ep_cfg_aggr *ep_aggr =
		(struct ipa_ep_cfg_aggr *)fields;

	memset(ep_aggr, 0, sizeof(struct ipa_ep_cfg_aggr));

	ep_aggr->aggr_en =
		(((val & IPA_ENDP_INIT_AGGR_n_AGGR_EN_BMSK) >>
			IPA_ENDP_INIT_AGGR_n_AGGR_EN_SHFT)
			== IPA_ENABLE_AGGR);
	ep_aggr->aggr =
		((val & IPA_ENDP_INIT_AGGR_n_AGGR_TYPE_BMSK) >>
			IPA_ENDP_INIT_AGGR_n_AGGR_TYPE_SHFT);
	ep_aggr->aggr_byte_limit =
		((val & IPA_ENDP_INIT_AGGR_n_AGGR_BYTE_LIMIT_BMSK) >>
			IPA_ENDP_INIT_AGGR_n_AGGR_BYTE_LIMIT_SHFT);
	ep_aggr->aggr_time_limit =
		((val & IPA_ENDP_INIT_AGGR_n_AGGR_TIME_LIMIT_BMSK) >>
			IPA_ENDP_INIT_AGGR_n_AGGR_TIME_LIMIT_SHFT);
	ep_aggr->aggr_pkt_limit =
		((val & IPA_ENDP_INIT_AGGR_n_AGGR_PKT_LIMIT_BMSK) >>
			IPA_ENDP_INIT_AGGR_n_AGGR_PKT_LIMIT_SHFT);
	ep_aggr->aggr_sw_eof_active =
		((val & IPA_ENDP_INIT_AGGR_n_AGGR_SW_EOF_ACTIVE_BMSK) >>
			IPA_ENDP_INIT_AGGR_n_AGGR_SW_EOF_ACTIVE_SHFT);
	ep_aggr->aggr_hard_byte_limit_en =
		((val & IPA_ENDP_INIT_AGGR_n_AGGR_HARD_BYTE_LIMIT_ENABLE_BMSK)
			>>
			IPA_ENDP_INIT_AGGR_n_AGGR_HARD_BYTE_LIMIT_ENABLE_SHFT);
}

static void ipareg_construct_endp_init_aggr_n(enum ipahal_reg_name reg,
	const void *fields, u32 *val)
{
	struct ipa_ep_cfg_aggr *ep_aggr =
		(struct ipa_ep_cfg_aggr *)fields;

	IPA_SETFIELD_IN_REG(*val, ep_aggr->aggr_en,
		IPA_ENDP_INIT_AGGR_n_AGGR_EN_SHFT,
		IPA_ENDP_INIT_AGGR_n_AGGR_EN_BMSK);

	IPA_SETFIELD_IN_REG(*val, ep_aggr->aggr,
		IPA_ENDP_INIT_AGGR_n_AGGR_TYPE_SHFT,
		IPA_ENDP_INIT_AGGR_n_AGGR_TYPE_BMSK);

	IPA_SETFIELD_IN_REG(*val, ep_aggr->aggr_byte_limit,
		IPA_ENDP_INIT_AGGR_n_AGGR_BYTE_LIMIT_SHFT,
		IPA_ENDP_INIT_AGGR_n_AGGR_BYTE_LIMIT_BMSK);

	IPA_SETFIELD_IN_REG(*val, ep_aggr->aggr_time_limit,
		IPA_ENDP_INIT_AGGR_n_AGGR_TIME_LIMIT_SHFT,
		IPA_ENDP_INIT_AGGR_n_AGGR_TIME_LIMIT_BMSK);

	IPA_SETFIELD_IN_REG(*val, ep_aggr->aggr_pkt_limit,
		IPA_ENDP_INIT_AGGR_n_AGGR_PKT_LIMIT_SHFT,
		IPA_ENDP_INIT_AGGR_n_AGGR_PKT_LIMIT_BMSK);

	IPA_SETFIELD_IN_REG(*val, ep_aggr->aggr_sw_eof_active,
		IPA_ENDP_INIT_AGGR_n_AGGR_SW_EOF_ACTIVE_SHFT,
		IPA_ENDP_INIT_AGGR_n_AGGR_SW_EOF_ACTIVE_BMSK);

	/* At IPAv3 hard_byte_limit is not supported */
	ep_aggr->aggr_hard_byte_limit_en = 0;
	IPA_SETFIELD_IN_REG(*val, ep_aggr->aggr_hard_byte_limit_en,
		IPA_ENDP_INIT_AGGR_n_AGGR_HARD_BYTE_LIMIT_ENABLE_SHFT,
		IPA_ENDP_INIT_AGGR_n_AGGR_HARD_BYTE_LIMIT_ENABLE_BMSK);
}

static void ipareg_construct_endp_init_hdr_ext_n(enum ipahal_reg_name reg,
	const void *fields, u32 *val)
{
	struct ipa_ep_cfg_hdr_ext *ep_hdr_ext;
	u8 hdr_endianness;

	ep_hdr_ext = (struct ipa_ep_cfg_hdr_ext *)fields;
	hdr_endianness = ep_hdr_ext->hdr_little_endian ? 0 : 1;

	IPA_SETFIELD_IN_REG(*val, ep_hdr_ext->hdr_pad_to_alignment,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_PAD_TO_ALIGNMENT_SHFT,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_PAD_TO_ALIGNMENT_BMSK_v3_0);

	IPA_SETFIELD_IN_REG(*val, ep_hdr_ext->hdr_total_len_or_pad_offset,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_TOTAL_LEN_OR_PAD_OFFSET_SHFT,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_TOTAL_LEN_OR_PAD_OFFSET_BMSK);

	IPA_SETFIELD_IN_REG(*val, ep_hdr_ext->hdr_payload_len_inc_padding,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_PAYLOAD_LEN_INC_PADDING_SHFT,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_PAYLOAD_LEN_INC_PADDING_BMSK);

	IPA_SETFIELD_IN_REG(*val, ep_hdr_ext->hdr_total_len_or_pad,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_TOTAL_LEN_OR_PAD_SHFT,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_TOTAL_LEN_OR_PAD_BMSK);

	IPA_SETFIELD_IN_REG(*val, ep_hdr_ext->hdr_total_len_or_pad_valid,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_TOTAL_LEN_OR_PAD_VALID_SHFT,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_TOTAL_LEN_OR_PAD_VALID_BMSK);

	IPA_SETFIELD_IN_REG(*val, hdr_endianness,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_ENDIANNESS_SHFT,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_ENDIANNESS_BMSK);
}

static void ipareg_construct_endp_init_hdr_n(enum ipahal_reg_name reg,
	const void *fields, u32 *val)
{
	struct ipa_ep_cfg_hdr *ep_hdr;

	ep_hdr = (struct ipa_ep_cfg_hdr *)fields;

	IPA_SETFIELD_IN_REG(*val, ep_hdr->hdr_metadata_reg_valid,
		IPA_ENDP_INIT_HDR_n_HDR_METADATA_REG_VALID_SHFT_v2,
		IPA_ENDP_INIT_HDR_n_HDR_METADATA_REG_VALID_BMSK_v2);

	IPA_SETFIELD_IN_REG(*val, ep_hdr->hdr_remove_additional,
		IPA_ENDP_INIT_HDR_n_HDR_LEN_INC_DEAGG_HDR_SHFT_v2,
		IPA_ENDP_INIT_HDR_n_HDR_LEN_INC_DEAGG_HDR_BMSK_v2);

	IPA_SETFIELD_IN_REG(*val, ep_hdr->hdr_a5_mux,
		IPA_ENDP_INIT_HDR_n_HDR_A5_MUX_SHFT,
		IPA_ENDP_INIT_HDR_n_HDR_A5_MUX_BMSK);

	IPA_SETFIELD_IN_REG(*val, ep_hdr->hdr_ofst_pkt_size,
		IPA_ENDP_INIT_HDR_n_HDR_OFST_PKT_SIZE_SHFT,
		IPA_ENDP_INIT_HDR_n_HDR_OFST_PKT_SIZE_BMSK);

	IPA_SETFIELD_IN_REG(*val, ep_hdr->hdr_ofst_pkt_size_valid,
		IPA_ENDP_INIT_HDR_n_HDR_OFST_PKT_SIZE_VALID_SHFT,
		IPA_ENDP_INIT_HDR_n_HDR_OFST_PKT_SIZE_VALID_BMSK);

	IPA_SETFIELD_IN_REG(*val, ep_hdr->hdr_additional_const_len,
		IPA_ENDP_INIT_HDR_n_HDR_ADDITIONAL_CONST_LEN_SHFT,
		IPA_ENDP_INIT_HDR_n_HDR_ADDITIONAL_CONST_LEN_BMSK);

	IPA_SETFIELD_IN_REG(*val, ep_hdr->hdr_ofst_metadata,
		IPA_ENDP_INIT_HDR_n_HDR_OFST_METADATA_SHFT,
		IPA_ENDP_INIT_HDR_n_HDR_OFST_METADATA_BMSK);

	IPA_SETFIELD_IN_REG(*val, ep_hdr->hdr_ofst_metadata_valid,
		IPA_ENDP_INIT_HDR_n_HDR_OFST_METADATA_VALID_SHFT,
		IPA_ENDP_INIT_HDR_n_HDR_OFST_METADATA_VALID_BMSK);

	IPA_SETFIELD_IN_REG(*val, ep_hdr->hdr_len,
		IPA_ENDP_INIT_HDR_n_HDR_LEN_SHFT,
		IPA_ENDP_INIT_HDR_n_HDR_LEN_BMSK);
}

static void ipareg_construct_route(enum ipahal_reg_name reg,
	const void *fields, u32 *val)
{
	struct ipahal_reg_route *route;

	route = (struct ipahal_reg_route *)fields;

	IPA_SETFIELD_IN_REG(*val, route->route_dis,
		IPA_ROUTE_ROUTE_DIS_SHFT,
		IPA_ROUTE_ROUTE_DIS_BMSK);

	IPA_SETFIELD_IN_REG(*val, route->route_def_pipe,
		IPA_ROUTE_ROUTE_DEF_PIPE_SHFT,
		IPA_ROUTE_ROUTE_DEF_PIPE_BMSK);

	IPA_SETFIELD_IN_REG(*val, route->route_def_hdr_table,
		IPA_ROUTE_ROUTE_DEF_HDR_TABLE_SHFT,
		IPA_ROUTE_ROUTE_DEF_HDR_TABLE_BMSK);

	IPA_SETFIELD_IN_REG(*val, route->route_def_hdr_ofst,
		IPA_ROUTE_ROUTE_DEF_HDR_OFST_SHFT,
		IPA_ROUTE_ROUTE_DEF_HDR_OFST_BMSK);

	IPA_SETFIELD_IN_REG(*val, route->route_frag_def_pipe,
		IPA_ROUTE_ROUTE_FRAG_DEF_PIPE_SHFT,
		IPA_ROUTE_ROUTE_FRAG_DEF_PIPE_BMSK);

	IPA_SETFIELD_IN_REG(*val, route->route_def_retain_hdr,
		IPA_ROUTE_ROUTE_DEF_RETAIN_HDR_SHFT,
		IPA_ROUTE_ROUTE_DEF_RETAIN_HDR_BMSK);
}

static void ipareg_construct_qsb_max_writes(enum ipahal_reg_name reg,
	const void *fields, u32 *val)
{
	struct ipahal_reg_qsb_max_writes *max_writes;

	max_writes = (struct ipahal_reg_qsb_max_writes *)fields;

	IPA_SETFIELD_IN_REG(*val, max_writes->qmb_0_max_writes,
			    IPA_QSB_MAX_WRITES_GEN_QMB_0_MAX_WRITES_SHFT,
			    IPA_QSB_MAX_WRITES_GEN_QMB_0_MAX_WRITES_BMSK);
	IPA_SETFIELD_IN_REG(*val, max_writes->qmb_1_max_writes,
			    IPA_QSB_MAX_WRITES_GEN_QMB_1_MAX_WRITES_SHFT,
			    IPA_QSB_MAX_WRITES_GEN_QMB_1_MAX_WRITES_BMSK);
}

static void ipareg_construct_qsb_max_reads(enum ipahal_reg_name reg,
	const void *fields, u32 *val)
{
	struct ipahal_reg_qsb_max_reads *max_reads;

	max_reads = (struct ipahal_reg_qsb_max_reads *)fields;

	IPA_SETFIELD_IN_REG(*val, max_reads->qmb_0_max_reads,
			    IPA_QSB_MAX_READS_GEN_QMB_0_MAX_READS_SHFT,
			    IPA_QSB_MAX_READS_GEN_QMB_0_MAX_READS_BMSK);
	IPA_SETFIELD_IN_REG(*val, max_reads->qmb_1_max_reads,
			    IPA_QSB_MAX_READS_GEN_QMB_1_MAX_READS_SHFT,
			    IPA_QSB_MAX_READS_GEN_QMB_1_MAX_READS_BMSK);
}

static void ipareg_construct_qsb_max_reads_v4_0(enum ipahal_reg_name reg,
	const void *fields, u32 *val)
{
	struct ipahal_reg_qsb_max_reads *max_reads;

	max_reads = (struct ipahal_reg_qsb_max_reads *)fields;

	IPA_SETFIELD_IN_REG(*val, max_reads->qmb_0_max_reads,
			    IPA_QSB_MAX_READS_GEN_QMB_0_MAX_READS_SHFT,
			    IPA_QSB_MAX_READS_GEN_QMB_0_MAX_READS_BMSK);
	IPA_SETFIELD_IN_REG(*val, max_reads->qmb_1_max_reads,
			    IPA_QSB_MAX_READS_GEN_QMB_1_MAX_READS_SHFT,
			    IPA_QSB_MAX_READS_GEN_QMB_1_MAX_READS_BMSK);
	IPA_SETFIELD_IN_REG(*val, max_reads->qmb_0_max_read_beats,
		    IPA_QSB_MAX_READS_GEN_QMB_0_MAX_READS_BEATS_SHFT_V4_0,
		    IPA_QSB_MAX_READS_GEN_QMB_0_MAX_READS_BEATS_BMSK_V4_0);
	IPA_SETFIELD_IN_REG(*val, max_reads->qmb_1_max_read_beats,
		    IPA_QSB_MAX_READS_GEN_QMB_1_MAX_READS_BEATS_SHFT_V4_0,
		    IPA_QSB_MAX_READS_GEN_QMB_1_MAX_READS_BEATS_BMSK_V4_0);
}

static void ipareg_parse_tx_cfg(enum ipahal_reg_name reg,
	void *fields, u32 val)
{
	struct ipahal_reg_tx_cfg *tx_cfg;

	tx_cfg = (struct ipahal_reg_tx_cfg *)fields;

	tx_cfg->tx0_prefetch_disable = IPA_GETFIELD_FROM_REG(val,
		IPA_TX_CFG_TX0_PREFETCH_DISABLE_SHFT_V3_5,
		IPA_TX_CFG_TX0_PREFETCH_DISABLE_BMSK_V3_5);

	tx_cfg->tx1_prefetch_disable = IPA_GETFIELD_FROM_REG(val,
		IPA_TX_CFG_TX1_PREFETCH_DISABLE_SHFT_V3_5,
		IPA_TX_CFG_TX1_PREFETCH_DISABLE_BMSK_V3_5);

	tx_cfg->tx0_prefetch_almost_empty_size = IPA_GETFIELD_FROM_REG(val,
		IPA_TX_CFG_PREFETCH_ALMOST_EMPTY_SIZE_SHFT_V3_5,
		IPA_TX_CFG_PREFETCH_ALMOST_EMPTY_SIZE_BMSK_V3_5);

	tx_cfg->tx1_prefetch_almost_empty_size =
		tx_cfg->tx0_prefetch_almost_empty_size;
}

static void ipareg_parse_tx_cfg_v4_0(enum ipahal_reg_name reg,
	void *fields, u32 val)
{
	struct ipahal_reg_tx_cfg *tx_cfg;

	tx_cfg = (struct ipahal_reg_tx_cfg *)fields;

	tx_cfg->tx0_prefetch_almost_empty_size = IPA_GETFIELD_FROM_REG(val,
		IPA_TX_CFG_PREFETCH_ALMOST_EMPTY_SIZE_TX0_SHFT_V4_0,
		IPA_TX_CFG_PREFETCH_ALMOST_EMPTY_SIZE_TX0_BMSK_V4_0);

	tx_cfg->tx1_prefetch_almost_empty_size = IPA_GETFIELD_FROM_REG(val,
		IPA_TX_CFG_PREFETCH_ALMOST_EMPTY_SIZE_TX1_SHFT_V4_0,
		IPA_TX_CFG_PREFETCH_ALMOST_EMPTY_SIZE_TX1_BMSK_V4_0);

	tx_cfg->dmaw_scnd_outsd_pred_en = IPA_GETFIELD_FROM_REG(val,
		IPA_TX_CFG_DMAW_SCND_OUTSD_PRED_EN_SHFT_V4_0,
		IPA_TX_CFG_DMAW_SCND_OUTSD_PRED_EN_BMSK_V4_0);

	tx_cfg->dmaw_scnd_outsd_pred_threshold = IPA_GETFIELD_FROM_REG(val,
		IPA_TX_CFG_DMAW_SCND_OUTSD_PRED_THRESHOLD_SHFT_V4_0,
		IPA_TX_CFG_DMAW_SCND_OUTSD_PRED_THRESHOLD_BMSK_V4_0);

	tx_cfg->dmaw_max_beats_256_dis = IPA_GETFIELD_FROM_REG(val,
		IPA_TX_CFG_DMAW_MAX_BEATS_256_DIS_SHFT_V4_0,
		IPA_TX_CFG_DMAW_MAX_BEATS_256_DIS_BMSK_V4_0);

	tx_cfg->pa_mask_en = IPA_GETFIELD_FROM_REG(val,
		IPA_TX_CFG_PA_MASK_EN_SHFT_V4_0,
		IPA_TX_CFG_PA_MASK_EN_BMSK_V4_0);
}

static void ipareg_construct_tx_cfg(enum ipahal_reg_name reg,
	const void *fields, u32 *val)
{
	struct ipahal_reg_tx_cfg *tx_cfg;

	tx_cfg = (struct ipahal_reg_tx_cfg *)fields;

	if (tx_cfg->tx0_prefetch_almost_empty_size !=
			tx_cfg->tx1_prefetch_almost_empty_size)
		ipa_assert();

	IPA_SETFIELD_IN_REG(*val, tx_cfg->tx0_prefetch_disable,
		IPA_TX_CFG_TX0_PREFETCH_DISABLE_SHFT_V3_5,
		IPA_TX_CFG_TX0_PREFETCH_DISABLE_BMSK_V3_5);

	IPA_SETFIELD_IN_REG(*val, tx_cfg->tx1_prefetch_disable,
		IPA_TX_CFG_TX1_PREFETCH_DISABLE_SHFT_V3_5,
		IPA_TX_CFG_TX1_PREFETCH_DISABLE_BMSK_V3_5);

	IPA_SETFIELD_IN_REG(*val, tx_cfg->tx0_prefetch_almost_empty_size,
		IPA_TX_CFG_PREFETCH_ALMOST_EMPTY_SIZE_SHFT_V3_5,
		IPA_TX_CFG_PREFETCH_ALMOST_EMPTY_SIZE_BMSK_V3_5);
}

static void ipareg_construct_tx_cfg_v4_0(enum ipahal_reg_name reg,
	const void *fields, u32 *val)
{
	struct ipahal_reg_tx_cfg *tx_cfg;

	tx_cfg = (struct ipahal_reg_tx_cfg *)fields;

	IPA_SETFIELD_IN_REG(*val, tx_cfg->tx0_prefetch_almost_empty_size,
		IPA_TX_CFG_PREFETCH_ALMOST_EMPTY_SIZE_TX0_SHFT_V4_0,
		IPA_TX_CFG_PREFETCH_ALMOST_EMPTY_SIZE_TX0_BMSK_V4_0);

	IPA_SETFIELD_IN_REG(*val, tx_cfg->tx1_prefetch_almost_empty_size,
		IPA_TX_CFG_PREFETCH_ALMOST_EMPTY_SIZE_TX1_SHFT_V4_0,
		IPA_TX_CFG_PREFETCH_ALMOST_EMPTY_SIZE_TX1_BMSK_V4_0);

	IPA_SETFIELD_IN_REG(*val, tx_cfg->dmaw_scnd_outsd_pred_threshold,
		IPA_TX_CFG_DMAW_SCND_OUTSD_PRED_THRESHOLD_SHFT_V4_0,
		IPA_TX_CFG_DMAW_SCND_OUTSD_PRED_THRESHOLD_BMSK_V4_0);

	IPA_SETFIELD_IN_REG(*val, tx_cfg->dmaw_max_beats_256_dis,
		IPA_TX_CFG_DMAW_MAX_BEATS_256_DIS_SHFT_V4_0,
		IPA_TX_CFG_DMAW_MAX_BEATS_256_DIS_BMSK_V4_0);

	IPA_SETFIELD_IN_REG(*val, tx_cfg->dmaw_scnd_outsd_pred_en,
		IPA_TX_CFG_DMAW_SCND_OUTSD_PRED_EN_SHFT_V4_0,
		IPA_TX_CFG_DMAW_SCND_OUTSD_PRED_EN_BMSK_V4_0);

	IPA_SETFIELD_IN_REG(*val, tx_cfg->pa_mask_en,
		IPA_TX_CFG_PA_MASK_EN_SHFT_V4_0,
		IPA_TX_CFG_PA_MASK_EN_BMSK_V4_0);
}

static void ipareg_construct_idle_indication_cfg(enum ipahal_reg_name reg,
	const void *fields, u32 *val)
{
	struct ipahal_reg_idle_indication_cfg *idle_indication_cfg;

	idle_indication_cfg = (struct ipahal_reg_idle_indication_cfg *)fields;

	IPA_SETFIELD_IN_REG(*val,
		idle_indication_cfg->enter_idle_debounce_thresh,
		IPA_IDLE_INDICATION_CFG_ENTER_IDLE_DEBOUNCE_THRESH_SHFT_V3_5,
		IPA_IDLE_INDICATION_CFG_ENTER_IDLE_DEBOUNCE_THRESH_BMSK_V3_5);

	IPA_SETFIELD_IN_REG(*val,
		idle_indication_cfg->const_non_idle_enable,
		IPA_IDLE_INDICATION_CFG_CONST_NON_IDLE_ENABLE_SHFT_V3_5,
		IPA_IDLE_INDICATION_CFG_CONST_NON_IDLE_ENABLE_BMSK_V3_5);
}

static void ipareg_construct_hps_queue_weights(enum ipahal_reg_name reg,
	const void *fields, u32 *val)
{
	struct ipahal_reg_rx_hps_weights *hps_weights;

	hps_weights = (struct ipahal_reg_rx_hps_weights *)fields;

	IPA_SETFIELD_IN_REG(*val,
		hps_weights->hps_queue_weight_0,
		IPA_HPS_FTCH_ARB_QUEUE_WEIGHTS_RX_HPS_QUEUE_WEIGHT_0_SHFT,
		IPA_HPS_FTCH_ARB_QUEUE_WEIGHTS_RX_HPS_QUEUE_WEIGHT_0_BMSK);

	IPA_SETFIELD_IN_REG(*val,
		hps_weights->hps_queue_weight_1,
		IPA_HPS_FTCH_ARB_QUEUE_WEIGHTS_RX_HPS_QUEUE_WEIGHT_1_SHFT,
		IPA_HPS_FTCH_ARB_QUEUE_WEIGHTS_RX_HPS_QUEUE_WEIGHT_1_BMSK);

	IPA_SETFIELD_IN_REG(*val,
		hps_weights->hps_queue_weight_2,
		IPA_HPS_FTCH_ARB_QUEUE_WEIGHTS_RX_HPS_QUEUE_WEIGHT_2_SHFT,
		IPA_HPS_FTCH_ARB_QUEUE_WEIGHTS_RX_HPS_QUEUE_WEIGHT_2_BMSK);

	IPA_SETFIELD_IN_REG(*val,
		hps_weights->hps_queue_weight_3,
		IPA_HPS_FTCH_ARB_QUEUE_WEIGHTS_RX_HPS_QUEUE_WEIGHT_3_SHFT,
		IPA_HPS_FTCH_ARB_QUEUE_WEIGHTS_RX_HPS_QUEUE_WEIGHT_3_BMSK);
}

static void ipareg_parse_hps_queue_weights(
	enum ipahal_reg_name reg, void *fields, u32 val)
{
	struct ipahal_reg_rx_hps_weights *hps_weights =
		(struct ipahal_reg_rx_hps_weights *)fields;

	memset(hps_weights, 0, sizeof(struct ipahal_reg_rx_hps_weights));

	hps_weights->hps_queue_weight_0 = IPA_GETFIELD_FROM_REG(val,
		IPA_HPS_FTCH_ARB_QUEUE_WEIGHTS_RX_HPS_QUEUE_WEIGHT_0_SHFT,
		IPA_HPS_FTCH_ARB_QUEUE_WEIGHTS_RX_HPS_QUEUE_WEIGHT_0_BMSK);

	hps_weights->hps_queue_weight_1 = IPA_GETFIELD_FROM_REG(val,
		IPA_HPS_FTCH_ARB_QUEUE_WEIGHTS_RX_HPS_QUEUE_WEIGHT_1_SHFT,
		IPA_HPS_FTCH_ARB_QUEUE_WEIGHTS_RX_HPS_QUEUE_WEIGHT_1_BMSK);

	hps_weights->hps_queue_weight_2 = IPA_GETFIELD_FROM_REG(val,
		IPA_HPS_FTCH_ARB_QUEUE_WEIGHTS_RX_HPS_QUEUE_WEIGHT_2_SHFT,
		IPA_HPS_FTCH_ARB_QUEUE_WEIGHTS_RX_HPS_QUEUE_WEIGHT_2_BMSK);

	hps_weights->hps_queue_weight_3 = IPA_GETFIELD_FROM_REG(val,
		IPA_HPS_FTCH_ARB_QUEUE_WEIGHTS_RX_HPS_QUEUE_WEIGHT_3_SHFT,
		IPA_HPS_FTCH_ARB_QUEUE_WEIGHTS_RX_HPS_QUEUE_WEIGHT_3_BMSK);
}

/*
 * struct ipahal_reg_obj - Register H/W information for specific IPA version
 * @construct - CB to construct register value from abstracted structure
 * @parse - CB to parse register value to abstracted structure
 * @offset - register offset relative to base address
 * @n_ofst - N parameterized register sub-offset
 */
struct ipahal_reg_obj {
	void (*construct)(enum ipahal_reg_name reg, const void *fields,
		u32 *val);
	void (*parse)(enum ipahal_reg_name reg, void *fields,
		u32 val);
	u32 offset;
	u32 n_ofst;
};

/*
 * This table contains the info regarding each register for IPAv3 and later.
 * Information like: offset and construct/parse functions.
 * All the information on the register on IPAv3 are statically defined below.
 * If information is missing regarding some register on some IPA version,
 *  the init function will fill it with the information from the previous
 *  IPA version.
 * Information is considered missing if all of the fields are 0.
 * If offset is -1, this means that the register is removed on the
 *  specific version.
 */
static struct ipahal_reg_obj ipahal_reg_objs[IPA_HW_MAX][IPA_REG_MAX] = {
	/* IPAv3 */
	[IPA_HW_v3_0][IPA_ROUTE] = {
		ipareg_construct_route, ipareg_parse_dummy,
		0x00000048, 0},
	[IPA_HW_v3_0][IPA_IRQ_STTS_EE_n] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00003008, 0x1000},
	[IPA_HW_v3_0][IPA_IRQ_EN_EE_n] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x0000300c, 0x1000},
	[IPA_HW_v3_0][IPA_IRQ_CLR_EE_n] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00003010, 0x1000},
	[IPA_HW_v3_0][IPA_IRQ_SUSPEND_INFO_EE_n] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00003098, 0x1000},
	[IPA_HW_v3_0][IPA_BCR] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x000001D0, 0},
	[IPA_HW_v3_0][IPA_ENABLED_PIPES] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00000038, 0},
	[IPA_HW_v3_0][IPA_COMP_SW_RESET] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00000040, 0},
	[IPA_HW_v3_0][IPA_VERSION] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00000034, 0},
	[IPA_HW_v3_0][IPA_TAG_TIMER] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00000060, 0 },
	[IPA_HW_v3_0][IPA_COMP_HW_VERSION] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00000030, 0},
	[IPA_HW_v3_0][IPA_SPARE_REG_1] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00005090, 0},
	[IPA_HW_v3_0][IPA_SPARE_REG_2] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00005094, 0},
	[IPA_HW_v3_0][IPA_COMP_CFG] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x0000003C, 0},
	[IPA_HW_v3_0][IPA_STATE_AGGR_ACTIVE] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x0000010C, 0},
	[IPA_HW_v3_0][IPA_ENDP_INIT_HDR_n] = {
		ipareg_construct_endp_init_hdr_n, ipareg_parse_dummy,
		0x00000810, 0x70},
	[IPA_HW_v3_0][IPA_ENDP_INIT_HDR_EXT_n] = {
		ipareg_construct_endp_init_hdr_ext_n, ipareg_parse_dummy,
		0x00000814, 0x70},
	[IPA_HW_v3_0][IPA_ENDP_INIT_AGGR_n] = {
		ipareg_construct_endp_init_aggr_n,
		ipareg_parse_endp_init_aggr_n,
		0x00000824, 0x70},
	[IPA_HW_v3_0][IPA_AGGR_FORCE_CLOSE] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x000001EC, 0},
	[IPA_HW_v3_0][IPA_ENDP_INIT_ROUTE_n] = {
		ipareg_construct_endp_init_route_n, ipareg_parse_dummy,
		0x00000828, 0x70},
	[IPA_HW_v3_0][IPA_ENDP_INIT_MODE_n] = {
		ipareg_construct_endp_init_mode_n, ipareg_parse_dummy,
		0x00000820, 0x70},
	[IPA_HW_v3_0][IPA_ENDP_INIT_NAT_n] = {
		ipareg_construct_endp_init_nat_n, ipareg_parse_dummy,
		0x0000080C, 0x70},
	[IPA_HW_v3_0][IPA_ENDP_INIT_CTRL_n] = {
		ipareg_construct_endp_init_ctrl_n,
		ipareg_parse_endp_init_ctrl_n,
		0x00000800, 0x70},
	[IPA_HW_v3_0][IPA_ENDP_INIT_CTRL_SCND_n] = {
		ipareg_construct_endp_init_ctrl_scnd_n, ipareg_parse_dummy,
		0x00000804, 0x70 },
	[IPA_HW_v3_0][IPA_ENDP_INIT_HOL_BLOCK_EN_n] = {
		ipareg_construct_endp_init_hol_block_en_n,
		ipareg_parse_dummy,
		0x0000082c, 0x70},
	[IPA_HW_v3_0][IPA_ENDP_INIT_HOL_BLOCK_TIMER_n] = {
		ipareg_construct_endp_init_hol_block_timer_n,
		ipareg_parse_dummy,
		0x00000830, 0x70},
	[IPA_HW_v3_0][IPA_ENDP_INIT_DEAGGR_n] = {
		ipareg_construct_endp_init_deaggr_n,
		ipareg_parse_dummy,
		0x00000834, 0x70},
	[IPA_HW_v3_0][IPA_ENDP_INIT_SEQ_n] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x0000083C, 0x70},
	[IPA_HW_v3_0][IPA_DEBUG_CNT_REG_n] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00000600, 0x4},
	[IPA_HW_v3_0][IPA_ENDP_INIT_CFG_n] = {
		ipareg_construct_endp_init_cfg_n, ipareg_parse_dummy,
		0x00000808, 0x70},
	[IPA_HW_v3_0][IPA_IRQ_EE_UC_n] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x0000301c, 0x1000},
	[IPA_HW_v3_0][IPA_ENDP_INIT_HDR_METADATA_MASK_n] = {
		ipareg_construct_endp_init_hdr_metadata_mask_n,
		ipareg_parse_dummy,
		0x00000818, 0x70},
	[IPA_HW_v3_0][IPA_ENDP_INIT_HDR_METADATA_n] = {
		ipareg_construct_endp_init_hdr_metadata_n,
		ipareg_parse_dummy,
		0x0000081c, 0x70},
	[IPA_HW_v3_0][IPA_ENDP_INIT_RSRC_GRP_n] = {
		ipareg_construct_endp_init_rsrc_grp_n,
		ipareg_parse_dummy,
		0x00000838, 0x70},
	[IPA_HW_v3_0][IPA_SHARED_MEM_SIZE] = {
		ipareg_construct_dummy, ipareg_parse_shared_mem_size,
		0x00000054, 0},
	[IPA_HW_v3_0][IPA_SRAM_DIRECT_ACCESS_n] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00007000, 0x4},
	[IPA_HW_v3_0][IPA_DEBUG_CNT_CTRL_n] = {
		ipareg_construct_debug_cnt_ctrl_n, ipareg_parse_dummy,
		0x00000640, 0x4},
	[IPA_HW_v3_0][IPA_UC_MAILBOX_m_n] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00032000, 0x4},
	[IPA_HW_v3_0][IPA_FILT_ROUT_HASH_FLUSH] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00000090, 0},
	[IPA_HW_v3_0][IPA_SINGLE_NDP_MODE] = {
		ipareg_construct_single_ndp_mode, ipareg_parse_single_ndp_mode,
		0x00000068, 0},
	[IPA_HW_v3_0][IPA_QCNCM] = {
		ipareg_construct_qcncm, ipareg_parse_qcncm,
		0x00000064, 0},
	[IPA_HW_v3_0][IPA_SYS_PKT_PROC_CNTXT_BASE] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x000001e0, 0},
	[IPA_HW_v3_0][IPA_LOCAL_PKT_PROC_CNTXT_BASE] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x000001e8, 0},
	[IPA_HW_v3_0][IPA_ENDP_STATUS_n] = {
		ipareg_construct_endp_status_n, ipareg_parse_dummy,
		0x00000840, 0x70},
	[IPA_HW_v3_0][IPA_ENDP_FILTER_ROUTER_HSH_CFG_n] = {
		ipareg_construct_hash_cfg_n, ipareg_parse_hash_cfg_n,
		0x0000085C, 0x70},
	[IPA_HW_v3_0][IPA_SRC_RSRC_GRP_01_RSRC_TYPE_n] = {
		ipareg_construct_rsrg_grp_xy, ipareg_parse_dummy,
		0x00000400, 0x20},
	[IPA_HW_v3_0][IPA_SRC_RSRC_GRP_23_RSRC_TYPE_n] = {
		ipareg_construct_rsrg_grp_xy, ipareg_parse_dummy,
		0x00000404, 0x20},
	[IPA_HW_v3_0][IPA_SRC_RSRC_GRP_45_RSRC_TYPE_n] = {
		ipareg_construct_rsrg_grp_xy, ipareg_parse_dummy,
		0x00000408, 0x20},
	[IPA_HW_v3_0][IPA_SRC_RSRC_GRP_67_RSRC_TYPE_n] = {
		ipareg_construct_rsrg_grp_xy, ipareg_parse_dummy,
		0x0000040C, 0x20},
	[IPA_HW_v3_0][IPA_DST_RSRC_GRP_01_RSRC_TYPE_n] = {
		ipareg_construct_rsrg_grp_xy, ipareg_parse_dummy,
		0x00000500, 0x20},
	[IPA_HW_v3_0][IPA_DST_RSRC_GRP_23_RSRC_TYPE_n] = {
		ipareg_construct_rsrg_grp_xy, ipareg_parse_dummy,
		0x00000504, 0x20},
	[IPA_HW_v3_0][IPA_DST_RSRC_GRP_45_RSRC_TYPE_n] = {
		ipareg_construct_rsrg_grp_xy, ipareg_parse_dummy,
		0x00000508, 0x20},
	[IPA_HW_v3_0][IPA_DST_RSRC_GRP_67_RSRC_TYPE_n] = {
		ipareg_construct_rsrg_grp_xy, ipareg_parse_dummy,
		0x0000050c, 0x20},
	[IPA_HW_v3_0][IPA_RX_HPS_CLIENTS_MIN_DEPTH_0] = {
		ipareg_construct_rx_hps_clients_depth0, ipareg_parse_dummy,
		0x000023C4, 0},
	[IPA_HW_v3_0][IPA_RX_HPS_CLIENTS_MIN_DEPTH_1] = {
		ipareg_construct_rx_hps_clients_depth1, ipareg_parse_dummy,
		0x000023C8, 0},
	[IPA_HW_v3_0][IPA_RX_HPS_CLIENTS_MAX_DEPTH_0] = {
		ipareg_construct_rx_hps_clients_depth0, ipareg_parse_dummy,
		0x000023CC, 0},
	[IPA_HW_v3_0][IPA_RX_HPS_CLIENTS_MAX_DEPTH_1] = {
		ipareg_construct_rx_hps_clients_depth1, ipareg_parse_dummy,
		0x000023D0, 0},
	[IPA_HW_v3_0][IPA_QSB_MAX_WRITES] = {
		ipareg_construct_qsb_max_writes, ipareg_parse_dummy,
		0x00000074, 0},
	[IPA_HW_v3_0][IPA_QSB_MAX_READS] = {
		ipareg_construct_qsb_max_reads, ipareg_parse_dummy,
		0x00000078, 0},
	[IPA_HW_v3_0][IPA_DPS_SEQUENCER_FIRST] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x0001e000, 0},
	[IPA_HW_v3_0][IPA_HPS_SEQUENCER_FIRST] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x0001e080, 0},


	/* IPAv3.1 */
	[IPA_HW_v3_1][IPA_IRQ_SUSPEND_INFO_EE_n] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00003030, 0x1000},
	[IPA_HW_v3_1][IPA_SUSPEND_IRQ_EN_EE_n] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00003034, 0x1000},
	[IPA_HW_v3_1][IPA_SUSPEND_IRQ_CLR_EE_n] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00003038, 0x1000},


	/* IPAv3.5 */
	[IPA_HW_v3_5][IPA_TX_CFG] = {
		ipareg_construct_tx_cfg, ipareg_parse_tx_cfg,
		0x000001FC, 0},
	[IPA_HW_v3_5][IPA_SRC_RSRC_GRP_01_RSRC_TYPE_n] = {
		ipareg_construct_rsrg_grp_xy_v3_5, ipareg_parse_dummy,
		0x00000400, 0x20},
	[IPA_HW_v3_5][IPA_SRC_RSRC_GRP_23_RSRC_TYPE_n] = {
		ipareg_construct_rsrg_grp_xy_v3_5, ipareg_parse_dummy,
		0x00000404, 0x20},
	[IPA_HW_v3_5][IPA_SRC_RSRC_GRP_45_RSRC_TYPE_n] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		-1, 0},
	[IPA_HW_v3_5][IPA_SRC_RSRC_GRP_67_RSRC_TYPE_n] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		-1, 0},
	[IPA_HW_v3_5][IPA_DST_RSRC_GRP_01_RSRC_TYPE_n] = {
		ipareg_construct_rsrg_grp_xy_v3_5, ipareg_parse_dummy,
		0x00000500, 0x20},
	[IPA_HW_v3_5][IPA_DST_RSRC_GRP_23_RSRC_TYPE_n] = {
		ipareg_construct_rsrg_grp_xy_v3_5, ipareg_parse_dummy,
		0x00000504, 0x20},
	[IPA_HW_v3_5][IPA_DST_RSRC_GRP_45_RSRC_TYPE_n] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		-1, 0},
	[IPA_HW_v3_5][IPA_DST_RSRC_GRP_67_RSRC_TYPE_n] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		-1, 0},
	[IPA_HW_v3_5][IPA_ENDP_INIT_RSRC_GRP_n] = {
		ipareg_construct_endp_init_rsrc_grp_n_v3_5,
		ipareg_parse_dummy,
		0x00000838, 0x70},
	[IPA_HW_v3_5][IPA_RX_HPS_CLIENTS_MIN_DEPTH_0] = {
		ipareg_construct_rx_hps_clients_depth0_v3_5,
		ipareg_parse_dummy,
		0x000023C4, 0},
	[IPA_HW_v3_5][IPA_RX_HPS_CLIENTS_MIN_DEPTH_1] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		-1, 0},
	[IPA_HW_v3_5][IPA_RX_HPS_CLIENTS_MAX_DEPTH_0] = {
		ipareg_construct_rx_hps_clients_depth0_v3_5,
		ipareg_parse_dummy,
		0x000023CC, 0},
	[IPA_HW_v3_5][IPA_RX_HPS_CLIENTS_MAX_DEPTH_1] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		-1, 0},
	[IPA_HW_v3_5][IPA_SPARE_REG_1] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00002780, 0},
	[IPA_HW_v3_5][IPA_SPARE_REG_2] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00002784, 0},
	[IPA_HW_v3_5][IPA_IDLE_INDICATION_CFG] = {
		ipareg_construct_idle_indication_cfg, ipareg_parse_dummy,
		0x00000220, 0},
	[IPA_HW_v3_5][IPA_HPS_FTCH_ARB_QUEUE_WEIGHT] = {
		ipareg_construct_hps_queue_weights,
		ipareg_parse_hps_queue_weights, 0x000005a4, 0},

	/* IPAv4.0 */
	[IPA_HW_v4_0][IPA_ENDP_INIT_CTRL_n] = {
		ipareg_construct_endp_init_ctrl_n_v4_0, ipareg_parse_dummy,
		0x00000800, 0x70 },
	[IPA_HW_v4_0][IPA_TX_CFG] = {
		ipareg_construct_tx_cfg_v4_0, ipareg_parse_tx_cfg_v4_0,
		0x000001FC, 0},
	[IPA_HW_v4_0][IPA_DEBUG_CNT_REG_n] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		-1, 0},
	[IPA_HW_v4_0][IPA_DEBUG_CNT_CTRL_n] = {
		ipareg_construct_debug_cnt_ctrl_n, ipareg_parse_dummy,
		-1, 0},
	[IPA_HW_v4_0][IPA_QCNCM] = {
		ipareg_construct_qcncm, ipareg_parse_qcncm,
		-1, 0},
	[IPA_HW_v4_0][IPA_SINGLE_NDP_MODE] = {
		ipareg_construct_single_ndp_mode, ipareg_parse_single_ndp_mode,
		-1, 0},
	[IPA_HW_v4_0][IPA_QSB_MAX_READS] = {
		ipareg_construct_qsb_max_reads_v4_0, ipareg_parse_dummy,
		0x00000078, 0},
	[IPA_HW_v4_0][IPA_FILT_ROUT_HASH_FLUSH] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x0000014c, 0},
	[IPA_HW_v4_0][IPA_STATE_AGGR_ACTIVE] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x000000b4, 0},
	[IPA_HW_v4_0][IPA_ENDP_INIT_ROUTE_n] = {
		ipareg_construct_endp_init_route_n, ipareg_parse_dummy,
		-1, 0},
	[IPA_HW_v4_0][IPA_ENDP_STATUS_n] = {
		ipareg_construct_endp_status_n_v4_0, ipareg_parse_dummy,
		0x00000840, 0x70},
	[IPA_HW_v4_0][IPA_CLKON_CFG] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00000044, 0},
	[IPA_HW_v4_0][IPA_ENDP_INIT_CONN_TRACK_n] = {
		ipareg_construct_endp_init_conn_track_n,
		ipareg_parse_dummy,
		0x00000850, 0x70},
	[IPA_HW_v4_0][IPA_STAT_QUOTA_BASE_n] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00000700, 0x4 },
	[IPA_HW_v4_0][IPA_STAT_QUOTA_MASK_n] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00000708, 0x4 },
	[IPA_HW_v4_0][IPA_STAT_TETHERING_BASE_n] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00000710, 0x4 },
	[IPA_HW_v4_0][IPA_STAT_TETHERING_MASK_n] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00000718, 0x4 },
	[IPA_HW_v4_0][IPA_STAT_FILTER_IPV4_BASE] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00000720, 0x0 },
	[IPA_HW_v4_0][IPA_STAT_FILTER_IPV6_BASE] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00000724, 0x0 },
	[IPA_HW_v4_0][IPA_STAT_ROUTER_IPV4_BASE] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00000728, 0x0 },
	[IPA_HW_v4_0][IPA_STAT_ROUTER_IPV6_BASE] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x0000072C, 0x0 },
	[IPA_HW_v4_0][IPA_STAT_FILTER_IPV4_START_ID] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00000730, 0x0 },
	[IPA_HW_v4_0][IPA_STAT_FILTER_IPV6_START_ID] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00000734, 0x0 },
	[IPA_HW_v4_0][IPA_STAT_ROUTER_IPV4_START_ID] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00000738, 0x0 },
	[IPA_HW_v4_0][IPA_STAT_ROUTER_IPV6_START_ID] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x0000073C, 0x0 },
	[IPA_HW_v4_0][IPA_STAT_FILTER_IPV4_END_ID] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00000740, 0x0 },
	[IPA_HW_v4_0][IPA_STAT_FILTER_IPV6_END_ID] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00000744, 0x0 },
	[IPA_HW_v4_0][IPA_STAT_ROUTER_IPV4_END_ID] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00000748, 0x0 },
	[IPA_HW_v4_0][IPA_STAT_ROUTER_IPV6_END_ID] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x0000074C, 0x0 },
	[IPA_HW_v4_0][IPA_STAT_DROP_CNT_BASE_n] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00000750, 0x4 },
	[IPA_HW_v4_0][IPA_STAT_DROP_CNT_MASK_n] = {
		ipareg_construct_dummy, ipareg_parse_dummy,
		0x00000758, 0x4 },
};

/*
 * ipahal_reg_init() - Build the registers information table
 *  See ipahal_reg_objs[][] comments
 *
 * Note: As global variables are initialized with zero, any un-overridden
 *  register entry will be zero. By this we recognize them.
 */
int ipahal_reg_init(enum ipa_hw_type ipa_hw_type)
{
	int i;
	int j;
	struct ipahal_reg_obj zero_obj;

	IPAHAL_DBG_LOW("Entry - HW_TYPE=%d\n", ipa_hw_type);

	if ((ipa_hw_type < 0) || (ipa_hw_type >= IPA_HW_MAX)) {
		IPAHAL_ERR("invalid IPA HW type (%d)\n", ipa_hw_type);
		return -EINVAL;
	}

	memset(&zero_obj, 0, sizeof(zero_obj));
	for (i = IPA_HW_v3_0 ; i < ipa_hw_type ; i++) {
		for (j = 0; j < IPA_REG_MAX ; j++) {
			if (!memcmp(&ipahal_reg_objs[i+1][j], &zero_obj,
				sizeof(struct ipahal_reg_obj))) {
				memcpy(&ipahal_reg_objs[i+1][j],
					&ipahal_reg_objs[i][j],
					sizeof(struct ipahal_reg_obj));
			} else {
				/*
				 * explicitly overridden register.
				 * Check validity
				 */
				if (!ipahal_reg_objs[i+1][j].offset) {
					IPAHAL_ERR(
					  "reg=%s with zero offset ipa_ver=%d\n",
					  ipahal_reg_name_str(j), i+1);
					WARN_ON(1);
				}
				if (!ipahal_reg_objs[i+1][j].construct) {
					IPAHAL_ERR(
					  "reg=%s with NULL construct func ipa_ver=%d\n",
					  ipahal_reg_name_str(j), i+1);
					WARN_ON(1);
				}
				if (!ipahal_reg_objs[i+1][j].parse) {
					IPAHAL_ERR(
					  "reg=%s with NULL parse func ipa_ver=%d\n",
					  ipahal_reg_name_str(j), i+1);
					WARN_ON(1);
				}
			}
		}
	}

	return 0;
}

/*
 * ipahal_reg_name_str() - returns string that represent the register
 * @reg_name: [in] register name
 */
const char *ipahal_reg_name_str(enum ipahal_reg_name reg_name)
{
	if (reg_name < 0 || reg_name >= IPA_REG_MAX) {
		IPAHAL_ERR("requested name of invalid reg=%d\n", reg_name);
		return "Invalid Register";
	}

	return ipareg_name_to_str[reg_name];
}

/*
 * ipahal_read_reg_n() - Get n parameterized reg value
 */
u32 ipahal_read_reg_n(enum ipahal_reg_name reg, u32 n)
{
	u32 offset;

	if (reg >= IPA_REG_MAX) {
		IPAHAL_ERR("Invalid register reg=%u\n", reg);
		WARN_ON(1);
		return -EFAULT;
	}

	IPAHAL_DBG_LOW("read from %s n=%u\n",
		ipahal_reg_name_str(reg), n);

	offset = ipahal_reg_objs[ipahal_ctx->hw_type][reg].offset;
	if (offset == -1) {
		IPAHAL_ERR("Read access to obsolete reg=%s\n",
			ipahal_reg_name_str(reg));
		WARN_ON(1);
		return -EFAULT;
	}
	offset += ipahal_reg_objs[ipahal_ctx->hw_type][reg].n_ofst * n;
	return ioread32(ipahal_ctx->base + offset);
}

/*
 * ipahal_read_reg_mn() - Get mn parameterized reg value
 */
u32 ipahal_read_reg_mn(enum ipahal_reg_name reg, u32 m, u32 n)
{
	u32 offset;

	if (reg >= IPA_REG_MAX) {
		IPAHAL_ERR("Invalid register reg=%u\n", reg);
		return -EFAULT;
	}

	IPAHAL_DBG_LOW("read %s m=%u n=%u\n",
		ipahal_reg_name_str(reg), m, n);
	offset = ipahal_reg_objs[ipahal_ctx->hw_type][reg].offset;
	if (offset == -1) {
		IPAHAL_ERR("Read access to obsolete reg=%s\n",
			ipahal_reg_name_str(reg));
		WARN_ON_ONCE(1);
		return -EFAULT;
	}
	/*
	 * Currently there is one register with m and n parameters
	 *	IPA_UC_MAILBOX_m_n. The m value of it is 0x80.
	 * If more such registers will be added in the future,
	 *	we can move the m parameter to the table above.
	 */
	offset += 0x80 * m;
	offset += ipahal_reg_objs[ipahal_ctx->hw_type][reg].n_ofst * n;
	return ioread32(ipahal_ctx->base + offset);
}

/*
 * ipahal_write_reg_mn() - Write to m/n parameterized reg a raw value
 */
void ipahal_write_reg_mn(enum ipahal_reg_name reg, u32 m, u32 n, u32 val)
{
	u32 offset;

	if (reg >= IPA_REG_MAX) {
		IPAHAL_ERR("Invalid register reg=%u\n", reg);
		return;
	}

	IPAHAL_DBG_LOW("write to %s m=%u n=%u val=%u\n",
		ipahal_reg_name_str(reg), m, n, val);
	offset = ipahal_reg_objs[ipahal_ctx->hw_type][reg].offset;
	if (offset == -1) {
		IPAHAL_ERR("Write access to obsolete reg=%s\n",
			ipahal_reg_name_str(reg));
		WARN_ON(1);
		return;
	}
	/*
	 * Currently there is one register with m and n parameters
	 *	IPA_UC_MAILBOX_m_n. The m value of it is 0x80.
	 * If more such registers will be added in the future,
	 *	we can move the m parameter to the table above.
	 */
	offset +=  0x80 * m;
	offset += ipahal_reg_objs[ipahal_ctx->hw_type][reg].n_ofst * n;
	iowrite32(val, ipahal_ctx->base + offset);
}

/*
 * ipahal_read_reg_n_fields() - Get the parsed value of n parameterized reg
 */
u32 ipahal_read_reg_n_fields(enum ipahal_reg_name reg, u32 n, void *fields)
{
	u32 val = 0;
	u32 offset;

	if (!fields) {
		IPAHAL_ERR("Input error fields=%p\n", fields);
		WARN_ON(1);
		return -EFAULT;
	}

	if (reg >= IPA_REG_MAX) {
		IPAHAL_ERR("Invalid register reg=%u\n", reg);
		WARN_ON(1);
		return -EFAULT;
	}

	IPAHAL_DBG_LOW("read from %s n=%u and parse it\n",
		ipahal_reg_name_str(reg), n);
	offset = ipahal_reg_objs[ipahal_ctx->hw_type][reg].offset;
	if (offset == -1) {
		IPAHAL_ERR("Read access to obsolete reg=%s\n",
			ipahal_reg_name_str(reg));
		WARN_ON(1);
		return -EFAULT;
	}
	offset += ipahal_reg_objs[ipahal_ctx->hw_type][reg].n_ofst * n;
	val = ioread32(ipahal_ctx->base + offset);
	ipahal_reg_objs[ipahal_ctx->hw_type][reg].parse(reg, fields, val);

	return val;
}

/*
 * ipahal_write_reg_n_fields() - Write to n parameterized reg a prased value
 */
void ipahal_write_reg_n_fields(enum ipahal_reg_name reg, u32 n,
		const void *fields)
{
	u32 val = 0;
	u32 offset;

	if (!fields) {
		IPAHAL_ERR("Input error fields=%p\n", fields);
		return;
	}

	if (reg >= IPA_REG_MAX) {
		IPAHAL_ERR("Invalid register reg=%u\n", reg);
		return;
	}

	IPAHAL_DBG_LOW("write to %s n=%u after constructing it\n",
		ipahal_reg_name_str(reg), n);
	offset = ipahal_reg_objs[ipahal_ctx->hw_type][reg].offset;
	if (offset == -1) {
		IPAHAL_ERR("Write access to obsolete reg=%s\n",
			ipahal_reg_name_str(reg));
		WARN_ON(1);
		return;
	}
	offset += ipahal_reg_objs[ipahal_ctx->hw_type][reg].n_ofst * n;
	ipahal_reg_objs[ipahal_ctx->hw_type][reg].construct(reg, fields, &val);

	iowrite32(val, ipahal_ctx->base + offset);
}

/*
 * Get the offset of a m/n parameterized register
 */
u32 ipahal_get_reg_mn_ofst(enum ipahal_reg_name reg, u32 m, u32 n)
{
	u32 offset;

	if (reg >= IPA_REG_MAX) {
		IPAHAL_ERR("Invalid register reg=%u\n", reg);
		WARN_ON(1);
		return -EFAULT;
	}

	IPAHAL_DBG_LOW("get offset of %s m=%u n=%u\n",
		ipahal_reg_name_str(reg), m, n);
	offset = ipahal_reg_objs[ipahal_ctx->hw_type][reg].offset;
	if (offset == -1) {
		IPAHAL_ERR("Access to obsolete reg=%s\n",
			ipahal_reg_name_str(reg));
		WARN_ON(1);
		return -EFAULT;
	}
	/*
	 * Currently there is one register with m and n parameters
	 *	IPA_UC_MAILBOX_m_n. The m value of it is 0x80.
	 * If more such registers will be added in the future,
	 *	we can move the m parameter to the table above.
	 */
	offset +=  0x80 * m;
	offset += ipahal_reg_objs[ipahal_ctx->hw_type][reg].n_ofst * n;

	return offset;
}

u32 ipahal_get_reg_base(void)
{
	return 0x00040000;
}


/*
 * Specific functions
 * These functions supply specific register values for specific operations
 *  that cannot be reached by generic functions.
 * E.g. To disable aggregation, need to write to specific bits of the AGGR
 *  register. The other bits should be untouched. This oeprate is very specific
 *  and cannot be generically defined. For such operations we define these
 *  specific functions.
 */

void ipahal_get_disable_aggr_valmask(struct ipahal_reg_valmask *valmask)
{
	if (!valmask) {
		IPAHAL_ERR("Input error\n");
		return;
	}

	valmask->val = (1 & IPA_ENDP_INIT_AGGR_n_AGGR_FORCE_CLOSE_BMSK) <<
		IPA_ENDP_INIT_AGGR_n_AGGR_FORCE_CLOSE_SHFT;
	valmask->mask = IPA_ENDP_INIT_AGGR_n_AGGR_FORCE_CLOSE_BMSK <<
		IPA_ENDP_INIT_AGGR_n_AGGR_FORCE_CLOSE_SHFT;

	valmask->val |= ((0 & IPA_ENDP_INIT_AGGR_n_AGGR_EN_BMSK) <<
		IPA_ENDP_INIT_AGGR_n_AGGR_EN_SHFT);
	valmask->mask |= ((IPA_ENDP_INIT_AGGR_n_AGGR_EN_BMSK <<
		IPA_ENDP_INIT_AGGR_n_AGGR_EN_SHFT));
}

u32 ipahal_aggr_get_max_byte_limit(void)
{
	return
		IPA_ENDP_INIT_AGGR_n_AGGR_BYTE_LIMIT_BMSK >>
		IPA_ENDP_INIT_AGGR_n_AGGR_BYTE_LIMIT_SHFT;
}

u32 ipahal_aggr_get_max_pkt_limit(void)
{
	return
		IPA_ENDP_INIT_AGGR_n_AGGR_PKT_LIMIT_BMSK >>
		IPA_ENDP_INIT_AGGR_n_AGGR_PKT_LIMIT_SHFT;
}

void ipahal_get_aggr_force_close_valmask(int ep_idx,
	struct ipahal_reg_valmask *valmask)
{
	u32 shft;
	u32 bmsk;

	if (!valmask) {
		IPAHAL_ERR("Input error\n");
		return;
	}

	memset(valmask, 0, sizeof(struct ipahal_reg_valmask));

	if (ipahal_ctx->hw_type <= IPA_HW_v3_1) {
		shft = IPA_AGGR_FORCE_CLOSE_AGGR_FORCE_CLOSE_PIPE_BITMAP_SHFT;
		bmsk = IPA_AGGR_FORCE_CLOSE_AGGR_FORCE_CLOSE_PIPE_BITMAP_BMSK;
	} else if (ipahal_ctx->hw_type <= IPA_HW_v3_5_1) {
		shft =
		IPA_AGGR_FORCE_CLOSE_AGGR_FORCE_CLOSE_PIPE_BITMAP_SHFT_V3_5;
		bmsk =
		IPA_AGGR_FORCE_CLOSE_AGGR_FORCE_CLOSE_PIPE_BITMAP_BMSK_V3_5;
	} else {
		shft =
		IPA_AGGR_FORCE_CLOSE_AGGR_FORCE_CLOSE_PIPE_BITMAP_SHFT_V4_0;
		bmsk =
		IPA_AGGR_FORCE_CLOSE_AGGR_FORCE_CLOSE_PIPE_BITMAP_BMSK_V4_0;
	}

	if (ep_idx > (sizeof(valmask->val) * 8 - 1)) {
		IPAHAL_ERR("too big ep_idx %d\n", ep_idx);
		ipa_assert();
		return;
	}
	IPA_SETFIELD_IN_REG(valmask->val, 1 << ep_idx, shft, bmsk);
	valmask->mask = bmsk << shft;
}

void ipahal_get_fltrt_hash_flush_valmask(
	struct ipahal_reg_fltrt_hash_flush *flush,
	struct ipahal_reg_valmask *valmask)
{
	if (!flush || !valmask) {
		IPAHAL_ERR("Input error: flush=%p ; valmask=%p\n",
			flush, valmask);
		return;
	}

	memset(valmask, 0, sizeof(struct ipahal_reg_valmask));

	if (flush->v6_rt)
		valmask->val |=
			(1<<IPA_FILT_ROUT_HASH_FLUSH_IPv6_ROUT_SHFT);
	if (flush->v6_flt)
		valmask->val |=
			(1<<IPA_FILT_ROUT_HASH_FLUSH_IPv6_FILT_SHFT);
	if (flush->v4_rt)
		valmask->val |=
			(1<<IPA_FILT_ROUT_HASH_FLUSH_IPv4_ROUT_SHFT);
	if (flush->v4_flt)
		valmask->val |=
			(1<<IPA_FILT_ROUT_HASH_FLUSH_IPv4_FILT_SHFT);

	valmask->mask = valmask->val;
}
