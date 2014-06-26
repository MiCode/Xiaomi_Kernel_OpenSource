/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#ifndef __RADIO_SILABS_H
#define __RADIO_SILABS_H

#include <linux/ioctl.h>
#include <linux/videodev2.h>

#define WRITE_REG_NUM           8
#define READ_REG_NUM            16

#define FMDBG(fmt, args...) pr_debug("silabs_radio: " fmt, ##args)

#define FMDERR(fmt, args...) pr_err("silabs_radio: " fmt, ##args)

/* For bounds checking. */
const unsigned char MIN_RDS_STD = 0x00;
const unsigned char MAX_RDS_STD = 0x02;
const unsigned char MIN_SRCH_MODE = 0x00;
const unsigned char MAX_SRCH_MODE = 0x01;

/* Standard buffer size */
#define STD_BUF_SIZE               (256)
/* Search direction */
#define SRCH_DIR_UP                 (1)
#define SRCH_DIR_DOWN               (0)
#define WAIT_TIMEOUT_MSEC 2000
#define SILABS_DELAY_MSEC 10
#define CTS_RETRY_COUNT 10
#define RADIO_NR -1
#define TURNING_ON 1
#define TURNING_OFF 0

/* to distinguish between seek, tune during STC int. */
#define NO_SEEK_TUNE_PENDING 0
#define TUNE_PENDING 1
#define SEEK_PENDING 2
#define SCAN_PENDING 3
#define WRAP_ENABLE 1
#define WRAP_DISABLE 0
#define VALID_MASK 0x01
/* it will check whether UPPER band reached or not */
#define BLTF_MASK 0x80
#define SAMPLE_RATE_48_KHZ 0xBB80
#define MIN_DWELL_TIME 0x00
#define MAX_DWELL_TIME 0x0F
#define START_SCAN 1
/*
 * When tuning, we need to divide freq by TUNE_STEP_SIZE
 * before sending it to chip
 */
#define TUNE_STEP_SIZE 10

#define TUNE_PARAM 16

#define OFFSET_OF_GRP_TYP 11
#define RDS_INT_BIT 0x01
#define FIFO_CNT_16 0x10
#define UNCORRECTABLE_RDS_EN 0xFF01
#define NO_OF_RDS_BLKS 4
#define MSB_OF_BLK_0 4
#define LSB_OF_BLK_0 5
#define MSB_OF_BLK_1 6
#define LSB_OF_BLK_1 7
#define MSB_OF_BLK_2 8
#define LSB_OF_BLK_2 9
#define MSB_OF_BLK_3 10
#define LSB_OF_BLK_3 11
#define MAX_RT_LEN 64
#define END_OF_RT 0x0d
#define MAX_PS_LEN 8
#define OFFSET_OF_PS 5
#define PS_VALIDATE_LIMIT 2
#define RT_VALIDATE_LIMIT 2
#define RDS_CMD_LEN 3
#define RDS_RSP_LEN 13
#define PS_EVT_DATA_LEN (MAX_PS_LEN + OFFSET_OF_PS)
#define NO_OF_PS 1
#define OFFSET_OF_RT 5
#define OFFSET_OF_PTY 5
#define MAX_LEN_2B_GRP_RT 32
#define CNT_FOR_2A_GRP_RT 4
#define CNT_FOR_2B_GRP_RT 2
#define PS_MASK 0x3
#define PTY_MASK 0x1F
#define NO_OF_CHARS_IN_EACH_ADD 2

/* commands */
#define POWER_UP_CMD  0x01
#define GET_REV_CMD 0x10
#define POWER_DOWN_CMD 0x11
#define SET_PROPERTY_CMD 0x12
#define GET_PROPERTY_CMD 0x13
#define GET_INT_STATUS_CMD 0x14
#define PATCH_ARGS_CMD 0x15
#define PATCH_DATA_CMD 0x16
#define FM_TUNE_FREQ_CMD 0x20
#define FM_SEEK_START_CMD 0x21
#define FM_TUNE_STATUS_CMD 0x22
#define FM_RSQ_STATUS_CMD 0x23
#define FM_RDS_STATUS_CMD 0x24
#define FM_AGC_STATUS_CMD 0x27
#define FM_AGC_OVERRIDE_CMD 0x28
#define GPIO_CTL_CMD 0x80
#define GPIO_SET_CMD 0x81

/* properties */
#define GPO_IEN_PROP 0x0001
#define DIGITAL_OUTPUT_FORMAT_PROP 0x0102
#define DIGITAL_OUTPUT_SAMPLE_RATE_PROP 0x0104
#define REFCLK_FREQ_PROP 0x0201
#define REFCLK_PRESCALE_PROP 0x0202
#define FM_DEEMPHASIS_PROP 0x1100
#define FM_CHANNEL_FILTER_PROP 0x1102
#define FM_ANTENNA_INPUT_PROP 0x1107


#define FM_SEEK_BAND_BOTTOM_PROP 0x1400
#define FM_SEEK_BAND_TOP_PROP 0x1401
#define FM_SEEK_FREQ_SPACING_PROP 0x1402
#define FM_SEEK_TUNE_SNR_THRESHOLD_PROP 0x1403
#define FM_SEEK_TUNE_RSSI_THRESHOLD_PROP 0x1404

#define FM_RDS_INT_SOURCE_PROP 0x1500
#define FM_RDS_INT_FIFO_COUNT_PROP 0x1501
#define FM_RDS_CONFIG_PROP 0x1502


/* BIT MASKS */
#define ENABLE_CTS_INT_MASK        (1 << 7)
#define ENABLE_GPO2_INT_MASK       (1 << 6)
#define PATCH_ENABLE_MASK          (1 << 5)
/* to use clock present on daughter card or MSM's */
#define CLOCK_ENABLE_MASK          (1 << 4)
#define FUNC_QUERY_LIB_ID_MASK      15
#define CANCEL_SEEK_MASK           (1 << 1)
#define INTACK_MASK                 1
#define SEEK_WRAP_MASK             (1 << 2)
#define SEEK_UP_MASK               (1 << 3)


/* BIT MASKS to parse response bytes */
#define CTS_INT_BIT_MASK           (1 << 7)
#define ERR_BIT_MASK               (1 << 6)
#define RSQ_INT_BIT_MASK           (1 << 3)
#define RDS_INT_BIT_MASK           (1 << 2)
#define STC_INT_BIT_MASK            1


#define DCLK_FALLING_EDGE_MASK     (1 << 7)

/* Command lengths */
#define SET_PROP_CMD_LEN 6
#define GET_PROP_CMD_LEN 4
#define GET_INT_STATUS_CMD_LEN 1
#define POWER_UP_CMD_LEN 3
#define POWER_DOWN_CMD_LEN 1
#define TUNE_FREQ_CMD_LEN 5
#define SEEK_CMD_LEN 2
#define TUNE_STATUS_CMD_LEN 2

#define HIGH_BYTE_16BIT(x)         (x >> 8)
#define LOW_BYTE_16BIT(x)          (x & 0xFF)


#define AUDIO_OPMODE_ANALOG   0x05
#define AUDIO_OPMODE_DIGITAL  0xB0

/* ERROR codes */
#define BAD_CMD  0x10
#define BAD_ARG1 0x11
#define BAD_ARG2 0x12
#define BAD_ARG3 0x13
#define BAD_ARG4 0x14
#define BAD_ARG5 0x15
#define BAD_ARG6 0x16
#define BAD_ARG7 0x17
#define BAD_PROP 0x20
#define BAD_BOOT_MODE 0x30

/* RDS */
#define FM_RDS_BUF 100
#define FM_RDS_STATUS_IN_INTACK     0x01
#define FM_RDS_STATUS_IN_MTFIFO     0x02
#define FM_RDS_STATUS_OUT_GRPLOST   0x04
#define FM_RDS_STATUS_OUT_BLED      0x03
#define FM_RDS_STATUS_OUT_BLEC      0x0C
#define FM_RDS_STATUS_OUT_BLEB      0x30
#define FM_RDS_STATUS_OUT_BLEA      0xC0
#define FM_RDS_STATUS_OUT_BLED_SHFT 0
#define FM_RDS_STATUS_OUT_BLEC_SHFT 2
#define FM_RDS_STATUS_OUT_BLEB_SHFT 4
#define FM_RDS_STATUS_OUT_BLEA_SHFT 6
#define RDS_TYPE_0A     (0 * 2 + 0)
#define RDS_TYPE_0B     (0 * 2 + 1)
#define RDS_TYPE_2A     (2 * 2 + 0)
#define RDS_TYPE_2B     (2 * 2 + 1)
#define UNCORRECTABLE           3
#define RT_VALIDATE_LIMIT 2

/* FM states */
enum radio_state_t {
	FM_OFF,
	FM_RECV,
	FM_RESET,
	FM_CALIB,
	FM_TURNING_OFF,
	FM_RECV_TURNING_ON,
	FM_MAX_NO_STATES,
};

enum emphasis_type {
	FM_RX_EMP75 = 0x0002,
	FM_RX_EMP50 = 0x0001
};

/* 3 valid values: 5 (50 kHz), 10 (100 kHz), and 20 (200 kHz). */
enum channel_space_type {
	FM_RX_SPACE_200KHZ = 0x0014,
	FM_RX_SPACE_100KHZ = 0x000A,
	FM_RX_SPACE_50KHZ = 0x0005
};

enum v4l2_cid_private_silabs_fm_t {
	V4L2_CID_PRIVATE_SILABS_SRCHMODE = (V4L2_CID_PRIVATE_BASE + 1),
	V4L2_CID_PRIVATE_SILABS_SCANDWELL,
	V4L2_CID_PRIVATE_SILABS_SRCHON,
	V4L2_CID_PRIVATE_SILABS_STATE,
	V4L2_CID_PRIVATE_SILABS_TRANSMIT_MODE,
	V4L2_CID_PRIVATE_SILABS_RDSGROUP_MASK,
	V4L2_CID_PRIVATE_SILABS_REGION,
	V4L2_CID_PRIVATE_SILABS_SIGNAL_TH,
	V4L2_CID_PRIVATE_SILABS_SRCH_PTY,
	V4L2_CID_PRIVATE_SILABS_SRCH_PI,
	V4L2_CID_PRIVATE_SILABS_SRCH_CNT,
	V4L2_CID_PRIVATE_SILABS_EMPHASIS,
	V4L2_CID_PRIVATE_SILABS_RDS_STD,
	V4L2_CID_PRIVATE_SILABS_SPACING,
	V4L2_CID_PRIVATE_SILABS_RDSON,
	V4L2_CID_PRIVATE_SILABS_RDSGROUP_PROC,
	V4L2_CID_PRIVATE_SILABS_LP_MODE,
	V4L2_CID_PRIVATE_SILABS_ANTENNA,
	V4L2_CID_PRIVATE_SILABS_RDSD_BUF,
	V4L2_CID_PRIVATE_SILABS_PSALL,
	/*v4l2 Tx controls*/
	V4L2_CID_PRIVATE_SILABS_TX_SETPSREPEATCOUNT,
	V4L2_CID_PRIVATE_SILABS_STOP_RDS_TX_PS_NAME,
	V4L2_CID_PRIVATE_SILABS_STOP_RDS_TX_RT,
	V4L2_CID_PRIVATE_SILABS_IOVERC,
	V4L2_CID_PRIVATE_SILABS_INTDET,
	V4L2_CID_PRIVATE_SILABS_MPX_DCC,
	V4L2_CID_PRIVATE_SILABS_AF_JUMP,
	V4L2_CID_PRIVATE_SILABS_RSSI_DELTA,
	V4L2_CID_PRIVATE_SILABS_HLSI,

	/*
	* Here we have IOCTl's that are specific to IRIS
	* (V4L2_CID_PRIVATE_BASE + 0x1E to V4L2_CID_PRIVATE_BASE + 0x28)
	*/
	V4L2_CID_PRIVATE_SILABS_SOFT_MUTE,/* 0x800001E*/
	V4L2_CID_PRIVATE_SILABS_RIVA_ACCS_ADDR,
	V4L2_CID_PRIVATE_SILABS_RIVA_ACCS_LEN,
	V4L2_CID_PRIVATE_SILABS_RIVA_PEEK,
	V4L2_CID_PRIVATE_SILABS_RIVA_POKE,
	V4L2_CID_PRIVATE_SILABS_SSBI_ACCS_ADDR,
	V4L2_CID_PRIVATE_SILABS_SSBI_PEEK,
	V4L2_CID_PRIVATE_SILABS_SSBI_POKE,
	V4L2_CID_PRIVATE_SILABS_TX_TONE,
	V4L2_CID_PRIVATE_SILABS_RDS_GRP_COUNTERS,
	V4L2_CID_PRIVATE_SILABS_SET_NOTCH_FILTER,/* 0x8000028 */

	V4L2_CID_PRIVATE_SILABS_SET_AUDIO_PATH,/* 0x8000029 */
	V4L2_CID_PRIVATE_SILABS_DO_CALIBRATION,/* 0x800002A : IRIS */
	V4L2_CID_PRIVATE_SILABS_SRCH_ALGORITHM,/* 0x800002B */
	V4L2_CID_PRIVATE_SILABS__GET_SINR, /* 0x800002C : IRIS */
	V4L2_CID_PRIVATE_SILABS_INTF_LOW_THRESHOLD, /* 0x800002D */
	V4L2_CID_PRIVATE_SILABS_INTF_HIGH_THRESHOLD, /* 0x800002E */
	V4L2_CID_PRIVATE_SILABS_SINR_THRESHOLD,  /* 0x800002F : IRIS */
	V4L2_CID_PRIVATE_SILABS_SINR_SAMPLES,  /* 0x8000030 : IRIS */
	V4L2_CID_PRIVATE_SILABS_SPUR_FREQ,
	V4L2_CID_PRIVATE_SILABS_SPUR_FREQ_RMSSI,
	V4L2_CID_PRIVATE_SILABS_SPUR_SELECTION,
	V4L2_CID_PRIVATE_SILABS_UPDATE_SPUR_TABLE,
	V4L2_CID_PRIVATE_SILABS_VALID_CHANNEL,

};

enum silabs_buf_t {
	SILABS_FM_BUF_SRCH_LIST,
	SILABS_FM_BUF_EVENTS,
	SILABS_FM_BUF_RT_RDS,
	SILABS_FM_BUF_PS_RDS,
	SILABS_FM_BUF_RAW_RDS,
	SILABS_FM_BUF_AF_LIST,
	SILABS_FM_BUF_MAX
};

enum silabs_evt_t {
	SILABS_EVT_RADIO_READY,
	SILABS_EVT_TUNE_SUCC,
	SILABS_EVT_SEEK_COMPLETE,
	SILABS_EVT_SCAN_NEXT,
	SILABS_EVT_NEW_RAW_RDS,
	SILABS_EVT_NEW_RT_RDS,
	SILABS_EVT_NEW_PS_RDS,
	SILABS_EVT_ERROR,
	SILABS_EVT_BELOW_TH,
	SILABS_EVT_ABOVE_TH,
	SILABS_EVT_STEREO,
	SILABS_EVT_MONO,
	SILABS_EVT_RDS_AVAIL,
	SILABS_EVT_RDS_NOT_AVAIL,
	SILABS_EVT_NEW_SRCH_LIST,
	SILABS_EVT_NEW_AF_LIST,
	SILABS_EVT_TXRDSDAT,
	SILABS_EVT_TXRDSDONE,
	SILABS_EVT_RADIO_DISABLED
};

enum silabs_region_t {
	SILABS_REGION_US,
	SILABS_REGION_EU,
	SILABS_REGION_JAPAN,
	SILABS_REGION_JAPAN_WIDE,
	SILABS_REGION_OTHER
};

enum silabs_interrupts_t {
	DISABLE_ALL_INTERRUPTS,
	ENABLE_STC_RDS_INTERRUPTS,
	ENABLE_STC_INTERRUPTS
};

struct silabs_fm_recv_conf_req {
	__u16	emphasis;
	__u16	ch_spacing;
	/* limits stored as actual freq / TUNE_STEP_SIZE */
	__u16	band_low_limit;
	__u16	band_high_limit;
};

static inline bool is_valid_chan_spacing(int spacing)
{
	if ((spacing == 0) ||
		(spacing == 1) ||
		(spacing == 2))
		return 1;
	else
		return 0;
}

static inline bool is_valid_rds_std(int rds_std)
{
	if ((rds_std >= MIN_RDS_STD) &&
		(rds_std <= MAX_RDS_STD))
		return 1;
	else
		return 0;
}

static inline bool is_valid_srch_mode(int srch_mode)
{
	if ((srch_mode >= MIN_SRCH_MODE) &&
		(srch_mode <= MAX_SRCH_MODE))
		return 1;
	else
		return 0;
}

struct fm_power_vreg_data {
	/* voltage regulator handle */
	struct regulator *reg;
	/* regulator name */
	const char *name;
	/* voltage levels to be set */
	unsigned int low_vol_level;
	unsigned int high_vol_level;
	bool set_voltage_sup;
	/* is this regulator enabled? */
	bool is_enabled;
};
#endif /* __RADIO_SILABS_H */
