#ifndef __LINUX_TAVARUA_H
#define __LINUX_TAVARUA_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <asm/sizes.h>
#else
#include <stdint.h>
#endif
#include <linux/ioctl.h>
#include <linux/videodev2.h>


#undef FM_DEBUG

/* constants */
#define  RDS_BLOCKS_NUM             (4)
#define BYTES_PER_BLOCK             (3)
#define MAX_PS_LENGTH              (96)
#define MAX_RT_LENGTH              (64)

#define XFRDAT0                    (0x20)
#define XFRDAT1                    (0x21)
#define XFRDAT2                    (0x22)

#define INTDET_PEEK_MSB            (0x88)
#define INTDET_PEEK_LSB            (0x26)

#define RMSSI_PEEK_MSB             (0x88)
#define RMSSI_PEEK_LSB             (0xA8)

#define MPX_DCC_BYPASS_POKE_MSB    (0x88)
#define MPX_DCC_BYPASS_POKE_LSB    (0xC0)

#define MPX_DCC_PEEK_MSB_REG1      (0x88)
#define MPX_DCC_PEEK_LSB_REG1      (0xC2)

#define MPX_DCC_PEEK_MSB_REG2      (0x88)
#define MPX_DCC_PEEK_LSB_REG2      (0xC3)

#define MPX_DCC_PEEK_MSB_REG3      (0x88)
#define MPX_DCC_PEEK_LSB_REG3      (0xC4)

#define ON_CHANNEL_TH_MSB          (0x0B)
#define ON_CHANNEL_TH_LSB          (0xA8)

#define OFF_CHANNEL_TH_MSB         (0x0B)
#define OFF_CHANNEL_TH_LSB         (0xAC)

#define ENF_200Khz                    (1)
#define SRCH200KHZ_OFFSET             (7)
#define SRCH_MASK                  (1 << SRCH200KHZ_OFFSET)

/* Standard buffer size */
#define STD_BUF_SIZE               (64)
/* Search direction */
#define SRCH_DIR_UP                 (0)
#define SRCH_DIR_DOWN               (1)

/* control options */
#define CTRL_ON                     (1)
#define CTRL_OFF                    (0)

#define US_LOW_BAND                (87.5)
#define US_HIGH_BAND               (108)

/* constant for Tx */

#define MASK_PI                    (0x0000FFFF)
#define MASK_PI_MSB                (0x0000FF00)
#define MASK_PI_LSB                (0x000000FF)
#define MASK_PTY                   (0x0000001F)
#define MASK_TXREPCOUNT            (0x0000000F)

#undef FMDBG
#ifdef FM_DEBUG
  #define FMDBG(fmt, args...) printk(KERN_INFO "tavarua_radio: " fmt, ##args)
#else
  #define FMDBG(fmt, args...)
#endif

#undef FMDERR
#define FMDERR(fmt, args...) printk(KERN_INFO "tavarua_radio: " fmt, ##args)

#undef FMDBG_I2C
#ifdef FM_DEBUG_I2C
  #define FMDBG_I2C(fmt, args...) printk(KERN_INFO "fm_i2c: " fmt, ##args)
#else
  #define FMDBG_I2C(fmt, args...)
#endif

/* function declarations */
/* FM Core audio paths. */
#define TAVARUA_AUDIO_OUT_ANALOG_OFF	(0)
#define TAVARUA_AUDIO_OUT_ANALOG_ON	(1)
#define TAVARUA_AUDIO_OUT_DIGITAL_OFF	(0)
#define TAVARUA_AUDIO_OUT_DIGITAL_ON	(1)

int tavarua_set_audio_path(int digital_on, int analog_on);

/* defines and enums*/

#define MARIMBA_A0	0x01010013
#define MARIMBA_2_1	0x02010204
#define BAHAMA_1_0	0x0302010A
#define BAHAMA_2_0	0x04020205
#define WAIT_TIMEOUT 2000
#define RADIO_INIT_TIME 15
#define TAVARUA_DELAY 10
/*
 * The frequency is set in units of 62.5 Hz when using V4L2_TUNER_CAP_LOW,
 * 62.5 kHz otherwise.
 * The tuner is able to have a channel spacing of 50, 100 or 200 kHz.
 * tuner->capability is therefore set to V4L2_TUNER_CAP_LOW
 * The FREQ_MUL is then: 1 MHz / 62.5 Hz = 16000
 */
#define FREQ_MUL (1000000 / 62.5)

enum v4l2_cid_private_tavarua_t {
	V4L2_CID_PRIVATE_TAVARUA_SRCHMODE = (V4L2_CID_PRIVATE_BASE + 1),
	V4L2_CID_PRIVATE_TAVARUA_SCANDWELL,
	V4L2_CID_PRIVATE_TAVARUA_SRCHON,
	V4L2_CID_PRIVATE_TAVARUA_STATE,
	V4L2_CID_PRIVATE_TAVARUA_TRANSMIT_MODE,
	V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_MASK,
	V4L2_CID_PRIVATE_TAVARUA_REGION,
	V4L2_CID_PRIVATE_TAVARUA_SIGNAL_TH,
	V4L2_CID_PRIVATE_TAVARUA_SRCH_PTY,
	V4L2_CID_PRIVATE_TAVARUA_SRCH_PI,
	V4L2_CID_PRIVATE_TAVARUA_SRCH_CNT,
	V4L2_CID_PRIVATE_TAVARUA_EMPHASIS,
	V4L2_CID_PRIVATE_TAVARUA_RDS_STD,
	V4L2_CID_PRIVATE_TAVARUA_SPACING,
	V4L2_CID_PRIVATE_TAVARUA_RDSON,
	V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_PROC,
	V4L2_CID_PRIVATE_TAVARUA_LP_MODE,
	V4L2_CID_PRIVATE_TAVARUA_ANTENNA,
	V4L2_CID_PRIVATE_TAVARUA_RDSD_BUF,
	V4L2_CID_PRIVATE_TAVARUA_PSALL,
	/*v4l2 Tx controls*/
	V4L2_CID_PRIVATE_TAVARUA_TX_SETPSREPEATCOUNT,
	V4L2_CID_PRIVATE_TAVARUA_STOP_RDS_TX_PS_NAME,
	V4L2_CID_PRIVATE_TAVARUA_STOP_RDS_TX_RT,
	V4L2_CID_PRIVATE_TAVARUA_IOVERC,
	V4L2_CID_PRIVATE_TAVARUA_INTDET,
	V4L2_CID_PRIVATE_TAVARUA_MPX_DCC,
	V4L2_CID_PRIVATE_TAVARUA_AF_JUMP,
	V4L2_CID_PRIVATE_TAVARUA_RSSI_DELTA,
	V4L2_CID_PRIVATE_TAVARUA_HLSI,

	/*
	* Here we have IOCTl's that are specific to IRIS
	* (V4L2_CID_PRIVATE_BASE + 0x1E to V4L2_CID_PRIVATE_BASE + 0x28)
	*/
	V4L2_CID_PRIVATE_SOFT_MUTE,/* 0x800001E*/
	V4L2_CID_PRIVATE_RIVA_ACCS_ADDR,
	V4L2_CID_PRIVATE_RIVA_ACCS_LEN,
	V4L2_CID_PRIVATE_RIVA_PEEK,
	V4L2_CID_PRIVATE_RIVA_POKE,
	V4L2_CID_PRIVATE_SSBI_ACCS_ADDR,
	V4L2_CID_PRIVATE_SSBI_PEEK,
	V4L2_CID_PRIVATE_SSBI_POKE,
	V4L2_CID_PRIVATE_TX_TONE,
	V4L2_CID_PRIVATE_RDS_GRP_COUNTERS,
	V4L2_CID_PRIVATE_SET_NOTCH_FILTER,/* 0x8000028 */

	V4L2_CID_PRIVATE_TAVARUA_SET_AUDIO_PATH,/* 0x8000029 */
	V4L2_CID_PRIVATE_TAVARUA_DO_CALIBRATION,/* 0x800002A : IRIS */
	V4L2_CID_PRIVATE_TAVARUA_SRCH_ALGORITHM,/* 0x800002B */
	V4L2_CID_PRIVATE_IRIS_GET_SINR, /* 0x800002C : IRIS */
	V4L2_CID_PRIVATE_INTF_LOW_THRESHOLD, /* 0x800002D */
	V4L2_CID_PRIVATE_INTF_HIGH_THRESHOLD, /* 0x800002E */
	V4L2_CID_PRIVATE_SINR_THRESHOLD,  /* 0x800002F : IRIS */
	V4L2_CID_PRIVATE_SINR_SAMPLES,  /* 0x8000030 : IRIS */

};

enum tavarua_buf_t {
	TAVARUA_BUF_SRCH_LIST,
	TAVARUA_BUF_EVENTS,
	TAVARUA_BUF_RT_RDS,
	TAVARUA_BUF_PS_RDS,
	TAVARUA_BUF_RAW_RDS,
	TAVARUA_BUF_AF_LIST,
	TAVARUA_BUF_MAX
};

enum tavarua_xfr_t {
	TAVARUA_XFR_SYNC,
	TAVARUA_XFR_ERROR,
	TAVARUA_XFR_SRCH_LIST,
	TAVARUA_XFR_RT_RDS,
	TAVARUA_XFR_PS_RDS,
	TAVARUA_XFR_AF_LIST,
	TAVARUA_XFR_MAX
};

enum channel_spacing {
	FM_CH_SPACE_200KHZ,
	FM_CH_SPACE_100KHZ,
	FM_CH_SPACE_50KHZ
};

enum step_size {
	NO_SRCH200khz,
	ENF_SRCH200khz
};

enum emphasis {
	EMP_75,
	EMP_50
};

enum rds_std {
	RBDS_STD,
	RDS_STD
};

/* offsets */
#define RAW_RDS		0x0F
#define RDS_BLOCK 	3

/* registers*/
#define MARIMBA_XO_BUFF_CNTRL 0x07
#define RADIO_REGISTERS 0x30
#define XFR_REG_NUM     16
#define STATUS_REG_NUM 	3

/* TX constants */
#define HEADER_SIZE	4
#define TX_ON		0x80
#define TAVARUA_TX_RT	RDS_RT_0
#define TAVARUA_TX_PS	RDS_PS_0

enum register_t {
	STATUS_REG1 = 0,
	STATUS_REG2,
	STATUS_REG3,
	RDCTRL,
	FREQ,
	TUNECTRL,
	SRCHRDS1,
	SRCHRDS2,
	SRCHCTRL,
	IOCTRL,
	RDSCTRL,
	ADVCTRL,
	AUDIOCTRL,
	RMSSI,
	IOVERC,
	AUDIOIND = 0x1E,
	XFRCTRL,
	FM_CTL0 = 0xFF,
	LEAKAGE_CNTRL = 0xFE,
};
#define BAHAMA_RBIAS_CTL1       0x07
#define	BAHAMA_FM_MODE_REG      0xFD
#define	BAHAMA_FM_CTL1_REG      0xFE
#define	BAHAMA_FM_CTL0_REG      0xFF
#define BAHAMA_FM_MODE_NORMAL   0x00
#define BAHAMA_LDO_DREG_CTL0    0xF0
#define BAHAMA_LDO_AREG_CTL0    0xF4

/* Radio Control */
#define RDCTRL_STATE_OFFSET	0
#define RDCTRL_STATE_MASK	(3 << RDCTRL_STATE_OFFSET)
#define RDCTRL_BAND_OFFSET	2
#define RDCTRL_BAND_MASK	(1 << RDCTRL_BAND_OFFSET)
#define RDCTRL_CHSPACE_OFFSET	3
#define RDCTRL_CHSPACE_MASK	(3 << RDCTRL_CHSPACE_OFFSET)
#define RDCTRL_DEEMPHASIS_OFFSET 5
#define RDCTRL_DEEMPHASIS_MASK	(1 << RDCTRL_DEEMPHASIS_OFFSET)
#define RDCTRL_HLSI_OFFSET	6
#define RDCTRL_HLSI_MASK	(3 << RDCTRL_HLSI_OFFSET)
#define RDSAF_OFFSET		6
#define RDSAF_MASK		(1 << RDSAF_OFFSET)

/* Tune Control */
#define TUNE_STATION	0x01
#define ADD_OFFSET	(1 << 1)
#define SIGSTATE	(1 << 5)
#define MOSTSTATE	(1 << 6)
#define RDSSYNC		(1 << 7)
/* Search Control */
#define SRCH_MODE_OFFSET	0
#define SRCH_MODE_MASK		(7 << SRCH_MODE_OFFSET)
#define SRCH_DIR_OFFSET		3
#define SRCH_DIR_MASK		(1 << SRCH_DIR_OFFSET)
#define SRCH_DWELL_OFFSET	4
#define SRCH_DWELL_MASK		(7 << SRCH_DWELL_OFFSET)
#define SRCH_STATE_OFFSET	7
#define SRCH_STATE_MASK		(1 << SRCH_STATE_OFFSET)

/* I/O Control */
#define IOC_HRD_MUTE	0x03
#define IOC_SFT_MUTE    (1 << 2)
#define IOC_MON_STR     (1 << 3)
#define IOC_SIG_BLND    (1 << 4)
#define IOC_INTF_BLND   (1 << 5)
#define IOC_ANTENNA     (1 << 6)
#define IOC_ANTENNA_OFFSET	6
#define IOC_ANTENNA_MASK     	(1 << IOC_ANTENNA_OFFSET)

/* RDS Control */
#define RDS_ON		0x01
#define RDSCTRL_STANDARD_OFFSET 1
#define RDSCTRL_STANDARD_MASK	(1 << RDSCTRL_STANDARD_OFFSET)

/* Advanced features controls */
#define RDSRTEN		(1 << 3)
#define RDSPSEN		(1 << 4)

/* Audio path control */
#define AUDIORX_ANALOG_OFFSET 	0
#define AUDIORX_ANALOG_MASK 	(1 << AUDIORX_ANALOG_OFFSET)
#define AUDIORX_DIGITAL_OFFSET 	1
#define AUDIORX_DIGITAL_MASK 	(1 << AUDIORX_DIGITAL_OFFSET)
#define AUDIOTX_OFFSET		2
#define AUDIOTX_MASK		(1 << AUDIOTX_OFFSET)
#define I2SCTRL_OFFSET		3
#define I2SCTRL_MASK		(1 << I2SCTRL_OFFSET)

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

enum audio_path {
	FM_DIGITAL_PATH,
	FM_ANALOG_PATH
};
#define SRCH_MODE	0x07
#define SRCH_DIR	0x08 /* 0-up 1-down */
#define SCAN_DWELL	0x70
#define SRCH_ON		0x80

/* RDS CONFIG */
#define RDS_CONFIG_PSALL 0x01

#define FM_ENABLE	0x22
#define SET_REG_FIELD(reg, val, offset, mask) \
	(reg = (reg & ~mask) | (((val) << offset) & mask))
#define GET_REG_FIELD(reg, offset, mask) ((reg & mask) >> offset)
#define RSH_DATA(val, offset)    ((val) >> (offset))
#define LSH_DATA(val, offset)    ((val) << (offset))
#define GET_ABS_VAL(val)        ((val) & (0xFF))

enum radio_state_t {
	FM_OFF,
	FM_RECV,
	FM_TRANS,
	FM_RESET,
};

#define XFRCTRL_WRITE   (1 << 7)

/* Interrupt status */

/* interrupt register 1 */
#define	READY		(1 << 0) /* Radio ready after powerup or reset */
#define	TUNE		(1 << 1) /* Tune completed */
#define	SEARCH		(1 << 2) /* Search completed (read FREQ) */
#define	SCANNEXT	(1 << 3) /* Scanning for next station */
#define	SIGNAL		(1 << 4) /* Signal indicator change (read SIGSTATE) */
#define	INTF		(1 << 5) /* Interference cnt has fallen outside range */
#define	SYNC		(1 << 6) /* RDS sync state change (read RDSSYNC) */
#define	AUDIO		(1 << 7) /* Audio Control indicator (read AUDIOIND) */

/* interrupt register 2 */
#define	RDSDAT		(1 << 0) /* New unread RDS data group available */
#define	BLOCKB		(1 << 1) /* Block-B match condition exists */
#define	PROGID		(1 << 2) /* Block-A or Block-C matched stored PI value*/
#define	RDSPS		(1 << 3) /* New RDS Program Service Table available */
#define	RDSRT		(1 << 4) /* New RDS Radio Text available */
#define	RDSAF		(1 << 5) /* New RDS AF List available */
#define	TXRDSDAT	(1 << 6) /* Transmitted an RDS group */
#define	TXRDSDONE	(1 << 7) /* RDS raw group one-shot transmit completed */

/* interrupt register 3 */
#define	TRANSFER	(1 << 0) /* Data transfer (XFR) completed */
#define	RDSPROC		(1 << 1) /* Dynamic RDS Processing complete */
#define	ERROR		(1 << 7) /* Err occurred.Read code to determine cause */


#define	FM_TX_PWR_LVL_0		0 /* Lowest power lvl that can be set for Tx */
#define	FM_TX_PWR_LVL_MAX	7 /* Max power lvl for Tx */
/* Transfer */
enum tavarua_xfr_ctrl_t {
	RDS_PS_0 = 0x01,
	RDS_PS_1,
	RDS_PS_2,
	RDS_PS_3,
	RDS_PS_4,
	RDS_PS_5,
	RDS_PS_6,
	RDS_RT_0,
	RDS_RT_1,
	RDS_RT_2,
	RDS_RT_3,
	RDS_RT_4,
	RDS_AF_0,
	RDS_AF_1,
	RDS_CONFIG,
	RDS_TX_GROUPS,
	RDS_COUNT_0,
	RDS_COUNT_1,
	RDS_COUNT_2,
	RADIO_CONFIG,
	RX_CONFIG,
	RX_TIMERS,
	RX_STATIONS_0,
	RX_STATIONS_1,
	INT_CTRL,
	ERROR_CODE,
	CHIPID,
	CAL_DAT_0 = 0x20,
	CAL_DAT_1,
	CAL_DAT_2,
	CAL_DAT_3,
	CAL_CFG_0,
	CAL_CFG_1,
	DIG_INTF_0,
	DIG_INTF_1,
	DIG_AGC_0,
	DIG_AGC_1,
	DIG_AGC_2,
	DIG_AUDIO_0,
	DIG_AUDIO_1,
	DIG_AUDIO_2,
	DIG_AUDIO_3,
	DIG_AUDIO_4,
	DIG_RXRDS,
	DIG_DCC,
	DIG_SPUR,
	DIG_MPXDCC,
	DIG_PILOT,
	DIG_DEMOD,
	DIG_MOST,
	DIG_TX_0,
	DIG_TX_1,
	PHY_TXGAIN = 0x3B,
	PHY_CONFIG,
	PHY_TXBLOCK,
	PHY_TCB,
	XFR_PEEK_MODE = 0x40,
	XFR_POKE_MODE = 0xC0,
	TAVARUA_XFR_CTRL_MAX
};

enum tavarua_evt_t {
	TAVARUA_EVT_RADIO_READY,
	TAVARUA_EVT_TUNE_SUCC,
	TAVARUA_EVT_SEEK_COMPLETE,
	TAVARUA_EVT_SCAN_NEXT,
	TAVARUA_EVT_NEW_RAW_RDS,
	TAVARUA_EVT_NEW_RT_RDS,
	TAVARUA_EVT_NEW_PS_RDS,
	TAVARUA_EVT_ERROR,
	TAVARUA_EVT_BELOW_TH,
	TAVARUA_EVT_ABOVE_TH,
	TAVARUA_EVT_STEREO,
	TAVARUA_EVT_MONO,
	TAVARUA_EVT_RDS_AVAIL,
	TAVARUA_EVT_RDS_NOT_AVAIL,
	TAVARUA_EVT_NEW_SRCH_LIST,
	TAVARUA_EVT_NEW_AF_LIST,
	TAVARUA_EVT_TXRDSDAT,
	TAVARUA_EVT_TXRDSDONE,
	TAVARUA_EVT_RADIO_DISABLED
};

enum tavarua_region_t {
	TAVARUA_REGION_US,
	TAVARUA_REGION_EU,
	TAVARUA_REGION_JAPAN,
	TAVARUA_REGION_JAPAN_WIDE,
	TAVARUA_REGION_OTHER
};

#endif /* __LINUX_TAVARUA_H */
