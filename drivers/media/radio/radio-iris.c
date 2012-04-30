/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved
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

#define DRIVER_AUTHOR "Archana Ramchandran <archanar@codeaurora.org>"
#define DRIVER_NAME "radio-iris"
#define DRIVER_CARD "Qualcomm FM Radio Transceiver"
#define DRIVER_DESC "Driver for Qualcomm FM Radio Transceiver "

#include <linux/version.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/kfifo.h>
#include <linux/param.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/videodev2.h>
#include <linux/mutex.h>
#include <linux/unistd.h>
#include <linux/atomic.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/radio-iris.h>
#include <asm/unaligned.h>

static unsigned int rds_buf = 100;
module_param(rds_buf, uint, 0);
MODULE_PARM_DESC(rds_buf, "RDS buffer entries: *100*");

static void radio_hci_cmd_task(unsigned long arg);
static void radio_hci_rx_task(unsigned long arg);
static struct video_device *video_get_dev(void);
static DEFINE_RWLOCK(hci_task_lock);

struct iris_device {
	struct device *dev;
	struct kfifo data_buf[IRIS_BUF_MAX];

	int pending_xfrs[IRIS_XFR_MAX];
	int xfr_bytes_left;
	int xfr_in_progress;
	struct completion sync_xfr_start;
	int tune_req;
	unsigned int mode;

	__u16 pi;
	__u8 pty;
	__u8 ps_repeatcount;
	__u8 prev_trans_rds;
	__u8 af_jump_bit;
	struct video_device *videodev;

	struct mutex lock;
	spinlock_t buf_lock[IRIS_BUF_MAX];
	wait_queue_head_t event_queue;
	wait_queue_head_t read_queue;

	struct radio_hci_dev *fm_hdev;

	struct v4l2_capability *g_cap;
	struct v4l2_control *g_ctl;

	struct hci_fm_mute_mode_req mute_mode;
	struct hci_fm_stereo_mode_req stereo_mode;
	struct hci_fm_station_rsp fm_st_rsp;
	struct hci_fm_search_station_req srch_st;
	struct hci_fm_search_rds_station_req srch_rds;
	struct hci_fm_search_station_list_req srch_st_list;
	struct hci_fm_recv_conf_req recv_conf;
	struct hci_fm_trans_conf_req_struct trans_conf;
	struct hci_fm_rds_grp_req rds_grp;
	unsigned char g_search_mode;
	unsigned char power_mode;
	int search_on;
	unsigned int tone_freq;
	unsigned char g_scan_time;
	unsigned int g_antenna;
	unsigned int g_rds_grp_proc_ps;
	unsigned char event_mask;
	enum iris_region_t region;
	struct hci_fm_dbg_param_rsp st_dbg_param;
	struct hci_ev_srch_list_compl srch_st_result;
	struct hci_fm_riva_poke   riva_data_req;
	struct hci_fm_ssbi_req    ssbi_data_accs;
	struct hci_fm_ssbi_peek   ssbi_peek_reg;
	struct hci_fm_sig_threshold_rsp sig_th;
	struct hci_fm_ch_det_threshold ch_det_threshold;
	struct hci_fm_data_rd_rsp default_data;
};

static struct video_device *priv_videodev;
static int iris_do_calibration(struct iris_device *radio);

static struct v4l2_queryctrl iris_v4l2_queryctrl[] = {
	{
	.id	= V4L2_CID_AUDIO_VOLUME,
	.type	= V4L2_CTRL_TYPE_INTEGER,
	.name	= "Volume",
	.minimum	= 0,
	.maximum	= 15,
	.step	=	1,
	.default_value	=	15,
	},
	{
	.id	=	V4L2_CID_AUDIO_BALANCE,
	.flags	= V4L2_CTRL_FLAG_DISABLED,
	},
	{
	.id	=	V4L2_CID_AUDIO_BASS,
	.flags	=	V4L2_CTRL_FLAG_DISABLED,
	},
	{
	.id	=	V4L2_CID_AUDIO_TREBLE,
	.flags	=	V4L2_CTRL_FLAG_DISABLED,
	},
	{
	.id	=	V4L2_CID_AUDIO_MUTE,
	.type	=	V4L2_CTRL_TYPE_BOOLEAN,
	.name	=	"Mute",
	.minimum	=	0,
	.maximum	=	1,
	.step	=	1,
	.default_value	= 1,
	},
	{
	.id	=	V4L2_CID_AUDIO_LOUDNESS,
	.flags	=	V4L2_CTRL_FLAG_DISABLED,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_SRCHMODE,
	.type	=	V4L2_CTRL_TYPE_INTEGER,
	.name	=	"Search mode",
	.minimum	=	0,
	.maximum	= 7,
	.step	= 1,
	.default_value	= 0,
	},
	{
	.id	= V4L2_CID_PRIVATE_IRIS_SCANDWELL,
	.type	= V4L2_CTRL_TYPE_INTEGER,
	.name	=	"Search dwell time",
	.minimum	= 0,
	.maximum	= 7,
	.step	= 1,
	.default_value	= 0,
	},
	{
	.id	= V4L2_CID_PRIVATE_IRIS_SRCHON,
	.type	= V4L2_CTRL_TYPE_BOOLEAN,
	.name	= "Search on/off",
	.minimum	= 0,
	.maximum	= 1,
	.step	= 1,
	.default_value	= 1,

	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_STATE,
	.type	= V4L2_CTRL_TYPE_INTEGER,
	.name	= "radio 0ff/rx/tx/reset",
	.minimum	= 0,
	.maximum	= 3,
	.step	= 1,
	.default_value	=	1,

	},
	{
	.id	= V4L2_CID_PRIVATE_IRIS_REGION,
	.type	= V4L2_CTRL_TYPE_INTEGER,
	.name	=	"radio standard",
	.minimum	=	0,
	.maximum	=	2,
	.step	=	1,
	.default_value	=	0,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_SIGNAL_TH,
	.type	=	V4L2_CTRL_TYPE_INTEGER,
	.name	=	"Signal Threshold",
	.minimum	=	0x80,
	.maximum	=	0x7F,
	.step	=	1,
	.default_value	=	0,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_SRCH_PTY,
	.type	=	V4L2_CTRL_TYPE_INTEGER,
	.name	=	"Search PTY",
	.minimum	=	0,
	.maximum	=	31,
	.default_value	=	0,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_SRCH_PI,
	.type	=	V4L2_CTRL_TYPE_INTEGER,
	.name	=	"Search PI",
	.minimum	=	0,
	.maximum	=	0xFF,
	.default_value	=	0,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_SRCH_CNT,
	.type	=	V4L2_CTRL_TYPE_INTEGER,
	.name	=	"Preset num",
	.minimum	=	0,
	.maximum	=	12,
	.default_value	=	0,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_EMPHASIS,
	.type	=	V4L2_CTRL_TYPE_BOOLEAN,
	.name	=	"Emphasis",
	.minimum	=	0,
	.maximum	=	1,
	.default_value	=	0,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_RDS_STD,
	.type	=	V4L2_CTRL_TYPE_BOOLEAN,
	.name	=	"RDS standard",
	.minimum	=	0,
	.maximum	=	1,
	.default_value	=	0,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_SPACING,
	.type	=	V4L2_CTRL_TYPE_INTEGER,
	.name	=	"Channel spacing",
	.minimum	=	0,
	.maximum	=	2,
	.default_value	=	0,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_RDSON,
	.type	=	V4L2_CTRL_TYPE_BOOLEAN,
	.name	=	"RDS on/off",
	.minimum	=	0,
	.maximum	=	1,
	.default_value	=	0,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_RDSGROUP_MASK,
	.type	=	V4L2_CTRL_TYPE_INTEGER,
	.name	=	"RDS group mask",
	.minimum	=	0,
	.maximum	=	0xFFFFFFFF,
	.default_value	=	0,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_RDSGROUP_PROC,
	.type	=	V4L2_CTRL_TYPE_INTEGER,
	.name	=	"RDS processing",
	.minimum	=	0,
	.maximum	=	0xFF,
	.default_value	=	0,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_RDSD_BUF,
	.type	=	V4L2_CTRL_TYPE_INTEGER,
	.name	=	"RDS data groups to buffer",
	.minimum	=	1,
	.maximum	=	21,
	.default_value	=	0,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_PSALL,
	.type	=	V4L2_CTRL_TYPE_BOOLEAN,
	.name	=	"pass all ps strings",
	.minimum	=	0,
	.maximum	=	1,
	.default_value	=	0,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_LP_MODE,
	.type	=	V4L2_CTRL_TYPE_BOOLEAN,
	.name	=	"Low power mode",
	.minimum	=	0,
	.maximum	=	1,
	.default_value	=	0,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_ANTENNA,
	.type	=	V4L2_CTRL_TYPE_BOOLEAN,
	.name	=	"headset/internal",
	.minimum	=	0,
	.maximum	=	1,
	.default_value	=	0,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_TX_SETPSREPEATCOUNT,
	.type	=	V4L2_CTRL_TYPE_INTEGER,
	.name	=	"Set PS REPEATCOUNT",
	.minimum	=	0,
	.maximum	=	15,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_STOP_RDS_TX_PS_NAME,
	.type	=	V4L2_CTRL_TYPE_BOOLEAN,
	.name	=	"Stop PS NAME",
	.minimum	=	0,
	.maximum	=	1,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_STOP_RDS_TX_RT,
	.type	=	V4L2_CTRL_TYPE_BOOLEAN,
	.name	=	"Stop RT",
	.minimum	=	0,
	.maximum	=	1,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_SOFT_MUTE,
	.type	=	V4L2_CTRL_TYPE_BOOLEAN,
	.name	=	"Soft Mute",
	.minimum	=	0,
	.maximum	=	1,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_RIVA_ACCS_ADDR,
	.type	=	V4L2_CTRL_TYPE_BOOLEAN,
	.name	=	"Riva addr",
	.minimum	=	0x3180000,
	.maximum	=	0x31E0004,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_RIVA_ACCS_LEN,
	.type	=	V4L2_CTRL_TYPE_INTEGER,
	.name	=	"Data len",
	.minimum	=	0,
	.maximum	=	0xFF,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_RIVA_PEEK,
	.type	=	V4L2_CTRL_TYPE_BOOLEAN,
	.name	=	"Riva peek",
	.minimum	=	0,
	.maximum	=	1,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_RIVA_POKE,
	.type	=	V4L2_CTRL_TYPE_INTEGER,
	.name	=	"Riva poke",
	.minimum	=	0x3180000,
	.maximum	=	0x31E0004,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_SSBI_ACCS_ADDR,
	.type	=	V4L2_CTRL_TYPE_INTEGER,
	.name	=	"Ssbi addr",
	.minimum	=	0x280,
	.maximum	=	0x37F,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_SSBI_PEEK,
	.type	=	V4L2_CTRL_TYPE_INTEGER,
	.name	=	"Ssbi peek",
	.minimum	=	0,
	.maximum	=	0x37F,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_SSBI_POKE,
	.type	=	V4L2_CTRL_TYPE_INTEGER,
	.name	=	"ssbi poke",
	.minimum	=	0x01,
	.maximum	=	0xFF,
	},
	{
	.id =	 V4L2_CID_PRIVATE_IRIS_HLSI,
	.type	=	V4L2_CTRL_TYPE_INTEGER,
	.name	=	"set hlsi",
	.minimum	=	0,
	.maximum	=	2,
	},
	{
	.id =	 V4L2_CID_PRIVATE_IRIS_RDS_GRP_COUNTERS,
	.type	=	V4L2_CTRL_TYPE_BOOLEAN,
	.name	=	"RDS grp",
	.minimum	=	0,
	.maximum	=	1,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_SET_NOTCH_FILTER,
	.type	=	V4L2_CTRL_TYPE_INTEGER,
	.name	=	"Notch filter",
	.minimum	=	0,
	.maximum	=	2,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_READ_DEFAULT,
	.type	=	V4L2_CTRL_TYPE_INTEGER,
	.name	=	"Read default",
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_WRITE_DEFAULT,
	.type	=	V4L2_CTRL_TYPE_INTEGER,
	.name	=	"Write default",
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_SET_CALIBRATION,
	.type	=	V4L2_CTRL_TYPE_BOOLEAN,
	.name	=	"SET Calibration",
	.minimum	=	0,
	.maximum	=	1,
	},
	{
	.id	=	V4L2_CID_PRIVATE_IRIS_DO_CALIBRATION,
	.type	=	V4L2_CTRL_TYPE_BOOLEAN,
	.name	=	"SET Calibration",
	.minimum	=	0,
	.maximum	=	1,
	},
	{
	.id     =       V4L2_CID_PRIVATE_IRIS_GET_SINR,
	.type   =       V4L2_CTRL_TYPE_INTEGER,
	.name   =       "GET SINR",
	.minimum        =       -128,
	.maximum        =       127,
	},
	{
	.id     =       V4L2_CID_PRIVATE_INTF_HIGH_THRESHOLD,
	.type   =       V4L2_CTRL_TYPE_INTEGER,
	.name   =       "Intf High Threshold",
	.minimum        =       0,
	.maximum        =       0xFF,
	.default_value  =       0,
	},
	{
	.id     =       V4L2_CID_PRIVATE_INTF_LOW_THRESHOLD,
	.type   =       V4L2_CTRL_TYPE_INTEGER,
	.name   =       "Intf low Threshold",
	.minimum        =       0,
	.maximum        =       0xFF,
	.default_value  =       0,
	},
	{
	.id     =       V4L2_CID_PRIVATE_SINR_THRESHOLD,
	.type   =       V4L2_CTRL_TYPE_INTEGER,
	.name   =       "SINR Threshold",
	.minimum        =       -128,
	.maximum        =       127,
	.default_value  =       0,
	},
	{
	.id     =       V4L2_CID_PRIVATE_SINR_SAMPLES,
	.type   =       V4L2_CTRL_TYPE_INTEGER,
	.name   =       "SINR samples",
	.minimum        =       1,
	.maximum        =       0xFF,
	.default_value  =       0,
	},
};

static void iris_q_event(struct iris_device *radio,
				enum iris_evt_t event)
{
	struct kfifo *data_b = &radio->data_buf[IRIS_BUF_EVENTS];
	unsigned char evt = event;
	if (kfifo_in_locked(data_b, &evt, 1, &radio->buf_lock[IRIS_BUF_EVENTS]))
		wake_up_interruptible(&radio->event_queue);
}

static int hci_send_frame(struct sk_buff *skb)
{
	struct radio_hci_dev *hdev = (struct radio_hci_dev *) skb->dev;

	if (!hdev) {
		kfree_skb(skb);
		return -ENODEV;
	}

	__net_timestamp(skb);

	skb_orphan(skb);
	return hdev->send(skb);
}

static void radio_hci_cmd_task(unsigned long arg)
{
	struct radio_hci_dev *hdev = (struct radio_hci_dev *) arg;
	struct sk_buff *skb;
	if (!(atomic_read(&hdev->cmd_cnt))
		&& time_after(jiffies, hdev->cmd_last_tx + HZ)) {
		FMDERR("%s command tx timeout", hdev->name);
		atomic_set(&hdev->cmd_cnt, 1);
	}

	skb = skb_dequeue(&hdev->cmd_q);
	if (atomic_read(&hdev->cmd_cnt) && skb) {
		kfree_skb(hdev->sent_cmd);
		hdev->sent_cmd = skb_clone(skb, GFP_ATOMIC);
		if (hdev->sent_cmd) {
			atomic_dec(&hdev->cmd_cnt);
			hci_send_frame(skb);
			hdev->cmd_last_tx = jiffies;
		} else {
			skb_queue_head(&hdev->cmd_q, skb);
			tasklet_schedule(&hdev->cmd_task);
		}
	}

}

static void radio_hci_rx_task(unsigned long arg)
{
	struct radio_hci_dev *hdev = (struct radio_hci_dev *) arg;
	struct sk_buff *skb;

	read_lock(&hci_task_lock);

	skb = skb_dequeue(&hdev->rx_q);
	radio_hci_event_packet(hdev, skb);

	read_unlock(&hci_task_lock);
}

int radio_hci_register_dev(struct radio_hci_dev *hdev)
{
	struct iris_device *radio = video_get_drvdata(video_get_dev());
	if (!radio) {
		FMDERR(":radio is null");
		return -EINVAL;
	}

	if (!hdev) {
		FMDERR("hdev is null");
		return -EINVAL;
	}

	hdev->flags = 0;

	tasklet_init(&hdev->cmd_task, radio_hci_cmd_task, (unsigned long)
		hdev);
	tasklet_init(&hdev->rx_task, radio_hci_rx_task, (unsigned long)
		hdev);

	init_waitqueue_head(&hdev->req_wait_q);

	skb_queue_head_init(&hdev->rx_q);
	skb_queue_head_init(&hdev->cmd_q);
	skb_queue_head_init(&hdev->raw_q);

	if (!radio)
		FMDERR(":radio is null");

	radio->fm_hdev = hdev;

	return 0;
}
EXPORT_SYMBOL(radio_hci_register_dev);

int radio_hci_unregister_dev(struct radio_hci_dev *hdev)
{
	struct iris_device *radio = video_get_drvdata(video_get_dev());
	if (!radio) {
		FMDERR(":radio is null");
		return -EINVAL;
	}

	tasklet_kill(&hdev->rx_task);
	tasklet_kill(&hdev->cmd_task);
	skb_queue_purge(&hdev->rx_q);
	skb_queue_purge(&hdev->cmd_q);
	skb_queue_purge(&hdev->raw_q);
	kfree(radio->fm_hdev);
	kfree(radio->videodev);

	return 0;
}
EXPORT_SYMBOL(radio_hci_unregister_dev);

int radio_hci_recv_frame(struct sk_buff *skb)
{
	struct radio_hci_dev *hdev = (struct radio_hci_dev *) skb->dev;
	if (!hdev) {
		FMDERR("%s hdev is null while receiving frame", hdev->name);
		kfree_skb(skb);
		return -ENXIO;
	}

	__net_timestamp(skb);

	radio_hci_event_packet(hdev, skb);
	kfree_skb(skb);
	return 0;
}
EXPORT_SYMBOL(radio_hci_recv_frame);

int radio_hci_send_cmd(struct radio_hci_dev *hdev, __u16 opcode, __u32 plen,
		void *param)
{
	int len = RADIO_HCI_COMMAND_HDR_SIZE + plen;
	struct radio_hci_command_hdr *hdr;
	struct sk_buff *skb;
	int ret = 0;

	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb) {
		FMDERR("%s no memory for command", hdev->name);
		return -ENOMEM;
	}

	hdr = (struct radio_hci_command_hdr *) skb_put(skb,
		RADIO_HCI_COMMAND_HDR_SIZE);
	hdr->opcode = cpu_to_le16(opcode);
	hdr->plen   = plen;

	if (plen)
		memcpy(skb_put(skb, plen), param, plen);

	skb->dev = (void *) hdev;

	ret = hci_send_frame(skb);

	return ret;
}
EXPORT_SYMBOL(radio_hci_send_cmd);

static int hci_fm_enable_recv_req(struct radio_hci_dev *hdev,
	unsigned long param)
{
	__u16 opcode = 0;

	opcode = hci_opcode_pack(HCI_OGF_FM_RECV_CTRL_CMD_REQ,
		HCI_OCF_FM_ENABLE_RECV_REQ);
	return radio_hci_send_cmd(hdev, opcode, 0, NULL);
}

static int hci_fm_tone_generator(struct radio_hci_dev *hdev,
	unsigned long param)
{
	struct iris_device *radio = video_get_drvdata(video_get_dev());
	__u16 opcode = 0;

	opcode = hci_opcode_pack(HCI_OGF_FM_DIAGNOSTIC_CMD_REQ,
		HCI_FM_SET_INTERNAL_TONE_GENRATOR);
	return radio_hci_send_cmd(hdev, opcode,
			sizeof(radio->tone_freq), &radio->tone_freq);
}

static int hci_fm_enable_trans_req(struct radio_hci_dev *hdev,
	unsigned long param)
{
	__u16 opcode = 0;

	opcode = hci_opcode_pack(HCI_OGF_FM_TRANS_CTRL_CMD_REQ,
		HCI_OCF_FM_ENABLE_TRANS_REQ);
	return radio_hci_send_cmd(hdev, opcode, 0, NULL);
}

static int hci_fm_disable_recv_req(struct radio_hci_dev *hdev,
	unsigned long param)
{
	__u16 opcode = 0;

	opcode = hci_opcode_pack(HCI_OGF_FM_RECV_CTRL_CMD_REQ,
		HCI_OCF_FM_DISABLE_RECV_REQ);
	return radio_hci_send_cmd(hdev, opcode, 0, NULL);
}

static int hci_fm_disable_trans_req(struct radio_hci_dev *hdev,
	unsigned long param)
{
	__u16 opcode = 0;

	opcode = hci_opcode_pack(HCI_OGF_FM_TRANS_CTRL_CMD_REQ,
		HCI_OCF_FM_DISABLE_TRANS_REQ);
	return radio_hci_send_cmd(hdev, opcode, 0, NULL);
}

static int hci_get_fm_recv_conf_req(struct radio_hci_dev *hdev,
	unsigned long param)
{
	__u16 opcode = 0;

	opcode = hci_opcode_pack(HCI_OGF_FM_RECV_CTRL_CMD_REQ,
		HCI_OCF_FM_GET_RECV_CONF_REQ);
	return radio_hci_send_cmd(hdev, opcode, 0, NULL);
}

static int hci_get_fm_trans_conf_req(struct radio_hci_dev *hdev,
	unsigned long param)
{
	u16 opcode = 0;

	opcode = hci_opcode_pack(HCI_OGF_FM_TRANS_CTRL_CMD_REQ,
		HCI_OCF_FM_GET_TRANS_CONF_REQ);
	return radio_hci_send_cmd(hdev, opcode, 0, NULL);
}
static int hci_set_fm_recv_conf_req(struct radio_hci_dev *hdev,
	unsigned long param)
{
	__u16 opcode = 0;

	struct hci_fm_recv_conf_req *recv_conf_req =
		(struct hci_fm_recv_conf_req *) param;

	opcode = hci_opcode_pack(HCI_OGF_FM_RECV_CTRL_CMD_REQ,
		HCI_OCF_FM_SET_RECV_CONF_REQ);
	return radio_hci_send_cmd(hdev, opcode, sizeof((*recv_conf_req)),
		recv_conf_req);
}

static int hci_set_fm_trans_conf_req(struct radio_hci_dev *hdev,
	unsigned long param)
{
	__u16 opcode = 0;

	struct hci_fm_trans_conf_req_struct *trans_conf_req =
		(struct hci_fm_trans_conf_req_struct *) param;

	opcode = hci_opcode_pack(HCI_OGF_FM_TRANS_CTRL_CMD_REQ,
		HCI_OCF_FM_SET_TRANS_CONF_REQ);
	return radio_hci_send_cmd(hdev, opcode, sizeof((*trans_conf_req)),
		trans_conf_req);
}

static int hci_fm_get_station_param_req(struct radio_hci_dev *hdev,
		unsigned long param)
{
	__u16 opcode = 0;

	opcode = hci_opcode_pack(HCI_OGF_FM_RECV_CTRL_CMD_REQ,
		HCI_OCF_FM_GET_STATION_PARAM_REQ);
	return radio_hci_send_cmd(hdev, opcode, 0, NULL);
}

static int hci_set_fm_mute_mode_req(struct radio_hci_dev *hdev,
		unsigned long param)
{
	__u16 opcode = 0;
	struct hci_fm_mute_mode_req *mute_mode_req =
		(struct hci_fm_mute_mode_req *) param;

	opcode = hci_opcode_pack(HCI_OGF_FM_RECV_CTRL_CMD_REQ,
		HCI_OCF_FM_SET_MUTE_MODE_REQ);
	return radio_hci_send_cmd(hdev, opcode, sizeof((*mute_mode_req)),
		mute_mode_req);
}


static int hci_trans_ps_req(struct radio_hci_dev *hdev,
		unsigned long param)
{
	__u16 opcode = 0;
	struct hci_fm_tx_ps *tx_ps_req =
		(struct hci_fm_tx_ps *) param;

	opcode = hci_opcode_pack(HCI_OGF_FM_TRANS_CTRL_CMD_REQ,
		HCI_OCF_FM_RDS_PS_REQ);

	return radio_hci_send_cmd(hdev, opcode, sizeof((*tx_ps_req)),
		tx_ps_req);
}

static int hci_trans_rt_req(struct radio_hci_dev *hdev,
		unsigned long param)
{
	__u16 opcode = 0;
	struct hci_fm_tx_rt *tx_rt_req =
		(struct hci_fm_tx_rt *) param;

	opcode = hci_opcode_pack(HCI_OGF_FM_TRANS_CTRL_CMD_REQ,
		HCI_OCF_FM_RDS_RT_REQ);

	return radio_hci_send_cmd(hdev, opcode, sizeof((*tx_rt_req)),
		tx_rt_req);
}

static int hci_set_fm_stereo_mode_req(struct radio_hci_dev *hdev,
		unsigned long param)
{
	__u16 opcode = 0;
	struct hci_fm_stereo_mode_req *stereo_mode_req =
		(struct hci_fm_stereo_mode_req *) param;
	opcode = hci_opcode_pack(HCI_OGF_FM_RECV_CTRL_CMD_REQ,
		HCI_OCF_FM_SET_STEREO_MODE_REQ);
	return radio_hci_send_cmd(hdev, opcode, sizeof((*stereo_mode_req)),
		stereo_mode_req);
}

static int hci_fm_set_antenna_req(struct radio_hci_dev *hdev,
	unsigned long param)
{
	__u16 opcode = 0;

	__u8 antenna = param;

	opcode = hci_opcode_pack(HCI_OGF_FM_RECV_CTRL_CMD_REQ,
		HCI_OCF_FM_SET_ANTENNA);
	return radio_hci_send_cmd(hdev, opcode, sizeof(antenna), &antenna);
}

static int hci_fm_set_sig_threshold_req(struct radio_hci_dev *hdev,
		unsigned long param)
{
	__u16 opcode = 0;

	__u8 sig_threshold = param;

	opcode = hci_opcode_pack(HCI_OGF_FM_RECV_CTRL_CMD_REQ,
		HCI_OCF_FM_SET_SIGNAL_THRESHOLD);
	return radio_hci_send_cmd(hdev, opcode, sizeof(sig_threshold),
		&sig_threshold);
}

static int hci_fm_set_event_mask(struct radio_hci_dev *hdev,
		unsigned long param)
{
	u16 opcode = 0;
	u8 event_mask = param;

	opcode = hci_opcode_pack(HCI_OGF_FM_RECV_CTRL_CMD_REQ,
		HCI_OCF_FM_SET_EVENT_MASK);
	return radio_hci_send_cmd(hdev, opcode, sizeof(event_mask),
		&event_mask);
}
static int hci_fm_get_sig_threshold_req(struct radio_hci_dev *hdev,
		unsigned long param)
{
	__u16 opcode = 0;

	opcode = hci_opcode_pack(HCI_OGF_FM_RECV_CTRL_CMD_REQ,
		HCI_OCF_FM_GET_SIGNAL_THRESHOLD);
	return radio_hci_send_cmd(hdev, opcode, 0, NULL);
}

static int hci_fm_get_program_service_req(struct radio_hci_dev *hdev,
		unsigned long param)
{
	__u16 opcode = 0;

	opcode = hci_opcode_pack(HCI_OGF_FM_RECV_CTRL_CMD_REQ,
		HCI_OCF_FM_GET_PROGRAM_SERVICE_REQ);
	return radio_hci_send_cmd(hdev, opcode, 0, NULL);
}

static int hci_fm_get_radio_text_req(struct radio_hci_dev *hdev,
		unsigned long param)
{
	__u16 opcode = 0;

	opcode = hci_opcode_pack(HCI_OGF_FM_RECV_CTRL_CMD_REQ,
		HCI_OCF_FM_GET_RADIO_TEXT_REQ);
	return radio_hci_send_cmd(hdev, opcode, 0, NULL);
}

static int hci_fm_get_af_list_req(struct radio_hci_dev *hdev,
	unsigned long param)
{
	__u16 opcode = 0;

	opcode = hci_opcode_pack(HCI_OGF_FM_RECV_CTRL_CMD_REQ,
		HCI_OCF_FM_GET_AF_LIST_REQ);
	return radio_hci_send_cmd(hdev, opcode, 0, NULL);
}

static int hci_fm_search_stations_req(struct radio_hci_dev *hdev,
		unsigned long param)
{
	__u16 opcode = 0;
	struct hci_fm_search_station_req *srch_stations =
		(struct hci_fm_search_station_req *) param;

	opcode = hci_opcode_pack(HCI_OGF_FM_RECV_CTRL_CMD_REQ,
		HCI_OCF_FM_SEARCH_STATIONS);
	return radio_hci_send_cmd(hdev, opcode, sizeof((*srch_stations)),
		srch_stations);
}

static int hci_fm_srch_rds_stations_req(struct radio_hci_dev *hdev,
		unsigned long param)
{
	__u16 opcode = 0;
	struct hci_fm_search_rds_station_req *srch_stations =
		(struct hci_fm_search_rds_station_req *) param;

	opcode = hci_opcode_pack(HCI_OGF_FM_RECV_CTRL_CMD_REQ,
		HCI_OCF_FM_SEARCH_RDS_STATIONS);
	return radio_hci_send_cmd(hdev, opcode, sizeof((*srch_stations)),
		srch_stations);
}

static int hci_fm_srch_station_list_req(struct radio_hci_dev *hdev,
		unsigned long param)
{
	__u16 opcode = 0;
	struct hci_fm_search_station_list_req *srch_list =
		(struct hci_fm_search_station_list_req *) param;

	opcode = hci_opcode_pack(HCI_OGF_FM_RECV_CTRL_CMD_REQ,
		HCI_OCF_FM_SEARCH_STATIONS_LIST);
	return radio_hci_send_cmd(hdev, opcode, sizeof((*srch_list)),
		srch_list);
}

static int hci_fm_cancel_search_req(struct radio_hci_dev *hdev,
		unsigned long param)
{
	__u16 opcode = 0;

	opcode = hci_opcode_pack(HCI_OGF_FM_RECV_CTRL_CMD_REQ,
		HCI_OCF_FM_CANCEL_SEARCH);
	return radio_hci_send_cmd(hdev, opcode, 0, NULL);
}

static int hci_fm_rds_grp_process_req(struct radio_hci_dev *hdev,
		unsigned long param)
{
	__u16 opcode = 0;

	__u32 fm_grps_process = param;

	opcode = hci_opcode_pack(HCI_OGF_FM_RECV_CTRL_CMD_REQ,
		HCI_OCF_FM_RDS_GRP_PROCESS);
	return radio_hci_send_cmd(hdev, opcode, sizeof(fm_grps_process),
		&fm_grps_process);
}

static int hci_fm_tune_station_req(struct radio_hci_dev *hdev,
	unsigned long param)
{
	__u16 opcode = 0;

	__u32 tune_freq = param;

	opcode = hci_opcode_pack(HCI_OGF_FM_COMMON_CTRL_CMD_REQ,
		HCI_OCF_FM_TUNE_STATION_REQ);
	return radio_hci_send_cmd(hdev, opcode, sizeof(tune_freq), &tune_freq);
}

static int hci_def_data_read_req(struct radio_hci_dev *hdev,
	unsigned long param)
{
	__u16 opcode = 0;
	struct hci_fm_def_data_rd_req *def_data_rd =
		(struct hci_fm_def_data_rd_req *) param;

	opcode = hci_opcode_pack(HCI_OGF_FM_COMMON_CTRL_CMD_REQ,
		HCI_OCF_FM_DEFAULT_DATA_READ);
	return radio_hci_send_cmd(hdev, opcode, sizeof((*def_data_rd)),
	def_data_rd);
}

static int hci_def_data_write_req(struct radio_hci_dev *hdev,
	unsigned long param)
{
	__u16 opcode = 0;
	struct hci_fm_def_data_wr_req *def_data_wr =
		(struct hci_fm_def_data_wr_req *) param;

	opcode = hci_opcode_pack(HCI_OGF_FM_COMMON_CTRL_CMD_REQ,
		HCI_OCF_FM_DEFAULT_DATA_WRITE);

	return radio_hci_send_cmd(hdev, opcode, (def_data_wr->length+2),
	def_data_wr);
}

static int hci_set_notch_filter_req(struct radio_hci_dev *hdev,
	unsigned long param)
{
	__u16 opcode = 0;
	__u8 notch_filter_val = param;

	opcode = hci_opcode_pack(HCI_OGF_FM_RECV_CTRL_CMD_REQ,
		HCI_OCF_FM_EN_NOTCH_CTRL);
	return radio_hci_send_cmd(hdev, opcode, sizeof(notch_filter_val),
	&notch_filter_val);
}



static int hci_fm_reset_req(struct radio_hci_dev *hdev, unsigned long param)
{
	__u16 opcode = 0;

	opcode = hci_opcode_pack(HCI_OGF_FM_COMMON_CTRL_CMD_REQ,
		HCI_OCF_FM_RESET);
	return radio_hci_send_cmd(hdev, opcode, 0, NULL);
}

static int hci_fm_get_feature_lists_req(struct radio_hci_dev *hdev,
		unsigned long param)
{
	__u16 opcode = 0;

	opcode = hci_opcode_pack(HCI_OGF_FM_COMMON_CTRL_CMD_REQ,
		HCI_OCF_FM_GET_FEATURE_LIST);
	return radio_hci_send_cmd(hdev, opcode, 0, NULL);
}

static int hci_fm_do_calibration_req(struct radio_hci_dev *hdev,
		unsigned long param)
{
	__u16 opcode = 0;

	__u8 mode = param;

	opcode = hci_opcode_pack(HCI_OGF_FM_COMMON_CTRL_CMD_REQ,
		HCI_OCF_FM_DO_CALIBRATION);
	return radio_hci_send_cmd(hdev, opcode, sizeof(mode), &mode);
}

static int hci_read_grp_counters_req(struct radio_hci_dev *hdev,
		unsigned long param)
{
	__u16 opcode = 0;

	__u8 reset_counters = param;
	opcode = hci_opcode_pack(HCI_OGF_FM_STATUS_PARAMETERS_CMD_REQ,
		HCI_OCF_FM_READ_GRP_COUNTERS);
	return radio_hci_send_cmd(hdev, opcode, sizeof(reset_counters),
		&reset_counters);
}

static int hci_peek_data_req(struct radio_hci_dev *hdev, unsigned long param)
{
	__u16 opcode = 0;
	struct hci_fm_riva_data *peek_data = (struct hci_fm_riva_data *)param;

	opcode = hci_opcode_pack(HCI_OGF_FM_DIAGNOSTIC_CMD_REQ,
		HCI_OCF_FM_PEEK_DATA);
	return radio_hci_send_cmd(hdev, opcode, sizeof((*peek_data)),
	peek_data);
}

static int hci_poke_data_req(struct radio_hci_dev *hdev, unsigned long param)
{
	__u16 opcode = 0;
	struct hci_fm_riva_poke *poke_data = (struct hci_fm_riva_poke *) param;

	opcode = hci_opcode_pack(HCI_OGF_FM_DIAGNOSTIC_CMD_REQ,
		HCI_OCF_FM_POKE_DATA);
	return radio_hci_send_cmd(hdev, opcode, sizeof((*poke_data)),
	poke_data);
}

static int hci_ssbi_peek_reg_req(struct radio_hci_dev *hdev,
	unsigned long param)
{
	__u16 opcode = 0;
	struct hci_fm_ssbi_peek *ssbi_peek = (struct hci_fm_ssbi_peek *) param;

	opcode = hci_opcode_pack(HCI_OGF_FM_DIAGNOSTIC_CMD_REQ,
		HCI_OCF_FM_SSBI_PEEK_REG);
	return radio_hci_send_cmd(hdev, opcode, sizeof((*ssbi_peek)),
	ssbi_peek);
}

static int hci_ssbi_poke_reg_req(struct radio_hci_dev *hdev,
	unsigned long param)
{
	__u16 opcode = 0;
	struct hci_fm_ssbi_req *ssbi_poke = (struct hci_fm_ssbi_req *) param;

	opcode = hci_opcode_pack(HCI_OGF_FM_DIAGNOSTIC_CMD_REQ,
		HCI_OCF_FM_SSBI_POKE_REG);
	return radio_hci_send_cmd(hdev, opcode, sizeof((*ssbi_poke)),
	ssbi_poke);
}

static int hci_fm_get_station_dbg_param_req(struct radio_hci_dev *hdev,
		unsigned long param)
{
	__u16 opcode = 0;

	opcode = hci_opcode_pack(HCI_OGF_FM_DIAGNOSTIC_CMD_REQ,
		HCI_OCF_FM_STATION_DBG_PARAM);
	return radio_hci_send_cmd(hdev, opcode, 0, NULL);
}

static int hci_fm_set_ch_det_th(struct radio_hci_dev *hdev,
	unsigned long param)
{
	struct hci_fm_ch_det_threshold *ch_det_th =
			 (struct hci_fm_ch_det_threshold *) param;
	u16 opcode = hci_opcode_pack(HCI_OGF_FM_RECV_CTRL_CMD_REQ,
		HCI_OCF_FM_SET_CH_DET_THRESHOLD);
	return radio_hci_send_cmd(hdev, opcode, sizeof((*ch_det_th)),
		ch_det_th);
}

static int hci_fm_get_ch_det_th(struct radio_hci_dev *hdev,
		unsigned long param)
{
	u16 opcode = hci_opcode_pack(HCI_OGF_FM_RECV_CTRL_CMD_REQ,
			HCI_OCF_FM_GET_CH_DET_THRESHOLD);
	return radio_hci_send_cmd(hdev, opcode, 0, NULL);
}

static int radio_hci_err(__u16 code)
{
	switch (code) {
	case 0:
		return 0;
	case 0x01:
		return -EBADRQC;
	case 0x02:
		return -ENOTCONN;
	case 0x03:
		return -EIO;
	case 0x07:
		return -ENOMEM;
	case 0x0c:
		return -EBUSY;
	case 0x11:
		return -EOPNOTSUPP;
	case 0x12:
		return -EINVAL;
	default:
		return -ENOSYS;
	}
}

static int __radio_hci_request(struct radio_hci_dev *hdev,
		int (*req)(struct radio_hci_dev *hdev,
			unsigned long param),
			unsigned long param, __u32 timeout)
{
	int err = 0;

	DECLARE_WAITQUEUE(wait, current);

	hdev->req_status = HCI_REQ_PEND;

	add_wait_queue(&hdev->req_wait_q, &wait);
	set_current_state(TASK_INTERRUPTIBLE);

	err = req(hdev, param);

	schedule_timeout(timeout);

	remove_wait_queue(&hdev->req_wait_q, &wait);

	if (signal_pending(current))
		return -EINTR;

	switch (hdev->req_status) {
	case HCI_REQ_DONE:
	case HCI_REQ_STATUS:
		err = radio_hci_err(hdev->req_result);
		break;

	case HCI_REQ_CANCELED:
		err = -hdev->req_result;
		break;

	default:
		err = -ETIMEDOUT;
		break;
	}

	hdev->req_status = hdev->req_result = 0;

	return err;
}

static inline int radio_hci_request(struct radio_hci_dev *hdev,
		int (*req)(struct
		radio_hci_dev * hdev, unsigned long param),
		unsigned long param, __u32 timeout)
{
	int ret = 0;

	ret = __radio_hci_request(hdev, req, param, timeout);

	return ret;
}

static inline int hci_conf_event_mask(__u8 *arg,
		struct radio_hci_dev *hdev)
{
	u8 event_mask = *arg;
	return  radio_hci_request(hdev, hci_fm_set_event_mask,
				event_mask, RADIO_HCI_TIMEOUT);
}
static int hci_set_fm_recv_conf(struct hci_fm_recv_conf_req *arg,
		struct radio_hci_dev *hdev)
{
	int ret = 0;
	struct hci_fm_recv_conf_req *set_recv_conf = arg;

	ret = radio_hci_request(hdev, hci_set_fm_recv_conf_req, (unsigned
		long)set_recv_conf, RADIO_HCI_TIMEOUT);

	return ret;
}

static int hci_set_fm_trans_conf(struct hci_fm_trans_conf_req_struct *arg,
		struct radio_hci_dev *hdev)
{
	int ret = 0;
	struct hci_fm_trans_conf_req_struct *set_trans_conf = arg;

	ret = radio_hci_request(hdev, hci_set_fm_trans_conf_req, (unsigned
		long)set_trans_conf, RADIO_HCI_TIMEOUT);

	return ret;
}

static int hci_fm_tune_station(__u32 *arg, struct radio_hci_dev *hdev)
{
	int ret = 0;
	__u32 tune_freq = *arg;

	ret = radio_hci_request(hdev, hci_fm_tune_station_req, tune_freq,
		RADIO_HCI_TIMEOUT);

	return ret;
}

static int hci_set_fm_mute_mode(struct hci_fm_mute_mode_req *arg,
	struct radio_hci_dev *hdev)
{
	int ret = 0;
	struct hci_fm_mute_mode_req *set_mute_conf = arg;

	ret = radio_hci_request(hdev, hci_set_fm_mute_mode_req, (unsigned
		long)set_mute_conf, RADIO_HCI_TIMEOUT);

	return ret;
}

static int hci_set_fm_stereo_mode(struct hci_fm_stereo_mode_req *arg,
	struct radio_hci_dev *hdev)
{
	int ret = 0;
	struct hci_fm_stereo_mode_req *set_stereo_conf = arg;

	ret = radio_hci_request(hdev, hci_set_fm_stereo_mode_req, (unsigned
		long)set_stereo_conf, RADIO_HCI_TIMEOUT);

	return ret;
}

static int hci_fm_set_antenna(__u8 *arg, struct radio_hci_dev *hdev)
{
	int ret = 0;
	__u8 antenna = *arg;

	ret = radio_hci_request(hdev, hci_fm_set_antenna_req, antenna,
		RADIO_HCI_TIMEOUT);

	return ret;
}

static int hci_fm_set_signal_threshold(__u8 *arg,
	struct radio_hci_dev *hdev)
{
	int ret = 0;
	__u8 sig_threshold = *arg;

	ret = radio_hci_request(hdev, hci_fm_set_sig_threshold_req,
		sig_threshold, RADIO_HCI_TIMEOUT);

	return ret;
}

static int hci_fm_search_stations(struct hci_fm_search_station_req *arg,
	struct radio_hci_dev *hdev)
{
	int ret = 0;
	struct hci_fm_search_station_req *srch_stations = arg;

	ret = radio_hci_request(hdev, hci_fm_search_stations_req, (unsigned
		long)srch_stations, RADIO_HCI_TIMEOUT);

	return ret;
}

static int hci_fm_search_rds_stations(struct hci_fm_search_rds_station_req *arg,
	struct radio_hci_dev *hdev)
{
	int ret = 0;
	struct hci_fm_search_rds_station_req *srch_stations = arg;

	ret = radio_hci_request(hdev, hci_fm_srch_rds_stations_req, (unsigned
		long)srch_stations, RADIO_HCI_TIMEOUT);

	return ret;
}

static int hci_fm_search_station_list
	(struct hci_fm_search_station_list_req *arg,
	struct radio_hci_dev *hdev)
{
	int ret = 0;
	struct hci_fm_search_station_list_req *srch_list = arg;

	ret = radio_hci_request(hdev, hci_fm_srch_station_list_req, (unsigned
		long)srch_list, RADIO_HCI_TIMEOUT);

	return ret;
}

static int hci_fm_rds_grp(struct hci_fm_rds_grp_req *arg,
	struct radio_hci_dev *hdev)
{
	return 0;
}

static int hci_fm_rds_grps_process(__u32 *arg, struct radio_hci_dev *hdev)
{
	int ret = 0;
	__u32 fm_grps_process = *arg;

	ret = radio_hci_request(hdev, hci_fm_rds_grp_process_req,
		fm_grps_process, RADIO_HCI_TIMEOUT);

	return ret;
}

int hci_def_data_read(struct hci_fm_def_data_rd_req *arg,
	struct radio_hci_dev *hdev)
{
	int ret = 0;
	struct hci_fm_def_data_rd_req *def_data_rd = arg;
	ret = radio_hci_request(hdev, hci_def_data_read_req, (unsigned
		long)def_data_rd, RADIO_HCI_TIMEOUT);

	return ret;
}

int hci_def_data_write(struct hci_fm_def_data_wr_req *arg,
	struct radio_hci_dev *hdev)
{
	int ret = 0;
	struct hci_fm_def_data_wr_req *def_data_wr = arg;
	ret = radio_hci_request(hdev, hci_def_data_write_req, (unsigned
		long)def_data_wr, RADIO_HCI_TIMEOUT);

	return ret;
}

int hci_fm_do_calibration(__u8 *arg, struct radio_hci_dev *hdev)
{
	int ret = 0;
	__u8 mode = *arg;

	ret = radio_hci_request(hdev, hci_fm_do_calibration_req, mode,
		RADIO_HCI_TIMEOUT);

	return ret;
}

static int hci_read_grp_counters(__u8 *arg, struct radio_hci_dev *hdev)
{
	int ret = 0;
	__u8 reset_counters = *arg;
	ret = radio_hci_request(hdev, hci_read_grp_counters_req,
		reset_counters, RADIO_HCI_TIMEOUT);

	return ret;
}

static int hci_set_notch_filter(__u8 *arg, struct radio_hci_dev *hdev)
{
	int ret = 0;
	__u8 notch_filter = *arg;
	ret = radio_hci_request(hdev, hci_set_notch_filter_req,
		notch_filter, RADIO_HCI_TIMEOUT);

	return ret;
}

static int hci_peek_data(struct hci_fm_riva_data *arg,
				struct radio_hci_dev *hdev)
{
	int ret = 0;
	struct hci_fm_riva_data *peek_data = arg;

	ret = radio_hci_request(hdev, hci_peek_data_req, (unsigned
		long)peek_data, RADIO_HCI_TIMEOUT);

	return ret;
}

static int hci_poke_data(struct hci_fm_riva_poke *arg,
			struct radio_hci_dev *hdev)
{
	int ret = 0;
	struct hci_fm_riva_poke *poke_data = arg;

	ret = radio_hci_request(hdev, hci_poke_data_req, (unsigned
		long)poke_data, RADIO_HCI_TIMEOUT);

	return ret;
}

static int hci_ssbi_peek_reg(struct hci_fm_ssbi_peek *arg,
	struct radio_hci_dev *hdev)
{
	int ret = 0;
	struct hci_fm_ssbi_peek *ssbi_peek_reg = arg;

	ret = radio_hci_request(hdev, hci_ssbi_peek_reg_req, (unsigned
		long)ssbi_peek_reg, RADIO_HCI_TIMEOUT);

	return ret;
}

static int hci_ssbi_poke_reg(struct hci_fm_ssbi_req *arg,
			struct radio_hci_dev *hdev)
{
	int ret = 0;
	struct hci_fm_ssbi_req *ssbi_poke_reg = arg;

	ret = radio_hci_request(hdev, hci_ssbi_poke_reg_req, (unsigned
		long)ssbi_poke_reg, RADIO_HCI_TIMEOUT);

	return ret;
}

static int hci_set_ch_det_thresholds_req(struct hci_fm_ch_det_threshold *arg,
		struct radio_hci_dev *hdev)
{
	int ret = 0;
	struct hci_fm_ch_det_threshold *ch_det_threshold = arg;
	ret = radio_hci_request(hdev, hci_fm_set_ch_det_th,
		 (unsigned long)ch_det_threshold, RADIO_HCI_TIMEOUT);
	return ret;
}

static int hci_fm_set_cal_req_proc(struct radio_hci_dev *hdev,
		unsigned long param)
{
	u16 opcode = 0;
	struct hci_fm_set_cal_req_proc *cal_req =
		(struct hci_fm_set_cal_req_proc *)param;

	opcode = hci_opcode_pack(HCI_OGF_FM_COMMON_CTRL_CMD_REQ,
		HCI_OCF_FM_SET_CALIBRATION);
	return radio_hci_send_cmd(hdev, opcode, sizeof(*cal_req),
		cal_req);
}

static int hci_fm_do_cal_req(struct radio_hci_dev *hdev,
		unsigned long param)
{
	u16 opcode = 0;
	u8 cal_mode = param;

	opcode = hci_opcode_pack(HCI_OGF_FM_COMMON_CTRL_CMD_REQ,
		HCI_OCF_FM_DO_CALIBRATION);
	return radio_hci_send_cmd(hdev, opcode, sizeof(cal_mode),
		&cal_mode);

}
static int hci_cmd(unsigned int cmd, struct radio_hci_dev *hdev)
{
	int ret = 0;
	unsigned long arg = 0;

	if (!hdev)
		return -ENODEV;

	switch (cmd) {
	case HCI_FM_ENABLE_RECV_CMD:
		ret = radio_hci_request(hdev, hci_fm_enable_recv_req, arg,
			msecs_to_jiffies(RADIO_HCI_TIMEOUT));
		break;

	case HCI_FM_DISABLE_RECV_CMD:
		ret = radio_hci_request(hdev, hci_fm_disable_recv_req, arg,
			msecs_to_jiffies(RADIO_HCI_TIMEOUT));
		break;

	case HCI_FM_GET_RECV_CONF_CMD:
		ret = radio_hci_request(hdev, hci_get_fm_recv_conf_req, arg,
			msecs_to_jiffies(RADIO_HCI_TIMEOUT));
		break;

	case HCI_FM_GET_STATION_PARAM_CMD:
		ret = radio_hci_request(hdev,
			hci_fm_get_station_param_req, arg,
			msecs_to_jiffies(RADIO_HCI_TIMEOUT));
		break;

	case HCI_FM_GET_SIGNAL_TH_CMD:
		ret = radio_hci_request(hdev,
			hci_fm_get_sig_threshold_req, arg,
			msecs_to_jiffies(RADIO_HCI_TIMEOUT));
		break;

	case HCI_FM_GET_PROGRAM_SERVICE_CMD:
		ret = radio_hci_request(hdev,
			hci_fm_get_program_service_req, arg,
			msecs_to_jiffies(RADIO_HCI_TIMEOUT));
		break;

	case HCI_FM_GET_RADIO_TEXT_CMD:
		ret = radio_hci_request(hdev, hci_fm_get_radio_text_req, arg,
			msecs_to_jiffies(RADIO_HCI_TIMEOUT));
		break;

	case HCI_FM_GET_AF_LIST_CMD:
		ret = radio_hci_request(hdev, hci_fm_get_af_list_req, arg,
			msecs_to_jiffies(RADIO_HCI_TIMEOUT));
		break;

	case HCI_FM_CANCEL_SEARCH_CMD:
		ret = radio_hci_request(hdev, hci_fm_cancel_search_req, arg,
			msecs_to_jiffies(RADIO_HCI_TIMEOUT));
		break;

	case HCI_FM_RESET_CMD:
		ret = radio_hci_request(hdev, hci_fm_reset_req, arg,
			msecs_to_jiffies(RADIO_HCI_TIMEOUT));
		break;

	case HCI_FM_GET_FEATURES_CMD:
		ret = radio_hci_request(hdev,
		hci_fm_get_feature_lists_req, arg,
			msecs_to_jiffies(RADIO_HCI_TIMEOUT));
		break;

	case HCI_FM_STATION_DBG_PARAM_CMD:
		ret = radio_hci_request(hdev,
		hci_fm_get_station_dbg_param_req, arg,
			msecs_to_jiffies(RADIO_HCI_TIMEOUT));
		break;

	case HCI_FM_ENABLE_TRANS_CMD:
		ret = radio_hci_request(hdev, hci_fm_enable_trans_req, arg,
			msecs_to_jiffies(RADIO_HCI_TIMEOUT));
		break;

	case HCI_FM_DISABLE_TRANS_CMD:
		ret = radio_hci_request(hdev, hci_fm_disable_trans_req, arg,
			msecs_to_jiffies(RADIO_HCI_TIMEOUT));
		break;

	case HCI_FM_GET_TX_CONFIG:
		ret = radio_hci_request(hdev, hci_get_fm_trans_conf_req, arg,
			msecs_to_jiffies(RADIO_HCI_TIMEOUT));
		break;
	case HCI_FM_GET_DET_CH_TH_CMD:
		ret = radio_hci_request(hdev, hci_fm_get_ch_det_th, arg,
					msecs_to_jiffies(RADIO_HCI_TIMEOUT));
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void radio_hci_req_complete(struct radio_hci_dev *hdev, int result)
{
	hdev->req_result = result;
	hdev->req_status = HCI_REQ_DONE;
	wake_up_interruptible(&hdev->req_wait_q);
}

static void radio_hci_status_complete(struct radio_hci_dev *hdev, int result)
{
	hdev->req_result = result;
	hdev->req_status = HCI_REQ_STATUS;
	wake_up_interruptible(&hdev->req_wait_q);
}

static void hci_cc_rsp(struct radio_hci_dev *hdev, struct sk_buff *skb)
{
	__u8 status = *((__u8 *) skb->data);

	if (status)
		return;

	radio_hci_req_complete(hdev, status);
}

static void hci_cc_fm_disable_rsp(struct radio_hci_dev *hdev,
	struct sk_buff *skb)
{
	__u8 status = *((__u8 *) skb->data);
	struct iris_device *radio = video_get_drvdata(video_get_dev());

	if (status)
		return;

	iris_q_event(radio, IRIS_EVT_RADIO_DISABLED);

	radio_hci_req_complete(hdev, status);
}

static void hci_cc_conf_rsp(struct radio_hci_dev *hdev, struct sk_buff *skb)
{
	struct hci_fm_conf_rsp  *rsp = (void *)skb->data;
	struct iris_device *radio = video_get_drvdata(video_get_dev());

	if (rsp->status)
		return;

	radio->recv_conf = rsp->recv_conf_rsp;
	radio_hci_req_complete(hdev, rsp->status);
}

static void hci_cc_fm_trans_get_conf_rsp(struct radio_hci_dev *hdev,
		struct sk_buff *skb)
{
	struct hci_fm_get_trans_conf_rsp  *rsp = (void *)skb->data;
	struct iris_device *radio = video_get_drvdata(video_get_dev());

	if (rsp->status)
		return;
	memcpy((void *)&radio->trans_conf,  (void*)&rsp->trans_conf_rsp,
			sizeof(rsp->trans_conf_rsp));
	radio_hci_req_complete(hdev, rsp->status);
}

static void hci_cc_fm_enable_rsp(struct radio_hci_dev *hdev,
	struct sk_buff *skb)
{
	struct hci_fm_conf_rsp  *rsp = (void *)skb->data;
	struct iris_device *radio = video_get_drvdata(video_get_dev());

	if (rsp->status)
		return;

	iris_q_event(radio, IRIS_EVT_RADIO_READY);

	radio_hci_req_complete(hdev, rsp->status);
}


static void hci_cc_fm_trans_set_conf_rsp(struct radio_hci_dev *hdev,
	struct sk_buff *skb)
{
	struct hci_fm_conf_rsp  *rsp = (void *)skb->data;
	struct iris_device *radio = video_get_drvdata(video_get_dev());

	if (rsp->status)
		return;

	iris_q_event(radio, HCI_EV_CMD_COMPLETE);

	radio_hci_req_complete(hdev, rsp->status);
}


static void hci_cc_sig_threshold_rsp(struct radio_hci_dev *hdev,
		struct sk_buff *skb)
{
	struct hci_fm_sig_threshold_rsp  *rsp = (void *)skb->data;
	struct iris_device *radio = video_get_drvdata(video_get_dev());

	if (rsp->status)
		return;

	memcpy(&radio->sig_th, rsp, sizeof(struct hci_fm_sig_threshold_rsp));
	radio_hci_req_complete(hdev, rsp->status);
}

static void hci_cc_station_rsp(struct radio_hci_dev *hdev, struct sk_buff *skb)
{
	struct iris_device *radio = video_get_drvdata(video_get_dev());
	struct hci_fm_station_rsp *rsp = (void *)skb->data;
	radio->fm_st_rsp = *(rsp);

	/* Tune is always succesful */
	radio_hci_req_complete(hdev, 0);
}

static void hci_cc_prg_srv_rsp(struct radio_hci_dev *hdev, struct sk_buff *skb)
{
	struct hci_fm_prgm_srv_rsp  *rsp = (void *)skb->data;

	if (rsp->status)
		return;

	radio_hci_req_complete(hdev, rsp->status);
}

static void hci_cc_rd_txt_rsp(struct radio_hci_dev *hdev, struct sk_buff *skb)
{
	struct hci_fm_radio_txt_rsp  *rsp = (void *)skb->data;

	if (rsp->status)
		return;

	radio_hci_req_complete(hdev, rsp->status);
}

static void hci_cc_af_list_rsp(struct radio_hci_dev *hdev, struct sk_buff *skb)
{
	struct hci_fm_af_list_rsp  *rsp = (void *)skb->data;

	if (rsp->status)
		return;

	radio_hci_req_complete(hdev, rsp->status);
}

static void hci_cc_feature_list_rsp(struct radio_hci_dev *hdev,
	struct sk_buff *skb)
{
	struct hci_fm_feature_list_rsp  *rsp = (void *)skb->data;
	struct iris_device *radio = video_get_drvdata(video_get_dev());
	struct v4l2_capability *v4l_cap = radio->g_cap;

	if (rsp->status)
		return;
	v4l_cap->capabilities = (rsp->feature_mask & 0x000002) |
		(rsp->feature_mask & 0x000001);

	radio_hci_req_complete(hdev, rsp->status);
}

static void hci_cc_dbg_param_rsp(struct radio_hci_dev *hdev,
	struct sk_buff *skb)
{
	struct iris_device *radio = video_get_drvdata(video_get_dev());
	struct hci_fm_dbg_param_rsp *rsp = (void *)skb->data;
	radio->st_dbg_param = *(rsp);

	if (radio->st_dbg_param.status)
		return;

	radio_hci_req_complete(hdev, radio->st_dbg_param.status);
}

static void iris_q_evt_data(struct iris_device *radio,
				char *data, int len, int event)
{
	struct kfifo *data_b = &radio->data_buf[event];
	if (kfifo_in_locked(data_b, data, len, &radio->buf_lock[event]))
		wake_up_interruptible(&radio->event_queue);
}

static void hci_cc_riva_peek_rsp(struct radio_hci_dev *hdev,
		struct sk_buff *skb)
{
	struct iris_device *radio = video_get_drvdata(video_get_dev());
	__u8 status = *((__u8 *) skb->data);
	int len;
	char *data;

	if (status)
		return;
	len = skb->data[RIVA_PEEK_LEN_OFSET] + RIVA_PEEK_PARAM;
	data = kmalloc(len, GFP_ATOMIC);

	if (!data) {
		FMDERR("Memory allocation failed");
		return;
	}

	memcpy(data, &skb->data[PEEK_DATA_OFSET], len);
	iris_q_evt_data(radio, data, len, IRIS_BUF_PEEK);
	radio_hci_req_complete(hdev, status);
	kfree(data);

}

static void hci_cc_riva_read_default_rsp(struct radio_hci_dev *hdev,
		struct sk_buff *skb)
{
	struct iris_device *radio = video_get_drvdata(video_get_dev());
	__u8 status = *((__u8 *) skb->data);
	__u8 len;

	if (status)
		return;
	len = skb->data[1];

	memset(&radio->default_data, 0 , sizeof(struct hci_fm_data_rd_rsp));
	memcpy(&radio->default_data, &skb->data[0], len+2);
	iris_q_evt_data(radio, &skb->data[0], len+2, IRIS_BUF_RD_DEFAULT);
	radio_hci_req_complete(hdev, status);
}

static void hci_cc_ssbi_peek_rsp(struct radio_hci_dev *hdev,
		struct sk_buff *skb)
{
	struct iris_device *radio = video_get_drvdata(video_get_dev());
	__u8 status = *((__u8 *) skb->data);
	char *data;

	if (status)
		return;
	data = kmalloc(SSBI_PEEK_LEN, GFP_ATOMIC);
	if (!data) {
		FMDERR("Memory allocation failed");
		return;
	}

	data[0] = skb->data[PEEK_DATA_OFSET];
	iris_q_evt_data(radio, data, SSBI_PEEK_LEN, IRIS_BUF_SSBI_PEEK);
	radio_hci_req_complete(hdev, status);
	kfree(data);
}

static void hci_cc_rds_grp_cntrs_rsp(struct radio_hci_dev *hdev,
		struct sk_buff *skb)
{
	struct iris_device *radio = video_get_drvdata(video_get_dev());
	__u8 status = *((__u8 *) skb->data);
	char *data;
	if (status)
		return;
	data = kmalloc(RDS_GRP_CNTR_LEN, GFP_ATOMIC);
	if (!data) {
		FMDERR("memory allocation failed");
		return;
	}
	memcpy(data, &skb->data[1], RDS_GRP_CNTR_LEN);
	iris_q_evt_data(radio, data, RDS_GRP_CNTR_LEN, IRIS_BUF_RDS_CNTRS);
	radio_hci_req_complete(hdev, status);
	kfree(data);

}

static void hci_cc_do_calibration_rsp(struct radio_hci_dev *hdev,
		struct sk_buff *skb)
{
	struct iris_device *radio = video_get_drvdata(video_get_dev());
	static struct hci_cc_do_calibration_rsp rsp ;
	rsp.status = skb->data[0];
	rsp.mode = skb->data[CALIB_MODE_OFSET];

	if (rsp.status) {
		FMDERR("status = %d", rsp.status);
		return;
	}
	if (rsp.mode == PROCS_CALIB_MODE) {
		memcpy(&rsp.data[0], &skb->data[CALIB_DATA_OFSET],
				PROCS_CALIB_SIZE);
	iris_q_evt_data(radio, rsp.data, PROCS_CALIB_SIZE,
					IRIS_BUF_CAL_DATA);
	} else {
		return;
	}

	radio_hci_req_complete(hdev, rsp.status);
}

static void hci_cc_get_ch_det_threshold_rsp(struct radio_hci_dev *hdev,
		struct sk_buff *skb)
{
	struct iris_device *radio = video_get_drvdata(video_get_dev());
	u8  status = skb->data[0];
	if (status) {
		FMDERR("status = %d", status);
		return;
	}
	memcpy(&radio->ch_det_threshold, &skb->data[1],
		sizeof(struct hci_fm_ch_det_threshold));
	radio_hci_req_complete(hdev, status);
}

static inline void hci_cmd_complete_event(struct radio_hci_dev *hdev,
		struct sk_buff *skb)
{
	struct hci_ev_cmd_complete *cmd_compl_ev = (void *) skb->data;
	__u16 opcode;

	skb_pull(skb, sizeof(*cmd_compl_ev));

	opcode = __le16_to_cpu(cmd_compl_ev->cmd_opcode);

	switch (opcode) {
	case hci_recv_ctrl_cmd_op_pack(HCI_OCF_FM_ENABLE_RECV_REQ):
	case hci_trans_ctrl_cmd_op_pack(HCI_OCF_FM_ENABLE_TRANS_REQ):
		hci_cc_fm_enable_rsp(hdev, skb);
		break;
	case hci_recv_ctrl_cmd_op_pack(HCI_OCF_FM_GET_RECV_CONF_REQ):
		hci_cc_conf_rsp(hdev, skb);
		break;

	case hci_recv_ctrl_cmd_op_pack(HCI_OCF_FM_DISABLE_RECV_REQ):
	case hci_trans_ctrl_cmd_op_pack(HCI_OCF_FM_DISABLE_TRANS_REQ):
		hci_cc_fm_disable_rsp(hdev, skb);
		break;

	case hci_recv_ctrl_cmd_op_pack(HCI_OCF_FM_SET_RECV_CONF_REQ):
	case hci_recv_ctrl_cmd_op_pack(HCI_OCF_FM_SET_MUTE_MODE_REQ):
	case hci_recv_ctrl_cmd_op_pack(HCI_OCF_FM_SET_STEREO_MODE_REQ):
	case hci_recv_ctrl_cmd_op_pack(HCI_OCF_FM_SET_ANTENNA):
	case hci_recv_ctrl_cmd_op_pack(HCI_OCF_FM_SET_SIGNAL_THRESHOLD):
	case hci_recv_ctrl_cmd_op_pack(HCI_OCF_FM_CANCEL_SEARCH):
	case hci_recv_ctrl_cmd_op_pack(HCI_OCF_FM_RDS_GRP):
	case hci_recv_ctrl_cmd_op_pack(HCI_OCF_FM_RDS_GRP_PROCESS):
	case hci_recv_ctrl_cmd_op_pack(HCI_OCF_FM_EN_WAN_AVD_CTRL):
	case hci_recv_ctrl_cmd_op_pack(HCI_OCF_FM_EN_NOTCH_CTRL):
	case hci_recv_ctrl_cmd_op_pack(HCI_OCF_FM_SET_CH_DET_THRESHOLD):
	case hci_trans_ctrl_cmd_op_pack(HCI_OCF_FM_RDS_RT_REQ):
	case hci_trans_ctrl_cmd_op_pack(HCI_OCF_FM_RDS_PS_REQ):
	case hci_common_cmd_op_pack(HCI_OCF_FM_DEFAULT_DATA_WRITE):
		hci_cc_rsp(hdev, skb);
		break;
	case hci_common_cmd_op_pack(HCI_OCF_FM_RESET):
	case hci_diagnostic_cmd_op_pack(HCI_OCF_FM_SSBI_POKE_REG):
	case hci_diagnostic_cmd_op_pack(HCI_OCF_FM_POKE_DATA):
	case hci_diagnostic_cmd_op_pack(HCI_FM_SET_INTERNAL_TONE_GENRATOR):
	case hci_common_cmd_op_pack(HCI_OCF_FM_SET_CALIBRATION):
	case hci_recv_ctrl_cmd_op_pack(HCI_OCF_FM_SET_EVENT_MASK):
		hci_cc_rsp(hdev, skb);
		break;

	case hci_diagnostic_cmd_op_pack(HCI_OCF_FM_SSBI_PEEK_REG):
		hci_cc_ssbi_peek_rsp(hdev, skb);
		break;
	case hci_recv_ctrl_cmd_op_pack(HCI_OCF_FM_GET_SIGNAL_THRESHOLD):
		hci_cc_sig_threshold_rsp(hdev, skb);
		break;

	case hci_recv_ctrl_cmd_op_pack(HCI_OCF_FM_GET_STATION_PARAM_REQ):
		hci_cc_station_rsp(hdev, skb);
		break;

	case hci_recv_ctrl_cmd_op_pack(HCI_OCF_FM_GET_PROGRAM_SERVICE_REQ):
		hci_cc_prg_srv_rsp(hdev, skb);
		break;

	case hci_recv_ctrl_cmd_op_pack(HCI_OCF_FM_GET_RADIO_TEXT_REQ):
		hci_cc_rd_txt_rsp(hdev, skb);
		break;

	case hci_recv_ctrl_cmd_op_pack(HCI_OCF_FM_GET_AF_LIST_REQ):
		hci_cc_af_list_rsp(hdev, skb);
		break;

	case hci_common_cmd_op_pack(HCI_OCF_FM_DEFAULT_DATA_READ):
		hci_cc_riva_read_default_rsp(hdev, skb);
		break;

	case hci_diagnostic_cmd_op_pack(HCI_OCF_FM_PEEK_DATA):
		hci_cc_riva_peek_rsp(hdev, skb);
		break;

	case hci_common_cmd_op_pack(HCI_OCF_FM_GET_FEATURE_LIST):
		hci_cc_feature_list_rsp(hdev, skb);
		break;

	case hci_diagnostic_cmd_op_pack(HCI_OCF_FM_STATION_DBG_PARAM):
		hci_cc_dbg_param_rsp(hdev, skb);
		break;
	case hci_trans_ctrl_cmd_op_pack(HCI_OCF_FM_SET_TRANS_CONF_REQ):
		hci_cc_fm_trans_set_conf_rsp(hdev, skb);
		break;

	case hci_status_param_op_pack(HCI_OCF_FM_READ_GRP_COUNTERS):
		hci_cc_rds_grp_cntrs_rsp(hdev, skb);
		break;
	case hci_common_cmd_op_pack(HCI_OCF_FM_DO_CALIBRATION):
		hci_cc_do_calibration_rsp(hdev, skb);
		break;

	case hci_trans_ctrl_cmd_op_pack(HCI_OCF_FM_GET_TRANS_CONF_REQ):
		hci_cc_fm_trans_get_conf_rsp(hdev, skb);
		break;
	case hci_recv_ctrl_cmd_op_pack(HCI_OCF_FM_GET_CH_DET_THRESHOLD):
		hci_cc_get_ch_det_threshold_rsp(hdev, skb);
		break;
	default:
		FMDERR("%s opcode 0x%x", hdev->name, opcode);
		break;
	}

}

static inline void hci_cmd_status_event(struct radio_hci_dev *hdev,
		struct sk_buff *skb)
{
	struct hci_ev_cmd_status *ev = (void *) skb->data;
	radio_hci_status_complete(hdev, ev->status);
}

static inline void hci_ev_tune_status(struct radio_hci_dev *hdev,
		struct sk_buff *skb)
{
	int i;
	struct iris_device *radio = video_get_drvdata(video_get_dev());

	memcpy(&radio->fm_st_rsp.station_rsp, &skb->data[0],
				sizeof(struct hci_ev_tune_status));
	iris_q_event(radio, IRIS_EVT_TUNE_SUCC);

	for (i = 0; i < IRIS_BUF_MAX; i++) {
		if (i >= IRIS_BUF_RT_RDS)
			kfifo_reset(&radio->data_buf[i]);
	}
	if (radio->fm_st_rsp.station_rsp.serv_avble)
		iris_q_event(radio, IRIS_EVT_ABOVE_TH);
	else
		iris_q_event(radio, IRIS_EVT_BELOW_TH);

	if (radio->fm_st_rsp.station_rsp.stereo_prg)
		iris_q_event(radio, IRIS_EVT_STEREO);

	if (radio->fm_st_rsp.station_rsp.mute_mode)
		iris_q_event(radio, IRIS_EVT_MONO);

	if (radio->fm_st_rsp.station_rsp.rds_sync_status)
		iris_q_event(radio, IRIS_EVT_RDS_AVAIL);
	else
		iris_q_event(radio, IRIS_EVT_RDS_NOT_AVAIL);
}

static inline void hci_ev_search_compl(struct radio_hci_dev *hdev,
		struct sk_buff *skb)
{
	struct iris_device *radio = video_get_drvdata(video_get_dev());
	iris_q_event(radio, IRIS_EVT_SEEK_COMPLETE);
}

static inline void hci_ev_srch_st_list_compl(struct radio_hci_dev *hdev,
		struct sk_buff *skb)
{
	struct iris_device *radio = video_get_drvdata(video_get_dev());
	struct hci_ev_srch_list_compl *ev ;
	int cnt;
	int stn_num;
	int rel_freq;
	int abs_freq;
	int len;

	ev = kmalloc(sizeof(*ev), GFP_ATOMIC);
	if (!ev) {
		FMDERR("Memory allocation failed");
		return ;
	}

	ev->num_stations_found = skb->data[STN_NUM_OFFSET];
	len = ev->num_stations_found * PARAMS_PER_STATION + STN_FREQ_OFFSET;

	for (cnt = STN_FREQ_OFFSET, stn_num = 0;
		(cnt < len) && (stn_num < ev->num_stations_found)
		&& (stn_num < ARRAY_SIZE(ev->rel_freq));
		cnt += PARAMS_PER_STATION, stn_num++) {
		abs_freq = *((int *)&skb->data[cnt]);
		rel_freq = abs_freq - radio->recv_conf.band_low_limit;
		rel_freq = (rel_freq * 20) / KHZ_TO_MHZ;

		ev->rel_freq[stn_num].rel_freq_lsb = GET_LSB(rel_freq);
		ev->rel_freq[stn_num].rel_freq_msb = GET_MSB(rel_freq);
	}

	len = ev->num_stations_found * 2 + sizeof(ev->num_stations_found);
	iris_q_event(radio, IRIS_EVT_NEW_SRCH_LIST);
	iris_q_evt_data(radio, (char *)ev, len, IRIS_BUF_SRCH_LIST);
	kfree(ev);
}

static inline void hci_ev_search_next(struct radio_hci_dev *hdev,
		struct sk_buff *skb)
{
	struct iris_device *radio = video_get_drvdata(video_get_dev());
	iris_q_event(radio, IRIS_EVT_SCAN_NEXT);
}

static inline void hci_ev_stereo_status(struct radio_hci_dev *hdev,
		struct sk_buff *skb)
{
	struct iris_device *radio = video_get_drvdata(video_get_dev());
	__u8 st_status = *((__u8 *) skb->data);
	if (st_status)
		iris_q_event(radio, IRIS_EVT_STEREO);
	else
		iris_q_event(radio, IRIS_EVT_MONO);
}


static inline void hci_ev_program_service(struct radio_hci_dev *hdev,
		struct sk_buff *skb)
{
	struct iris_device *radio = video_get_drvdata(video_get_dev());
	int len;
	char *data;

	len = (skb->data[RDS_PS_LENGTH_OFFSET] * RDS_STRING) + RDS_OFFSET;
	iris_q_event(radio, IRIS_EVT_NEW_PS_RDS);
	data = kmalloc(len, GFP_ATOMIC);
	if (!data) {
		FMDERR("Failed to allocate memory");
		return;
	}

	data[0] = skb->data[RDS_PS_LENGTH_OFFSET];
	data[1] = skb->data[RDS_PTYPE];
	data[2] = skb->data[RDS_PID_LOWER];
	data[3] = skb->data[RDS_PID_HIGHER];
	data[4] = 0;

	memcpy(data+RDS_OFFSET, &skb->data[RDS_PS_DATA_OFFSET], len-RDS_OFFSET);

	iris_q_evt_data(radio, data, len, IRIS_BUF_PS_RDS);

	kfree(data);
}


static inline void hci_ev_radio_text(struct radio_hci_dev *hdev,
	struct sk_buff *skb)
{
	struct iris_device *radio = video_get_drvdata(video_get_dev());
	int len = 0;
	char *data;

	iris_q_event(radio, IRIS_EVT_NEW_RT_RDS);

	while ((skb->data[len+RDS_OFFSET] != 0x0d) && (len < RX_RT_DATA_LENGTH))
		len++;
	data = kmalloc(len+RDS_OFFSET, GFP_ATOMIC);
	if (!data) {
		FMDERR("Failed to allocate memory");
		return;
	}

	data[0] = len;
	data[1] = skb->data[RDS_PTYPE];
	data[2] = skb->data[RDS_PID_LOWER];
	data[3] = skb->data[RDS_PID_HIGHER];
	data[4] = 0;

	memcpy(data+RDS_OFFSET, &skb->data[RDS_OFFSET], len);
	data[len+RDS_OFFSET] = 0x00;

	iris_q_evt_data(radio, data, len+RDS_OFFSET, IRIS_BUF_RT_RDS);

	kfree(data);
}

static void hci_ev_af_list(struct radio_hci_dev *hdev,
	struct sk_buff *skb)
{
	struct iris_device *radio = video_get_drvdata(video_get_dev());
	struct hci_ev_af_list ev;

	ev.tune_freq = *((int *) &skb->data[0]);
	ev.pi_code = *((__le16 *) &skb->data[PI_CODE_OFFSET]);
	ev.af_size = skb->data[AF_SIZE_OFFSET];
	memcpy(&ev.af_list[0], &skb->data[AF_LIST_OFFSET], ev.af_size);
	iris_q_event(radio, IRIS_EVT_NEW_AF_LIST);
	iris_q_evt_data(radio, (char *)&ev, sizeof(ev), IRIS_BUF_AF_LIST);
}

static void hci_ev_rds_lock_status(struct radio_hci_dev *hdev,
	struct sk_buff *skb)
{
	struct iris_device *radio = video_get_drvdata(video_get_dev());
	__u8 rds_status = skb->data[0];

	if (rds_status)
		iris_q_event(radio, IRIS_EVT_RDS_AVAIL);
	else
		iris_q_event(radio, IRIS_EVT_RDS_NOT_AVAIL);
}

static void hci_ev_service_available(struct radio_hci_dev *hdev,
	struct sk_buff *skb)
{
	struct iris_device *radio = video_get_drvdata(video_get_dev());
	u8 serv_avble = skb->data[0];
	if (serv_avble)
		iris_q_event(radio, IRIS_EVT_ABOVE_TH);
	else
		iris_q_event(radio, IRIS_EVT_BELOW_TH);
}

static void hci_ev_rds_grp_complete(struct radio_hci_dev *hdev,
	struct sk_buff *skb)
{
	struct iris_device *radio = video_get_drvdata(video_get_dev());
	iris_q_event(radio, IRIS_EVT_TXRDSDONE);
}

void radio_hci_event_packet(struct radio_hci_dev *hdev, struct sk_buff *skb)
{
	struct radio_hci_event_hdr *hdr;
	u8 event;

	if (skb == NULL) {
		FMDERR("Socket buffer is NULL");
		return;
	}

	hdr = (void *) skb->data;
	event = hdr->evt;

	skb_pull(skb, RADIO_HCI_EVENT_HDR_SIZE);

	switch (event) {
	case HCI_EV_TUNE_STATUS:
		hci_ev_tune_status(hdev, skb);
		break;
	case HCI_EV_SEARCH_PROGRESS:
	case HCI_EV_SEARCH_RDS_PROGRESS:
	case HCI_EV_SEARCH_LIST_PROGRESS:
		hci_ev_search_next(hdev, skb);
		break;
	case HCI_EV_STEREO_STATUS:
		hci_ev_stereo_status(hdev, skb);
		break;
	case HCI_EV_RDS_LOCK_STATUS:
		hci_ev_rds_lock_status(hdev, skb);
		break;
	case HCI_EV_SERVICE_AVAILABLE:
		hci_ev_service_available(hdev, skb);
		break;
	case HCI_EV_RDS_RX_DATA:
		break;
	case HCI_EV_PROGRAM_SERVICE:
		hci_ev_program_service(hdev, skb);
		break;
	case HCI_EV_RADIO_TEXT:
		hci_ev_radio_text(hdev, skb);
		break;
	case HCI_EV_FM_AF_LIST:
		hci_ev_af_list(hdev, skb);
		break;
	case HCI_EV_TX_RDS_GRP_COMPL:
		hci_ev_rds_grp_complete(hdev, skb);
		break;
	case HCI_EV_TX_RDS_CONT_GRP_COMPL:
		break;

	case HCI_EV_CMD_COMPLETE:
		hci_cmd_complete_event(hdev, skb);
		break;

	case HCI_EV_CMD_STATUS:
		hci_cmd_status_event(hdev, skb);
		break;

	case HCI_EV_SEARCH_COMPLETE:
	case HCI_EV_SEARCH_RDS_COMPLETE:
		hci_ev_search_compl(hdev, skb);
		break;

	case HCI_EV_SEARCH_LIST_COMPLETE:
		hci_ev_srch_st_list_compl(hdev, skb);
		break;

	default:
		break;
	}
}

/*
 * fops/IOCTL helper functions
 */

static int iris_search(struct iris_device *radio, int on, int dir)
{
	int retval = 0;
	enum search_t srch = radio->g_search_mode & SRCH_MODE;
	radio->search_on = on;

	if (on) {
		switch (srch) {
		case SCAN_FOR_STRONG:
		case SCAN_FOR_WEAK:
			radio->srch_st_list.srch_list_dir = dir;
			radio->srch_st_list.srch_list_mode = srch;
			radio->srch_st_list.srch_list_max = 0;
			retval = hci_fm_search_station_list(
				&radio->srch_st_list, radio->fm_hdev);
			break;
		case RDS_SEEK_PTY:
		case RDS_SCAN_PTY:
		case RDS_SEEK_PI:
			srch = srch - SEARCH_RDS_STNS_MODE_OFFSET;
			radio->srch_rds.srch_station.srch_mode = srch;
			radio->srch_rds.srch_station.srch_dir = dir;
			radio->srch_rds.srch_station.scan_time =
				radio->g_scan_time;
			retval = hci_fm_search_rds_stations(&radio->srch_rds,
				radio->fm_hdev);
			break;
		default:
			radio->srch_st.srch_mode = srch;
			radio->srch_st.scan_time = radio->g_scan_time;
			radio->srch_st.srch_dir = dir;
			retval = hci_fm_search_stations(
				&radio->srch_st, radio->fm_hdev);
			break;
		}

	} else {
		retval = hci_cmd(HCI_FM_CANCEL_SEARCH_CMD, radio->fm_hdev);
	}

	return retval;
}

static int set_low_power_mode(struct iris_device *radio, int power_mode)
{

	int rds_grps_proc = 0x00;
	int retval = 0;
	if (radio->power_mode != power_mode) {

		if (power_mode) {
			radio->event_mask = 0x00;
			if (radio->af_jump_bit)
				rds_grps_proc = 0x00 | AF_JUMP_ENABLE;
			else
				rds_grps_proc = 0x00;
			retval = hci_fm_rds_grps_process(
				&rds_grps_proc,
				radio->fm_hdev);
			if (retval < 0) {
				FMDERR("Disable RDS failed");
				return retval;
			}
			retval = hci_conf_event_mask(&radio->event_mask,
				radio->fm_hdev);
		} else {

			radio->event_mask = SIG_LEVEL_INTR |
					RDS_SYNC_INTR | AUDIO_CTRL_INTR;
			retval = hci_conf_event_mask(&radio->event_mask,
				radio->fm_hdev);
			if (retval < 0) {
				FMDERR("Enable Async events failed");
				return retval;
			}
			retval = hci_fm_rds_grps_process(
				&radio->g_rds_grp_proc_ps,
				radio->fm_hdev);
		}
		radio->power_mode = power_mode;
	}
	return retval;
}
static int iris_recv_set_region(struct iris_device *radio, int req_region)
{
	int retval;
	radio->region = req_region;

	switch (radio->region) {
	case IRIS_REGION_US:
		radio->recv_conf.band_low_limit =
			REGION_US_EU_BAND_LOW;
		radio->recv_conf.band_high_limit =
			REGION_US_EU_BAND_HIGH;
		break;
	case IRIS_REGION_EU:
		radio->recv_conf.band_low_limit =
			REGION_US_EU_BAND_LOW;
		radio->recv_conf.band_high_limit =
			REGION_US_EU_BAND_HIGH;
		break;
	case IRIS_REGION_JAPAN:
		radio->recv_conf.band_low_limit =
			REGION_JAPAN_STANDARD_BAND_LOW;
		radio->recv_conf.band_high_limit =
			REGION_JAPAN_STANDARD_BAND_HIGH;
		break;
	case IRIS_REGION_JAPAN_WIDE:
		radio->recv_conf.band_low_limit =
			REGION_JAPAN_WIDE_BAND_LOW;
		radio->recv_conf.band_high_limit =
			REGION_JAPAN_WIDE_BAND_HIGH;
		break;
	default:
		/* The user specifies the value.
		   So nothing needs to be done */
		break;
	}

	retval = hci_set_fm_recv_conf(
			&radio->recv_conf,
			radio->fm_hdev);

	return retval;
}


static int iris_trans_set_region(struct iris_device *radio, int req_region)
{
	int retval;
	radio->region = req_region;

	switch (radio->region) {
	case IRIS_REGION_US:
		radio->trans_conf.band_low_limit =
			REGION_US_EU_BAND_LOW;
		radio->trans_conf.band_high_limit =
			REGION_US_EU_BAND_HIGH;
		break;
	case IRIS_REGION_EU:
		radio->trans_conf.band_low_limit =
			REGION_US_EU_BAND_LOW;
		radio->trans_conf.band_high_limit =
			REGION_US_EU_BAND_HIGH;
		break;
	case IRIS_REGION_JAPAN:
		radio->trans_conf.band_low_limit =
			REGION_JAPAN_STANDARD_BAND_LOW;
		radio->trans_conf.band_high_limit =
			REGION_JAPAN_STANDARD_BAND_HIGH;
		break;
	case IRIS_REGION_JAPAN_WIDE:
		radio->recv_conf.band_low_limit =
			REGION_JAPAN_WIDE_BAND_LOW;
		radio->recv_conf.band_high_limit =
			REGION_JAPAN_WIDE_BAND_HIGH;
	default:
		break;
	}

	retval = hci_set_fm_trans_conf(
			&radio->trans_conf,
				radio->fm_hdev);
	return retval;
}


static int iris_set_freq(struct iris_device *radio, unsigned int freq)
{

	int retval;
	retval = hci_fm_tune_station(&freq, radio->fm_hdev);
	if (retval < 0)
		FMDERR("Error while setting the frequency : %d\n", retval);
	return retval;
}


static int iris_vidioc_queryctrl(struct file *file, void *priv,
		struct v4l2_queryctrl *qc)
{
	unsigned char i;
	int retval = -EINVAL;

	for (i = 0; i < ARRAY_SIZE(iris_v4l2_queryctrl); i++) {
		if (qc->id && qc->id == iris_v4l2_queryctrl[i].id) {
			memcpy(qc, &(iris_v4l2_queryctrl[i]), sizeof(*qc));
			retval = 0;
			break;
		}
	}

	return retval;
}

static int iris_do_calibration(struct iris_device *radio)
{
	char cal_mode = 0x00;
	int retval = 0x00;

	cal_mode = PROCS_CALIB_MODE;
	retval = hci_cmd(HCI_FM_ENABLE_RECV_CMD,
			radio->fm_hdev);
	if (retval < 0) {
		FMDERR("Enable failed before calibration %x", retval);
		return retval;
	}
	retval = radio_hci_request(radio->fm_hdev, hci_fm_do_cal_req,
		(unsigned long)cal_mode, RADIO_HCI_TIMEOUT);
	if (retval < 0) {
		FMDERR("Do Process calibration failed %x", retval);
		return retval;
	}
	retval = hci_cmd(HCI_FM_DISABLE_RECV_CMD,
			radio->fm_hdev);
	if (retval < 0)
		FMDERR("Disable Failed after calibration %d", retval);
	return retval;
}
static int iris_vidioc_g_ctrl(struct file *file, void *priv,
		struct v4l2_control *ctrl)
{
	struct iris_device *radio = video_get_drvdata(video_devdata(file));
	int retval = 0;

	switch (ctrl->id) {
	case V4L2_CID_AUDIO_VOLUME:
		break;
	case V4L2_CID_AUDIO_MUTE:
		ctrl->value = radio->mute_mode.hard_mute;
		break;
	case V4L2_CID_PRIVATE_IRIS_SRCHMODE:
		ctrl->value = radio->g_search_mode;
		break;
	case V4L2_CID_PRIVATE_IRIS_SCANDWELL:
		ctrl->value = radio->g_scan_time;
		break;
	case V4L2_CID_PRIVATE_IRIS_SRCHON:
		ctrl->value = radio->search_on;
		break;
	case V4L2_CID_PRIVATE_IRIS_STATE:
		ctrl->value = radio->mode;
		break;
	case V4L2_CID_PRIVATE_IRIS_IOVERC:
		retval = hci_cmd(HCI_FM_STATION_DBG_PARAM_CMD, radio->fm_hdev);
		if (retval < 0)
			return retval;
		ctrl->value = radio->st_dbg_param.io_verc;
		break;
	case V4L2_CID_PRIVATE_IRIS_INTDET:
		retval = hci_cmd(HCI_FM_STATION_DBG_PARAM_CMD, radio->fm_hdev);
		if (retval < 0)
			return retval;
		ctrl->value = radio->st_dbg_param.in_det_out;
		break;
	case V4L2_CID_PRIVATE_IRIS_REGION:
		ctrl->value = radio->region;
		break;
	case V4L2_CID_PRIVATE_IRIS_SIGNAL_TH:
		retval = hci_cmd(HCI_FM_GET_SIGNAL_TH_CMD, radio->fm_hdev);
		if (retval < 0) {
			FMDERR("Error in get signal threshold %d\n", retval);
			return retval;
		}
		ctrl->value = radio->sig_th.sig_threshold;
		break;
	case V4L2_CID_PRIVATE_IRIS_SRCH_PTY:
		ctrl->value = radio->srch_rds.srch_pty;
		break;
	case V4L2_CID_PRIVATE_IRIS_SRCH_PI:
		ctrl->value = radio->srch_rds.srch_pi;
		break;
	case V4L2_CID_PRIVATE_IRIS_SRCH_CNT:
		ctrl->value = radio->srch_st_result.num_stations_found;
		break;
	case V4L2_CID_PRIVATE_IRIS_EMPHASIS:
		if (radio->mode == FM_RECV) {
			ctrl->value = radio->recv_conf.emphasis;
		} else if (radio->mode == FM_TRANS) {
			ctrl->value = radio->trans_conf.emphasis;
		} else {
			FMDERR("Error in radio mode"
				" %d\n", retval);
			return -EINVAL;
		}
		break;
	case V4L2_CID_PRIVATE_IRIS_RDS_STD:
		if (radio->mode == FM_RECV) {
			ctrl->value = radio->recv_conf.rds_std;
		} else if (radio->mode == FM_TRANS) {
			ctrl->value = radio->trans_conf.rds_std;
		} else {
			FMDERR("Error in radio mode"
				" %d\n", retval);
			return -EINVAL;
		}
		break;
	case V4L2_CID_PRIVATE_IRIS_SPACING:
		if (radio->mode == FM_RECV) {
			ctrl->value = radio->recv_conf.ch_spacing;
		} else {
			FMDERR("Error in radio mode"
				" %d\n", retval);
			return -EINVAL;
		}
		break;
	case V4L2_CID_PRIVATE_IRIS_RDSON:
		if (radio->mode == FM_RECV) {
			ctrl->value = radio->recv_conf.rds_std;
		} else {
			FMDERR("Error in radio mode"
				" %d\n", retval);
			return -EINVAL;
		}
		break;
	case V4L2_CID_PRIVATE_IRIS_RDSGROUP_MASK:
		ctrl->value = radio->rds_grp.rds_grp_enable_mask;
		break;
	case V4L2_CID_PRIVATE_IRIS_RDSGROUP_PROC:
	case V4L2_CID_PRIVATE_IRIS_PSALL:
		ctrl->value = (radio->g_rds_grp_proc_ps << RDS_CONFIG_OFFSET);
		break;
	case V4L2_CID_PRIVATE_IRIS_RDSD_BUF:
		ctrl->value = radio->rds_grp.rds_buf_size;
		break;
	case V4L2_CID_PRIVATE_IRIS_LP_MODE:
		ctrl->value = radio->power_mode;
		break;
	case V4L2_CID_PRIVATE_IRIS_ANTENNA:
		ctrl->value = radio->g_antenna;
		break;
	case V4L2_CID_PRIVATE_IRIS_SOFT_MUTE:
		ctrl->value = radio->mute_mode.soft_mute;
		break;
	case V4L2_CID_PRIVATE_IRIS_DO_CALIBRATION:
		retval = iris_do_calibration(radio);
		break;
	case V4L2_CID_PRIVATE_IRIS_GET_SINR:
		if (radio->mode == FM_RECV) {
			retval = hci_cmd(HCI_FM_GET_STATION_PARAM_CMD,
						 radio->fm_hdev);
			if (retval < 0) {
				FMDERR("Get SINR Failed");
				return retval;
			}
			ctrl->value = radio->fm_st_rsp.station_rsp.sinr;

		} else
			retval = -EINVAL;
		break;
	case V4L2_CID_PRIVATE_INTF_HIGH_THRESHOLD:
		retval = hci_cmd(HCI_FM_GET_DET_CH_TH_CMD, radio->fm_hdev);
		if (retval < 0) {
			FMDERR("Get High det threshold failed %x", retval);
			return retval;
		}
		ctrl->value = radio->ch_det_threshold.high_th;
		break;
	case V4L2_CID_PRIVATE_INTF_LOW_THRESHOLD:
		retval = hci_cmd(HCI_FM_GET_DET_CH_TH_CMD, radio->fm_hdev);
		if (retval < 0) {
			FMDERR("Get Low det threshold failed %x", retval);
			return retval;
		}
		ctrl->value = radio->ch_det_threshold.low_th;
		break;
	case V4L2_CID_PRIVATE_SINR_THRESHOLD:
		retval = hci_cmd(HCI_FM_GET_DET_CH_TH_CMD, radio->fm_hdev);
		if (retval < 0) {
			FMDERR("Get SINR threshold failed %x", retval);
			return retval;
		}
		ctrl->value = radio->ch_det_threshold.sinr;
		break;
	case V4L2_CID_PRIVATE_SINR_SAMPLES:
		retval = hci_cmd(HCI_FM_GET_DET_CH_TH_CMD, radio->fm_hdev);
		if (retval < 0) {
			FMDERR("Get SINR samples failed %x", retval);
			return retval;
		}

		ctrl->value = radio->ch_det_threshold.sinr_samples;
		break;
	default:
		retval = -EINVAL;
	}
	if (retval < 0)
		FMDERR("get control failed with %d, id: %d\n",
			retval, ctrl->id);
	return retval;
}

static int iris_vidioc_g_ext_ctrls(struct file *file, void *priv,
			struct v4l2_ext_controls *ctrl)
{
	int retval = 0;
	char *data = NULL;
	struct iris_device *radio = video_get_drvdata(video_devdata(file));
	struct hci_fm_def_data_rd_req default_data_rd;

	switch ((ctrl->controls[0]).id) {
	case V4L2_CID_PRIVATE_IRIS_READ_DEFAULT:
		data = (ctrl->controls[0]).string;
		memset(&default_data_rd, 0, sizeof(default_data_rd));
		if (copy_from_user(&default_data_rd.mode, data,
					sizeof(default_data_rd)))
			return -EFAULT;
		retval = hci_def_data_read(&default_data_rd, radio->fm_hdev);
		break;
	default:
		retval = -EINVAL;
	}

	return retval;
}

static int iris_vidioc_s_ext_ctrls(struct file *file, void *priv,
			struct v4l2_ext_controls *ctrl)
{
	int retval = 0;
	int bytes_to_copy;
	struct hci_fm_tx_ps tx_ps;
	struct hci_fm_tx_rt tx_rt;
	struct hci_fm_def_data_wr_req default_data;
	struct hci_fm_set_cal_req_proc proc_cal_req;

	struct iris_device *radio = video_get_drvdata(video_devdata(file));
	char *data = NULL;

	switch ((ctrl->controls[0]).id) {
	case V4L2_CID_RDS_TX_PS_NAME:
		FMDBG("In V4L2_CID_RDS_TX_PS_NAME\n");
		/*Pass a sample PS string */

		memset(tx_ps.ps_data, 0, MAX_PS_LENGTH);
		bytes_to_copy = min((int)(ctrl->controls[0]).size,
			MAX_PS_LENGTH);
		data = (ctrl->controls[0]).string;

		if (copy_from_user(tx_ps.ps_data,
				data, bytes_to_copy))
				return -EFAULT;
		tx_ps.ps_control =  0x01;
		tx_ps.pi = radio->pi;
		tx_ps.pty = radio->pty;
		tx_ps.ps_repeatcount = radio->ps_repeatcount;
		tx_ps.ps_len = bytes_to_copy;

		retval = radio_hci_request(radio->fm_hdev, hci_trans_ps_req,
				(unsigned long)&tx_ps, RADIO_HCI_TIMEOUT);
		break;
	case V4L2_CID_RDS_TX_RADIO_TEXT:
		bytes_to_copy =
		    min((int)(ctrl->controls[0]).size, MAX_RT_LENGTH);
		data = (ctrl->controls[0]).string;

		memset(tx_rt.rt_data, 0, MAX_RT_LENGTH);

		if (copy_from_user(tx_rt.rt_data,
				data, bytes_to_copy))
				return -EFAULT;

		tx_rt.rt_control =  0x01;
		tx_rt.pi = radio->pi;
		tx_rt.pty = radio->pty;
		tx_rt.ps_len = bytes_to_copy;

		retval = radio_hci_request(radio->fm_hdev, hci_trans_rt_req,
				(unsigned long)&tx_rt, RADIO_HCI_TIMEOUT);
		break;
	case V4L2_CID_PRIVATE_IRIS_WRITE_DEFAULT:
		data = (ctrl->controls[0]).string;
		memset(&default_data, 0, sizeof(default_data));
		/*
		 * Check if length of the 'FM Default Data' to be sent
		 * is within the maximum  'FM Default Data' packet limit.
		 * Max. 'FM Default Data' packet length is 251 bytes:
		 *	1 byte    - XFR Mode
		 *	1 byte    - length of the default data
		 *	249 bytes - actual data to be configured
		 */
		if (ctrl->controls[0].size > (DEFAULT_DATA_SIZE + 2)) {
			pr_err("%s: Default data buffer overflow!\n", __func__);
			return -EINVAL;
		}

		/* copy only 'size' bytes of data as requested by user */
		retval = copy_from_user(&default_data, data,
			ctrl->controls[0].size);
		if (retval > 0) {
			pr_err("%s: Failed to copy %d bytes of default data"
				" passed by user\n", __func__, retval);
			return -EFAULT;
		}
		FMDBG("%s: XFR Mode\t: 0x%x\n", __func__, default_data.mode);
		FMDBG("%s: XFR Data Length\t: %d\n", __func__,
			default_data.length);
		/*
		 * Check if the 'length' of the actual XFR data to be configured
		 * is valid or not. Length of actual XFR data should be always
		 * 2 bytes less than the total length of the 'FM Default Data'.
		 * Length of 'FM Default Data' DEF_DATA_LEN: (1+1+XFR Data Size)
		 * Length of 'Actual XFR Data' XFR_DATA_LEN: (DEF_DATA_LEN - 2)
		 */
		if (default_data.length != (ctrl->controls[0].size - 2)) {
			pr_err("%s: Invalid 'length' parameter passed for "
				"actual xfr data\n", __func__);
			return -EINVAL;
		}
		retval = hci_def_data_write(&default_data, radio->fm_hdev);
		break;
	case V4L2_CID_PRIVATE_IRIS_SET_CALIBRATION:
		data = (ctrl->controls[0]).string;
		bytes_to_copy = (ctrl->controls[0]).size;
		if (bytes_to_copy < PROCS_CALIB_SIZE) {
			FMDERR("data is less than required size");
			return -EFAULT;
		}
		memset(proc_cal_req.data, 0, PROCS_CALIB_SIZE);
		proc_cal_req.mode = PROCS_CALIB_MODE;
		if (copy_from_user(&proc_cal_req.data[0],
				data, sizeof(proc_cal_req.data)))
				return -EFAULT;
		retval = radio_hci_request(radio->fm_hdev,
				hci_fm_set_cal_req_proc,
				(unsigned long)&proc_cal_req,
				 RADIO_HCI_TIMEOUT);
		if (retval < 0)
			FMDERR("Set Process calibration failed %d", retval);
		break;
	default:
		FMDBG("Shouldn't reach here\n");
		retval = -1;
	}
	return retval;
}

static int iris_vidioc_s_ctrl(struct file *file, void *priv,
		struct v4l2_control *ctrl)
{
	struct iris_device *radio = video_get_drvdata(video_devdata(file));
	int retval = 0;
	unsigned int rds_grps_proc = 0;
	__u8 temp_val = 0;
	unsigned long arg = 0;
	struct hci_fm_tx_ps tx_ps = {0};
	struct hci_fm_tx_rt tx_rt = {0};
	struct hci_fm_def_data_rd_req rd_txgain;
	struct hci_fm_def_data_wr_req wr_txgain;

	switch (ctrl->id) {
	case V4L2_CID_PRIVATE_IRIS_TX_TONE:
		radio->tone_freq = ctrl->value;
		retval = radio_hci_request(radio->fm_hdev,
				hci_fm_tone_generator, arg,
				msecs_to_jiffies(RADIO_HCI_TIMEOUT));
		if (retval < 0)
			FMDERR("Error while setting the tone %d", retval);
		break;
	case V4L2_CID_AUDIO_VOLUME:
		break;
	case V4L2_CID_AUDIO_MUTE:
		radio->mute_mode.hard_mute = ctrl->value;
		radio->mute_mode.soft_mute = IOC_SFT_MUTE;
		retval = hci_set_fm_mute_mode(
				&radio->mute_mode,
				radio->fm_hdev);
		if (retval < 0)
			FMDERR("Error while set FM hard mute"" %d\n",
			retval);
		break;
	case V4L2_CID_PRIVATE_IRIS_SRCHMODE:
		radio->g_search_mode = ctrl->value;
		break;
	case V4L2_CID_PRIVATE_IRIS_SCANDWELL:
		radio->g_scan_time = ctrl->value;
		break;
	case V4L2_CID_PRIVATE_IRIS_SRCHON:
		iris_search(radio, ctrl->value, SRCH_DIR_UP);
		break;
	case V4L2_CID_PRIVATE_IRIS_STATE:
		switch (ctrl->value) {
		case FM_RECV:
			retval = hci_cmd(HCI_FM_ENABLE_RECV_CMD,
							 radio->fm_hdev);
			if (retval < 0) {
				FMDERR("Error while enabling RECV FM"
							" %d\n", retval);
				return retval;
			}
			radio->mode = FM_RECV;
			radio->mute_mode.soft_mute = CTRL_ON;
			retval = hci_set_fm_mute_mode(
						&radio->mute_mode,
							radio->fm_hdev);
			if (retval < 0) {
				FMDERR("Failed to enable Smute\n");
				return retval;
			}
			radio->stereo_mode.stereo_mode = CTRL_OFF;
			radio->stereo_mode.sig_blend = CTRL_ON;
			radio->stereo_mode.intf_blend = CTRL_ON;
			radio->stereo_mode.most_switch = CTRL_ON;
			retval = hci_set_fm_stereo_mode(
						&radio->stereo_mode,
							radio->fm_hdev);
			if (retval < 0) {
				FMDERR("Failed to set stereo mode\n");
				return retval;
			}
			radio->event_mask = SIG_LEVEL_INTR |
						RDS_SYNC_INTR | AUDIO_CTRL_INTR;
			retval = hci_conf_event_mask(&radio->event_mask,
							radio->fm_hdev);
			if (retval < 0) {
				FMDERR("Enable Async events failed");
				return retval;
			}
			retval = hci_cmd(HCI_FM_GET_RECV_CONF_CMD,
						radio->fm_hdev);
			if (retval < 0)
				FMDERR("Failed to get the Recv Config\n");
			break;
		case FM_TRANS:
			retval = hci_cmd(HCI_FM_ENABLE_TRANS_CMD,
							 radio->fm_hdev);
			radio->mode = FM_TRANS;
			if (retval < 0) {
				FMDERR("Error while enabling TRANS FM"
							" %d\n", retval);
				return retval;
			}
			retval = hci_cmd(HCI_FM_GET_TX_CONFIG, radio->fm_hdev);
			if (retval < 0)
				FMDERR("get frequency failed %d\n", retval);
			break;
		case FM_OFF:
			switch (radio->mode) {
			case FM_RECV:
				retval = hci_cmd(HCI_FM_DISABLE_RECV_CMD,
						radio->fm_hdev);
				if (retval < 0)
					FMDERR("Err on disable recv FM"
						   " %d\n", retval);
				break;
			case FM_TRANS:
				retval = hci_cmd(HCI_FM_DISABLE_TRANS_CMD,
						radio->fm_hdev);

				if (retval < 0)
					FMDERR("Err disabling trans FM"
						" %d\n", retval);
				break;
			default:
				retval = -EINVAL;
			}
			break;
		default:
			retval = -EINVAL;
		}
		break;
	case V4L2_CID_PRIVATE_IRIS_REGION:
		if (radio->mode == FM_RECV) {
			retval = iris_recv_set_region(radio, ctrl->value);
		} else {
			if (radio->mode == FM_TRANS)
				retval = iris_trans_set_region(radio,
						ctrl->value);
			else
				retval = -EINVAL;
		}
		break;
	case V4L2_CID_PRIVATE_IRIS_SIGNAL_TH:
		temp_val = ctrl->value;
		retval = hci_fm_set_signal_threshold(
				&temp_val,
				radio->fm_hdev);
		if (retval < 0) {
			FMDERR("Error while setting signal threshold\n");
			break;
		}
		break;
	case V4L2_CID_PRIVATE_IRIS_SRCH_PTY:
		radio->srch_rds.srch_pty = ctrl->value;
		radio->srch_st_list.srch_pty = ctrl->value;
		break;
	case V4L2_CID_PRIVATE_IRIS_SRCH_PI:
		radio->srch_rds.srch_pi = ctrl->value;
		break;
	case V4L2_CID_PRIVATE_IRIS_SRCH_CNT:
		break;
	case V4L2_CID_PRIVATE_IRIS_SPACING:
		if (radio->mode == FM_RECV) {
			radio->recv_conf.ch_spacing = ctrl->value;
			retval = hci_set_fm_recv_conf(
					&radio->recv_conf,
						radio->fm_hdev);
			if (retval < 0)
				FMDERR("Error in setting channel spacing");
		}
		break;
	case V4L2_CID_PRIVATE_IRIS_EMPHASIS:
		switch (radio->mode) {
		case FM_RECV:
			radio->recv_conf.emphasis = ctrl->value;
			retval = hci_set_fm_recv_conf(
					&radio->recv_conf,
						radio->fm_hdev);
			if (retval < 0)
				FMDERR("Error in setting emphasis");
			break;
		case FM_TRANS:
			radio->trans_conf.emphasis = ctrl->value;
			retval = hci_set_fm_trans_conf(
					&radio->trans_conf,
						radio->fm_hdev);
			if (retval < 0)
				FMDERR("Error in setting emphasis");
			break;
		default:
			retval = -EINVAL;
		}
		break;
	case V4L2_CID_PRIVATE_IRIS_RDS_STD:
		switch (radio->mode) {
		case FM_RECV:
			radio->recv_conf.rds_std = ctrl->value;
			retval = hci_set_fm_recv_conf(
					&radio->recv_conf,
						radio->fm_hdev);
			if (retval < 0)
				FMDERR("Error in rds_std");
			break;
		case FM_TRANS:
			radio->trans_conf.rds_std = ctrl->value;
			retval = hci_set_fm_trans_conf(
					&radio->trans_conf,
						radio->fm_hdev);
			if (retval < 0)
				FMDERR("Error in rds_Std");
			break;
		default:
			retval = -EINVAL;
		}
		break;
	case V4L2_CID_PRIVATE_IRIS_RDSON:
		switch (radio->mode) {
		case FM_RECV:
			radio->recv_conf.rds_std = ctrl->value;
			retval = hci_set_fm_recv_conf(
					&radio->recv_conf,
						radio->fm_hdev);
			if (retval < 0)
				FMDERR("Error in rds_std");
			break;
		case FM_TRANS:
			radio->trans_conf.rds_std = ctrl->value;
			retval = hci_set_fm_trans_conf(
					&radio->trans_conf,
						radio->fm_hdev);
			if (retval < 0)
				FMDERR("Error in rds_Std");
			break;
		default:
			retval = -EINVAL;
		}
		break;
	case V4L2_CID_PRIVATE_IRIS_RDSGROUP_MASK:
		radio->rds_grp.rds_grp_enable_mask = ctrl->value;
		retval = hci_fm_rds_grp(&radio->rds_grp, radio->fm_hdev);
		break;
	case V4L2_CID_PRIVATE_IRIS_RDSGROUP_PROC:
		rds_grps_proc = radio->g_rds_grp_proc_ps | ctrl->value;
		radio->g_rds_grp_proc_ps = (rds_grps_proc >> RDS_CONFIG_OFFSET);
		retval = hci_fm_rds_grps_process(
				&radio->g_rds_grp_proc_ps,
				radio->fm_hdev);
		break;
	case V4L2_CID_PRIVATE_IRIS_RDSD_BUF:
		radio->rds_grp.rds_buf_size = ctrl->value;
		break;
	case V4L2_CID_PRIVATE_IRIS_PSALL:
		rds_grps_proc = (ctrl->value << RDS_CONFIG_OFFSET);
		radio->g_rds_grp_proc_ps |= rds_grps_proc;
		retval = hci_fm_rds_grps_process(
				&radio->g_rds_grp_proc_ps,
				radio->fm_hdev);
		break;
	case V4L2_CID_PRIVATE_IRIS_AF_JUMP:
		/*Clear the current AF jump settings*/
		radio->g_rds_grp_proc_ps &= ~(1 << RDS_AF_JUMP_OFFSET);
		radio->af_jump_bit = ctrl->value;
		rds_grps_proc = 0x00;
		rds_grps_proc = (ctrl->value << RDS_AF_JUMP_OFFSET);
		radio->g_rds_grp_proc_ps |= rds_grps_proc;
		retval = hci_fm_rds_grps_process(
				&radio->g_rds_grp_proc_ps,
				radio->fm_hdev);
		break;
	case V4L2_CID_PRIVATE_IRIS_LP_MODE:
		set_low_power_mode(radio, ctrl->value);
		break;
	case V4L2_CID_PRIVATE_IRIS_ANTENNA:
		temp_val = ctrl->value;
		retval = hci_fm_set_antenna(&temp_val, radio->fm_hdev);
		if (retval < 0) {
			FMDERR("Set Antenna failed retval = %x", retval);
			return retval;
		}
		radio->g_antenna =  ctrl->value;
		break;
	case V4L2_CID_RDS_TX_PTY:
		radio->pty = ctrl->value;
		break;
	case V4L2_CID_RDS_TX_PI:
		radio->pi = ctrl->value;
		break;
	case V4L2_CID_PRIVATE_IRIS_STOP_RDS_TX_PS_NAME:
		tx_ps.ps_control =  0x00;
		retval = radio_hci_request(radio->fm_hdev, hci_trans_ps_req,
				(unsigned long)&tx_ps, RADIO_HCI_TIMEOUT);
		break;
	case V4L2_CID_PRIVATE_IRIS_STOP_RDS_TX_RT:
		tx_rt.rt_control =  0x00;
		retval = radio_hci_request(radio->fm_hdev, hci_trans_rt_req,
				(unsigned long)&tx_rt, RADIO_HCI_TIMEOUT);
		break;
	case V4L2_CID_PRIVATE_IRIS_TX_SETPSREPEATCOUNT:
		radio->ps_repeatcount = ctrl->value;
		break;
	case V4L2_CID_TUNE_POWER_LEVEL:
		if (ctrl->value > FM_TX_PWR_LVL_MAX)
			ctrl->value = FM_TX_PWR_LVL_MAX;
		if (ctrl->value < FM_TX_PWR_LVL_0)
			ctrl->value = FM_TX_PWR_LVL_0;
		rd_txgain.mode = FM_TX_PHY_CFG_MODE;
		rd_txgain.length = FM_TX_PHY_CFG_LEN;
		rd_txgain.param_len = 0x00;
		rd_txgain.param = 0x00;

		retval = hci_def_data_read(&rd_txgain, radio->fm_hdev);
		if (retval < 0) {
			FMDERR("Default data read failed for PHY_CFG %d\n",
			retval);
			break;
		}
		memset(&wr_txgain, 0, sizeof(wr_txgain));
		wr_txgain.mode = FM_TX_PHY_CFG_MODE;
		wr_txgain.length = FM_TX_PHY_CFG_LEN;
		memcpy(&wr_txgain.data, &radio->default_data.data,
					radio->default_data.ret_data_len);
		wr_txgain.data[FM_TX_PWR_GAIN_OFFSET] =
				(ctrl->value) * FM_TX_PWR_LVL_STEP_SIZE;
		retval = hci_def_data_write(&wr_txgain, radio->fm_hdev);
		if (retval < 0)
			FMDERR("Default write failed for PHY_TXGAIN %d\n",
			retval);
		break;
	case V4L2_CID_PRIVATE_IRIS_SOFT_MUTE:
		radio->mute_mode.soft_mute = ctrl->value;
		retval = hci_set_fm_mute_mode(
				&radio->mute_mode,
				radio->fm_hdev);
		if (retval < 0)
			FMDERR("Error while setting FM soft mute"" %d\n",
			retval);
		break;
	case V4L2_CID_PRIVATE_IRIS_RIVA_ACCS_ADDR:
		radio->riva_data_req.cmd_params.start_addr = ctrl->value;
		break;
	case V4L2_CID_PRIVATE_IRIS_RIVA_ACCS_LEN:
		radio->riva_data_req.cmd_params.length = ctrl->value;
		break;
	case V4L2_CID_PRIVATE_IRIS_RIVA_POKE:
		memcpy(radio->riva_data_req.data, (void *)ctrl->value,
					radio->riva_data_req.cmd_params.length);
		radio->riva_data_req.cmd_params.subopcode = RIVA_POKE_OPCODE;
		retval = hci_poke_data(&radio->riva_data_req , radio->fm_hdev);
		break;
	case V4L2_CID_PRIVATE_IRIS_SSBI_ACCS_ADDR:
		radio->ssbi_data_accs.start_addr = ctrl->value;
		break;
	case V4L2_CID_PRIVATE_IRIS_SSBI_POKE:
		radio->ssbi_data_accs.data = ctrl->value;
		retval = hci_ssbi_poke_reg(&radio->ssbi_data_accs ,
								radio->fm_hdev);
		break;
	case V4L2_CID_PRIVATE_IRIS_RIVA_PEEK:
		radio->riva_data_req.cmd_params.subopcode = RIVA_PEEK_OPCODE;
		ctrl->value = hci_peek_data(&radio->riva_data_req.cmd_params ,
						radio->fm_hdev);
		break;
	case V4L2_CID_PRIVATE_IRIS_SSBI_PEEK:
		radio->ssbi_peek_reg.start_address = ctrl->value;
		hci_ssbi_peek_reg(&radio->ssbi_peek_reg, radio->fm_hdev);
		break;
	case V4L2_CID_PRIVATE_IRIS_RDS_GRP_COUNTERS:
		temp_val = ctrl->value;
		hci_read_grp_counters(&temp_val, radio->fm_hdev);
		break;
	case V4L2_CID_PRIVATE_IRIS_HLSI:
		retval = hci_cmd(HCI_FM_GET_RECV_CONF_CMD,
						radio->fm_hdev);
		if (retval)
			break;
		radio->recv_conf.hlsi = ctrl->value;
		retval = hci_set_fm_recv_conf(
					&radio->recv_conf,
						radio->fm_hdev);
		break;
	case V4L2_CID_PRIVATE_IRIS_SET_NOTCH_FILTER:
		temp_val = ctrl->value;
		retval = hci_set_notch_filter(&temp_val, radio->fm_hdev);
		break;
	case V4L2_CID_PRIVATE_INTF_HIGH_THRESHOLD:
		retval = hci_cmd(HCI_FM_GET_DET_CH_TH_CMD, radio->fm_hdev);
		if (retval < 0) {
			FMDERR("Failed to get chnl det thresholds  %d", retval);
			return retval;
		}
		radio->ch_det_threshold.high_th = ctrl->value;
		retval = hci_set_ch_det_thresholds_req(&radio->ch_det_threshold,
							 radio->fm_hdev);
		if (retval < 0) {
			FMDERR("Failed to set High det threshold %d ", retval);
			return retval;
		}
		break;

	case V4L2_CID_PRIVATE_INTF_LOW_THRESHOLD:
		retval = hci_cmd(HCI_FM_GET_DET_CH_TH_CMD, radio->fm_hdev);
		if (retval < 0) {
			FMDERR("Failed to get chnl det thresholds  %d", retval);
			return retval;
		}
		radio->ch_det_threshold.low_th = ctrl->value;
		retval = hci_set_ch_det_thresholds_req(&radio->ch_det_threshold,
							 radio->fm_hdev);
		if (retval < 0) {
			FMDERR("Failed to Set Low det threshold %d", retval);
			return retval;
		}
		break;

	case V4L2_CID_PRIVATE_SINR_THRESHOLD:
		retval = hci_cmd(HCI_FM_GET_DET_CH_TH_CMD, radio->fm_hdev);
		if (retval < 0) {
			FMDERR("Failed to get chnl det thresholds  %d", retval);
			return retval;
		}
		radio->ch_det_threshold.sinr = ctrl->value;
		retval = hci_set_ch_det_thresholds_req(&radio->ch_det_threshold,
							 radio->fm_hdev);
		if (retval < 0) {
			FMDERR("Failed to set SINR threshold %d", retval);
			return retval;
		}
		break;

	case V4L2_CID_PRIVATE_SINR_SAMPLES:
		retval = hci_cmd(HCI_FM_GET_DET_CH_TH_CMD, radio->fm_hdev);
		if (retval < 0) {
			FMDERR("Failed to get chnl det thresholds  %d", retval);
			return retval;
		}
		radio->ch_det_threshold.sinr_samples = ctrl->value;
		retval = hci_set_ch_det_thresholds_req(&radio->ch_det_threshold,
							 radio->fm_hdev);
	       if (retval < 0) {
			FMDERR("Failed to set SINR samples  %d", retval);
			return retval;
		}
		break;

	case V4L2_CID_PRIVATE_IRIS_SRCH_ALGORITHM:
	case V4L2_CID_PRIVATE_IRIS_SET_AUDIO_PATH:
		/*
		These private controls are place holders to keep the
		driver compatible with changes done in the frameworks
		which are specific to TAVARUA.
		*/
		retval = 0;
		break;
	default:
		retval = -EINVAL;
	}
	return retval;
}

static int iris_vidioc_g_tuner(struct file *file, void *priv,
		struct v4l2_tuner *tuner)
{
	int retval;
	struct iris_device *radio = video_get_drvdata(video_devdata(file));

	if (tuner->index > 0) {
		FMDERR("Invalid Tuner Index");
		return -EINVAL;
	}
	if (radio->mode == FM_RECV) {
		retval = hci_cmd(HCI_FM_GET_STATION_PARAM_CMD, radio->fm_hdev);
		if (retval < 0) {
			FMDERR("Failed to Get station params");
			return retval;
		}
		tuner->type = V4L2_TUNER_RADIO;
		tuner->rangelow  =
			radio->recv_conf.band_low_limit * TUNE_PARAM;
		tuner->rangehigh =
			radio->recv_conf.band_high_limit * TUNE_PARAM;
		tuner->rxsubchans = V4L2_TUNER_SUB_MONO | V4L2_TUNER_SUB_STEREO;
		tuner->capability = V4L2_TUNER_CAP_LOW;
		tuner->signal = radio->fm_st_rsp.station_rsp.rssi;
		tuner->audmode = radio->fm_st_rsp.station_rsp.stereo_prg;
		tuner->afc = 0;
	} else if (radio->mode == FM_TRANS) {
		retval = hci_cmd(HCI_FM_GET_TX_CONFIG, radio->fm_hdev);
		if (retval < 0) {
			FMDERR("get Tx config failed %d\n", retval);
			return retval;
		} else {
			tuner->type = V4L2_TUNER_RADIO;
			tuner->rangelow =
				radio->trans_conf.band_low_limit * TUNE_PARAM;
			tuner->rangehigh =
				radio->trans_conf.band_high_limit * TUNE_PARAM;
		}

	} else
		return -EINVAL;
	return 0;
}

static int iris_vidioc_s_tuner(struct file *file, void *priv,
		struct v4l2_tuner *tuner)
{
	struct iris_device *radio = video_get_drvdata(video_devdata(file));
	int retval = 0;
	if (tuner->index > 0)
		return -EINVAL;

	if (radio->mode == FM_RECV) {
		radio->recv_conf.band_low_limit = tuner->rangelow / TUNE_PARAM;
		radio->recv_conf.band_high_limit =
			tuner->rangehigh / TUNE_PARAM;
		if (tuner->audmode == V4L2_TUNER_MODE_MONO) {
			radio->stereo_mode.stereo_mode = 0x01;
			retval = hci_set_fm_stereo_mode(
					&radio->stereo_mode,
					radio->fm_hdev);
		} else {
			radio->stereo_mode.stereo_mode = 0x00;
			retval = hci_set_fm_stereo_mode(
					&radio->stereo_mode,
					radio->fm_hdev);
		}
		if (retval < 0)
			FMDERR(": set tuner failed with %d\n", retval);
		return retval;
	} else if (radio->mode == FM_TRANS) {
			radio->trans_conf.band_low_limit =
				tuner->rangelow / TUNE_PARAM;
			radio->trans_conf.band_high_limit =
				tuner->rangehigh / TUNE_PARAM;
	} else
		return -EINVAL;

	return retval;
}

static int iris_vidioc_g_frequency(struct file *file, void *priv,
		struct v4l2_frequency *freq)
{
	struct iris_device *radio = video_get_drvdata(video_devdata(file));
	if ((freq != NULL) && (radio != NULL)) {
		freq->frequency =
			radio->fm_st_rsp.station_rsp.station_freq * TUNE_PARAM;
	} else
		return -EINVAL;
	return 0;
}

static int iris_vidioc_s_frequency(struct file *file, void *priv,
					struct v4l2_frequency *freq)
{
	struct iris_device  *radio = video_get_drvdata(video_devdata(file));
	int retval = -1;
	freq->frequency = freq->frequency / TUNE_PARAM;

	if (freq->type != V4L2_TUNER_RADIO)
		return -EINVAL;

	/* We turn off RDS prior to tuning to a new station.
	   because of a bug in SoC which prevents tuning
	   during RDS transmission.
	 */
	if (radio->mode == FM_TRANS
		&& (radio->trans_conf.rds_std == 0 ||
			radio->trans_conf.rds_std == 1)) {
		radio->prev_trans_rds = radio->trans_conf.rds_std;
		radio->trans_conf.rds_std = 2;
		hci_set_fm_trans_conf(&radio->trans_conf,
				radio->fm_hdev);
	}

	retval = iris_set_freq(radio, freq->frequency);

	if (radio->mode == FM_TRANS
		 && radio->trans_conf.rds_std == 2
			&& (radio->prev_trans_rds == 1
				|| radio->prev_trans_rds == 0)) {
		radio->trans_conf.rds_std = radio->prev_trans_rds;
		hci_set_fm_trans_conf(&radio->trans_conf,
				radio->fm_hdev);
	}

	if (retval < 0)
		FMDERR(" set frequency failed with %d\n", retval);
	return retval;
}

static int iris_vidioc_dqbuf(struct file *file, void *priv,
				struct v4l2_buffer *buffer)
{
	struct iris_device  *radio = video_get_drvdata(video_devdata(file));
	enum iris_buf_t buf_type = buffer->index;
	struct kfifo *data_fifo;
	unsigned char *buf = (unsigned char *)buffer->m.userptr;
	unsigned int len = buffer->length;
	if (!access_ok(VERIFY_WRITE, buf, len))
		return -EFAULT;
	if ((buf_type < IRIS_BUF_MAX) && (buf_type >= 0)) {
		data_fifo = &radio->data_buf[buf_type];
		if (buf_type == IRIS_BUF_EVENTS)
			if (wait_event_interruptible(radio->event_queue,
				kfifo_len(data_fifo)) < 0)
				return -EINTR;
	} else {
		FMDERR("invalid buffer type\n");
		return -EINVAL;
	}
	buffer->bytesused = kfifo_out_locked(data_fifo, buf, len,
					&radio->buf_lock[buf_type]);

	return 0;
}

static int iris_vidioc_g_fmt_type_private(struct file *file, void *priv,
						struct v4l2_format *f)
{
	return 0;

}

static int iris_vidioc_s_hw_freq_seek(struct file *file, void *priv,
					struct v4l2_hw_freq_seek *seek)
{
	struct iris_device *radio = video_get_drvdata(video_devdata(file));
	int dir;
	if (seek->seek_upward)
		dir = SRCH_DIR_UP;
	else
		dir = SRCH_DIR_DOWN;
	return iris_search(radio, CTRL_ON, dir);
}

static int iris_vidioc_querycap(struct file *file, void *priv,
	struct v4l2_capability *capability)
{
	struct iris_device *radio;
	radio = video_get_drvdata(video_devdata(file));
	strlcpy(capability->driver, DRIVER_NAME, sizeof(capability->driver));
	strlcpy(capability->card, DRIVER_CARD, sizeof(capability->card));
	radio->g_cap = capability;
	return 0;
}


static const struct v4l2_ioctl_ops iris_ioctl_ops = {
	.vidioc_querycap              = iris_vidioc_querycap,
	.vidioc_queryctrl             = iris_vidioc_queryctrl,
	.vidioc_g_ctrl                = iris_vidioc_g_ctrl,
	.vidioc_s_ctrl                = iris_vidioc_s_ctrl,
	.vidioc_g_tuner               = iris_vidioc_g_tuner,
	.vidioc_s_tuner               = iris_vidioc_s_tuner,
	.vidioc_g_frequency           = iris_vidioc_g_frequency,
	.vidioc_s_frequency           = iris_vidioc_s_frequency,
	.vidioc_s_hw_freq_seek        = iris_vidioc_s_hw_freq_seek,
	.vidioc_dqbuf                 = iris_vidioc_dqbuf,
	.vidioc_g_fmt_type_private    = iris_vidioc_g_fmt_type_private,
	.vidioc_s_ext_ctrls           = iris_vidioc_s_ext_ctrls,
	.vidioc_g_ext_ctrls           = iris_vidioc_g_ext_ctrls,
};

static const struct v4l2_file_operations iris_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = video_ioctl2,
};

static struct video_device iris_viddev_template = {
	.fops                   = &iris_fops,
	.ioctl_ops              = &iris_ioctl_ops,
	.name                   = DRIVER_NAME,
	.release                = video_device_release,
};

static struct video_device *video_get_dev(void)
{
	return priv_videodev;
}

static int __init iris_probe(struct platform_device *pdev)
{
	struct iris_device *radio;
	int retval;
	int radio_nr = -1;
	int i;

	if (!pdev) {
		FMDERR(": pdev is null\n");
		return -ENOMEM;
	}

	radio = kzalloc(sizeof(struct iris_device), GFP_KERNEL);
	if (!radio) {
		FMDERR(": Could not allocate radio device\n");
		return -ENOMEM;
	}

	radio->dev = &pdev->dev;
	platform_set_drvdata(pdev, radio);

	radio->videodev = video_device_alloc();
	if (!radio->videodev) {
		FMDERR(": Could not allocate V4L device\n");
		kfree(radio);
		return -ENOMEM;
	}

	memcpy(radio->videodev, &iris_viddev_template,
	  sizeof(iris_viddev_template));

	for (i = 0; i < IRIS_BUF_MAX; i++) {
		int kfifo_alloc_rc = 0;
		spin_lock_init(&radio->buf_lock[i]);

		if ((i == IRIS_BUF_RAW_RDS) || (i == IRIS_BUF_PEEK))
			kfifo_alloc_rc = kfifo_alloc(&radio->data_buf[i],
				rds_buf*3, GFP_KERNEL);
		else if ((i == IRIS_BUF_CAL_DATA) || (i == IRIS_BUF_RT_RDS))
			kfifo_alloc_rc = kfifo_alloc(&radio->data_buf[i],
				STD_BUF_SIZE*2, GFP_KERNEL);
		else
			kfifo_alloc_rc = kfifo_alloc(&radio->data_buf[i],
				STD_BUF_SIZE, GFP_KERNEL);

		if (kfifo_alloc_rc != 0) {
			FMDERR("failed allocating buffers %d\n",
				   kfifo_alloc_rc);
			for (; i > -1; i--) {
				kfifo_free(&radio->data_buf[i]);
				kfree(radio);
				return -ENOMEM;
			}
		}
	}

	mutex_init(&radio->lock);
	init_completion(&radio->sync_xfr_start);
	radio->tune_req = 0;
	radio->prev_trans_rds = 2;
	init_waitqueue_head(&radio->event_queue);
	init_waitqueue_head(&radio->read_queue);

	video_set_drvdata(radio->videodev, radio);

	if (NULL == video_get_drvdata(radio->videodev))
		FMDERR(": video_get_drvdata failed\n");

	retval = video_register_device(radio->videodev, VFL_TYPE_RADIO,
								   radio_nr);
	if (retval) {
		FMDERR(": Could not register video device\n");
		video_device_release(radio->videodev);
		for (; i > -1; i--)
			kfifo_free(&radio->data_buf[i]);
		kfree(radio);
		return retval;
	} else {
		priv_videodev = kzalloc(sizeof(struct video_device),
			GFP_KERNEL);
		memcpy(priv_videodev, radio->videodev,
			sizeof(struct video_device));
	}
	return 0;
}


static int __devexit iris_remove(struct platform_device *pdev)
{
	int i;
	struct iris_device *radio = platform_get_drvdata(pdev);

	video_unregister_device(radio->videodev);

	for (i = 0; i < IRIS_BUF_MAX; i++)
		kfifo_free(&radio->data_buf[i]);

	kfree(radio);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver iris_driver = {
	.driver = {
		.owner  = THIS_MODULE,
		.name   = "iris_fm",
	},
	.remove = __devexit_p(iris_remove),
};

static int __init iris_radio_init(void)
{
	return platform_driver_probe(&iris_driver, iris_probe);
}
module_init(iris_radio_init);

static void __exit iris_radio_exit(void)
{
	platform_driver_unregister(&iris_driver);
}
module_exit(iris_radio_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
