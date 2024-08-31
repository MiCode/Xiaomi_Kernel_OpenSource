/*
 * Infineon Technologies OUI and vendor specific assignments
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

#ifndef IFX_VENDOR_H
#define IFX_VENDOR_H

/*
 * This file is a registry of identifier assignments from the Infineon
 * OUI 00:03:19 for purposes other than MAC address assignment. New identifiers
 * can be assigned through normal review process for changes to the upstream
 * hostap.git repository.
 */
#define OUI_IFX		0x000319

/*
 * enum ifx_nl80211_vendor_subcmds - IFX nl80211 vendor command identifiers
 *
 * @IFX_VENDOR_SCMD_UNSPEC: Reserved value 0
 *
 * @IFX_VENDOR_SCMD_FRAMEBURST: Vendor command to enable/disable Frameburst
 *
 * @IFX_VENDOR_SCMD_MUEDCA_OPT_ENABLE: Vendor command to enable/disable HE MU-EDCA opt
 *
 * @IFX_VENDOR_SCMD_LDPC_CAP: Vendor command enable/disable LDPC Capability
 *
 * @IFX_VENDOR_SCMD_AMSDU: Vendor command to enable/disable AMSDU on all the TID queues
 *
 * @IFX_VENDOR_SCMD_TWT: Vendor subcommand to configure TWT
 *	Uses attributes defined in enum ifx_vendor_attr_twt.
 *
 * @IFX_VENDOR_SCMD_OCE_ENABLE: Vendor command to enable/disable OCE Capability
 *
 * @IFX_VENDOR_SCMD_RANDMAC: Vendor command to enable/disable RANDMAC Capability
 *
 * @IFX_VENDOR_SCMD_WNM: Vendor command to do WNM relatives
 *
 * @IFX_VENDOR_SCMD_MAX: This acts as a the tail of cmds list.
 *      Make sure it located at the end of the list.
 */
enum ifx_nl80211_vendor_subcmds {
	/*
	 * TODO: IFX Vendor subcmd enum IDs between 1-10 are reserved
	 * to be be filled later with BRCM Vendor subcmds that are
	 * already used by IFX.
	 */
	IFX_VENDOR_SCMD_UNSPEC		= 0,
	/* Reserved 1-5 */
	IFX_VENDOR_SCMD_FRAMEBURST	= 6,
	/* Reserved 7-9 */
#ifdef P2P_RAND
	IFX_VENDOR_SCMD_SET_P2P_RAND_MAC = 10,
#endif /* P2P_RAND */
	IFX_VENDOR_SCMD_MUEDCA_OPT_ENABLE = 11,
	IFX_VENDOR_SCMD_LDPC_CAP	= 12,
	IFX_VENDOR_SCMD_AMSDU		= 13,
	IFX_VENDOR_SCMD_TWT		= 14,
	IFX_VENDOR_SCMD_OCE_ENABLE      = 15,
	/* Reserved 16 */
	IFX_VENDOR_SCMD_RANDMAC         = 17,
	IFX_VENDOR_SCMD_ACS             = 18,
	IFX_VENDOR_SCMD_WNM             = 25,
	IFX_VENDOR_SCMD_MAX
};

/*
 * enum ifx_vendor_attr - IFX nl80211 vendor attributes
 *
 * @IFX_VENDOR_ATTR_UNSPEC: Reserved value 0
 *
 * @IFX_VENDOR_ATTR_MAX: This acts as a the tail of attrs list.
 *      Make sure it located at the end of the list.
 */
enum ifx_vendor_attr {
	/*
	 * TODO: IFX Vendor attr enum IDs between 0-10 are reserved
	 * to be filled later with BRCM Vendor attrs that are
	 * already used by IFX.
	 */
	IFX_VENDOR_ATTR_UNSPEC		= 0,
	IFX_VENDOR_ATTR_PAD		= 1,
	IFX_VENDOR_ATTR_MAC_ADDR        = 2,
	/* Reserved 1-10 */
	IFX_VENDOR_ATTR_MAX		= 11
};

/*
 * enum ifx_vendor_attr_twt - Attributes for the TWT vendor command
 *
 * @IFX_VENDOR_ATTR_TWT_UNSPEC: Reserved value 0
 *
 * @IFX_VENDOR_ATTR_TWT_OPER: To specify the type of TWT operation
 *	to be performed. Uses attributes defined in enum ifx_twt_oper.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAMS: Nester attributes representing the
 *	parameters configured for TWT. These parameters are defined in
 *	the enum ifx_vendor_attr_twt_param.
 *
 * @IFX_VENDOR_ATTR_TWT_MAX: This acts as a the tail of cmds list.
 *      Make sure it located at the end of the list.
 */
enum ifx_vendor_attr_twt {
	IFX_VENDOR_ATTR_TWT_UNSPEC,
	IFX_VENDOR_ATTR_TWT_OPER,
	IFX_VENDOR_ATTR_TWT_PARAMS,
	IFX_VENDOR_ATTR_TWT_MAX
};

/*
 * enum ifx_twt_oper - TWT operation to be specified using the vendor
 * attribute IFX_VENDOR_ATTR_TWT_OPER
 *
 * @IFX_TWT_OPER_UNSPEC: Reserved value 0
 *
 * @IFX_TWT_OPER_SETUP: Setup a TWT session. Required parameters are
 *	obtained through the nested attrs under IFX_VENDOR_ATTR_TWT_PARAMS.
 *
 * @IFX_TWT_OPER_TEARDOWN: Teardown the already negotiated TWT session.
 *	Required parameters are obtained through the nested attrs under
 *	IFX_VENDOR_ATTR_TWT_PARAMS.
 *
 * @IFX_TWT_OPER_MAX: This acts as a the tail of the list.
 *      Make sure it located at the end of the list.
 */
enum ifx_twt_oper {
	IFX_TWT_OPER_UNSPEC,
	IFX_TWT_OPER_SETUP,
	IFX_TWT_OPER_TEARDOWN,
	IFX_TWT_OPER_MAX
};

/*
 * enum ifx_vendor_attr_twt_param - TWT parameters
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_UNSPEC: Reserved value 0
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_NEGO_TYPE: Specifies the type of Negotiation to be
 *	done during Setup. The four possible types are
 *	0 - Individual TWT Negotiation
 *	1 - Wake TBTT Negotiation
 *	2 - Broadcast TWT in Beacon
 *	3 - Broadcast TWT Membership Negotiation
 *
 *	The possible values are defined in the enum ifx_twt_param_nego_type
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_SETUP_CMD_TYPE: Specifies the type of TWT Setup frame
 *	when sent by the TWT Requesting STA
 *	0 - Request
 *	1 - Suggest
 *	2 - Demand
 *
 *	when sent by the TWT Responding STA.
 *	3 - Grouping
 *	4 - Accept
 *	5 - Alternate
 *	6 - Dictate
 *	7 - Reject
 *
 *	The possible values are defined in the enum ifx_twt_oper_setup_cmd_type.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_DIALOG_TOKEN: Dialog Token used by the TWT Requesting STA to
 *	identify the TWT Setup request/response transaction.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_WAKE_TIME: Target Wake Time.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_WAKE_TIME_OFFSET: Target Wake Time Offset.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_MIN_WAKE_DURATION: Nominal Minimum TWT Wake Duration.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_WAKE_INTVL_EXPONENT: TWT Wake Interval Exponent.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_WAKE_INTVL_MANTISSA: TWT Wake Interval Mantissa.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_REQUESTOR: Specify this is a TWT Requesting / Responding STA.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_TRIGGER: Specify Trigger based / Non-Trigger based TWT Session.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_IMPLICIT: Specify Implicit / Explicit TWT session.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_FLOW_TYPE: Specify Un-Announced / Announced TWT session.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_FLOW_ID: Flow ID of an iTWT session.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_BCAST_TWT_ID: Brocast TWT ID of a bTWT session.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_PROTECTION: Specifies whether Tx within SP is protected.
 *	Set to 1 to indicate that TXOPs within the TWT SPs shall be initiated
 *	with a NAV protection mechanism, such as (MU) RTS/CTS or CTS-to-self frame;
 *	otherwise, it shall set it to 0.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_CHANNEL: TWT channel field which is set to 0, unless
 * 	the HE STA sets up a subchannel selective transmission operation.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_TWT_INFO_FRAME_DISABLED: TWT Information frame RX handing
 *	disabled / enabled.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_MIN_WAKE_DURATION_UNIT: Nominal Minimum TWT Wake Duration
 *	Unit. 0 represents unit in "256 usecs" and 1 represents unit in "TUs".
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_TEARDOWN_ALL_TWT: Teardown all negotiated TWT sessions.
 *
 * @IFX_VENDOR_ATTR_TWT_PARAM_MAX: This acts as a the tail of the list.
 *      Make sure it located at the end of the list.
 */
enum ifx_vendor_attr_twt_param {
	IFX_VENDOR_ATTR_TWT_PARAM_UNSPEC,
	IFX_VENDOR_ATTR_TWT_PARAM_NEGO_TYPE,
	IFX_VENDOR_ATTR_TWT_PARAM_SETUP_CMD_TYPE,
	IFX_VENDOR_ATTR_TWT_PARAM_DIALOG_TOKEN,
	IFX_VENDOR_ATTR_TWT_PARAM_WAKE_TIME,
	IFX_VENDOR_ATTR_TWT_PARAM_WAKE_TIME_OFFSET,
	IFX_VENDOR_ATTR_TWT_PARAM_MIN_WAKE_DURATION,
	IFX_VENDOR_ATTR_TWT_PARAM_WAKE_INTVL_EXPONENT,
	IFX_VENDOR_ATTR_TWT_PARAM_WAKE_INTVL_MANTISSA,
	IFX_VENDOR_ATTR_TWT_PARAM_REQUESTOR,
	IFX_VENDOR_ATTR_TWT_PARAM_TRIGGER,
	IFX_VENDOR_ATTR_TWT_PARAM_IMPLICIT,
	IFX_VENDOR_ATTR_TWT_PARAM_FLOW_TYPE,
	IFX_VENDOR_ATTR_TWT_PARAM_FLOW_ID,
	IFX_VENDOR_ATTR_TWT_PARAM_BCAST_TWT_ID,
	IFX_VENDOR_ATTR_TWT_PARAM_PROTECTION,
	IFX_VENDOR_ATTR_TWT_PARAM_CHANNEL,
	IFX_VENDOR_ATTR_TWT_PARAM_TWT_INFO_FRAME_DISABLED,
	IFX_VENDOR_ATTR_TWT_PARAM_MIN_WAKE_DURATION_UNIT,
	IFX_VENDOR_ATTR_TWT_PARAM_TEARDOWN_ALL_TWT,
	IFX_VENDOR_ATTR_TWT_PARAM_MAX
};

enum ifx_twt_param_nego_type {
	IFX_TWT_PARAM_NEGO_TYPE_INVALID			= -1,
	IFX_TWT_PARAM_NEGO_TYPE_ITWT			= 0,
	IFX_TWT_PARAM_NEGO_TYPE_WAKE_TBTT		= 1,
	IFX_TWT_PARAM_NEGO_TYPE_BTWT_IE_BCN		= 2,
	IFX_TWT_PARAM_NEGO_TYPE_BTWT			= 3,
	IFX_TWT_PARAM_NEGO_TYPE_MAX			= 4
};

enum ifx_twt_oper_setup_cmd_type {
	IFX_TWT_OPER_SETUP_CMD_TYPE_INVALID	= -1,
	IFX_TWT_OPER_SETUP_CMD_TYPE_REQUEST	= 0,
	IFX_TWT_OPER_SETUP_CMD_TYPE_SUGGEST	= 1,
	IFX_TWT_OPER_SETUP_CMD_TYPE_DEMAND	= 2,
	IFX_TWT_OPER_SETUP_CMD_TYPE_GROUPING	= 3,
	IFX_TWT_OPER_SETUP_CMD_TYPE_ACCEPT	= 4,
	IFX_TWT_OPER_SETUP_CMD_TYPE_ALTERNATE	= 5,
	IFX_TWT_OPER_SETUP_CMD_TYPE_DICTATE	= 6,
	IFX_TWT_OPER_SETUP_CMD_TYPE_REJECT	= 7,
	IFX_TWT_OPER_SETUP_CMD_TYPE_MAX		= 8
};

/*
 * enum ifx_vendor_attr_wnm - Attributes for WNM vendor command
 *
 * @IFX_VENDOR_ATTR_WNM_UNSPEC: Reserved value 0
 *
 * @IFX_VENDOR_ATTR_WNM_CMD: To specify the type of WNM operation
 *	to be performed. Uses command type defined in enum ifx_wnm_config_cmd_type.
 *
 * @IFX_VENDOR_ATTR_WNM_PARAMS: To specify the type of WNM operation
 *	to be performed. Uses parameters defined in enum ifx_vendor_attr_wnm_maxidle_param.
 *
 * @IFX_VENDOR_ATTR_WNM_MAX: This acts as a the tail of the list.
 *      Make sure it located at the end of the list.
 */
enum ifx_vendor_attr_wnm {
	IFX_VENDOR_ATTR_WNM_UNSPEC,
	IFX_VENDOR_ATTR_WNM_CMD,
	IFX_VENDOR_ATTR_WNM_PARAMS,
	IFX_VENDOR_ATTR_WNM_MAX
};

/*
 * enum ifx_wnm_config_cmd_type - WNM command
 *
 * @IFX_WNM_CONFIG_CMD_TYPE_INVALID: Invalid command as -1
 *
 * @IFX_WNM_CONFIG_CMD_IOV_WNM_MAXIDLE: Set/Get maxidle.
 *	set: need to give parameters 1.period 2.option
 *	get: get maxidle value from fw
 *
 * @IFX_WNM_CONFIG_CMD_TYPE_MAX: This acts as a the tail of the list.
 *      Make sure it located at the end of the list.
 *
 */
enum ifx_wnm_config_cmd_type {
	IFX_WNM_CONFIG_CMD_TYPE_INVALID				= -1,
	//align internal definition
	/* reserved CMD 1 */
	IFX_WNM_CONFIG_CMD_IOV_WNM_MAXIDLE			= 2,
	/* reserved CMD 3~33 */
	IFX_WNM_CONFIG_CMD_TYPE_MAX
};

/*
 * enum ifx_vendor_attr_wnm_maxidle_param - WNM parameters
 *
 * @IFX_VENDOR_ATTR_WNM_MAXIDLE_PARAM_UNSPEC: Reserved value 0
 *
 * @IFX_VENDOR_ATTR_WNM_MAXIDLE_PARAM_GET_INFO: get value or not.
 *	TRUE(1): get value
 *	False:(0): set value
 *
 * @IFX_VENDOR_ATTR_WNM_MAXIDLE_PARAM_IDLE_PERIOD: idle period.
 *
 * @IFX_VENDOR_ATTR_WNM_MAXIDLE_PARAM_PROTECTION_OPT: option.
 *	TRUE(1): protected
 *	False:(0): unprotected
 *
 * @IFX_VENDOR_ATTR_WNM_MAXIDLE_PARAM_MAX: This acts as a the tail of the list.
 *      Make sure it located at the end of the list.
 *
 */
enum ifx_vendor_attr_wnm_maxidle_param {
	IFX_VENDOR_ATTR_WNM_MAXIDLE_PARAM_UNSPEC,
	IFX_VENDOR_ATTR_WNM_MAXIDLE_PARAM_GET_INFO,
	IFX_VENDOR_ATTR_WNM_MAXIDLE_PARAM_IDLE_PERIOD,
	IFX_VENDOR_ATTR_WNM_MAXIDLE_PARAM_PROTECTION_OPT,
	IFX_VENDOR_ATTR_WNM_MAXIDLE_PARAM_MAX
};

#ifdef WL_SUPPORT_ACS_OFFLOAD
enum wl_vendor_attr_acs_offload {
	IFX_VENDOR_ATTR_ACS_CHANNEL_INVALID = 0,
	IFX_VENDOR_ATTR_ACS_PRIMARY_FREQ,
	IFX_VENDOR_ATTR_ACS_SECONDARY_FREQ,
	IFX_VENDOR_ATTR_ACS_VHT_SEG0_CENTER_CHANNEL,
	IFX_VENDOR_ATTR_ACS_VHT_SEG1_CENTER_CHANNEL,
	IFX_VENDOR_ATTR_ACS_HW_MODE,
	IFX_VENDOR_ATTR_ACS_HT_ENABLED,
	IFX_VENDOR_ATTR_ACS_HT40_ENABLED,
	IFX_VENDOR_ATTR_ACS_VHT_ENABLED,
	IFX_VENDOR_ATTR_ACS_CHWIDTH,
	IFX_VENDOR_ATTR_ACS_CH_LIST,
	IFX_VENDOR_ATTR_ACS_FREQ_LIST,
	IFX_VENDOR_ATTR_ACS_LAST
};
#endif /* WL_SUPPORT_ACS_OFFLOAD */

/*
 * enum ifx_nl80211_vendor_events - IFX nl80211 vendor event identifiers
 *
 * @IFX_VENDOR_EVENT_XR_CONNECTED: Vendor event to send XR Connected interface name
 *
 * @IFX_VENDOR_EVENT_XR_STA_MAC_CHANGE: Vendor event to inform XR STA MAC change
 */
enum ifx_nl80211_vendor_events {
	/*
	 * TODO: IFX Vendor events enum IDs between 0-43 are reserved
	 * to be be filled later with BRCM and Google Vendor events that are
	 * already used by IFX.
	 */
	IFX_VENDOR_EVENT_XR_CONNECTED = 44,
	IFX_VENDOR_EVENT_XR_STA_MAC_CHANGE = 45
};
#endif /* IFX_VENDOR_H */
