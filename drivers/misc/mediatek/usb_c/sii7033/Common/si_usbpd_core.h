/*
*Copyright (C) 2013-2014 Silicon Image, Inc.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation version 2.
* This program is distributed AS-IS WITHOUT ANY WARRANTY of any
* kind, whether express or implied; INCLUDING without the implied warranty
* of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
* See the GNU General Public License for more details at
* http://www.gnu.org/licenses/gpl-2.0.html.
*/
#ifndef SI_USBPD_CORE_H
#define SI_USBPD_CORE_H
#include <Wrap.h>

/*Sink Electrical timing values*/
#define tNewSnkRevertToSrc		90
#define tNewSrc				275
#define tSnkHardResetPrepare		15
#define tSnkNewPower			15
#define tSnkRecover			150
#define tSnkStdby			15
#define tSnkTransition			35
#define T_SWAPRECOVER			1
#define tTurnOnImpliedSink		15

/*Pd Stack timers*/
#define tHardReset		5
#define tHardResetComplete	5
#define tNoResponse		5500
#define tPSSourceOff		920
#define tPSSourceOn		480
#define tPSTransition		550
#define tReceiverResponse	15
#define tRemoveVBUS		920
#define tSenderResponse		30
#define tSendSourceCap		2000	/*2 sec */
#define tSinkActivity		150	/*150 ms */
#define tSinkRequest		100	/*ms */
#define tSinkWaitCap		2500	/*2.5 s */
#define tSoftReset		5	/*ms */
#define tSourceActivity		50	/*50 ms */
#define tSwapSinkReady		15	/*ms */
#define tSwapSourceStart	20	/*ms */
#define tTypeCSendSourceCap	200	/*200 ms */
#define tTypeCSinkWaitCap	250	/*250 ms */
#define tVCONNSourceOff		25	/*ms */
#define tVCONNSourceOn		100	/*ms */
#define tVDMBusy		100	/*ms */
#define tVDMEnterMode		25	/*ms */
#define tVDMExitMode		25	/*ms */
#define tVDMReceiverResponse	15	/*ms */
#define tVDMSenderResponse	30	/*30 ms */
#define tVDMWaitModeEntry	100	/*ms */
#define tVDMWaitModeExit	100	/*ms */
#define tBISTCOUNT		60	/*ms */

#define HardResetCompleteTimer		tHardResetComplete
#define NoResponseTimer			tNoResponse
#define PSSourceOffTimer		tPSSourceOff
#define PSSourceOnTimer			tPSSourceOn
#define PSTransitionTimer		tPSTransition
#define SenderResponseTimer		tSenderResponse
#define SinkActivityTimer		tSinkActivity
#define SinkRequestTimer		tSinkRequest
#define SinkWaitCapTimer		tSinkWaitCap
/*#define SinkWaitCapTimer		tTypeCSinkWaitCap*/
#define SourceActivityTimer		tSourceActivity
#define SourceCapabilityTimer		tSendSourceCap
#define SwapRecoveryTimer		tSwapRecover
#define SwapSourceStartTimer		tSwapSourceStart
#define VCONNOnTimer			tVCONNSourceOn
#define VDMModeEntryTimer		tVDMWaitModeEntry
#define VDMModeExitTimer		tVDMWaitModeExit
#define VDMResponseTimer		tVDMSenderResponse

/*Counter*/
#define N_CAPSCOUNT			50
#define N_HARDRESETCOUNT		2
#define N_MESSAGEIDCOUNT		7
#define N_RETRYCOUNT			3

#define BISTERRORCOUNTER	0xFFFF
#define CAPSCOUNTER		N_CAPSCOUNT
#define HARDRESETCOUNTER	N_HARDRESETCOUNT
#define MESSAGEIDCOUNTER	N_MESSAGEIDCOUNT
#define RETRYCOUNTER		N_RETRYCOUNT
#define OFFSET_1 0
#define OFFSET_2 1


/*Power Delivery protocol header defination*/
#define USBPD_ROLE_SINK		0
#define USBPD_ROLE_SOURCE	1
#define USBPD_ROLE_UFP		0
#define USBPD_ROLE_DFP		1

/*Cable plug*/
#define DFP_UFP			0
#define CABLE_PLUG		1

#define SII_USBPD_MSG_RETRY		3
#define SII_USBPD_MAX_OBJ		7

#define USBPD_REV20		1
#define PD_MSG_BUF		28
#define PD_NO_OF_OBJ		7


/*----------------- msg_header for Power Delivery---------------------*/
#define SII_USBPD_HEADER(msg_type, pwr_role, data_role, msg_id, count) \
	((msg_type) | ((data_role) << 5) | (USBPD_REV20 << 6) | \
	 ((pwr_role) << 8) | ((msg_id) << 9) | ((count) << 12))

#define MSG_CNT(msg_header)  (((msg_header) >> 12) & 7)
#define MSG_TYPE(msg_header) ((msg_header) & 0xF)
/*#define ID(msg_header)   (((msg_header) >> 9) & 7)*/

/*------------------ msg_header for Power Delivery-------------------*/

#define PDO_TYPE_FIXED    (0 << 30)
#define PDO_TYPE_BATTERY  (1 << 30)
#define PDO_TYPE_VARIABLE (2 << 30)
#define PDO_TYPE_MASK     (3 << 30)

#define PDO_FIXED_DUAL_ROLE (1 << 29)	/* Dual role device */
#define PDO_FIXED_SUSPEND   (1 << 28)	/* USB Suspend supported */
#define PDO_FIXED_EXTERNAL  (1 << 27)	/* Externally powered */
#define PDO_FIXED_COMM_CAP  (1 << 26)	/* USB Communications Capable */
#define PDO_FIXED_DATA_SWAP (1 << 25)	/* Data role swap command supported */
#define PDO_FIXED_PEAK_CURR (1 << 20)	/* [21..20] Peak current */
#define SRC_CAP_FIXED_VOLT(voltage)  (((voltage)/50) << 10)
#define SRC_CAP_FIXED_CURR(current)  (((current)/10) << 0)

#define SRC_CAP_FIXED(fixed_supply, drp_pwr, suspend, ext_pwr, \
		usb_com_cable, data_swp, p_cur, vol, curr) \
		(((fixed_supply) << 30) | ((drp_pwr) << 29) | (suspend << 28) \
		 | ((ext_pwr) << 27) | ((usb_com_cable) << 26) \
		 | ((data_swp) << 25) | ((p_cur) << 20) \
		 | SRC_CAP_FIXED_VOLT(vol)  \
		 | SRC_CAP_FIXED_CURR(curr))

#define RDO_OBJ_POS(n)             (((n) & 0x7) << 28)
#define RDO_POS(rdo)               (((rdo) >> 28) & 0x7)
#define RDO_GIVE_BACK              (1 << 27)
#define RDO_CAP_MISMATCH           (1 << 26)
#define RDO_COMM_CAP               (1 << 25)
#define RDO_NO_SUSPEND             (1 << 24)
#define RDO_FIXED_VAR_OP_CURR(ma)  ((((ma) / 10) & 0x3FF) << 10)
#define RDO_FIXED_VAR_MAX_CURR(ma) ((((ma) / 10) & 0x3FF) << 0)

#define RDO_BATT_OP_POWER(mw)      ((((mw) / 250) & 0x3FF) << 10)
#define RDO_BATT_MAX_POWER(mw)     ((((mw) / 250) & 0x3FF) << 10)

#define RDO_FIXED(n, op_ma, max_ma, flags) \
	(RDO_OBJ_POS(n) | (flags) | \
	 RDO_FIXED_VAR_OP_CURR(op_ma) | \
	 RDO_FIXED_VAR_MAX_CURR(max_ma))


#define RDO_BATT(n, op_mw, max_mw, flags) \
	(RDO_OBJ_POS(n) | (flags) | \
	 RDO_BATT_OP_POWER(op_mw) | \
	 RDO_BATT_MAX_POWER(max_mw))

#define PDO_FIXED_VOLT(mv)  (((mv)/50) << 10)	/* Voltage in 50mV units */
#define PDO_FIXED_CURR(ma)  (((ma)/10) << 0)	/* Max current in 10mA units */

#define PDO_FIXED(mv, ma, flags) (PDO_FIXED_VOLT(mv) |\
		PDO_FIXED_CURR(ma) | (flags))


#define PDO_FIXED_FLAGS (PDO_FIXED_DUAL_ROLE | PDO_FIXED_SUSPEND | PDO_FIXED_EXTERNAL | \
		PDO_FIXED_COMM_CAP | PDO_FIXED_DATA_SWAP)

#define SNK_CAP_FIXED_VOLT(voltage)  (((voltage)/50) << 10)
#define SNK_CAP_FIXED_CURR(current)  (((current)/10) << 0)

#define SII_PD_DATA_SNK_CAP_FIXED(fixed_supply, drp_pwr, high_cap, ext_pwr, \
		usb_com_cable, data_swp, p_cur, vol, curr) \
		(((fixed_supply) << 30) | ((drp_pwr) << 29) | (high_cap << 28) \
		| ((ext_pwr) << 27) | ((usb_com_cable) << 26) \
		| ((data_swp) << 25) | ((p_cur) << 20) \
		| SII_PD_DATA_SNK_CAP_FIXED_VOLT(vol) \
		| SII_PD_DATA_SNK_CAP_FIXED_CURR(curr))

#define MAX_PD_PAYLOAD	30
#define PD_OPERATING_POWER_MW		5000
#define RDO_CAP_MISMATCH	(1 << 26)
#define RATED_CURRENT		3000

#define PDO_TYPE_FIXED	(0 << 30)
#define PDO_TYPE_BATTERY	(1 << 30)
#define PDO_TYPE_VARIABLE	(2 << 30)
#define PDO_TYPE_MASK	(3 << 30)

#define PDO_FIXED_DUAL_ROLE (1 << 29)	/* Dual role device */
#define PDO_FIXED_SUSPEND   (1 << 28)	/* USB Suspend supported */
#define PDO_FIXED_EXTERNAL  (1 << 27)	/* Externally powered */
#define PDO_FIXED_COMM_CAP  (1 << 26)	/* USB Communications Capable */
#define PDO_FIXED_DATA_SWAP (1 << 25)	/* Data role swap command supported */
/*#define PDO_FIXED_PEAK_CURR () [21..20] Peak current */
#define PDO_FIXED_VOLT(mv)  (((mv)/50) << 10)	/* Voltage in 50mV units */
#define PDO_FIXED_CURR(ma)  (((ma)/10) << 0)	/* Max current in 10mA units */

#define PDO_FIXED(mv, ma, flags) (PDO_FIXED_VOLT(mv) |\
		PDO_FIXED_CURR(ma) | (flags))

#define PDO_VAR_MAX_VOLT(mv) ((((mv) / 50) & 0x3FF) << 20)
#define PDO_VAR_MIN_VOLT(mv) ((((mv) / 50) & 0x3FF) << 10)
#define PDO_VAR_OP_CURR(ma)  ((((ma) / 10) & 0x3FF) << 0)

#define PDO_VAR(min_mv, max_mv, op_ma) \
	(PDO_VAR_MIN_VOLT(min_mv) | \
	 PDO_VAR_MAX_VOLT(max_mv) | \
	 PDO_VAR_OP_CURR(op_ma)   | \
	 PDO_TYPE_VARIABLE)

#define PD_VDO_VID(vdo)  ((vdo) >> 16)
#define PD_VDO_SVDM(vdo) (((vdo) >> 15) & 1)
#define PD_VDO_OPOS(vdo) (((vdo) >> 8) & 0x7)
#define PD_VDO_CMD(vdo)  ((vdo) & 0x1f)
#define PD_VDO_CMDT(vdo) (((vdo) >> 6) & 0x3)

#define VDO_SVDM_TYPE     (1 << 15)
#define VDO_SVDM_VERS(x)  (x << 13)
#define VDO_OPOS(x)       (x << 8)
#define VDO_CMDT(x)       (x << 6)
#define VDO_OPOS_MASK     VDO_OPOS(0x7)
#define VDO_CMDT_MASK     VDO_CMDT(0x3)

#define CMDT_INIT     0
#define CMDT_RSP_ACK  1
#define CMDT_RSP_NAK  2
#define CMDT_RSP_BUSY 3

#define VDO_SRC_INITIATOR (0 << 5)
#define VDO_SRC_RESPONDER (1 << 5)

#define CMD_DISCOVER_IDENT  1
#define CMD_DISCOVER_SVID   2
#define CMD_DISCOVER_MODES  3
#define CMD_ENTER_MODE      4
#define CMD_EXIT_MODE       5
#define CMD_ATTENTION       6

#define VDO_IDH(usbh, usbd, ptype, is_modal, vid)\
	((usbh) << 31 | (usbd) << 30 | ((ptype) & 0x7) << 27\
	 | (is_modal) << 26 | ((vid) & 0xffff))


#define PD_HOST(vdo)	  (((vdo) >> 31) & 0x1)
#define PD_DEVICE(vdo)    (((vdo) >> 30) & 0x1)
#define PD_MODAL(vdo)	  (((vdo) >> 26) & 0x1)
#define PD_IDH_PTYPE(vdo) (((vdo) >> 27) & 0x7)
#define PD_IDH_VID(vdo)   ((vdo) & 0xffff)

#define MODE_CNT 1
#define OPOS 1

#define VDO_MODE_GOOGLE(mode) (mode & 0xff)

#define VDO_INDEX_HDR     0
#define VDO_INDEX_IDH     1
#define VDO_INDEX_CSTAT   2
#define VDO_INDEX_CABLE   3
#define VDO_INDEX_PRODUCT 3
#define VDO_INDEX_AMA     4
#define VDO_I(name) VDO_INDEX_##name

#define IDH_PTYPE_UNDEF  0
#define IDH_PTYPE_HUB    1
#define IDH_PTYPE_PERIPH 2
#define IDH_PTYPE_PCABLE 3
#define IDH_PTYPE_ACABLE 4
#define IDH_PTYPE_AMA    5

#define VDO_IDH(usbh, usbd, ptype, is_modal, vid)\
	((usbh) << 31 | (usbd) << 30 | ((ptype) & 0x7) << 27\
	 | (is_modal) << 26 | ((vid) & 0xffff))

#define PD_IDH_PTYPE(vdo) (((vdo) >> 27) & 0x7)
#define PD_IDH_VID(vdo)   ((vdo) & 0xffff)
#define VDO_PRODUCT(pid, bcd) (((pid) & 0xffff) << 16 | ((bcd) & 0xffff))
#define VDO_CSTAT(tid)    ((tid) & 0xfffff)

#define VDO_SVID(svid0, svid1) (((svid0) & 0xffff) << 16 | ((svid1) & 0xffff))

#define VDO_AMA(hw, fw, tx1d, tx2d, rx1d, rx2d, vcpwr, vcr, vbr, usbss)\
	(((hw) & 0x7) << 28 | ((fw) & 0x7) << 24\
	 | (tx1d) << 11 | (tx2d) << 10 | (rx1d) << 9 | (rx2d) << 8\
	 | ((vcpwr) & 0x3) << 5 | (vcr) << 4 | (vbr) << 3\
	 | ((usbss) & 0x7))

#define PD_VDO_AMA_HW_VER(vdo)	(((vdo) >> 28) & 0xF)
#define PD_VDO_AMA_FW_VER(vdo)	(((vdo) >> 24) & 0xF)
#define PD_VDO_AMA_SS_TX1(vdo)	(((vdo) >> 11) & 1)
#define PD_VDO_AMA_SS_TX2(vdo)	(((vdo) >> 10) & 1)
#define PD_VDO_AMA_SS_RX1(vdo)	(((vdo) >> 9) & 1)
#define PD_VDO_AMA_SS_RX2(vdo)	(((vdo) >> 8) & 1)
#define PD_VDO_AMA_VCONN_PWR(vdo)	(((vdo) >> 5) & 7)
#define PD_VDO_AMA_VCONN_REQ(vdo)	(((vdo) >> 4) & 1)
#define PD_VDO_AMA_VBUS_REQ(vdo)	(((vdo) >> 3) & 1)
#define PD_VDO_AMA_SS_SUPP(vdo)	(((vdo) >> 0) & 7)

#define SVID_DISCOVERY_MAX 16
#define VDO_SVID(svid0, svid1) (((svid0) & 0xffff) << 16 | ((svid1) & 0xffff))
#define PD_VDO_SVID_SVID0(vdo) ((vdo) >> 16)
#define PD_VDO_SVID_SVID1(vdo) ((vdo) & 0xffff)
#define PDO_MAX_OBJECTS   7
#define PDO_MODES (PDO_MAX_OBJECTS - 1)

struct svdm_svid_data {
	uint16_t svid;
	int mode_cnt;
	uint32_t mode_vdo[PDO_MODES];
};

/*--------------- pd spec notations-----------*/
#define RDO_FIXED_OP_CURR(op_cur)   (((op_cur) / 10) << 10)
#define RDO_FIXED_MAX_CURR(max_cur) (((max_cur) / 10) << 0)

#define RDO_WGB(obj_pos, gb_flag, cap_mismatch, usb_comm_cable, \
		no_usb_suspend, op_cur, max_cur) \
(((obj_pos) << 28) | ((gb_flag << 27)) | ((cap_mismatch) << 26) \
| ((usb_comm_cable) << 25) | ((no_usb_suspend) << 24) \
| RDO_FIXED_OP_CURR(op_cur) | RDO_FIXED_MAX_CURR(max_cur))


#define DISCOVER_IDENTITY	1
#define DISCOVER_SVID	2
#define DISCOVER_MODE	3
#define ENTER_MODE	4
#define EXIT_MODE	5
#define ATTENTION	6

#define INITIATOR		0x00
#define RESP_ACK		0x01
#define RESP_NACK		0x02
#define RESP_BUSY		0x03

#define SII_USBPD_VDO_SVID(svdo)	((svdo) >> 16)
#define SII_USBPD_VDO_SVDM(svdo)	(((svdo) >> 15) & 1)
#define SII_USBPD_VDO_OBJPOSITION(svdo)	(((svdo) >> 8) & 0x7)
#define SII_USBPD_VDO_CMDT(svdo)	(((svdo) >> 6) & 0x3)
#define SII_USBPD_VDO_CMD(svdo)		((svdo) & 0x1f)


#define SVDM_HEADER(usb_svid, svdm_type, svdm_version, obj_pos, cmd_type, cmd) \
	(((usb_svid) << 16) | ((svdm_type) << 15) | ((svdm_version) << 13)\
	 | ((obj_pos) << 8) | ((cmd_type) << 6) | ((cmd) << 0))

#define IDH_PRODUCT_TYPE_UNDEFINED	0x00
#define IDH_PRODUCT_TYPE_HUB	0x01
#define IDH_PRODUCT_PTYPE_PERIPHERAL	0x02
#define IDH_PRODUCT_PTYPE_PASSIVE_CABLE	0x03
#define IDH_PRODUCT_PTYPE_ACTIVE_CABLE	0x04
#define IDH_PRODUCT_PTYPE_AMA	0x05

#define CABLE_VDO_ATYPE	0x00
#define CABLE_VDO_BTYPE	0x01
#define CABLE_VDO_CTYPE	0x02

#define CABLE_VDO_PLUG	0
#define CABLE_VDO_RECEPTACLE	1

#define CABLE_LATENCY_1M		0x01
#define CABLE_LATENCY_2M		0x02
#define CABLE_LATENCY_3M		0x03
#define CABLE_LATENCY_4M		0x04
#define CABLE_LATENCY_5M		0x05
#define CABLE_LATENCY_6M		0x06
#define CABLE_LATENCY_7M		0x07
#define CABLE_LATENCY_100M	0x10
#define CABLE_LATENCY_200M	0x11
#define CABLE_LATENCY_300M	0x12

#define CABLE_LATENCY_TERM_PASSIVE2_VCONN			0x00
#define CABLE_LATENCY_TERM_PASSIVE2_WO_VCONN	0x01
#define CABLE_LATENCY_TERM_ACTIVE1					0x02
#define CABLE_LATENCY_TERM_ACTIVE2					0x03

#define CABLE_LATENCY_VBUS_CURRENT_1_5A				0x00
#define CABLE_LATENCY_VBUS_CURRENT_3A				0x01
#define CABLE_LATENCY_VBUS_CURRENT_5A				0x02

#define CABLE_LATENCY_USBSUPERSPEED_USB2_ONLY	 0
#define CABLE_USBSUPERSPEED_USB31_GEN1		1
#define CABLE_USBSUPERSPEEED_USB31_GEN12			2


#define AMA_VDO_VCONN_POWER_1W	0x00
#define AMA_VDO_VCONN_POWER_1W5	0x01
#define AMA_VDO_VCONN_POWER_2W	0x02
#define AMA_VDO_VCONN_POWER_3W	0x03
#define AMA_VDO_VCONN_POWER_4W	0x04
#define AMA_VDO_VCONN_POWER_5W	0x05
#define AMA_VDO_VCONN_POWER_6W	0x06

#define AMA_VDO_USBSUPERSPEED_USB20_ONLY	0x00
#define AMA_VDO_USBSUPERSPEED_USB31_GEN1_USB20	0x01
#define AMA_VDO_USBSUPERSPEED_USB31_GEN12_USB20	0x02
#define AMA_VDO_USBSUPERSPEED_BILLBOARDONLY20	0x03

#define USB_VID_MHL 0xFF02
#define USB_PD_SID  0xFFFF

#define VDM_HDR     0
#define VDM_IDH     1
#define VDM_CERT    2
#define VDM_PTYPE   3
#define VDM_CABLE   4
#define VDM_AMA     5

#define SVDM_IDH(usb_host, usb_device, product_type, modal_supp, usb_vid) \
	((usb_host) << 31 | (usb_device) << 30 | ((product_type) & 0x7) << 27\
	 | (modal_supp) << 26 | ((usb_vid) & 0xffff))

#define SVDM_AMA(hw_version, fw_version, sstx1_diection, sstx2_direction, \
		ssrx1_direction, ss_rx2_direction, vconn_pwr, \
		vconn_req, vbus_req, ssusb_supp) \
(((hw_version) & 0x7) << 28 | ((fw_version) & 0x7) << 24 \
| (sstx1_diection) << 11 | (sstx2_direction) << 10 \
| (ssrx1_direction) << 9 | (ss_rx2_direction) << 8 \
| ((vconn_pwr) & 0x3) << 5 | (vconn_req) << 4 \
| (vbus_req) << 3 | ((ssusb_supp) & 0x7))

#define SII_PREPARE_CERT_STAT_HEADER(cert_reserv, test_id) \
	(((cert_reserv) << 20) | ((test_id) << 0))

#define SII_PREPARE_PRODUCT_VDO_HEADER(usb_pid, bcd_dev) \
	(((usb_pid) << 16) | ((bcd_dev) << 16))

#define SII_PREPARE_CABLE_HEADER(hw_ver, fw_ver, c_to_abc, c_to_plug, cb_lat,\
		cb_term, tx1_dir, tx2_dir, rx1_dir, rx2_dir, \
		vbus_curr_cap, vbus_th_cable, sop_2_support, \
		ss_sig_support) \
(((hw_ver) << 28) | ((fw_ver) << 24) | \
((c_to_abc) << 18) | ((c_to_plug) << 17)| \
((cb_lat) << 13) | ((cb_term) << 11) | \
((tx1_dir) << 10) | ((tx2_dir) << 9) | \
((rx1_dir) << 8) | ((rx2_dir) << 7)  | \
((vbus_curr_cap) << 5) | ((vbus_th_cable) << 4) | \
((sop_2_support) << 3) | ((ss_sig_support) << 0))


#define SVDM_SVIDS(vdm_svid_0, vdm_svid_1) (((vdm_svid_0) & 0xffff) << 16 \
		| ((vdm_svid_1) & 0xffff))
#define SVDM_SVID_0(vdm_svid) ((vdm_svid_0) >> 16)
#define SVDM_SVID_1(vdo_svid) ((vdo_svid_1) & 0xffff)

struct pd_inq {
	uint16_t command;
	uint32_t *msgdata;
	uint8_t count;
};

struct pd_cb_params {
#define GOOD_CRC_RCVD        1
#define GOTOMIN_RCVD         2
#define ACCEPT_RCVD          3
#define REJECT_RCVD          4
#define PING_RCVD            5
#define PS_RDY_RCVD          6
#define GET_SOURCE_CAP_RCVD  7
#define GET_SINK_CAP_RCVD    8
#define DR_SWAP_RCVD         9
#define PR_SWAP_RCVD         10
#define VCONN_SWAP_RCVD      11
#define WAIT_RCVD            12
#define SOFT_RESET_RCVD      13
#define SRC_CAP_RCVD         14
#define SNK_CAP_RCVD         15
#define REQ_RCVD             16
#define VALID_REQ            17
#define CAN_BE_MET      18
#define	CAN_NOT_BE_MET       19
#define	LATER_GB     20
#define BIST_RCVD	     21
#define GET_SINK_SRC_CAP_RCVD    22
	unsigned long sm_cmd_inputs;

#define SVDM_DISC_IDEN_RCVD   1
#define DISCOVER_SVID_RCVD    2
#define DISCOVER_MODE_RCVD    3
#define ENTER_MODE_RCVD       4
#define EXIT_MODE_RCVD        5
#define VDM_ATTENTION_RCVD    6

#define VDM_ATTENTION_ACK_RCVD   7
#define SVDM_DISC_IDEN_ACK_RCVD  8
#define DISCOVER_SVID_ACK_RCVD   9
#define DISCOVER_MODE_ACK_RCVD   10
#define ENTER_MODE_ACK_RCVD      11
#define EXIT_MODE_ACK_RCVD       12

#define VDM_ATTENTION_NACK_RCVD  13
#define SVDM_DISC_IDEN_NACK_RCVD 14
#define DISCOVER_SVID_NACK_RCVD  15
#define DISCOVER_MODE_NACK_RCVD  16
#define ENTER_MODE_NACK_RCVD     17
#define EXIT_MODE_NACK_RCVD      18

#define VDM_ATTENTION_BUSY_RCVD  20
#define SVDM_DISC_IDEN_BUSY_RCVD 21
#define DISCOVER_SVID_BUSY_RCVD  22
#define DISCOVER_MODE_BUSY_RCVD  23
#define ENTER_MODE_BUSY_RCVD     24
#define EXIT_MODE_BUSY_RCVD      25
#define MHL_ESTABLISHD		 26
#define MHL_NOT_ESTABLISHD       27
#define MHL_EXITED		 28
#define MHL_NOT_EXITED		 29
#define	DISPLAY_PORT_RCVD	 30
#define	VDM_MSG_RCVD		 31
	unsigned long svdm_sm_inputs;

#define UVDM_RCVD	1
	unsigned long uvdm_sm_inputs;

	uint16_t cmd;
	uint32_t cmd_data;	/*VDM data */
	bool sent;
	/* Data Req from PE to Protocol layer */
	uint8_t *data;
	/* Data Req from Protocol to PE layer */
	uint8_t send_data[30];
	/*uint32_t *pdata; */
	uint8_t count;
	void *context;
	void *req_context;
	bool is_tx;
};

typedef bool(*cmd_report_t) (void *data, struct pd_cb_params *cbparams);

struct pd_outq {
	uint16_t command;
	uint8_t length;
	uint8_t *msgdata;
};

/*enum sent_stat {
	SUCCESS = 7,
	RETRY = 8,
	TIMEDOUT = 9,
	DONE = 10,
	FAILED = 11,
};*/

struct config_param {
	unsigned char pwr_role;
	unsigned char data_role;
};

struct sii_usbpd_protocol {
	bool (*evnt_notify_fn)(void *, struct pd_cb_params *);
	struct sii70xx_drv_context *drv_context;
	uint8_t pwr_role;
	uint8_t data_role;
	uint8_t caps_counter;
	uint8_t hr_counter;
	uint8_t retry_counter;
	uint32_t svdm_resp[6];
	struct pd_cb_params *cb_params;
	struct pd_outq *out_command;
	struct pd_inq *in_command;
	uint8_t pd_hw_ver;
	uint8_t pd_fw_ver;
	uint8_t ss_tx1_func;
	uint8_t ss_tx2_func;
	uint8_t ss_rx1_func;
	uint8_t ss_rx2_func;
	uint8_t vconn;
	uint8_t vconn_req;
	uint8_t vbus_req;
	uint8_t usb_ss_supp;
	uint16_t pd_modes[SVID_DISCOVERY_MAX];
	uint32_t send_msg[7];
	struct si_data_intf *rx_msg;
	uint8_t svid_mode;
};

enum volt_idx {
	PDO_IDX_5V = 0,
	PDO_IDX_12V_1 = 1,
	PDO_IDX_12V_3 = 2,
	PDO_IDX_12V_5 = 3,
	PDO_IDX_20V_3 = 4,
	PDO_IDX_20V_5 = 5,
	PDO_IDX_COUNT
};

enum pdo_type {
	FIXED_SUPPLY = 0,
	VARIABLE_SUPPLY = 1,
	BATTERY_SUPPLY
};

enum obj_fields {
	USBPD_OBJ_1 = 1,
	USBPD_OBJ_2,
	USBPD_OBJ_3,
	USBPD_OBJ_4,
	USBPD_OBJ_5,
	USBPD_OBJ_6,
	USBPD_MAX_OBJ
};

enum data_msg {
	/*Source or Dual- Role operating as Sink - SOP only */
	SRCCAP = 1,
	/*Sink only - SOP only */
	REQ = 2,
	/*Source or Sink - SOP*  */
	BIST = 3,
	/* Dual-Role - SOP only */
	SNKCAP = 4,
	/* Source, Sink or Cable Plug - SOP* */
	VDM = 15
};

enum ctrl_msg {
	CTRL_MSG__GOODCRC = 1,
	CTRL_MSG__GO_TO_MIN,
	CTRL_MSG__ACCEPT,
	CTRL_MSG__REJECT,
	CTRL_MSG__PING,
	CTRL_MSG__PS_RDY,
	CTRL_MSG__GET_SRC_CAP,
	CTRL_MSG__GET_SINK_CAP,
	CTRL_MSG__DR_SWAP,
	CTRL_MSG__PR_SWAP,
	CTRL_MSG__VCONN_SWAP,
	CTRL_MSG__WAIT,
	CTRL_MSG__SOFT_RESET
};
void *usbpd_core_init(void *drv_context,
		      struct config_param *params,
		      bool (*event_notify_fn)(void *, struct pd_cb_params *));
void usbpd_core_exit(void *);

bool usbpd_process_msg(struct sii_usbpd_protocol *pd, uint8_t *buf, uint8_t count);

void usbpd_core_reset(struct sii_usbpd_protocol *pd);

bool sii_usbpd_xmit_data_msg(struct sii_usbpd_protocol *pd,
			     enum data_msg type, uint32_t *src_pdo, uint8_t count);

bool usbpd_xmit_ctrl_msg(struct sii_usbpd_protocol *pd, enum ctrl_msg type);

bool usbpd_svdm_init_resp(struct sii_usbpd_protocol *pd, uint8_t cmd, bool is_rcvd, uint32_t *vdo);

bool usbpd_send_vdm_cmd(struct sii_usbpd_protocol *pd,
			enum data_msg msg_type, uint8_t type, uint8_t pdo);

bool send_custom_vdm_message(struct sii_usbpd_protocol *pd, enum data_msg msg_type, uint8_t type);

bool send_custom_vdm_msg_resp(struct sii_usbpd_protocol *pd, enum data_msg msg_type);

void update_pwr_role(void *context, uint8_t updated_role);
void update_data_role(void *context, uint8_t updated_role);
void bist_timer_start(void *pd);
#endif
