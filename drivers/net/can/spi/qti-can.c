// SPDX-License-Identifier: GPL-2.0-only

/* Copyright (c) 2015-2021, The Linux Foundation. All rights reserved. */
/* Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved. */

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/workqueue.h>
#include <linux/spi/spi.h>
#include <linux/can.h>
#include <linux/can/dev.h>
#include <linux/can/error.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/uaccess.h>
#include <linux/pm.h>
#include <asm/arch_timer.h>
#include <asm/div64.h>
#include <linux/suspend.h>
#include <linux/pm_runtime.h>

#define MAX_TX_BUFFERS			1
#define XFER_BUFFER_SIZE		64
#define RX_ASSEMBLY_BUFFER_SIZE		128
#define RX_FD_BUFFER_SIZE		82
#define QTI_CAN_FW_QUERY_RETRY_COUNT	3
#define DRIVER_MODE_RAW_FRAMES		0
#define DRIVER_MODE_PROPERTIES		1
#define DRIVER_MODE_AMB			2
#define QUERY_FIRMWARE_TIMEOUT_MS	100
#define EUPGRADE			140
#define QTIMER_DIV			192
#define QTIMER_MUL			10000
#define TIMESTAMP_PRINT_CNTR		10
#define TIME_OFFSET_MAX_THD		30
#define TIME_OFFSET_MIN_THD		-30
#define CAN_FD_HEADER			14
#define CAN_FD_PACKET_DATA		32
#define CAN_FD_PACKET_SIZE		46
#define CAN_FD_MAX_DATA_SIZE		64
#define CAN_STANDARD_PACKET_SIZE	22
#define CALYPSO_MAX_CAN_CLK_FREQ	40000000 /* 40MHz */

static int static_pos_checksum_en;
static int dynamic_pos_checksum_en;
static int checksum_enable;

struct qti_can {
	struct net_device	**netdev;
	struct spi_device	*spidev;
	struct mutex spi_lock; /* SPI device lock */
	struct workqueue_struct *tx_wq;
	char *tx_buf, *rx_buf;
	int xfer_length;
	atomic_t msg_seq;
	char *assembly_buffer;
	u8 assembly_buffer_size;
	char *fd_buffer;
	atomic_t netif_queue_stop;
	struct completion response_completion;
	int wait_cmd;
	int cmd_result;
	int driver_mode;
	int clk_freq_mhz;
	int max_can_channels;
	int bits_per_word;
	int reset_delay_msec;
	int reset;
	bool support_can_fd;
	bool use_qtimer;
	bool can_fw_cmd_timeout_req;
	u32 rem_all_buffering_timeout_ms;
	u32 can_fw_cmd_timeout_ms;
	s64 time_diff;
	bool active_low;
};

struct qti_can_netdev_privdata {
	struct can_priv can;
	struct qti_can *qti_can;
	u8 netdev_index;
};

struct qti_can_tx_work {
	struct work_struct work;
	struct sk_buff *skb;
	struct net_device *netdev;
};

/* Message definitions */
struct spi_mosi { /* TLV for MOSI line */
	u8 cmd;
	u8 len;
	u16 seq;
	u8 data[];
} __packed;

struct spi_miso { /* TLV for MISO line */
	u8 cmd;
	u8 len;
	u16 seq; /* should match seq field from request, or 0 for unsols */
	u8 data[];
} __packed;

#define CMD_GET_FW_VERSION		0x81
#define CMD_CAN_SEND_FRAME		0x82
#define CMD_CAN_ADD_FILTER		0x83
#define CMD_CAN_REMOVE_FILTER		0x84
#define CMD_CAN_RECEIVE_FRAME		0x85
#define CMD_CAN_CONFIG_BIT_TIMING	0x86
#define CMD_CAN_DATA_BUFF_ADD		0x87
#define CMD_CAN_DATA_BUFF_REMOVE	0X88
#define CMD_CAN_RELEASE_BUFFER		0x89
#define CMD_CAN_DATA_BUFF_REMOVE_ALL	0x8A
#define CMD_PROPERTY_WRITE		0x8B
#define CMD_PROPERTY_READ		0x8C
#define CMD_GET_FW_BR_VERSION		0x95
#define CMD_BEGIN_FIRMWARE_UPGRADE	0x96
#define CMD_FIRMWARE_UPGRADE_DATA	0x97
#define CMD_END_FIRMWARE_UPGRADE	0x98
#define CMD_BEGIN_BOOT_ROM_UPGRADE	0x99
#define CMD_BOOT_ROM_UPGRADE_DATA	0x9A
#define CMD_END_BOOT_ROM_UPGRADE	0x9B
#define CMD_END_FW_UPDATE_FILE		0x9C
#define CMD_UPDATE_TIME_INFO		0x9D
#define CMD_SUSPEND_EVENT		0x9E
#define CMD_RESUME_EVENT		0x9F

#define IOCTL_RELEASE_CAN_BUFFER	(SIOCDEVPRIVATE + 0)
#define IOCTL_ENABLE_BUFFERING		(SIOCDEVPRIVATE + 1)
#define IOCTL_ADD_FRAME_FILTER		(SIOCDEVPRIVATE + 2)
#define IOCTL_REMOVE_FRAME_FILTER	(SIOCDEVPRIVATE + 3)
#define IOCTL_DISABLE_BUFFERING		(SIOCDEVPRIVATE + 5)
#define IOCTL_DISABLE_ALL_BUFFERING	(SIOCDEVPRIVATE + 6)
#define IOCTL_GET_FW_BR_VERSION		(SIOCDEVPRIVATE + 7)
#define IOCTL_BEGIN_FIRMWARE_UPGRADE	(SIOCDEVPRIVATE + 8)
#define IOCTL_FIRMWARE_UPGRADE_DATA	(SIOCDEVPRIVATE + 9)
#define IOCTL_END_FIRMWARE_UPGRADE	(SIOCDEVPRIVATE + 10)
#define IOCTL_BEGIN_BOOT_ROM_UPGRADE	(SIOCDEVPRIVATE + 11)
#define IOCTL_BOOT_ROM_UPGRADE_DATA	(SIOCDEVPRIVATE + 12)
#define IOCTL_END_BOOT_ROM_UPGRADE	(SIOCDEVPRIVATE + 13)
#define IOCTL_END_FW_UPDATE_FILE	(SIOCDEVPRIVATE + 14)

#define IFR_DATA_OFFSET		0x100
struct can_fw_resp {
	u8 maj;
	u8 min : 4;
	u8 sub_min : 4;
	u8 ver[48];
} __packed;

struct can_write_req {
	u8 can_if;
	u32 mid;
	u8 dlc;
	u8 data[CAN_FD_MAX_DATA_SIZE];
} __packed;

struct can_write_resp {
	u8 err;
} __packed;

struct can_filter_req {
	u8 can_if;
	u32 mid;
	u32 mask;
} __packed;

struct can_add_filter_resp {
	u8 err;
} __packed;

struct can_receive_frame {
	u8 can_if;
	__le64 ts;
	__le32 mid;
	u8 dlc;
	u8 data[8];
} __packed;

struct canfd_receive_frame {
	u8 can_if;
	__le64 ts;
	__le32 mid;
	u8 dlc;
	u8 data[CAN_FD_MAX_DATA_SIZE];
} __packed;

struct can_config_bit_timing {
	u8 can_if;
	u32 prop_seg;
	u32 phase_seg1;
	u32 phase_seg2;
	u32 sjw;
	u32 brp;
} __packed;

struct can_time_info {
	__le64 time;
} __packed;

static struct can_bittiming_const rh850_bittiming_const = {
	.name = "qti_can",
	.tseg1_min = 1,
	.tseg1_max = 16,
	.tseg2_min = 1,
	.tseg2_max = 16,
	.sjw_max = 4,
	.brp_min = 1,
	.brp_max = 70,
	.brp_inc = 1,
};

static struct can_bittiming_const flexcan_bittiming_const = {
	.name = "qti_can",
	.tseg1_min = 4,
	.tseg1_max = 16,
	.tseg2_min = 2,
	.tseg2_max = 8,
	.sjw_max = 4,
	.brp_min = 1,
	.brp_max = 256,
	.brp_inc = 1,
};

static struct can_bittiming_const qti_can_bittiming_const;

static struct can_bittiming_const qti_can_data_bittiming_const = {
	.name = "qti_can",
	.tseg1_min = 1,
	.tseg1_max = 16,
	.tseg2_min = 1,
	.tseg2_max = 16,
	.sjw_max = 4,
	.brp_min = 1,
	.brp_max = 70,
	.brp_inc = 1,
};

struct vehicle_property {
	int id;
	__le64 ts;
	int zone;
	int val_type;
	u32 data_len;
	union {
		u8 bval;
		int val;
		int val_arr[4];
		float f_value;
		float float_arr[4];
		u8 str[36];
	};
} __packed;

struct qti_can_release_can_buffer {
	u8 enable;
} __packed;

struct qti_can_buffer {
	u8 can_if;
	u32 mid;
	u32 mask;
} __packed;

struct can_fw_br_resp {
	u8 maj;
	u8 min : 4;
	u8 sub_min : 4;
	u8 ver[32];
	u8 br_maj;
	u8 br_min;
	u8 curr_exec_mode;
} __packed;

struct qti_can_ioctl_req {
	u8 len;
	u8 data[64];
} __packed;

static int qti_can_rx_message(struct qti_can *priv_data);

static irqreturn_t qti_can_irq(int irq, void *priv)
{
	struct qti_can *priv_data = priv;

	qti_can_rx_message(priv_data);
	return IRQ_HANDLED;
}

static inline bool property_bool(struct device_node *np, const char *str)
{
	u32 tmp_val = 0;

	if (of_property_read_u32(np, str, &tmp_val) < 0)
		return false;
	else
		return (bool)tmp_val;
}

static inline u64 qtimer_time(void)
{
	u64 qt_count = 0;

	qt_count = arch_timer_read_counter();

	return mul_u64_u32_div(qt_count, QTIMER_MUL, QTIMER_DIV);
}

static void qti_canfd_receive_frame(struct qti_can *priv_data,
				    struct canfd_receive_frame *frame)
{
	struct canfd_frame *cf;
	struct sk_buff *skb;
	struct skb_shared_hwtstamps *skt;
	ktime_t nsec;
	struct net_device *netdev;
	int i;
	struct device *dev;
	s64 ts_offset_corrected;
	static u16 buff_frames_disc_cntr;
	static u8 disp_disc_cntr = 1;

	dev = &priv_data->spidev->dev;
	if (frame->can_if >= priv_data->max_can_channels) {
		dev_err(&priv_data->spidev->dev, "qti_can rcv error. Channel is %d\n",
			frame->can_if);
		return;
	}

	netdev = priv_data->netdev[frame->can_if];
	skb = alloc_canfd_skb(netdev, &cf);
	if (!skb) {
		dev_err(&priv_data->spidev->dev, "skb alloc failed. frame->can_if %d\n",
			frame->can_if);
		return;
	}

	dev_dbg(&priv_data->spidev->dev, "rcv frame %d %llu %x %d %x %x %x %x %x %x %x %x\n",
		frame->can_if, frame->ts, frame->mid, frame->dlc,
		frame->data[0], frame->data[1], frame->data[2], frame->data[3],
		frame->data[4], frame->data[5], frame->data[6], frame->data[7]);
	cf->can_id = le32_to_cpu(frame->mid);
	cf->len = frame->dlc;

	for (i = 0; i < cf->len; i++)
		cf->data[i] = frame->data[i];

	ts_offset_corrected = le64_to_cpu(frame->ts)
		+ priv_data->time_diff;

	/* CAN frames which are received before SOC powers up are discarded */
	if (ts_offset_corrected > 0) {
		if (disp_disc_cntr == 1) {
			dev_info(&priv_data->spidev->dev,
				 "No of buff frames discarded is %d\n",
				 buff_frames_disc_cntr);
			disp_disc_cntr = 0;
		}

		nsec = ms_to_ktime(ts_offset_corrected);
		skt = skb_hwtstamps(skb);
		skt->hwtstamp = nsec;
		skb->tstamp = nsec;

		netif_rx(skb);

		dev_dbg(&priv_data->spidev->dev, "hwtstamp: %lld\n", ktime_to_ms(skt->hwtstamp));
		netdev->stats.rx_packets++;
	} else {
		buff_frames_disc_cntr++;
		dev_kfree_skb(skb);
	}
}

static void qti_can_receive_frame(struct qti_can *priv_data,
				  struct can_receive_frame *frame)
{
	struct can_frame *cf;
	struct sk_buff *skb;
	struct skb_shared_hwtstamps *skt;
	ktime_t nsec;
	struct net_device *netdev;
	int i;
	struct device *dev;
	s64 ts_offset_corrected;
	static u16 buff_frames_disc_cntr;
	static u8 disp_disc_cntr = 1;

	dev = &priv_data->spidev->dev;
	if (frame->can_if >= priv_data->max_can_channels) {
		dev_err(&priv_data->spidev->dev, "qti_can rcv error. Channel is %d\n",
			frame->can_if);
		return;
	}

	netdev = priv_data->netdev[frame->can_if];
	skb = alloc_can_skb(netdev, &cf);
	if (!skb) {
		dev_err(&priv_data->spidev->dev, "skb alloc failed. frame->can_if %d\n",
			frame->can_if);
		return;
	}

	dev_dbg(&priv_data->spidev->dev, "rcv frame %d %llu %x %d %x %x %x %x %x %x %x %x\n",
		frame->can_if, frame->ts, frame->mid, frame->dlc,
		frame->data[0], frame->data[1], frame->data[2], frame->data[3],
		frame->data[4], frame->data[5], frame->data[6], frame->data[7]);
	cf->can_id = le32_to_cpu(frame->mid);
	cf->can_dlc = frame->dlc;

	for (i = 0; i < cf->can_dlc; i++)
		cf->data[i] = frame->data[i];

	ts_offset_corrected = le64_to_cpu(frame->ts)
		+ priv_data->time_diff;

	/* CAN frames which are received before SOC powers up are discarded */
	if (ts_offset_corrected > 0) {
		if (disp_disc_cntr == 1) {
			dev_info(&priv_data->spidev->dev,
				 "No of buff frames discarded is %d\n",
				 buff_frames_disc_cntr);
			disp_disc_cntr = 0;
		}

		nsec = ms_to_ktime(ts_offset_corrected);
		skt = skb_hwtstamps(skb);
		skt->hwtstamp = nsec;
		skb->tstamp = nsec;

		netif_rx(skb);

		dev_dbg(&priv_data->spidev->dev, "hwtstamp: %lld\n", ktime_to_ms(skt->hwtstamp));
		netdev->stats.rx_packets++;
	} else {
		buff_frames_disc_cntr++;
		kfree_skb(skb);
	}
}

static void qti_can_receive_property(struct qti_can *priv_data,
				     struct vehicle_property *property)
{
	struct canfd_frame *cfd;
	u8 *p;
	struct sk_buff *skb;
	struct skb_shared_hwtstamps *skt;
	ktime_t nsec;
	struct net_device *netdev;
	struct device *dev;
	int i;

	/* can0 as the channel with properties */
	dev = &priv_data->spidev->dev;
	netdev = priv_data->netdev[0];
	skb = alloc_canfd_skb(netdev, &cfd);
	if (!skb) {
		dev_err(&priv_data->spidev->dev, "skb alloc failed. frame->can_if %d\n", 0);
		return;
	}

	dev_dbg(&priv_data->spidev->dev, "rcv property:0x%x data:%2x %2x %2x %2x", property->id,
		property->str[0], property->str[1],
		property->str[2], property->str[3]);
	cfd->can_id = 0x00;
	cfd->len = sizeof(struct vehicle_property);

	p = (u8 *)property;
	for (i = 0; i < cfd->len; i++)
		cfd->data[i] = p[i];

	nsec = ns_to_ktime(le64_to_cpu(property->ts));
	skt = skb_hwtstamps(skb);
	skt->hwtstamp = nsec;
	dev_dbg(&priv_data->spidev->dev, "  hwtstamp %lld\n", ktime_to_ms(skt->hwtstamp));
	skb->tstamp = nsec;
	netif_rx(skb);
	netdev->stats.rx_packets++;
}

static int qti_can_process_response(struct qti_can *priv_data,
				    struct spi_miso *resp, int length)
{
	int ret = 0;
	u64 mstime;
	static s64 prev_time_diff;
	static u8 first_offset_est = 1;
	s64 offset_variation = 0;
	static u8 offset_print_cntr;
	struct canfd_receive_frame *frame;

	dev_dbg(&priv_data->spidev->dev, "<%x %2d [%d]\n", resp->cmd, resp->len, resp->seq);
	if (resp->cmd == CMD_CAN_RECEIVE_FRAME) {
		if (resp->len > CAN_STANDARD_PACKET_SIZE) {
			dev_dbg(&priv_data->spidev->dev, "can fd receive\n");
			frame = (struct canfd_receive_frame *)&resp->data;
			qti_canfd_receive_frame(priv_data, frame);
		} else {
			struct can_receive_frame *frame =
					(struct can_receive_frame *)&resp->data;
			if ((resp->len - (frame->dlc + sizeof(frame->dlc))) <
				(sizeof(*frame) - (sizeof(frame->dlc)
				+ sizeof(frame->data)))) {
				dev_err(&priv_data->spidev->dev, "len:%d, size:%zu\n",
					resp->len, sizeof(*frame));
				dev_err(&priv_data->spidev->dev,
					"Check the f/w version & upgrade to latest!!\n");
				ret = -EUPGRADE;
				goto exit;
			}
			if (resp->len > length) {
				/* Error. This should never happen */
				dev_err(&priv_data->spidev->dev, "%s error: Saving %d bytes\n",
					__func__, length);
				memcpy(priv_data->assembly_buffer, (char *)resp,
				       length);
				priv_data->assembly_buffer_size = length;
			} else {
				qti_can_receive_frame(priv_data, frame);
			}
		}
	} else if (resp->cmd == CMD_PROPERTY_READ) {
		struct vehicle_property *property =
				(struct vehicle_property *)&resp->data;

		if (resp->len > length) {
			/* Error. This should never happen */
			dev_err(&priv_data->spidev->dev, "%s error: Saving %d bytes\n",
				__func__, length);
			memcpy(priv_data->assembly_buffer, (char *)resp,
			       length);
			priv_data->assembly_buffer_size = length;
		} else {
			qti_can_receive_property(priv_data, property);
		}
	} else if (resp->cmd  == CMD_GET_FW_VERSION) {
		struct can_fw_resp *fw_resp = (struct can_fw_resp *)resp->data;

		if (fw_resp->maj == 4) {
			if (fw_resp->min == 0) {
				if (fw_resp->sub_min == 5 || fw_resp->sub_min == 6) {
					dev_info(&priv_data->spidev->dev,
						 "static position checksum enabled\n");
					static_pos_checksum_en = 1;
					checksum_enable = 1;
				}
			} else if (fw_resp->min == 1) {
				dev_info(&priv_data->spidev->dev,
					 "static position checksum enabled\n");
				static_pos_checksum_en = 1;
				checksum_enable = 1;
				dev_info(&priv_data->spidev->dev,
					 "can-fd support can0 enabled\n");
			} else if (fw_resp->min > 1) {
				dev_info(&priv_data->spidev->dev,
					 "dynamic position checksum enabled\n");
				dynamic_pos_checksum_en = 1;
				checksum_enable = 1;
				dev_info(&priv_data->spidev->dev,
					 "can-fd support can0 enabled\n");
			}
		} else if (fw_resp->maj > 4) {
			dev_info(&priv_data->spidev->dev, "dynamic position checksum enabled\n");
			dynamic_pos_checksum_en = 1;
			checksum_enable = 1;
			dev_info(&priv_data->spidev->dev, "can-fd support can0 enabled\n");
		}

		dev_info(&priv_data->spidev->dev, "fw %d.%d.%d\n",
			 fw_resp->maj, fw_resp->min, fw_resp->sub_min);
		dev_info(&priv_data->spidev->dev, "fw string %s\n",
			 fw_resp->ver);
	} else if (resp->cmd  == CMD_GET_FW_BR_VERSION) {
		struct can_fw_br_resp *fw_resp =
				(struct can_fw_br_resp *)resp->data;

		dev_info(&priv_data->spidev->dev, "fw_can %d.%d.%d\n",
			 fw_resp->maj, fw_resp->min, fw_resp->sub_min);
		dev_info(&priv_data->spidev->dev, "fw string %s\n",
			 fw_resp->ver);
		dev_info(&priv_data->spidev->dev, "fw_br %d.%d exec_mode %d\n",
			 fw_resp->br_maj, fw_resp->br_min,
			 fw_resp->curr_exec_mode);
		ret = fw_resp->curr_exec_mode << 28;
		ret |= (fw_resp->br_maj & 0xF) << 24;
		ret |= (fw_resp->br_min & 0xFF) << 16;
		ret |= (fw_resp->maj & 0xF) << 8;
		ret |= (fw_resp->min & 0xF) << 4;
		ret |= (fw_resp->sub_min & 0xF);
	} else if (resp->cmd == CMD_UPDATE_TIME_INFO) {
		struct can_time_info *time_data =
			(struct can_time_info *)resp->data;

		if (priv_data->use_qtimer)
			mstime = div_u64(qtimer_time(), NSEC_PER_MSEC);
		else
			mstime = ktime_to_ms(ktime_get_boottime());

		priv_data->time_diff = mstime - (le64_to_cpu(time_data->time));

		if (first_offset_est == 1) {
			prev_time_diff = priv_data->time_diff;
			first_offset_est = 0;
		}

		offset_variation = priv_data->time_diff -
					prev_time_diff;

		if (offset_variation > TIME_OFFSET_MAX_THD ||
		    offset_variation < TIME_OFFSET_MIN_THD) {
			if (offset_print_cntr < TIMESTAMP_PRINT_CNTR) {
				dev_info(&priv_data->spidev->dev,
					 "Off Exceeded: Curr off is %lld\n",
					 priv_data->time_diff);
				dev_info(&priv_data->spidev->dev,
					 "Prev off is %lld\n",
				prev_time_diff);
				offset_print_cntr++;
			}
			/* Set curr off to prev off if */
			/* variation is beyond threshold */
			priv_data->time_diff = prev_time_diff;

		} else {
			/* Set prev off to curr off if */
			/* variation is within threshold */
			prev_time_diff = priv_data->time_diff;
		}
	}

exit:
	if (resp->cmd == priv_data->wait_cmd) {
		priv_data->cmd_result = ret;
		complete(&priv_data->response_completion);
	}
	return ret;
}

static int qti_can_fd_process_rx(struct qti_can *priv_data, char *rx_buf)
{
	struct spi_miso *resp;
	int ret = 0;
	/* u64 rx_buf_idx, idx = 0; */
	int i = 0;
	u8 rx_checksum = 0;
	int checksum_rx_len = 0;

	struct can_receive_frame *frame;
	void *data;
	struct spi_miso *resp_fd;

	resp = (struct spi_miso *)rx_buf;
	dev_dbg(&priv_data->spidev->dev, "spi -> cmd %X len %d seq %d\n",
		resp->cmd, resp->len, resp->seq);
	if (static_pos_checksum_en)
		checksum_rx_len = XFER_BUFFER_SIZE - 2;
	if (resp->seq == 0) {
		if (dynamic_pos_checksum_en)
			checksum_rx_len = CAN_FD_PACKET_SIZE + 4;

		for (i = 0; i < checksum_rx_len; i++)
			rx_checksum ^= rx_buf[i];

		if (!checksum_enable)
			rx_checksum = rx_buf[checksum_rx_len];

		if (rx_checksum == rx_buf[checksum_rx_len]) {
			//store canfd frame in buffer;
			memset(priv_data->fd_buffer, 0, RX_FD_BUFFER_SIZE);
			memcpy(priv_data->fd_buffer, resp, CAN_FD_PACKET_SIZE + 4);
		} else {
			dev_err(&priv_data->spidev->dev, "checksum validation failed\n");
			dev_err(&priv_data->spidev->dev,
				"cmd_id: %x chksum_rxcal: %x chksum_rx: %x\n chksum_rx_len: %d\n",
				resp->cmd, rx_checksum, rx_buf[checksum_rx_len], checksum_rx_len);
			ret = -EINVAL;
			}
	} else if (resp->seq == 1) {
		if (dynamic_pos_checksum_en)
			checksum_rx_len = resp->len - CAN_FD_PACKET_DATA + 4;

		for (i = 0; i < checksum_rx_len; i++)
			rx_checksum ^= rx_buf[i];

		if (!checksum_enable)
			rx_checksum = rx_buf[checksum_rx_len];

		if (rx_checksum == rx_buf[checksum_rx_len]) {
			// add data of second spi packet frame to resp_fd
			frame = (struct can_receive_frame *)resp->data;
			memcpy((priv_data->fd_buffer) + CAN_FD_PACKET_SIZE + 4,
			       frame->data, frame->dlc - CAN_FD_PACKET_DATA);
			data = priv_data->fd_buffer;
			resp_fd = (struct spi_miso *)data;
			ret = qti_can_process_response(priv_data, resp_fd, 0);
		} else {
			dev_err(&priv_data->spidev->dev, "checksum validation failed\n");
			dev_err(&priv_data->spidev->dev,
				"cmd_id: %x chksum_rxcal: %x chksum_rx: %x\n chksum_rx_len: %d\n",
				resp->cmd, rx_checksum, rx_buf[checksum_rx_len], checksum_rx_len);
			ret = -EINVAL;
		}
	}
	return ret;
}

static int qti_can_process_rx(struct qti_can *priv_data, char *rx_buf)
{
	struct spi_miso *resp;
	int length_processed = 0, actual_length = priv_data->xfer_length;
	int ret = 0;
	/* u64 rx_buf_idx, idx = 0; */
	int i = 0;
	u8 rx_checksum = 0;
	u8 rx_checksum_rcvd = 0;
	int checksum_rx_len = 0;

	if (rx_buf[0] == CMD_CAN_RECEIVE_FRAME && rx_buf[1] > CAN_FD_PACKET_SIZE) {
		ret = qti_can_fd_process_rx(priv_data, rx_buf);
		return ret;
	}

	while (length_processed < actual_length) {
		int length_left = actual_length - length_processed;
		int length = 0; /* length of consumed chunk */
		void *data;

		if (priv_data->assembly_buffer_size > 0) {
			dev_dbg(&priv_data->spidev->dev, "callback: Reassembling %d bytes\n",
				priv_data->assembly_buffer_size);
			/* should copy just 1 byte instead, since cmd should */
			/* already been copied as being first byte */
			memcpy(priv_data->assembly_buffer +
			       priv_data->assembly_buffer_size,
			       rx_buf, 2);
			data = priv_data->assembly_buffer;
			resp = (struct spi_miso *)data;
			length = resp->len + sizeof(*resp)
					- priv_data->assembly_buffer_size;
			if (length > 0)
				memcpy(priv_data->assembly_buffer +
				       priv_data->assembly_buffer_size,
				       rx_buf, length);
			length_left += priv_data->assembly_buffer_size;
			priv_data->assembly_buffer_size = 0;
		} else {
			data = rx_buf + length_processed;
			resp = (struct spi_miso *)data;
			if (resp->cmd == 0x00 || resp->cmd == 0xFF ||
			    resp->cmd < 0x81 || resp->cmd > 0x9F ||
				(resp->cmd > 0x8C && resp->cmd < 0x95)) {
				/* special case. ignore cmd==0x00, 0xFF  */
				length_processed += 1;
				continue;
			}
			length = resp->len + sizeof(struct spi_miso);
		}
		dev_dbg(&priv_data->spidev->dev, "processing. p %d -> l %d (t %d)\n",
			length_processed, length_left, priv_data->xfer_length);
		length_processed += length;
		if (length_left >= sizeof(*resp) &&
		    resp->len + sizeof(*resp) <= length_left) {
			struct spi_miso *resp =
					(struct spi_miso *)data;

			char *temp_data = (char *)data;

			if (resp->cmd == CMD_CAN_RECEIVE_FRAME && static_pos_checksum_en)
				/* Two CAN frames can be received in single spi packet*/
				/* 63rd byte is checksum */
				checksum_rx_len = XFER_BUFFER_SIZE - 2;
			else if (dynamic_pos_checksum_en &&
				 resp->len > CAN_STANDARD_PACKET_SIZE)
				/* This is a fix for dynamic checksum change */
				/* specific to CAN-FD frames with data length */
				/* 12/16/20/24 bytes as v4.2.0 CAN FW is packing */
				/* checksum in 50th byte (4 + 14 + 32) for the */
				/* CAN frames with 12/16/20/24 data length */
				checksum_rx_len = CAN_FD_PACKET_SIZE + 4;
			else
				checksum_rx_len = (resp->len) + 4;

			if (static_pos_checksum_en) {
				rx_checksum_rcvd = temp_data[XFER_BUFFER_SIZE - 2];
				temp_data[XFER_BUFFER_SIZE - 2] = 0;
			} else if (dynamic_pos_checksum_en) {
				rx_checksum_rcvd = temp_data[checksum_rx_len];
				temp_data[checksum_rx_len] = 0;
			}
			rx_checksum = 0;
			if (checksum_enable) {
				for (i = 0; i < checksum_rx_len; i++)
					rx_checksum ^= temp_data[i];

				if (rx_checksum == rx_checksum_rcvd) {
					ret = qti_can_process_response(priv_data, resp,
								       length_left);
				} else {
					dev_err(&priv_data->spidev->dev,
						"checksum validation failed\n");
					dev_err(&priv_data->spidev->dev,
						"cmd_id: %x cs_rx_calc: %x cs_rx: %x\n cs_len: %d\n",
						resp->cmd, rx_checksum,
						rx_checksum_rcvd, checksum_rx_len);
					ret = -EINVAL;
				}
			} else {
				ret = qti_can_process_response(priv_data, resp,
							       length_left);
			}

		} else if (length_left > 0) {
			/* Not full message. Store however much we have for */
			/* later assembly */
			dev_dbg(&priv_data->spidev->dev, "callback: Storing %d bytes of response\n",
				length_left);
			memcpy(priv_data->assembly_buffer, data, length_left);
			       priv_data->assembly_buffer_size = length_left;
			break;
		}
	}
	return ret;
}

static int qti_can_do_spi_transaction(struct qti_can *priv_data)
{
	struct spi_device *spi;
	struct spi_transfer *xfer;
	struct spi_message *msg;
	struct device *dev;
	int ret;
	int i = 0;
	u8 tx_checksum = 0;
	int checksum_tx_len = 0;
	struct spi_mosi *req;
	u64 rx_buf_idx, idx = 0;

	spi = priv_data->spidev;
	dev = &spi->dev;
	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	xfer = kzalloc(sizeof(*xfer), GFP_KERNEL);
	if (!xfer || !msg)
		return -ENOMEM;
	dev_dbg(&priv_data->spidev->dev, ">%x %2d [%d]\n", priv_data->tx_buf[0],
		priv_data->tx_buf[1], priv_data->tx_buf[2]);

	if (static_pos_checksum_en || dynamic_pos_checksum_en) {
		req = (struct spi_mosi *)(priv_data->tx_buf);
		if (req->cmd == CMD_CAN_SEND_FRAME && static_pos_checksum_en)
			checksum_tx_len = XFER_BUFFER_SIZE - 2;
		else if (req->cmd == CMD_CAN_SEND_FRAME &&
			 req->len > CAN_FD_PACKET_SIZE && req->seq == 0)
			checksum_tx_len = CAN_FD_PACKET_SIZE + 4;
		else if (req->cmd == CMD_CAN_SEND_FRAME &&
			 req->len > CAN_FD_PACKET_SIZE && req->seq == 1)
			checksum_tx_len = (req->len) - CAN_FD_PACKET_DATA + 4;
		else
			checksum_tx_len = (req->len) + 4;
		for (i = 0; i < checksum_tx_len; i++)
			tx_checksum ^= priv_data->tx_buf[i];

		if (static_pos_checksum_en) {
			priv_data->tx_buf[(XFER_BUFFER_SIZE - 2)] = tx_checksum;
		} else if (dynamic_pos_checksum_en) {
			if (req->cmd == CMD_CAN_SEND_FRAME &&
			    req->len > CAN_FD_PACKET_SIZE && req->seq == 0)
				priv_data->tx_buf[CAN_FD_PACKET_SIZE + 4] = tx_checksum;
			else if (req->cmd == CMD_CAN_SEND_FRAME &&
				 req->len > CAN_FD_PACKET_SIZE && req->seq == 1)
				priv_data->tx_buf[req->len - CAN_FD_PACKET_DATA + 4] = tx_checksum;
			else
				priv_data->tx_buf[req->len + 4] = tx_checksum;
		}
	}

	spi_message_init(msg);
	spi_message_add_tail(xfer, msg);
	xfer->tx_buf = priv_data->tx_buf;
	xfer->rx_buf = priv_data->rx_buf;
	xfer->len = priv_data->xfer_length;
	xfer->bits_per_word = priv_data->bits_per_word;
	ret = spi_sync(spi, msg);
	dev_dbg(&priv_data->spidev->dev, "spi_sync ret %d\n", ret);
	for (rx_buf_idx = 0; rx_buf_idx < 6; rx_buf_idx++) {
		idx = 10 * rx_buf_idx;
		dev_dbg(&priv_data->spidev->dev, "%X %X %X %X %X %X %X %X %X %X\n",
			priv_data->rx_buf[idx + 0], priv_data->rx_buf[idx + 1],
			priv_data->rx_buf[idx + 2], priv_data->rx_buf[idx + 3],
			priv_data->rx_buf[idx + 4], priv_data->rx_buf[idx + 5],
			priv_data->rx_buf[idx + 6], priv_data->rx_buf[idx + 7],
			priv_data->rx_buf[idx + 8], priv_data->rx_buf[idx + 9]);
	}
	dev_dbg(&priv_data->spidev->dev, "%X %X %X %X\n",
		priv_data->rx_buf[60], priv_data->rx_buf[61],
		priv_data->rx_buf[62], priv_data->rx_buf[63]);

	if (ret == 0)
		qti_can_process_rx(priv_data, priv_data->rx_buf);

	kfree(msg);
	kfree(xfer);
	return ret;
}

static int qti_can_rx_message(struct qti_can *priv_data)
{
	char *tx_buf, *rx_buf;
	int ret;

	mutex_lock(&priv_data->spi_lock);
	tx_buf = priv_data->tx_buf;
	rx_buf = priv_data->rx_buf;
	memset(tx_buf, 0, XFER_BUFFER_SIZE);
	memset(rx_buf, 0, XFER_BUFFER_SIZE);
	priv_data->xfer_length = XFER_BUFFER_SIZE;

	ret = qti_can_do_spi_transaction(priv_data);
	mutex_unlock(&priv_data->spi_lock);

	return ret;
}

static int qti_can_query_firmware_version(struct qti_can *priv_data)
{
	char *tx_buf, *rx_buf;
	int ret;
	struct spi_mosi *req;
	unsigned long jiffies = msecs_to_jiffies(QUERY_FIRMWARE_TIMEOUT_MS);

	mutex_lock(&priv_data->spi_lock);
	tx_buf = priv_data->tx_buf;
	rx_buf = priv_data->rx_buf;
	memset(tx_buf, 0, XFER_BUFFER_SIZE);
	memset(rx_buf, 0, XFER_BUFFER_SIZE);
	priv_data->xfer_length = XFER_BUFFER_SIZE;

	req = (struct spi_mosi *)tx_buf;
	req->cmd = CMD_GET_FW_VERSION;
	req->len = 0;
	req->seq = atomic_inc_return(&priv_data->msg_seq);
	req->data[0] = 0xAA; // checksum enable flag
	req->data[1] = 0xAA; // can-fd enable flag
	req->data[2] = 0xAA; // dynamic position checksum enable flag

	priv_data->wait_cmd = CMD_GET_FW_VERSION;
	priv_data->cmd_result = -1;
	reinit_completion(&priv_data->response_completion);

	ret = qti_can_do_spi_transaction(priv_data);
	mutex_unlock(&priv_data->spi_lock);

	if (ret == 0) {
		dev_dbg(&priv_data->spidev->dev,
			"waiting for completion with timeout of %lu jiffies\n",
			jiffies);
		wait_for_completion_interruptible_timeout(&priv_data->response_completion,
							  jiffies);
		dev_dbg(&priv_data->spidev->dev, "done waiting");
		ret = priv_data->cmd_result;
	}

	return ret;
}

static int qti_can_set_bitrate(struct net_device *netdev)
{
	char *tx_buf, *rx_buf;
	int ret;
	struct spi_mosi *req;
	struct can_config_bit_timing *req_d;
	struct qti_can *priv_data;
	struct can_priv *priv = netdev_priv(netdev);
	struct qti_can_netdev_privdata *qti_can_priv;

	qti_can_priv = netdev_priv(netdev);
	priv_data = qti_can_priv->qti_can;

	netdev_info(netdev, "ch%i,  bitrate setting>%i",
		    qti_can_priv->netdev_index, priv->bittiming.bitrate);
	netdev_dbg(netdev, "sjw>%i brp>%i ph_sg1>%i ph_sg2>%i smpl_pt>%i tq>%i pr_seg>%i",
		   priv->bittiming.sjw, priv->bittiming.brp,
		   priv->bittiming.phase_seg1,
		   priv->bittiming.phase_seg2,
		   priv->bittiming.sample_point,
		   priv->bittiming.tq, priv->bittiming.prop_seg);

	mutex_lock(&priv_data->spi_lock);
	tx_buf = priv_data->tx_buf;
	rx_buf = priv_data->rx_buf;
	memset(tx_buf, 0, XFER_BUFFER_SIZE);
	memset(rx_buf, 0, XFER_BUFFER_SIZE);
	priv_data->xfer_length = XFER_BUFFER_SIZE;

	req = (struct spi_mosi *)tx_buf;
	req->cmd = CMD_CAN_CONFIG_BIT_TIMING;
	req->len = sizeof(struct can_config_bit_timing);
	req->seq = atomic_inc_return(&priv_data->msg_seq);
	req_d = (struct can_config_bit_timing *)req->data;
	req_d->can_if = qti_can_priv->netdev_index;
	req_d->prop_seg = priv->bittiming.prop_seg;
	req_d->phase_seg1 = priv->bittiming.phase_seg1;
	req_d->phase_seg2 = priv->bittiming.phase_seg2;
	req_d->sjw = priv->bittiming.sjw;
	req_d->brp = priv->bittiming.brp;
	ret = qti_can_do_spi_transaction(priv_data);
	mutex_unlock(&priv_data->spi_lock);

	return ret;
}

static int qti_can_write(struct qti_can *priv_data,
			 int can_channel, struct canfd_frame *cf)
{
	char *tx_buf, *rx_buf;
	char fd_buf[CAN_FD_PACKET_DATA] = "";
	bool send_fd_frame = false;
	int data_length_left = 0, k = 0;
	int ret, i;
	struct spi_mosi *req;
	struct can_write_req *req_d;
	struct net_device *netdev;

	if (can_channel < 0 || can_channel >= priv_data->max_can_channels) {
		dev_err(&priv_data->spidev->dev, "%s error. Channel is %d\n", __func__,
			can_channel);
		return -EINVAL;
	}

	mutex_lock(&priv_data->spi_lock);
	tx_buf = priv_data->tx_buf;
	rx_buf = priv_data->rx_buf;
	memset(tx_buf, 0, XFER_BUFFER_SIZE);
	memset(rx_buf, 0, XFER_BUFFER_SIZE);
	priv_data->xfer_length = XFER_BUFFER_SIZE;

	req = (struct spi_mosi *)tx_buf;
	if (priv_data->driver_mode == DRIVER_MODE_RAW_FRAMES) {
		req->cmd = CMD_CAN_SEND_FRAME;
		req->len = CAN_FD_HEADER + cf->len;
		req->seq = atomic_inc_return(&priv_data->msg_seq);
		req_d = (struct can_write_req *)req->data;
		req_d->can_if = can_channel;
		req_d->mid = cf->can_id;
		req_d->dlc = cf->len;
		if (req_d->dlc > CAN_FD_PACKET_DATA) {
			send_fd_frame = true;
			for (i = 0; i < CAN_FD_PACKET_DATA; i++)
				req_d->data[i] = cf->data[i];
			while (i < cf->len) {
				fd_buf[k] = cf->data[i];
				i++;
				k++;
			}
			req->seq = 0;
		} else {
			for (i = 0; i < cf->len; i++)
				req_d->data[i] = cf->data[i];
		}
	} else if (priv_data->driver_mode == DRIVER_MODE_PROPERTIES ||
		priv_data->driver_mode == DRIVER_MODE_AMB) {
		req->cmd = CMD_PROPERTY_WRITE;
		req->len = sizeof(struct vehicle_property);
		req->seq = atomic_inc_return(&priv_data->msg_seq);
		for (i = 0; i < cf->len; i++)
			req->data[i] = cf->data[i];
	} else {
		dev_err(&priv_data->spidev->dev, "%s: wrong driver mode %i\n",
			__func__, priv_data->driver_mode);
	}

	ret = qti_can_do_spi_transaction(priv_data);
	if (send_fd_frame) {
		req->seq = 1;
		data_length_left = cf->len - CAN_FD_PACKET_DATA;
		memset(req_d->data, 0, CAN_FD_PACKET_DATA);
		for (i = 0; i < data_length_left; i++)
			req_d->data[i] = fd_buf[i];
		ret = qti_can_do_spi_transaction(priv_data);
	}
	netdev = priv_data->netdev[can_channel];
	netdev->stats.tx_packets++;
	mutex_unlock(&priv_data->spi_lock);

	return ret;
}

static int qti_can_netdev_open(struct net_device *netdev)
{
	int err;

	netdev_dbg(netdev, "Open");
	err = open_candev(netdev);
	if (err)
		return err;

	netif_start_queue(netdev);

	return 0;
}

static int qti_can_netdev_close(struct net_device *netdev)
{
	netdev_dbg(netdev, "Close");

	netif_stop_queue(netdev);
	close_candev(netdev);
	return 0;
}

static void qti_can_send_can_frame(struct work_struct *ws)
{
	struct qti_can_tx_work *tx_work;
	struct canfd_frame *cf;
	struct qti_can *priv_data;
	struct net_device *netdev;
	struct qti_can_netdev_privdata *netdev_priv_data;
	int can_channel;

	tx_work = container_of(ws, struct qti_can_tx_work, work);
	netdev = tx_work->netdev;
	netdev_priv_data = netdev_priv(netdev);
	priv_data = netdev_priv_data->qti_can;
	can_channel = netdev_priv_data->netdev_index;

	dev_dbg(&priv_data->spidev->dev, "send_can_frame ws %pK\n", ws);
	dev_dbg(&priv_data->spidev->dev, "send_can_frame tx %pK\n", tx_work);

	cf = (struct canfd_frame *)tx_work->skb->data;
	qti_can_write(priv_data, can_channel, cf);

	kfree_skb(tx_work->skb);
	kfree(tx_work);
}

static netdev_tx_t qti_can_netdev_start_xmit(struct sk_buff *skb,
					     struct net_device *netdev)
{
	struct qti_can_netdev_privdata *netdev_priv_data = netdev_priv(netdev);
	struct qti_can *priv_data = netdev_priv_data->qti_can;
	struct qti_can_tx_work *tx_work;

	netdev_dbg(netdev, "netdev_start_xmit");
	if (can_dropped_invalid_skb(netdev, skb)) {
		netdev_err(netdev, "Dropping invalid can frame\n");
		return NETDEV_TX_OK;
	}
	tx_work = kzalloc(sizeof(*tx_work), GFP_ATOMIC);
	if (!tx_work)
		return NETDEV_TX_OK;
	INIT_WORK(&tx_work->work, qti_can_send_can_frame);
	tx_work->netdev = netdev;
	tx_work->skb = skb;
	queue_work(priv_data->tx_wq, &tx_work->work);

	return NETDEV_TX_OK;
}

static int qti_can_send_release_can_buffer_cmd(struct net_device *netdev)
{
	char *tx_buf, *rx_buf;
	int ret;
	struct spi_mosi *req;
	struct qti_can *priv_data;
	struct qti_can_netdev_privdata *netdev_priv_data;
	int *mode;

	netdev_priv_data = netdev_priv(netdev);
	priv_data = netdev_priv_data->qti_can;
	mutex_lock(&priv_data->spi_lock);
	tx_buf = priv_data->tx_buf;
	rx_buf = priv_data->rx_buf;
	memset(tx_buf, 0, XFER_BUFFER_SIZE);
	memset(rx_buf, 0, XFER_BUFFER_SIZE);
	priv_data->xfer_length = XFER_BUFFER_SIZE;

	req = (struct spi_mosi *)tx_buf;
	req->cmd = CMD_CAN_RELEASE_BUFFER;
	req->len = sizeof(int);
	req->seq = atomic_inc_return(&priv_data->msg_seq);
	mode = (int *)req->data;
	*mode = priv_data->driver_mode;

	ret = qti_can_do_spi_transaction(priv_data);
	mutex_unlock(&priv_data->spi_lock);
	return ret;
}

static int qti_can_data_buffering(struct net_device *netdev,
				  struct ifreq *ifr, int cmd)
{
	char *tx_buf, *rx_buf;
	int ret;
	u32 timeout;
	struct spi_mosi *req;
	struct qti_can_buffer *enable_buffering;
	struct qti_can_buffer *add_request;
	struct qti_can *priv_data;
	struct qti_can_netdev_privdata *netdev_priv_data;
	struct spi_device *spi;

	netdev_priv_data = netdev_priv(netdev);
	priv_data = netdev_priv_data->qti_can;
	spi = priv_data->spidev;
	timeout = priv_data->can_fw_cmd_timeout_ms;

	mutex_lock(&priv_data->spi_lock);
	tx_buf = priv_data->tx_buf;
	rx_buf = priv_data->rx_buf;
	memset(tx_buf, 0, XFER_BUFFER_SIZE);
	memset(rx_buf, 0, XFER_BUFFER_SIZE);
	priv_data->xfer_length = XFER_BUFFER_SIZE;
	if (!ifr) {
		mutex_unlock(&priv_data->spi_lock);
		return -EINVAL;
	}
	add_request = kzalloc(sizeof(*add_request), GFP_KERNEL);
	if (!add_request) {
		mutex_unlock(&priv_data->spi_lock);
		return -ENOMEM;
	}

	if (copy_from_user(add_request, ifr->ifr_data,
			   sizeof(struct qti_can_buffer))) {
		mutex_unlock(&priv_data->spi_lock);
		kfree(add_request);
		return -EFAULT;
	}

	req = (struct spi_mosi *)tx_buf;
	if (cmd == IOCTL_ENABLE_BUFFERING)
		req->cmd = CMD_CAN_DATA_BUFF_ADD;
	else
		req->cmd = CMD_CAN_DATA_BUFF_REMOVE;
	req->len = sizeof(struct qti_can_buffer);
	req->seq = atomic_inc_return(&priv_data->msg_seq);

	enable_buffering = (struct qti_can_buffer *)req->data;
	enable_buffering->can_if = add_request->can_if;
	enable_buffering->mid = add_request->mid;
	enable_buffering->mask = add_request->mask;

	if (priv_data->can_fw_cmd_timeout_req) {
		priv_data->wait_cmd = req->cmd;
		priv_data->cmd_result = -1;
		reinit_completion(&priv_data->response_completion);
	}

	ret = qti_can_do_spi_transaction(priv_data);
	kfree(add_request);
	mutex_unlock(&priv_data->spi_lock);

	if (ret == 0 && priv_data->can_fw_cmd_timeout_req) {
		dev_dbg(&priv_data->spidev->dev, "%s ready to wait for response\n", __func__);
		ret = wait_for_completion_interruptible_timeout(&priv_data->response_completion,
								msecs_to_jiffies(timeout));
		ret = priv_data->cmd_result;
	}
	return ret;
}

static int qti_can_remove_all_buffering(struct net_device *netdev)
{
	char *tx_buf, *rx_buf;
	int ret;
	u32 timeout;
	struct spi_mosi *req;
	struct qti_can *priv_data;
	struct qti_can_netdev_privdata *netdev_priv_data;

	netdev_priv_data = netdev_priv(netdev);
	priv_data = netdev_priv_data->qti_can;
	timeout = priv_data->rem_all_buffering_timeout_ms;

	mutex_lock(&priv_data->spi_lock);
	tx_buf = priv_data->tx_buf;
	rx_buf = priv_data->rx_buf;
	memset(tx_buf, 0, XFER_BUFFER_SIZE);
	memset(rx_buf, 0, XFER_BUFFER_SIZE);
	priv_data->xfer_length = XFER_BUFFER_SIZE;

	req = (struct spi_mosi *)tx_buf;
	req->cmd = CMD_CAN_DATA_BUFF_REMOVE_ALL;
	req->len = 0;
	req->seq = atomic_inc_return(&priv_data->msg_seq);

	if (priv_data->can_fw_cmd_timeout_req) {
		priv_data->wait_cmd = req->cmd;
		priv_data->cmd_result = -1;
		reinit_completion(&priv_data->response_completion);
	}

	ret = qti_can_do_spi_transaction(priv_data);
	mutex_unlock(&priv_data->spi_lock);

	if (ret == 0 && priv_data->can_fw_cmd_timeout_req) {
		dev_dbg(&priv_data->spidev->dev, "%s wait for response\n", __func__);
		ret = wait_for_completion_interruptible_timeout(&priv_data->response_completion,
								msecs_to_jiffies(timeout));
		ret = priv_data->cmd_result;
	}

	return ret;
}

static int qti_can_frame_filter(struct net_device *netdev,
				struct ifreq *ifr, int cmd)
{
	char *tx_buf, *rx_buf;
	int ret;
	struct spi_mosi *req;
	struct can_filter_req *add_filter;
	struct can_filter_req *filter_request;
	struct qti_can *priv_data;
	struct qti_can_netdev_privdata *netdev_priv_data;
	struct spi_device *spi;

	netdev_priv_data = netdev_priv(netdev);
	priv_data = netdev_priv_data->qti_can;
	spi = priv_data->spidev;

	mutex_lock(&priv_data->spi_lock);
	tx_buf = priv_data->tx_buf;
	rx_buf = priv_data->rx_buf;
	memset(tx_buf, 0, XFER_BUFFER_SIZE);
	memset(rx_buf, 0, XFER_BUFFER_SIZE);
	priv_data->xfer_length = XFER_BUFFER_SIZE;

	if (!ifr) {
		mutex_unlock(&priv_data->spi_lock);
		return -EINVAL;
	}

	filter_request = kzalloc(sizeof(*filter_request),
				 GFP_KERNEL);
	if (!filter_request) {
		mutex_unlock(&priv_data->spi_lock);
		return -ENOMEM;
	}

	if (copy_from_user(filter_request, ifr->ifr_data,
			   sizeof(struct can_filter_req))) {
		mutex_unlock(&priv_data->spi_lock);
		kfree(filter_request);
		return -EFAULT;
	}

	req = (struct spi_mosi *)tx_buf;
	if (cmd == IOCTL_ADD_FRAME_FILTER)
		req->cmd = CMD_CAN_ADD_FILTER;
	else
		req->cmd = CMD_CAN_REMOVE_FILTER;

	req->len = sizeof(struct can_filter_req);
	req->seq = atomic_inc_return(&priv_data->msg_seq);

	add_filter = (struct can_filter_req *)req->data;
	add_filter->can_if = filter_request->can_if;
	add_filter->mid = filter_request->mid;
	add_filter->mask = filter_request->mask;

	ret = qti_can_do_spi_transaction(priv_data);
	kfree(filter_request);
	mutex_unlock(&priv_data->spi_lock);
	return ret;
}

static int qti_can_send_spi_locked(struct qti_can *priv_data, int cmd, int len,
				   u8 *data)
{
	char *tx_buf, *rx_buf;
	struct spi_mosi *req;
	int ret;

	tx_buf = priv_data->tx_buf;
	rx_buf = priv_data->rx_buf;
	memset(tx_buf, 0, XFER_BUFFER_SIZE);
	memset(rx_buf, 0, XFER_BUFFER_SIZE);
	priv_data->xfer_length = XFER_BUFFER_SIZE;

	req = (struct spi_mosi *)tx_buf;
	req->cmd = cmd;
	req->len = len;
	req->seq = atomic_inc_return(&priv_data->msg_seq);

	if (unlikely(len > 64))
		return -EINVAL;
	memcpy(req->data, data, len);

	ret = qti_can_do_spi_transaction(priv_data);
	return ret;
}

static int qti_can_convert_ioctl_cmd_to_spi_cmd(int ioctl_cmd)
{
	switch (ioctl_cmd) {
	case IOCTL_GET_FW_BR_VERSION:
		return CMD_GET_FW_BR_VERSION;
	case IOCTL_BEGIN_FIRMWARE_UPGRADE:
		return CMD_BEGIN_FIRMWARE_UPGRADE;
	case IOCTL_FIRMWARE_UPGRADE_DATA:
		return CMD_FIRMWARE_UPGRADE_DATA;
	case IOCTL_END_FIRMWARE_UPGRADE:
		return CMD_END_FIRMWARE_UPGRADE;
	case IOCTL_BEGIN_BOOT_ROM_UPGRADE:
		return CMD_BEGIN_BOOT_ROM_UPGRADE;
	case IOCTL_BOOT_ROM_UPGRADE_DATA:
		return CMD_BOOT_ROM_UPGRADE_DATA;
	case IOCTL_END_BOOT_ROM_UPGRADE:
		return CMD_END_BOOT_ROM_UPGRADE;
	case IOCTL_END_FW_UPDATE_FILE:
		return CMD_END_FW_UPDATE_FILE;
	}
	return -EINVAL;
}

static int qti_can_end_fwupgrade_ioctl(struct net_device *netdev,
				       struct ifreq *ifr, int cmd)
{
	int spi_cmd, ret;

	struct qti_can *priv_data;
	struct qti_can_netdev_privdata *netdev_priv_data;
	struct spi_device *spi;
	int len = 0;
	u8 *data = NULL;

	netdev_priv_data = netdev_priv(netdev);
	priv_data = netdev_priv_data->qti_can;
	spi = priv_data->spidev;
	spi_cmd = qti_can_convert_ioctl_cmd_to_spi_cmd(cmd);
	dev_dbg(&priv_data->spidev->dev, "%s spi_cmd %x\n", __func__, spi_cmd);
	if (spi_cmd < 0) {
		dev_err(&priv_data->spidev->dev, "%s wrong command %d\n", __func__, cmd);
		return spi_cmd;
	}

	if (!ifr)
		return -EINVAL;

	mutex_lock(&priv_data->spi_lock);
	dev_dbg(&priv_data->spidev->dev, "%s len %d\n", __func__, len);

	ret = qti_can_send_spi_locked(priv_data, spi_cmd, len, data);

	mutex_unlock(&priv_data->spi_lock);

	return ret;
}

static int qti_can_do_blocking_ioctl(struct net_device *netdev,
				     struct ifreq *ifr, int cmd)
{
	int spi_cmd, ret;

	struct qti_can *priv_data;
	struct qti_can_netdev_privdata *netdev_priv_data;
	struct qti_can_ioctl_req *ioctl_data = NULL;
	struct spi_device *spi;
	int len = 0;
	u8 *data = NULL;

	netdev_priv_data = netdev_priv(netdev);
	priv_data = netdev_priv_data->qti_can;
	spi = priv_data->spidev;

	spi_cmd = qti_can_convert_ioctl_cmd_to_spi_cmd(cmd);
	dev_dbg(&priv_data->spidev->dev, "%s spi_cmd %x\n", __func__, spi_cmd);
	if (spi_cmd < 0) {
		dev_err(&priv_data->spidev->dev, "%s wrong command %d\n", __func__, cmd);
		return spi_cmd;
	}

	if (!ifr)
		return -EINVAL;

	mutex_lock(&priv_data->spi_lock);
	if (spi_cmd == CMD_FIRMWARE_UPGRADE_DATA ||
	    spi_cmd == CMD_BOOT_ROM_UPGRADE_DATA) {
		ioctl_data = kzalloc(sizeof(*ioctl_data),
				     GFP_KERNEL);
		if (!ioctl_data) {
			mutex_unlock(&priv_data->spi_lock);
			return -ENOMEM;
		}

		if (copy_from_user(ioctl_data, ifr->ifr_data,
				   sizeof(struct qti_can_ioctl_req))) {
			mutex_unlock(&priv_data->spi_lock);
			kfree(ioctl_data);
			return -EFAULT;
		}

		if (ioctl_data->len < 0) {
			mutex_unlock(&priv_data->spi_lock);
			dev_err(&priv_data->spidev->dev, "ioctl_data->len is: %d\n",
				ioctl_data->len);
			return -EINVAL;
		}

		/* Regular NULL check will fail here as ioctl_data is at
		 * some offset
		 */
		if ((void *)ioctl_data > (void *)0x100) {
			len = ioctl_data->len;
			data = ioctl_data->data;
		}
	}
	dev_dbg(&priv_data->spidev->dev, "%s len %d\n", __func__, len);

	if (len > 64 || len < 0) {
		mutex_unlock(&priv_data->spi_lock);
		dev_err(&priv_data->spidev->dev, "len value[%d] is not correct!!\n", len);
		return -EINVAL;
	}

	priv_data->wait_cmd = spi_cmd;
	priv_data->cmd_result = -1;
	reinit_completion(&priv_data->response_completion);

	ret = qti_can_send_spi_locked(priv_data, spi_cmd, len, data);

	kfree(ioctl_data);
	mutex_unlock(&priv_data->spi_lock);

	if (ret == 0) {
		dev_dbg(&priv_data->spidev->dev, "%s ready to wait for response\n", __func__);
		wait_for_completion_interruptible_timeout(&priv_data->response_completion,
							  5 * HZ);
		ret = priv_data->cmd_result;
	}
	return ret;
}

static int qti_can_netdev_do_ioctl(struct net_device *netdev,
				   struct ifreq *ifr,
				   void __user *data,
				   int cmd)
{
	struct qti_can *priv_data;
	struct qti_can_netdev_privdata *netdev_priv_data;
	int *mode;
	int ret = -EINVAL;
	struct spi_device *spi;

	netdev_priv_data = netdev_priv(netdev);
	priv_data = netdev_priv_data->qti_can;
	spi = priv_data->spidev;
	dev_dbg(&priv_data->spidev->dev, "%s %x\n", __func__, cmd);

	switch (cmd) {
	case IOCTL_RELEASE_CAN_BUFFER:
		if (!ifr)
			return -EINVAL;

		/* Regular NULL check will fail here as ioctl_data is at
		 * some offset
		 */
		if (ifr->ifr_data > (void __user *)IFR_DATA_OFFSET) {
			mutex_lock(&priv_data->spi_lock);
			mode = kzalloc(sizeof(*mode), GFP_KERNEL);
			if (!mode) {
				mutex_unlock(&priv_data->spi_lock);
				return -ENOMEM;
			}
			if (copy_from_user(mode, ifr->ifr_data, sizeof(int))) {
				mutex_unlock(&priv_data->spi_lock);
				kfree(mode);
				return -EFAULT;
			}
			priv_data->driver_mode = *mode;
			dev_err(&priv_data->spidev->dev, "qti_can_driver_mode %d\n",
				priv_data->driver_mode);
			kfree(mode);
			mutex_unlock(&priv_data->spi_lock);
		}
		qti_can_send_release_can_buffer_cmd(netdev);
		ret = 0;
		break;
	case IOCTL_ENABLE_BUFFERING:
	case IOCTL_DISABLE_BUFFERING:
		qti_can_data_buffering(netdev, ifr, cmd);
		ret = 0;
		break;
	case IOCTL_DISABLE_ALL_BUFFERING:
		qti_can_remove_all_buffering(netdev);
		ret = 0;
		break;
	case IOCTL_ADD_FRAME_FILTER:
	case IOCTL_REMOVE_FRAME_FILTER:
		qti_can_frame_filter(netdev, ifr, cmd);
		ret = 0;
		break;
	case IOCTL_END_FIRMWARE_UPGRADE:
		ret = qti_can_end_fwupgrade_ioctl(netdev, ifr, cmd);
		break;
	case IOCTL_GET_FW_BR_VERSION:
	case IOCTL_BEGIN_FIRMWARE_UPGRADE:
	case IOCTL_FIRMWARE_UPGRADE_DATA:
	case IOCTL_BEGIN_BOOT_ROM_UPGRADE:
	case IOCTL_BOOT_ROM_UPGRADE_DATA:
	case IOCTL_END_BOOT_ROM_UPGRADE:
	case IOCTL_END_FW_UPDATE_FILE:
		ret = qti_can_do_blocking_ioctl(netdev, ifr, cmd);
		break;
	}
	dev_dbg(&priv_data->spidev->dev, "%s ret %d\n", __func__, ret);

	return ret;
}

static const struct net_device_ops qti_can_netdev_ops = {
	.ndo_open = qti_can_netdev_open,
	.ndo_stop = qti_can_netdev_close,
	.ndo_start_xmit = qti_can_netdev_start_xmit,
	.ndo_siocdevprivate = qti_can_netdev_do_ioctl,
};

static int qti_can_create_netdev(struct spi_device *spi,
				 struct qti_can *priv_data, int index)
{
	struct net_device *netdev;
	struct qti_can_netdev_privdata *netdev_priv_data;

	dev_dbg(&priv_data->spidev->dev, "%s %d\n", __func__, index);
	if (index < 0 || index >= priv_data->max_can_channels) {
		dev_err(&priv_data->spidev->dev, "%s wrong index %d\n", __func__, index);
		return -EINVAL;
	}
	netdev = alloc_candev(sizeof(*netdev_priv_data), MAX_TX_BUFFERS);
	if (!netdev) {
		dev_err(&priv_data->spidev->dev, "Couldn't alloc candev\n");
		return -ENOMEM;
	}

	netdev->mtu = CANFD_MTU;

	netdev_priv_data = netdev_priv(netdev);
	netdev_priv_data->qti_can = priv_data;
	netdev_priv_data->netdev_index = index;

	priv_data->netdev[index] = netdev;

	netdev->netdev_ops = &qti_can_netdev_ops;
	SET_NETDEV_DEV(netdev, &spi->dev);
	netdev_priv_data->can.ctrlmode_supported = CAN_CTRLMODE_3_SAMPLES |
						   CAN_CTRLMODE_LISTENONLY;
	if (priv_data->support_can_fd)
		netdev_priv_data->can.ctrlmode_supported |= CAN_CTRLMODE_FD;
	netdev_priv_data->can.bittiming_const = &qti_can_bittiming_const;
	netdev_priv_data->can.data_bittiming_const =
						&qti_can_data_bittiming_const;
	netdev_priv_data->can.clock.freq = priv_data->clk_freq_mhz;
	netdev_priv_data->can.do_set_bittiming = qti_can_set_bitrate;

	return 0;
}

static struct qti_can *qti_can_create_priv_data(struct spi_device *spi)
{
	struct qti_can *priv_data;
	int err;
	struct device *dev;

	dev = &spi->dev;
	priv_data = devm_kzalloc(dev, sizeof(*priv_data), GFP_KERNEL);
	if (!priv_data) {
		err = -ENOMEM;
		return NULL;
	}
	spi_set_drvdata(spi, priv_data);
	atomic_set(&priv_data->netif_queue_stop, 0);
	priv_data->spidev = spi;
	priv_data->assembly_buffer = devm_kzalloc(dev,
						  RX_ASSEMBLY_BUFFER_SIZE,
						  GFP_KERNEL);
	if (!priv_data->assembly_buffer) {
		err = -ENOMEM;
		goto cleanup_privdata;
	}
	priv_data->fd_buffer = devm_kzalloc(dev, RX_FD_BUFFER_SIZE,
					    GFP_KERNEL);
	if (!priv_data->fd_buffer) {
		err = -ENOMEM;
		goto cleanup_privdata;
	}

	priv_data->tx_wq = alloc_workqueue("qti_can_tx_wq", 0, 0);
	if (!priv_data->tx_wq) {
		dev_err(&priv_data->spidev->dev, "Couldn't alloc workqueue\n");
		err = -ENOMEM;
		goto cleanup_privdata;
	}

	priv_data->tx_buf = devm_kzalloc(dev,
					 XFER_BUFFER_SIZE,
					 GFP_KERNEL);
	priv_data->rx_buf = devm_kzalloc(dev,
					 XFER_BUFFER_SIZE,
					 GFP_KERNEL);
	if (!priv_data->tx_buf || !priv_data->rx_buf) {
		dev_err(&priv_data->spidev->dev, "Couldn't alloc tx or rx buffers\n");
		err = -ENOMEM;
		goto cleanup_privdata;
	}
	priv_data->xfer_length = 0;
	priv_data->driver_mode = DRIVER_MODE_RAW_FRAMES;

	mutex_init(&priv_data->spi_lock);
	atomic_set(&priv_data->msg_seq, 0);
	init_completion(&priv_data->response_completion);
	return priv_data;

cleanup_privdata:
	if (priv_data) {
		if (priv_data->tx_wq)
			destroy_workqueue(priv_data->tx_wq);
	}
	return NULL;
}

static const struct of_device_id qti_can_match_table[] = {
	{ .compatible = "qcom,renesas,rh850" },
	{ .compatible = "qcom,nxp,mpc5746c" },
	{ }
};

static int qti_can_probe(struct spi_device *spi)
{
	int err, retry = 0, query_err = -1, i;
	struct qti_can *priv_data = NULL;
	struct device *dev;

	dev = &spi->dev;

	err = spi_setup(spi);
	if (err) {
		dev_err(dev, "spi_setup failed: %d\n", err);
		return err;
	}

	priv_data = qti_can_create_priv_data(spi);
	if (!priv_data) {
		dev_err(dev, "Failed to create qti_can priv_data\n");
		err = -ENOMEM;
		return err;
	}

	err = of_property_read_u32(spi->dev.of_node, "qcom,clk-freq-mhz",
				   &priv_data->clk_freq_mhz);
	if (err) {
		dev_err(&priv_data->spidev->dev, "DT property: qcom,clk-freq-hz not defined\n");
		return err;
	}

	if (priv_data->clk_freq_mhz > CALYPSO_MAX_CAN_CLK_FREQ) {
		dev_err(&priv_data->spidev->dev, "DT property: qcom,clk-freq-hz Invalid\n");
		return err;
	}

	err = of_property_read_u32(spi->dev.of_node, "qcom,max-can-channels",
				   &priv_data->max_can_channels);
	if (err) {
		dev_err(&priv_data->spidev->dev,
			"DT property: qcom,max-can-channels not defined\n");
		return err;
	}

	err = of_property_read_u32(spi->dev.of_node, "qcom,bits-per-word",
				   &priv_data->bits_per_word);
	if (err)
		priv_data->bits_per_word = 16;

	err = of_property_read_u32(spi->dev.of_node, "qcom,reset-delay-msec",
				   &priv_data->reset_delay_msec);
	if (err)
		priv_data->reset_delay_msec = 1;

	priv_data->can_fw_cmd_timeout_req =
			of_property_read_bool(spi->dev.of_node,
					      "qcom,can-fw-cmd-timeout-req");

	err = of_property_read_u32(spi->dev.of_node,
				   "qcom,can-fw-cmd-timeout-ms",
					&priv_data->can_fw_cmd_timeout_ms);
	if (err)
		priv_data->can_fw_cmd_timeout_ms = 0;

	err = of_property_read_u32(spi->dev.of_node,
				   "qcom,rem-all-buffering-timeout-ms",
				   &priv_data->rem_all_buffering_timeout_ms);
	if (err)
		priv_data->rem_all_buffering_timeout_ms = 0;

	priv_data->reset = of_get_named_gpio(spi->dev.of_node,
					     "qcom,reset-gpio", 0);

	if (of_get_property(spi->dev.of_node, "gpio-activelow", NULL))
		priv_data->active_low = true; /* Active_Low */
	else
		priv_data->active_low = false; /* Active_High */

	if (gpio_is_valid(priv_data->reset)) {
		err = gpio_request(priv_data->reset, "qti-can-reset");
		if (err < 0) {
			dev_err(&priv_data->spidev->dev, "failed to request gpio %d: %d\n",
				priv_data->reset, err);
			return err;
		}

		gpio_direction_output(priv_data->reset, !priv_data->active_low);
		/* delay to generate non-zero reset pulse width */
		udelay(1);
		gpio_direction_output(priv_data->reset, priv_data->active_low);
		/* wait for controller to come up after reset */
		msleep(priv_data->reset_delay_msec);
	} else {
		msleep(priv_data->reset_delay_msec);
	}

	priv_data->support_can_fd = of_property_read_bool(spi->dev.of_node,
							  "support-can-fd");

	priv_data->use_qtimer = property_bool(spi->dev.of_node,
					      "qcom,use_qtimer");
	dev_dbg(&priv_data->spidev->dev, "DT property: qcom,use_qtimer:%d\n",
		priv_data->use_qtimer);

	if (of_device_is_compatible(spi->dev.of_node, "qcom,nxp,mpc5746c"))
		qti_can_bittiming_const = flexcan_bittiming_const;
	else if (of_device_is_compatible(spi->dev.of_node,
					 "qcom,renesas,rh850"))
		qti_can_bittiming_const = rh850_bittiming_const;

	priv_data->netdev = devm_kcalloc(dev,
					 priv_data->max_can_channels,
					 sizeof(priv_data->netdev[0]),
					 GFP_KERNEL);
	if (!priv_data->netdev) {
		err = -ENOMEM;
		return err;
	}

	for (i = 0; i < priv_data->max_can_channels; i++) {
		err = qti_can_create_netdev(spi, priv_data, i);
		if (err) {
			dev_err(&priv_data->spidev->dev,
				"Failed to create CAN device: %d\n", err);
			goto cleanup_candev;
		}

		err = register_candev(priv_data->netdev[i]);
		if (err) {
			dev_err(&priv_data->spidev->dev,
				"Failed to register CAN device: %d\n", err);
			goto unregister_candev;
		}
	}

	err = request_threaded_irq(spi->irq, NULL, qti_can_irq,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				   "qti-can", priv_data);
	if (err) {
		dev_err(&priv_data->spidev->dev, "Failed to request irq: %d\n", err);
		goto unregister_candev;
	}
	dev_dbg(dev, "Request irq %d ret %d\n", spi->irq, err);

	if (err)
		dev_info(&priv_data->spidev->dev, "register_pm_notifier_error\n");

	while ((query_err != 0) && (retry < QTI_CAN_FW_QUERY_RETRY_COUNT)) {
		dev_dbg(dev, "Trying to query fw version %d\n", retry);
		query_err = qti_can_query_firmware_version(priv_data);
		priv_data->assembly_buffer_size = 0;
		retry++;
	}
	dev_info(dev, "Retry count for fw version query is %d\n", retry);
	if (query_err) {
		dev_err(&priv_data->spidev->dev, "QTI CAN probe failed\n");
		err = -ENODEV;
		goto free_irq;
	}
	return 0;

free_irq:
	free_irq(spi->irq, priv_data);
unregister_candev:
	for (i = 0; i < priv_data->max_can_channels; i++)
		unregister_candev(priv_data->netdev[i]);
cleanup_candev:
	if (priv_data) {
		for (i = 0; i < priv_data->max_can_channels; i++) {
			if (priv_data->netdev[i])
				free_candev(priv_data->netdev[i]);
		}
		if (priv_data->tx_wq)
			destroy_workqueue(priv_data->tx_wq);
	}
	return err;
}

static int qti_can_remove(struct spi_device *spi)
{
	struct qti_can *priv_data = spi_get_drvdata(spi);
	int i;

	for (i = 0; i < priv_data->max_can_channels; i++) {
		unregister_candev(priv_data->netdev[i]);
		free_candev(priv_data->netdev[i]);
	}
	destroy_workqueue(priv_data->tx_wq);
	return 0;
}

static struct spi_driver qti_can_driver = {
	.driver = {
		.name = "qti-can",
		.of_match_table = qti_can_match_table,
		.owner = THIS_MODULE,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
	.probe = qti_can_probe,
	.remove = qti_can_remove,
};
module_spi_driver(qti_can_driver);

MODULE_DESCRIPTION("QTI CAN controller module");
MODULE_LICENSE("GPL v2");
