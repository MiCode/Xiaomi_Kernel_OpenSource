/*
 *
 * Copyright (c) 2011-2013 The Linux Foundation. All rights reserved.
 *
 * This file is based on include/net/bluetooth/hci_core.h
 *
 * Written 2000,2001 by Maxim Krasnyansky <maxk@qualcomm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) AND AUTHOR(S) BE LIABLE FOR ANY
 * CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * ALL LIABILITY, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PATENTS,
 * COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS, RELATING TO USE OF THIS
 * SOFTWARE IS DISCLAIMED.
 */

#ifndef __RADIO_HCI_CORE_H
#define __RADIO_HCI_CORE_H

#include <linux/skbuff.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include "radio-iris-commands.h"
const unsigned char MIN_TX_TONE_VAL = 0x00;
const unsigned char MAX_TX_TONE_VAL = 0x07;
const unsigned char MIN_HARD_MUTE_VAL = 0x00;
const unsigned char MAX_HARD_MUTE_VAL = 0x03;
const unsigned char MIN_SRCH_MODE = 0x00;
const unsigned char MAX_SRCH_MODE = 0x09;
const unsigned char MIN_SCAN_DWELL = 0x00;
const unsigned char MAX_SCAN_DWELL = 0x0F;
const unsigned char MIN_SIG_TH = 0x00;
const unsigned char MAX_SIG_TH = 0x03;
const unsigned char MIN_PTY = 0X00;
const unsigned char MAX_PTY = 0x1F;
const unsigned short MIN_PI = 0x0000;
const unsigned short MAX_PI = 0xFFFF;
const unsigned char MIN_SRCH_STATIONS_CNT = 0x00;
const unsigned char MAX_SRCH_STATIONS_CNT = 0x14;
const unsigned char MIN_CHAN_SPACING = 0x00;
const unsigned char MAX_CHAN_SPACING = 0x02;
const unsigned char MIN_EMPHASIS = 0x00;
const unsigned char MAX_EMPHASIS = 0x01;
const unsigned char MIN_RDS_STD = 0x00;
const unsigned char MAX_RDS_STD = 0x02;
const unsigned char MIN_ANTENNA_VAL = 0x00;
const unsigned char MAX_ANTENNA_VAL = 0x01;
const unsigned char MIN_TX_PS_REPEAT_CNT = 0x01;
const unsigned char MAX_TX_PS_REPEAT_CNT = 0x0F;
const unsigned char MIN_SOFT_MUTE = 0x00;
const unsigned char MAX_SOFT_MUTE = 0x01;
const unsigned char MIN_PEEK_ACCESS_LEN = 0x01;
const unsigned char MAX_PEEK_ACCESS_LEN = 0xF9;
const unsigned char MIN_RESET_CNTR = 0x00;
const unsigned char MAX_RESET_CNTR = 0x01;
const unsigned char MIN_HLSI = 0x00;
const unsigned char MAX_HLSI = 0x02;
const unsigned char MIN_NOTCH_FILTER = 0x00;
const unsigned char MAX_NOTCH_FILTER = 0x02;
const unsigned char MIN_INTF_DET_OUT_LW_TH = 0x00;
const unsigned char MAX_INTF_DET_OUT_LW_TH = 0xFF;
const unsigned char MIN_INTF_DET_OUT_HG_TH = 0x00;
const unsigned char MAX_INTF_DET_OUT_HG_TH = 0xFF;
const signed char MIN_SINR_TH = -128;
const signed char MAX_SINR_TH = 127;
const unsigned char MIN_SINR_SAMPLES = 0x01;
const unsigned char MAX_SINR_SAMPLES = 0xFF;

/* ---- HCI Packet structures ---- */
#define RADIO_HCI_COMMAND_HDR_SIZE sizeof(struct radio_hci_command_hdr)
#define RADIO_HCI_EVENT_HDR_SIZE   sizeof(struct radio_hci_event_hdr)

/* HCI data types */
#define RADIO_HCI_COMMAND_PKT   0x11
#define RADIO_HCI_EVENT_PKT     0x14
/*HCI reponce packets*/
#define MAX_RIVA_PEEK_RSP_SIZE   251
/* default data access */
#define DEFAULT_DATA_OFFSET 2
#define DEFAULT_DATA_SIZE 249
/* Power levels are 0-7, but SOC will expect values from 0-255
 * So the each level step size will be 255/7 = 36 */
#define FM_TX_PWR_LVL_STEP_SIZE 36
#define FM_TX_PWR_LVL_0         0 /* Lowest power lvl that can be set for Tx */
#define FM_TX_PWR_LVL_MAX       7 /* Max power lvl for Tx */
#define FM_TX_PHY_CFG_MODE   0x3c
#define FM_TX_PHY_CFG_LEN    0x10
#define FM_TX_PWR_GAIN_OFFSET 14
/**RDS CONFIG MODE**/
#define FM_RDS_CNFG_MODE	0x0f
#define FM_RDS_CNFG_LEN		0x10
#define AF_RMSSI_TH_LSB_OFFSET	10
#define AF_RMSSI_TH_MSB_OFFSET	11
#define AF_RMSSI_SAMPLES_OFFSET	15
/**RX CONFIG MODE**/
#define FM_RX_CONFG_MODE	0x15
#define FM_RX_CNFG_LEN		0x20
#define GD_CH_RMSSI_TH_OFFSET	12
#define MAX_GD_CH_RMSSI_TH	127
#define SRCH_ALGO_TYPE_OFFSET  25
#define SINRFIRSTSTAGE_OFFSET  26
#define RMSSIFIRSTSTAGE_OFFSET 27
#define CF0TH12_BYTE1_OFFSET   8
#define CF0TH12_BYTE2_OFFSET   9
#define CF0TH12_BYTE3_OFFSET   10
#define CF0TH12_BYTE4_OFFSET   11
#define MAX_SINR_FIRSTSTAGE	127
#define MAX_RMSSI_FIRSTSTAGE	127
#define RDS_PS0_XFR_MODE 0x01
#define RDS_PS0_LEN 6
#define RX_REPEATE_BYTE_OFFSET 5

#define FM_AF_LIST_MAX_SIZE   200
#define AF_LIST_MAX     (FM_AF_LIST_MAX_SIZE / 4) /* Each AF frequency consist
							of sizeof(int) bytes */
/* HCI timeouts */
#define RADIO_HCI_TIMEOUT	(10000)	/* 10 seconds */

#define TUNE_PARAM 16
struct radio_hci_command_hdr {
	__le16	opcode;		/* OCF & OGF */
	__u8	plen;
} __packed;

struct radio_hci_event_hdr {
	__u8	evt;
	__u8	plen;
} __packed;

struct radio_hci_dev {
	char		name[8];
	unsigned long	flags;
	__u16		id;
	__u8		bus;
	__u8		dev_type;
	__u8		dev_name[248];
	__u8		dev_class[3];
	__u8		features[8];
	__u8		commands[64];

	unsigned int	data_block_len;
	unsigned long	cmd_last_tx;

	struct sk_buff		*sent_cmd;

	__u32			req_status;
	__u32			req_result;
	atomic_t	cmd_cnt;

	struct tasklet_struct	cmd_task;
	struct tasklet_struct	rx_task;
	struct tasklet_struct	tx_task;

	struct sk_buff_head	rx_q;
	struct sk_buff_head	raw_q;
	struct sk_buff_head	cmd_q;

	struct mutex		req_lock;
	wait_queue_head_t	req_wait_q;

	int (*open)(struct radio_hci_dev *hdev);
	int (*close)(struct radio_hci_dev *hdev);
	int (*flush)(struct radio_hci_dev *hdev);
	int (*send)(struct sk_buff *skb);
	void (*destruct)(struct radio_hci_dev *hdev);
	void (*notify)(struct radio_hci_dev *hdev, unsigned int evt);
};

int radio_hci_register_dev(struct radio_hci_dev *hdev);
int radio_hci_unregister_dev(struct radio_hci_dev *hdev);
int radio_hci_recv_frame(struct sk_buff *skb);
int radio_hci_send_cmd(struct radio_hci_dev *hdev, __u16 opcode, __u32 plen,
	void *param);
void radio_hci_event_packet(struct radio_hci_dev *hdev, struct sk_buff *skb);

/* Opcode OCF */
/* HCI recv control commands opcode */
#define HCI_OCF_FM_ENABLE_RECV_REQ          0x0001
#define HCI_OCF_FM_DISABLE_RECV_REQ         0x0002
#define HCI_OCF_FM_GET_RECV_CONF_REQ        0x0003
#define HCI_OCF_FM_SET_RECV_CONF_REQ        0x0004
#define HCI_OCF_FM_SET_MUTE_MODE_REQ        0x0005
#define HCI_OCF_FM_SET_STEREO_MODE_REQ      0x0006
#define HCI_OCF_FM_SET_ANTENNA              0x0007
#define HCI_OCF_FM_SET_SIGNAL_THRESHOLD     0x0008
#define HCI_OCF_FM_GET_SIGNAL_THRESHOLD     0x0009
#define HCI_OCF_FM_GET_STATION_PARAM_REQ    0x000A
#define HCI_OCF_FM_GET_PROGRAM_SERVICE_REQ  0x000B
#define HCI_OCF_FM_GET_RADIO_TEXT_REQ       0x000C
#define HCI_OCF_FM_GET_AF_LIST_REQ          0x000D
#define HCI_OCF_FM_SEARCH_STATIONS          0x000E
#define HCI_OCF_FM_SEARCH_RDS_STATIONS      0x000F
#define HCI_OCF_FM_SEARCH_STATIONS_LIST     0x0010
#define HCI_OCF_FM_CANCEL_SEARCH            0x0011
#define HCI_OCF_FM_RDS_GRP                  0x0012
#define HCI_OCF_FM_RDS_GRP_PROCESS          0x0013
#define HCI_OCF_FM_EN_WAN_AVD_CTRL          0x0014
#define HCI_OCF_FM_EN_NOTCH_CTRL            0x0015
#define HCI_OCF_FM_SET_EVENT_MASK           0x0016
#define HCI_OCF_FM_SET_CH_DET_THRESHOLD     0x0017
#define HCI_OCF_FM_GET_CH_DET_THRESHOLD     0x0018
/* HCI trans control commans opcode*/
#define HCI_OCF_FM_ENABLE_TRANS_REQ         0x0001
#define HCI_OCF_FM_DISABLE_TRANS_REQ        0x0002
#define HCI_OCF_FM_GET_TRANS_CONF_REQ       0x0003
#define HCI_OCF_FM_SET_TRANS_CONF_REQ       0x0004
#define HCI_OCF_FM_RDS_RT_REQ               0x0008
#define HCI_OCF_FM_RDS_PS_REQ               0x0009


/* HCI common control commands opcode */
#define HCI_OCF_FM_TUNE_STATION_REQ         0x0001
#define HCI_OCF_FM_DEFAULT_DATA_READ        0x0002
#define HCI_OCF_FM_DEFAULT_DATA_WRITE       0x0003
#define HCI_OCF_FM_RESET                    0x0004
#define HCI_OCF_FM_GET_FEATURE_LIST         0x0005
#define HCI_OCF_FM_DO_CALIBRATION           0x0006
#define HCI_OCF_FM_SET_CALIBRATION          0x0007

/*HCI Status parameters commands*/
#define HCI_OCF_FM_READ_GRP_COUNTERS        0x0001

/*HCI Diagnostic commands*/
#define HCI_OCF_FM_PEEK_DATA                0x0002
#define HCI_OCF_FM_POKE_DATA                0x0003
#define HCI_OCF_FM_SSBI_PEEK_REG            0x0004
#define HCI_OCF_FM_SSBI_POKE_REG            0x0005
#define HCI_OCF_FM_STATION_DBG_PARAM        0x0007
#define HCI_FM_SET_INTERNAL_TONE_GENRATOR   0x0008

/* Opcode OGF */
#define HCI_OGF_FM_RECV_CTRL_CMD_REQ            0x0013
#define HCI_OGF_FM_TRANS_CTRL_CMD_REQ           0x0014
#define HCI_OGF_FM_COMMON_CTRL_CMD_REQ          0x0015
#define HCI_OGF_FM_STATUS_PARAMETERS_CMD_REQ    0x0016
#define HCI_OGF_FM_TEST_CMD_REQ                 0x0017
#define HCI_OGF_FM_DIAGNOSTIC_CMD_REQ           0x003F

/* Command opcode pack/unpack */
#define hci_opcode_pack(ogf, ocf)	(__u16) ((ocf & 0x03ff)|(ogf << 10))
#define hci_opcode_ogf(op)		(op >> 10)
#define hci_opcode_ocf(op)		(op & 0x03ff)
#define hci_recv_ctrl_cmd_op_pack(ocf) \
	(__u16) hci_opcode_pack(HCI_OGF_FM_RECV_CTRL_CMD_REQ, ocf)
#define hci_trans_ctrl_cmd_op_pack(ocf) \
	(__u16) hci_opcode_pack(HCI_OGF_FM_TRANS_CTRL_CMD_REQ, ocf)
#define hci_common_cmd_op_pack(ocf)	\
	(__u16) hci_opcode_pack(HCI_OGF_FM_COMMON_CTRL_CMD_REQ, ocf)
#define hci_status_param_op_pack(ocf)	\
	(__u16) hci_opcode_pack(HCI_OGF_FM_STATUS_PARAMETERS_CMD_REQ, ocf)
#define hci_diagnostic_cmd_op_pack(ocf)	\
	(__u16) hci_opcode_pack(HCI_OGF_FM_DIAGNOSTIC_CMD_REQ, ocf)


/* HCI commands with no arguments*/
#define HCI_FM_ENABLE_RECV_CMD 1
#define HCI_FM_DISABLE_RECV_CMD 2
#define HCI_FM_GET_RECV_CONF_CMD 3
#define HCI_FM_GET_STATION_PARAM_CMD 4
#define HCI_FM_GET_SIGNAL_TH_CMD 5
#define HCI_FM_GET_PROGRAM_SERVICE_CMD 6
#define HCI_FM_GET_RADIO_TEXT_CMD 7
#define HCI_FM_GET_AF_LIST_CMD 8
#define HCI_FM_CANCEL_SEARCH_CMD 9
#define HCI_FM_RESET_CMD 10
#define HCI_FM_GET_FEATURES_CMD 11
#define HCI_FM_STATION_DBG_PARAM_CMD 12
#define HCI_FM_ENABLE_TRANS_CMD 13
#define HCI_FM_DISABLE_TRANS_CMD 14
#define HCI_FM_GET_TX_CONFIG 15
#define HCI_FM_GET_DET_CH_TH_CMD 16

/* Defines for FM TX*/
#define TX_PS_DATA_LENGTH 108
#define TX_RT_DATA_LENGTH 64
#define PS_STRING_LEN     9

/* ----- HCI Command request ----- */
struct hci_fm_recv_conf_req {
	__u8	emphasis;
	__u8	ch_spacing;
	__u8	rds_std;
	__u8	hlsi;
	__u32	band_low_limit;
	__u32	band_high_limit;
} __packed;

/* ----- HCI Command request ----- */
struct hci_fm_trans_conf_req_struct {
	__u8	emphasis;
	__u8	rds_std;
	__u32	band_low_limit;
	__u32	band_high_limit;
} __packed;


/* ----- HCI Command request ----- */
struct hci_fm_tx_ps {
	__u8    ps_control;
	__u16	pi;
	__u8	pty;
	__u8	ps_repeatcount;
	__u8	ps_num;
	__u8    ps_data[TX_PS_DATA_LENGTH];
} __packed;

struct hci_fm_tx_rt {
	__u8    rt_control;
	__u16	pi;
	__u8	pty;
	__u8	rt_len;
	__u8    rt_data[TX_RT_DATA_LENGTH];
} __packed;

struct hci_fm_mute_mode_req {
	__u8	hard_mute;
	__u8	soft_mute;
} __packed;

struct hci_fm_stereo_mode_req {
	__u8    stereo_mode;
	__u8    sig_blend;
	__u8    intf_blend;
	__u8    most_switch;
} __packed;

struct hci_fm_search_station_req {
	__u8    srch_mode;
	__u8    scan_time;
	__u8    srch_dir;
} __packed;

struct hci_fm_search_rds_station_req {
	struct hci_fm_search_station_req srch_station;
	__u8    srch_pty;
	__u16   srch_pi;
} __packed;

struct hci_fm_search_station_list_req {
	__u8    srch_list_mode;
	__u8    srch_list_dir;
	__u32   srch_list_max;
	__u8    srch_pty;
} __packed;

struct hci_fm_rds_grp_req {
	__u32   rds_grp_enable_mask;
	__u32   rds_buf_size;
	__u8    en_rds_change_filter;
} __packed;

struct hci_fm_en_avd_ctrl_req {
	__u8    no_freqs;
	__u8    freq_index;
	__u8    lo_shft;
	__u16   freq_min;
	__u16   freq_max;
} __packed;

struct hci_fm_def_data_rd_req {
	__u8    mode;
	__u8    length;
	__u8    param_len;
	__u8    param;
} __packed;

struct hci_fm_def_data_wr_req {
	__u8    mode;
	__u8    length;
	__u8   data[DEFAULT_DATA_SIZE];
} __packed;

struct hci_fm_riva_data {
	__u8 subopcode;
	__u32   start_addr;
	__u8    length;
} __packed;

struct hci_fm_riva_poke {
	struct hci_fm_riva_data cmd_params;
	__u8    data[MAX_RIVA_PEEK_RSP_SIZE];
} __packed;

struct hci_fm_ssbi_req {
	__u16   start_addr;
	__u8    data;
} __packed;
struct hci_fm_ssbi_peek {
	__u16 start_address;
} __packed;

struct hci_fm_ch_det_threshold {
	char sinr;
	__u8 sinr_samples;
	__u8 low_th;
	__u8 high_th;

} __packed;

/*HCI events*/
#define HCI_EV_TUNE_STATUS              0x01
#define HCI_EV_RDS_LOCK_STATUS          0x02
#define HCI_EV_STEREO_STATUS            0x03
#define HCI_EV_SERVICE_AVAILABLE        0x04
#define HCI_EV_SEARCH_PROGRESS          0x05
#define HCI_EV_SEARCH_RDS_PROGRESS      0x06
#define HCI_EV_SEARCH_LIST_PROGRESS     0x07
#define HCI_EV_RDS_RX_DATA              0x08
#define HCI_EV_PROGRAM_SERVICE          0x09
#define HCI_EV_RADIO_TEXT               0x0A
#define HCI_EV_FM_AF_LIST               0x0B
#define HCI_EV_TX_RDS_GRP_AVBLE         0x0C
#define HCI_EV_TX_RDS_GRP_COMPL         0x0D
#define HCI_EV_TX_RDS_CONT_GRP_COMPL    0x0E
#define HCI_EV_CMD_COMPLETE             0x0F
#define HCI_EV_CMD_STATUS               0x10
#define HCI_EV_TUNE_COMPLETE            0x11
#define HCI_EV_SEARCH_COMPLETE          0x12
#define HCI_EV_SEARCH_RDS_COMPLETE      0x13
#define HCI_EV_SEARCH_LIST_COMPLETE     0x14

#define HCI_REQ_DONE	  0
#define HCI_REQ_PEND	  1
#define HCI_REQ_CANCELED  2
#define HCI_REQ_STATUS    3

#define MAX_RAW_RDS_GRPS	21

#define RDSGRP_DATA_OFFSET	 0x1

/*RT PLUS*/
#define DUMMY_CLASS		0
#define RT_PLUS_LEN_1_TAG	3
#define RT_ERT_FLAG_BIT		5

/*TAG1*/
#define TAG1_MSB_OFFSET		3
#define TAG1_MSB_MASK		7
#define TAG1_LSB_OFFSET		5
#define TAG1_POS_MSB_MASK	31
#define TAG1_POS_MSB_OFFSET	1
#define TAG1_POS_LSB_OFFSET	7
#define TAG1_LEN_OFFSET		1
#define TAG1_LEN_MASK		63

/*TAG2*/
#define TAG2_MSB_OFFSET		5
#define TAG2_MSB_MASK		1
#define TAG2_LSB_OFFSET		3
#define TAG2_POS_MSB_MASK	7
#define TAG2_POS_MSB_OFFSET	3
#define TAG2_POS_LSB_OFFSET	5
#define TAG2_LEN_MASK		31

#define AGT_MASK		31
/*Extract 5 left most bits of lsb of 2nd block*/
#define AGT(x)			(x & AGT_MASK)
/*16 bits of 4th block*/
#define AID(lsb, msb)		((msb << 8) | (lsb))
/*Extract 5 right most bits of msb of 2nd block*/
#define GTC(blk2msb)		(blk2msb >> 3)

#define GRP_3A			0x6
#define RT_PLUS_AID		0x4bd7

/*ERT*/
#define ERT_AID			0x6552
#define CARRIAGE_RETURN		0x000D
#define MAX_ERT_SEGMENT		31
#define ERT_FORMAT_DIR_BIT	1

#define EXTRACT_BIT(data, bit_pos) ((data & (1 << bit_pos)) >> bit_pos)

struct hci_ev_tune_status {
	__u8    sub_event;
	__le32  station_freq;
	__u8    serv_avble;
	__u8    rssi;
	__u8    stereo_prg;
	__u8    rds_sync_status;
	__u8    mute_mode;
	char    sinr;
	__u8	intf_det_th;
} __packed;

struct rds_blk_data {
	__u8	rdsMsb;
	__u8	rdsLsb;
	__u8	blockStatus;
} __packed;

struct rds_grp_data {
	struct rds_blk_data rdsBlk[4];
} __packed;

struct hci_ev_rds_rx_data {
	__u8    num_rds_grps;
	struct  rds_grp_data rds_grp_data[MAX_RAW_RDS_GRPS];
} __packed;

struct hci_ev_prg_service {
	__le16   pi_prg_id;
	__u8    pty_prg_type;
	__u8    ta_prg_code_type;
	__u8    ta_ann_code_flag;
	__u8    ms_switch_code_flag;
	__u8    dec_id_ctrl_code_flag;
	__u8    ps_num;
	__u8    prg_service_name[119];
} __packed;

struct hci_ev_radio_text {
	__le16   pi_prg_id;
	__u8    pty_prg_type;
	__u8    ta_prg_code_type;
	__u8    txt_ab_flag;
	__u8    radio_txt[64];
} __packed;

struct hci_ev_af_list {
	__le32   tune_freq;
	__le16   pi_code;
	__u8    af_size;
	__u8    af_list[FM_AF_LIST_MAX_SIZE];
} __packed;

struct hci_ev_cmd_complete {
	__u8    num_hci_cmd_pkts;
	__le16   cmd_opcode;
} __packed;

struct hci_ev_cmd_status {
	__u8    status;
	__u8    num_hci_cmd_pkts;
	__le16   status_opcode;
} __packed;

struct hci_ev_srch_st {
	__le32    station_freq;
	__u8    rds_cap;
	__u8   pty;
	__le16   status_opcode;
} __packed;

struct hci_ev_rel_freq {
	__u8  rel_freq_msb;
	__u8  rel_freq_lsb;

} __packed;
struct hci_ev_srch_list_compl {
	__u8    num_stations_found;
	struct hci_ev_rel_freq  rel_freq[20];
} __packed;

/* ----- HCI Event Response ----- */
struct hci_fm_conf_rsp {
	__u8    status;
	struct hci_fm_recv_conf_req recv_conf_rsp;
} __packed;

struct hci_fm_get_trans_conf_rsp {
	__u8    status;
	struct hci_fm_trans_conf_req_struct trans_conf_rsp;
} __packed;
struct hci_fm_sig_threshold_rsp {
	__u8    status;
	__u8    sig_threshold;
} __packed;

struct hci_fm_station_rsp {
	struct hci_ev_tune_status station_rsp;
} __packed;

struct hci_fm_prgm_srv_rsp {
	__u8    status;
	struct hci_ev_prg_service prg_srv;
} __packed;

struct hci_fm_radio_txt_rsp {
	__u8    status;
	struct hci_ev_radio_text rd_txt;
} __packed;

struct hci_fm_af_list_rsp {
	__u8    status;
	struct hci_ev_af_list rd_txt;
} __packed;

struct hci_fm_data_rd_rsp {
	__u8    status;
	__u8    ret_data_len;
	__u8    data[DEFAULT_DATA_SIZE];
} __packed;

struct hci_fm_feature_list_rsp {
	__u8    status;
	__u8    feature_mask;
} __packed;

struct hci_fm_dbg_param_rsp {
	__u8    status;
	__u8    blend;
	__u8    soft_mute;
	__u8    inf_blend;
	__u8    inf_soft_mute;
	__u8    pilot_pil;
	__u8    io_verc;
	__u8    in_det_out;
} __packed;

#define CLKSPURID_INDEX0	0
#define CLKSPURID_INDEX1	5
#define CLKSPURID_INDEX2	10
#define CLKSPURID_INDEX3	15
#define CLKSPURID_INDEX4	20
#define CLKSPURID_INDEX5	25

#define MAX_SPUR_FREQ_LIMIT	30
#define CKK_SPUR		0x3B
#define SPUR_DATA_SIZE		0x4
#define SPUR_ENTRIES_PER_ID	0x5

#define COMPUTE_SPUR(val)         ((((val) - (76000)) / (50)))
#define GET_FREQ(val, bit)        ((bit == 1) ? ((val) >> 8) : ((val) & 0xFF))
#define GET_SPUR_ENTRY_LEVEL(val) ((val) / (5))

struct hci_fm_spur_data {
	__u32	freq[MAX_SPUR_FREQ_LIMIT];
	__s8	rmssi[MAX_SPUR_FREQ_LIMIT];
	__u8	enable[MAX_SPUR_FREQ_LIMIT];
} __packed;


/* HCI dev events */
#define RADIO_HCI_DEV_REG			1
#define RADIO_HCI_DEV_WRITE			2

#define hci_req_lock(d)		mutex_lock(&d->req_lock)
#define hci_req_unlock(d)	mutex_unlock(&d->req_lock)

/* FM RDS */
#define RDS_PTYPE 2
#define RDS_PID_LOWER 1
#define RDS_PID_HIGHER 0
#define RDS_OFFSET 5
#define RDS_PS_LENGTH_OFFSET 7
#define RDS_STRING 8
#define RDS_PS_DATA_OFFSET 8
#define RDS_CONFIG_OFFSET  3
#define RDS_AF_JUMP_OFFSET 4
#define PI_CODE_OFFSET 4
#define AF_SIZE_OFFSET 6
#define AF_LIST_OFFSET 7
#define RT_A_B_FLAG_OFFSET 4
/*FM states*/

enum radio_state_t {
	FM_OFF,
	FM_RECV,
	FM_TRANS,
	FM_RESET,
	FM_CALIB,
	FM_TURNING_OFF,
	FM_RECV_TURNING_ON,
	FM_TRANS_TURNING_ON,
	FM_MAX_NO_STATES,
};

enum emphasis_type {
	FM_RX_EMP75 = 0x0,
	FM_RX_EMP50 = 0x1
};

enum channel_space_type {
	FM_RX_SPACE_200KHZ = 0x0,
	FM_RX_SPACE_100KHZ = 0x1,
	FM_RX_SPACE_50KHZ = 0x2
};

enum high_low_injection {
	AUTO_HI_LO_INJECTION = 0x0,
	LOW_SIDE_INJECTION = 0x1,
	HIGH_SIDE_INJECTION = 0x2
};

enum fm_rds_type {
	FM_RX_RDBS_SYSTEM = 0x0,
	FM_RX_RDS_SYSTEM = 0x1
};

enum iris_region_t {
	IRIS_REGION_US,
	IRIS_REGION_EU,
	IRIS_REGION_JAPAN,
	IRIS_REGION_JAPAN_WIDE,
	IRIS_REGION_OTHER
};

#define STD_BUF_SIZE        (256)

enum iris_buf_t {
	IRIS_BUF_SRCH_LIST,
	IRIS_BUF_EVENTS,
	IRIS_BUF_RT_RDS,
	IRIS_BUF_PS_RDS,
	IRIS_BUF_RAW_RDS,
	IRIS_BUF_AF_LIST,
	IRIS_BUF_PEEK,
	IRIS_BUF_SSBI_PEEK,
	IRIS_BUF_RDS_CNTRS,
	IRIS_BUF_RD_DEFAULT,
	IRIS_BUF_CAL_DATA,
	IRIS_BUF_RT_PLUS,
	IRIS_BUF_ERT,
	IRIS_BUF_MAX,
};

enum iris_xfr_t {
	IRIS_XFR_SYNC,
	IRIS_XFR_ERROR,
	IRIS_XFR_SRCH_LIST,
	IRIS_XFR_RT_RDS,
	IRIS_XFR_PS_RDS,
	IRIS_XFR_AF_LIST,
	IRIS_XFR_MAX
};

#undef FMDBG
#ifdef FM_DEBUG
#define FMDBG(fmt, args...) pr_info("iris_radio: " fmt, ##args)
#else
#define FMDBG(fmt, args...)
#endif

#undef FMDERR
#define FMDERR(fmt, args...) pr_err("iris_radio: " fmt, ##args)

/* Search options */
enum search_t {
	SEEK,
	SCAN,
	SCAN_FOR_STRONG,
	SCAN_FOR_WEAK,
	RDS_SEEK_PTY,
	RDS_SCAN_PTY,
	RDS_SEEK_PI,
	RDS_AF_JUMP,
};

enum spur_entry_levels {
	ENTRY_0,
	ENTRY_1,
	ENTRY_2,
	ENTRY_3,
	ENTRY_4,
	ENTRY_5,
};

/* Band limits */
#define REGION_US_EU_BAND_LOW              87500
#define REGION_US_EU_BAND_HIGH             108000
#define REGION_JAPAN_STANDARD_BAND_LOW     76000
#define REGION_JAPAN_STANDARD_BAND_HIGH    90000
#define REGION_JAPAN_WIDE_BAND_LOW         90000
#define REGION_JAPAN_WIDE_BAND_HIGH        108000

#define SRCH_MODE	0x07
#define SRCH_DIR	0x08 /* 0-up 1-down */
#define SCAN_DWELL	0x70
#define SRCH_ON		0x80

/* I/O Control */
#define IOC_HRD_MUTE	0x03
#define IOC_SFT_MUTE	0x01
#define IOC_MON_STR	0x01
#define IOC_SIG_BLND	0x01
#define IOC_INTF_BLND	0x01
#define IOC_ANTENNA	0x01

/* RDS Control */
#define RDS_ON		0x01
#define RDS_BUF_SZ  100

/* constants */
#define  RDS_BLOCKS_NUM	(4)
#define BYTES_PER_BLOCK	(3)
#define MAX_PS_LENGTH	(108)
#define MAX_RT_LENGTH	(64)
#define RDS_GRP_CNTR_LEN (36)
#define RX_RT_DATA_LENGTH (63)
/* Search direction */
#define SRCH_DIR_UP		(0)
#define SRCH_DIR_DOWN		(1)

/*Search RDS stations*/
#define SEARCH_RDS_STNS_MODE_OFFSET 4

/*Search Station list */
#define PARAMS_PER_STATION 0x08
#define STN_NUM_OFFSET     0x01
#define STN_FREQ_OFFSET    0x02
#define KHZ_TO_MHZ         1000
#define GET_MSB(x)((x >> 8) & 0xFF)
#define GET_LSB(x)((x) & 0xFF)

/* control options */
#define CTRL_ON			(1)
#define CTRL_OFF		(0)

/*Diagnostic commands*/

#define RIVA_PEEK_OPCODE 0x0D
#define RIVA_POKE_OPCODE 0x0C

#define PEEK_DATA_OFSET 0x1
#define RIVA_PEEK_PARAM     0x6
#define RIVA_PEEK_LEN_OFSET  0x6
#define SSBI_PEEK_LEN    0x01
/*Calibration data*/
#define PROCS_CALIB_MODE  1
#define PROCS_CALIB_SIZE  23
#define DC_CALIB_MODE     2
#define DC_CALIB_SIZE     48
#define RSB_CALIB_MODE    3
#define RSB_CALIB_SIZE    4
#define CALIB_DATA_OFSET  2
#define CALIB_MODE_OFSET  1
#define MAX_CALIB_SIZE 75

/* Channel validity */
#define INVALID_CHANNEL		(0)
#define VALID_CHANNEL		(1)

struct hci_fm_set_cal_req_proc {
	__u8    mode;
	/*Max process calibration data size*/
	__u8    data[PROCS_CALIB_SIZE];
} __packed;

struct hci_fm_set_cal_req_dc {
	__u8    mode;
	/*Max DC calibration data size*/
	__u8    data[DC_CALIB_SIZE];
} __packed;

struct hci_cc_do_calibration_rsp {
	__u8 status;
	__u8 mode;
	__u8 data[MAX_CALIB_SIZE];
} __packed;

/* Low Power mode*/
#define SIG_LEVEL_INTR  (1 << 0)
#define RDS_SYNC_INTR   (1 << 1)
#define AUDIO_CTRL_INTR (1 << 2)
#define AF_JUMP_ENABLE  (1 << 4)

int hci_def_data_read(struct hci_fm_def_data_rd_req *arg,
	struct radio_hci_dev *hdev);
int hci_def_data_write(struct hci_fm_def_data_wr_req *arg,
	struct radio_hci_dev *hdev);
int hci_fm_do_calibration(__u8 *arg, struct radio_hci_dev *hdev);
int hci_fm_do_calibration(__u8 *arg, struct radio_hci_dev *hdev);

static inline int is_valid_tone(int tone)
{
	if ((tone >= MIN_TX_TONE_VAL) &&
		(tone <= MAX_TX_TONE_VAL))
		return 1;
	else
		return 0;
}

static inline int is_valid_hard_mute(int hard_mute)
{
	if ((hard_mute >= MIN_HARD_MUTE_VAL) &&
		(hard_mute <= MAX_HARD_MUTE_VAL))
		return 1;
	else
		return 0;
}

static inline int is_valid_srch_mode(int srch_mode)
{
	if ((srch_mode >= MIN_SRCH_MODE) &&
		(srch_mode <= MAX_SRCH_MODE))
		return 1;
	else
		return 0;
}

static inline int is_valid_scan_dwell_prd(int scan_dwell_prd)
{
	if ((scan_dwell_prd >= MIN_SCAN_DWELL) &&
		(scan_dwell_prd <= MAX_SCAN_DWELL))
		return 1;
	else
		return 0;
}

static inline int is_valid_sig_th(int sig_th)
{
	if ((sig_th >= MIN_SIG_TH) &&
		(sig_th <= MAX_SIG_TH))
		return 1;
	else
		return 0;
}

static inline int is_valid_pty(int pty)
{
	if ((pty >= MIN_PTY) &&
		(pty <= MAX_PTY))
		return 1;
	else
		return 0;
}

static inline int is_valid_pi(int pi)
{
	if ((pi >= MIN_PI) &&
		(pi <= MAX_PI))
		return 1;
	else
		return 0;
}

static inline int is_valid_srch_station_cnt(int cnt)
{
	if ((cnt >= MIN_SRCH_STATIONS_CNT) &&
		(cnt <= MAX_SRCH_STATIONS_CNT))
		return 1;
	else
		return 0;
}

static inline int is_valid_chan_spacing(int spacing)
{
	if ((spacing >= MIN_CHAN_SPACING) &&
		(spacing <= MAX_CHAN_SPACING))
		return 1;
	else
		return 0;
}

static inline int is_valid_emphasis(int emphasis)
{
	if ((emphasis >= MIN_EMPHASIS) &&
		(emphasis <= MAX_EMPHASIS))
		return 1;
	else
		return 0;
}

static inline int is_valid_rds_std(int rds_std)
{
	if ((rds_std >= MIN_RDS_STD) &&
		(rds_std <= MAX_RDS_STD))
		return 1;
	else
		return 0;
}

static inline int is_valid_antenna(int antenna_type)
{
	if ((antenna_type >= MIN_ANTENNA_VAL) &&
		(antenna_type <= MAX_ANTENNA_VAL))
		return 1;
	else
		return 0;
}

static inline int is_valid_ps_repeat_cnt(int cnt)
{
	if ((cnt >= MIN_TX_PS_REPEAT_CNT) &&
		(cnt <= MAX_TX_PS_REPEAT_CNT))
		return 1;
	else
		return 0;
}

static inline int is_valid_soft_mute(int soft_mute)
{
	if ((soft_mute >= MIN_SOFT_MUTE) &&
		(soft_mute <= MAX_SOFT_MUTE))
		return 1;
	else
		return 0;
}

static inline int is_valid_peek_len(int len)
{
	if ((len >= MIN_PEEK_ACCESS_LEN) &&
		(len <= MAX_PEEK_ACCESS_LEN))
		return 1;
	else
		return 0;
}

static inline int is_valid_reset_cntr(int cntr)
{
	if ((cntr >= MIN_RESET_CNTR) &&
		(cntr <= MAX_RESET_CNTR))
		return 1;
	else
		return 0;
}

static inline int is_valid_hlsi(int hlsi)
{
	if ((hlsi >= MIN_HLSI) &&
		(hlsi <= MAX_HLSI))
		return 1;
	else
		return 0;
}

static inline int is_valid_notch_filter(int filter)
{
	if ((filter >= MIN_NOTCH_FILTER) &&
		(filter <= MAX_NOTCH_FILTER))
		return 1;
	else
		return 0;
}

static inline int is_valid_intf_det_low_th(int th)
{
	if ((th >= MIN_INTF_DET_OUT_LW_TH) &&
		(th <= MAX_INTF_DET_OUT_LW_TH))
		return 1;
	else
		return 0;
}

static inline int is_valid_intf_det_hgh_th(int th)
{
	if ((th >= MIN_INTF_DET_OUT_HG_TH) &&
		(th <= MAX_INTF_DET_OUT_HG_TH))
		return 1;
	else
		return 0;
}

static inline int is_valid_sinr_th(int th)
{
	if ((th >= MIN_SINR_TH) &&
		(th <= MAX_SINR_TH))
		return 1;
	else
		return 0;
}

static inline int is_valid_sinr_samples(int samples_cnt)
{
	if ((samples_cnt >= MIN_SINR_SAMPLES) &&
		(samples_cnt <= MAX_SINR_SAMPLES))
		return 1;
	else
		return 0;
}

static inline int is_valid_fm_state(int state)
{
	if ((state >= 0) && (state < FM_MAX_NO_STATES))
		return 1;
	else
		return 0;
}
#endif /* __RADIO_HCI_CORE_H */
