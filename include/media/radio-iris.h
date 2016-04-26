/*
 *
 * Copyright (c) 2011-2015 The Linux Foundation. All rights reserved.
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

#ifndef __RADIO_IRIS_H
#define __RADIO_IRIS_H

#include <uapi/media/radio-iris.h>

#include <linux/skbuff.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/atomic.h>

extern struct mutex fm_smd_enable;

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
	void (*close_smd)(void);
};

int radio_hci_register_dev(struct radio_hci_dev *hdev);
int radio_hci_unregister_dev(void);
int radio_hci_recv_frame(struct sk_buff *skb);
int radio_hci_send_cmd(struct radio_hci_dev *hdev, __u16 opcode, __u32 plen,
	void *param);
void radio_hci_event_packet(struct radio_hci_dev *hdev, struct sk_buff *skb);

#define hci_req_lock(d)		mutex_lock(&d->req_lock)
#define hci_req_unlock(d)	mutex_unlock(&d->req_lock)

#define FMDBG(fmt, args...) pr_debug(fmt "\n", ##args)

#undef FMDERR
#define FMDERR(fmt, args...) pr_err("iris_radio: " fmt, ##args)

/* HCI timeouts */
#define RADIO_HCI_TIMEOUT	(1500)	/* 1.5 seconds */

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

static inline int is_valid_blend_value(int val)
{
	if ((val >= MIN_BLEND_HI) && (val <= MAX_BLEND_HI))
		return 1;
	else
		return 0;
}

#endif

