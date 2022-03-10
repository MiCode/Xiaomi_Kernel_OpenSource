/* SPDX-License-Identifier: GPL-2.0 */

/**
 * Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
 * 2018-10-16 File created.
 */

#ifndef __FSM_DEV_H__
#define __FSM_DEV_H__

#ifndef pr_fmt
#define pr_fmt(fmt) "%s: " fmt "\n", __func__
#endif
#if defined(__KERNEL__)
#include <linux/module.h>
#include <linux/regmap.h>
//#include <linux/miscdevice.h>
#include <linux/ioctl.h>
#include <linux/workqueue.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/delay.h>
#if defined(CONFIG_REGULATOR)
#include <linux/regulator/consumer.h>
#endif
#elif defined(FSM_HAL_SUPPORT)
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#if defined(CONFIG_FSM_STEREO)
#include "fsm_list.h"
#endif

/* debug info */
#ifdef FSM_DEBUG
#undef NDEBUG
#define LOG_NDEBUG 0  // open LOGV
#endif

#if defined(LOG_TAG)
#undef LOG_TAG
#endif
#define LOG_TAG "fsm_hal"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#if defined(FSM_APP)
#if defined(FSM_DEBUG)
#define pr_debug(fmt, args...) printf("%s: " fmt "\n", __func__, ##args)
#else
#define pr_debug(fmt, args...)
#endif
#define pr_info(fmt, args...) printf("%s: " fmt "\n", __func__, ##args)
#elif defined(__NDK_BUILD__)
#include<android/log.h>
#define pr_debug(fmt, args...) __android_log_print(ANDROID_LOG_DEBUG,\
		LOG_TAG, "%s: " fmt, __func__, ##args)
#define pr_info(fmt, args...) __android_log_print(ANDROID_LOG_INFO,\
		LOG_TAG, "%s: " fmt, __func__, ##args)
#elif defined(__ANDROID__)
#include <utils/Log.h>
#define pr_debug(fmt, args...) __android_log_print(ANDROID_LOG_DEBUG,\
		LOG_TAG, "%s: " fmt, __func__, ##args)
#define pr_info(fmt, args...) __android_log_print(ANDROID_LOG_INFO,\
		LOG_TAG, "%s: " fmt, __func__, ##args)
#else
#define pr_debug(fmt, args...)
#define pr_info(fmt, args...)
#endif
#endif
#define pr_addr(type, fmt, args...) \
		pr_##type("%02X: " fmt, fsm_dev->addr, ##args)

#define FSM_DRV_NAME     "fs16xx"
#define FSM_FW_NAME      "fs16xx.fsm"

/* VERSION INFORMATION */
#define FSM_CODE_VERSION "v3.0.3"
#define FSM_CODE_DATE    "20200901-1539"
#define FSM_GIT_BRANCH   ""
#define FSM_GIT_COMMIT   ""

#define FSM_FUNC_EXIT(ret) \
	do { \
		if (ret) \
			pr_info("err: %d", ret); \
	} while (0)

#define FSM_ADDR_EXIT(ret) \
	do { \
		if (ret) \
			pr_addr(err, "err: %d", ret); \
	} while (0)

#define UNUSED(expr) do { (void)(expr); } while (0)

enum dev_id_index {
	FS1601S_DEV_ID = 0x3,
	FS1603_DEV_ID = 0x5,
	FS1818_DEV_ID = 0x6,
	FS1860_DEV_ID = 0x7,
	FS1896_DEV_ID = 0xB,
	FSM_DEV_ID_MAX,
};
#define FS1894S_REVID 0X05B1

#define CUST_NAME_SIZE        (32)
#define STRING_LEN_MAX        (255)
#define STEREO_COEF_LEN       (10)
#define FSM_MAGNIF_FACTOR     (10) // magnification factor: 2^(x)
#define FSM_SPKR_ALLOWANCE    (30) // percentage: %
#define FSM_RCVR_ALLOWANCE    (20) // percentage: %
#define FSM_WAIT_STABLE_RETRY (200)
#define FSM_I2C_RETRY         (20)
#define FSM_ZMDELTA_MAX       0x150
#define FSM_RS_TRIM_DEFAULT   0x8F
#define FSM_MAGNIF_TEMPR_COEF 0xFFFF
#define FSM_OTP_ACC_KEY2      0xCA91
#define FSM_DATA_TYPE_RE25    0
#define FSM_DATA_TYPE_ZMDATA  1
#define MODULE_INITED         (13579)
#define FSM_MAGNIF(val)       ((val) << FSM_MAGNIF_FACTOR)
#define FSM_VBATH_COUNT       (10)
#define FSM_VBAT_HIGH         (3600) // mV
#define FSM_VBAT_LOW          (3400) // mV
// gain step: -0.375dB, max: -0.375 * 8 = -3dB
#define FSM_VBAT_STEP         ((FSM_VBAT_HIGH - FSM_VBAT_LOW) / 8) // mV

#define FSM_DEV_MAX   (4)
#define FSM_ADDR_BASE (0x34)
#define MIN(a, b)     ((a) < (b) ? (a) : (b))
#define BIT(nr)       (1UL << (nr))
#define HIGH8(val)    ((val >> 8) & 0xFF)
#define LOW8(val)     (val & 0xFF)
#define WORD(addr)    ((*(addr) << 8) & *((addr) + 1))
#define REG(bf)       (bf & 0xFF)
#define FSM_REG_SIZE  sizeof(uint16_t)
#define FSM_ADDR_SIZE sizeof(uint8_t)

/* scenes defination */
#define FSM_SCENE_UNKNOW           (0)
#define FSM_SCENE_MUSIC            BIT(0)
#define FSM_SCENE_VOICE            BIT(1)
#define FSM_SCENE_VOIP             BIT(2)
#define FSM_SCENE_RING             BIT(3)
#define FSM_SCENE_LOW_PWR          BIT(4)
#define FSM_SCENE_MMI_ALL          BIT(5)
#define FSM_SCENE_MMI_ALL_BYPASS   BIT(6)
#define FSM_SCENE_TOP_LEFT         BIT(7)
#define FSM_SCENE_TOP_LEFT_BYPASS  BIT(8)
#define FSM_SCENE_TOP_RIGHT        BIT(9)
#define FSM_SCENE_TOP_RIGHT_BYPASS BIT(10)
#define FSM_SCENE_BOT_LEFT         BIT(11)
#define FSM_SCENE_BOT_LEFT_BYPASS  BIT(12)
#define FSM_SCENE_BOT_RIGHT        BIT(13)
#define FSM_SCENE_BOT_RIGHT_BYPASS BIT(14)
#define FSM_SCENE_RCV              BIT(15) // unused
#define FSM_SCENE_MAX              (15)
#define FSM_SCENE_COMMON           (0xFFFF)
#define FSM_VOLUME_MAX             (0xFF)

/* f0 test */
#define ACS_COEF_COUNT   12
#define COEF_LEN         5
#define FREQ_START       200  // 100*N
#define FREQ_END         1500 // bigger than FREQ_START && 100*N
#define F0_FREQ_STEP     100  // 50*N, N =1,2,3...
#define F0_COUNT_MAX     ((FREQ_END - FREQ_START) / F0_FREQ_STEP + 1)
#define FREQ_EXT_COUNT   300
#define F0_TEST_DELAY_MS 500

#define FSM_POS_MONO (0)
#define FSM_POS_LTOP BIT(3)
#define FSM_POS_LBTM BIT(2)
#define FSM_POS_RTOP BIT(1)
#define FSM_POS_RBTM BIT(0)

//#undef NULL
//#define NULL ((void *)0)
//
//enum {
//	false	= 0,
//	true	= 1
//};

enum fsm_error {
	FSM_ERROR_OK = 0,
	FSM_ERROR_BAD_PARMS,
	FSM_ERROR_I2C_FATAL,
	FSM_ERROR_NO_SCENE_MATCH,
	FSM_ERROR_RE25_INVAILD,
	FSM_ERROR_MAX,
};

enum fsm_channel {
	FSM_CHN_UNKNOW = 0,
	FSM_CHN_LEFT   = 1,
	FSM_CHN_RIGHT  = 2,
	FSM_CHN_MONO   = 3,
};

enum fsm_format {
	FSM_FMT_DSP    = 1,
	FSM_FMT_MSB    = 2,
	FSM_FMT_I2S    = 3,
	FSM_FMT_LSB_16 = 4,
	FSM_FMT_LSB_20 = 6,
	FSM_FMT_LSB_24 = 7,
};

enum fsm_bstmode {
	FSM_BSTMODE_FLW = 0,
	FSM_BSTMODE_BST = 1,
	FSM_BSTMODE_ADP = 2,
};

enum fsm_dsp_state {
	FSM_DSP_OFF = 0,
	FSM_DSP_ON  = 1,
};

enum fsm_wait_type {
	FSM_WAIT_AMP_ON = 1,
	FSM_WAIT_AMP_ADC_ON,
	FSM_WAIT_AMP_OFF,
	FSM_WAIT_AMP_ADC_OFF,
	FSM_WAIT_AMP_ADC_PLL_OFF,
	FSM_WAIT_TSIGNAL_ON,
	FSM_WAIT_TSIGNAL_OFF,
	FSM_WAIT_OTP_READY,
	FSM_WAIT_BOOST_SSEND,
	FSM_WAIT_STATUS_ON,
};

enum fsm_eq_ram_type {
	FSM_EQ_RAM0 = 0,
	FSM_EQ_RAM1,
	FSM_EQ_RAM_MAX,
};

struct fsm_version {
	char git_branch[50];
	char git_commit[41];
	char code_date[18];
	char code_version[10];
};
typedef struct fsm_version fsm_version_t;

struct fsm_pll_config {
	uint32_t bclk;
	uint16_t c1;
	uint16_t c2;
	uint16_t c3;
};
typedef struct fsm_pll_config fsm_pll_config_t;

#define PRESET_VER_FS1601S 0x8301 // FS1601S
#define PRESET_VER_FS1603  0x8501 // FS1603 series
#define PRESET_VER_FS1801  0x8A01 // FS1801 series
#define PRESET_VER_FS1860  0x8701 // FS1860
#define PRESET_VER3        (0x93)
#define IS_PRESET_V3(ver)  (HIGH8(ver) == PRESET_VER3)

enum fsm_dsc_type {
	FSM_DSC_DEV_INFO = 0,
	FSM_DSC_SPK_INFO,
	FSM_DSC_REG_COMMON,
	FSM_DSC_REG_SCENES,
	FSM_DSC_PRESET_EQ,
	FSM_DSC_STEREO_COEF,
	FSM_DSC_EXCER_RAM,
	FSM_DSC_MAX,
};

enum fsm_info_type {
	FSM_INFO_SPK_TMAX = 0,
	FSM_INFO_SPK_TEMPR_COEF,
	FSM_INFO_SPK_TEMPR_SEL,
	FSM_INFO_SPK_RES,
	FSM_INFO_SPK_RAPP,
	FSM_INFO_SPK_TRE,
	FSM_INFO_SPK_TM6,
	FSM_INFO_SPK_TM24,
	FSM_INFO_SPK_TERR,
	FSM_INFO_SPK_TM01,
	FSM_INFO_SPK_TM02,
	FSM_INFO_SPK_TM03,
	FSM_INFO_SPK_TM04,
	FSM_INFO_SPK_TM05,
	FSM_INFO_SPK_TM06,
	FSM_INFO_RSRL_RATIO,
	FSM_INFO_SPK_POSITION,
	FSM_INFO_SPK_MAX,
};

enum fsm_test_type {
	TEST_NONE = 0,
	TEST_RE25,
	TEST_F0,
};

#pragma pack(push, 1)
struct fsm_date {
	uint32_t min   : 6;
	uint32_t hour  : 5;
	uint32_t day   : 5;
	uint32_t month : 4;
	uint32_t year  : 12;
};
typedef struct fsm_date fsm_date_t;

struct data32_list {
	uint16_t len;
	uint32_t data[];
};
typedef struct data32_list preset_data_t;
typedef struct data32_list ram_data_t;

struct uint24 {
	uint8_t DL;
	uint8_t DM;
	uint8_t DH;
};
typedef struct uint24 uint24_t;

struct data24_list {
	uint16_t len;
	uint24_t data[];
};

struct data16_list {
	uint16_t len;
	uint16_t data[];
};
typedef struct data16_list info_list_t;
typedef struct data16_list stereo_coef_t;

struct fsm_index {
	uint16_t offset;
	uint16_t type;
};
typedef struct fsm_index fsm_index_t;

struct dev_list {
	uint16_t preset_ver;
	char project[8];
	char customer[8];
	fsm_date_t date;
	uint16_t data_len;
	uint16_t crc16;
	uint16_t len;
	uint16_t bus;
	uint16_t addr;
	uint16_t dev_type;
	uint16_t npreset;
	uint16_t reg_scenes;
	uint16_t eq_scenes;
	fsm_index_t index[];
};
typedef struct dev_list dev_list_t;

struct preset_list {
	uint16_t len;
	uint16_t scene;
	uint32_t data[];
};
typedef struct preset_list preset_list_t;

struct reg_unit {
	uint16_t addr : 8;
	uint16_t pos  : 4;
	uint16_t len  : 4;
	uint16_t value;
};
typedef struct reg_unit reg_unit_t;

struct reg_comm {
	uint16_t len;
	reg_unit_t reg[];
};
typedef struct reg_comm reg_comm_t;

struct regs_unit {
	uint16_t scene;
	reg_unit_t reg;
};
typedef struct regs_unit regs_unit_t;

struct reg_scene {
	uint16_t len;
	regs_unit_t regs[];
};
typedef struct reg_scene reg_scene_t;

struct reg_temp {
	uint16_t new_val;
	uint16_t old_val;
};
typedef struct reg_temp reg_temp_t;

struct preset_header {
	uint16_t version;
	char customer[8];
	char project[8];
	fsm_date_t date;
	uint16_t size;
	uint16_t crc16;
	uint16_t ndev;
};

struct preset_file {
	struct preset_header hdr;
	fsm_index_t index[];
};

#pragma pack(pop)

struct fsm_srate {
	uint32_t srate  : 28;
	uint32_t bf_val : 4;
};

struct fsm_dev;

struct fsm_ops {
	int (*reg_init)(struct fsm_dev *);
	int (*dev_init)(struct fsm_dev *);
	int (*pll_config)(struct fsm_dev *, bool on);
	int (*i2s_config)(struct fsm_dev *);
	int (*switch_preset)(struct fsm_dev *);
	int (*check_stable)(struct fsm_dev *, int type);
	int (*start_up)(struct fsm_dev *);
	int (*set_unmute)(struct fsm_dev *);
	int (*set_tsignal)(struct fsm_dev *, bool enable);
	int (*set_mute)(struct fsm_dev *, bool mute);
	int (*shut_down)(struct fsm_dev *);
	int (*deinit)(struct fsm_dev *);
	int (*set_monitor)(struct fsm_dev *);
	int (*dev_recover)(struct fsm_dev *);
	int (*cal_zmdata)(struct fsm_dev *);
	int (*pre_calib)(struct fsm_dev *);
	int (*store_otp)(struct fsm_dev *, uint8_t valOTP);
	int (*post_calib)(struct fsm_dev *);
	int (*pre_f0_test)(struct fsm_dev *);
	int (*f0_test)(struct fsm_dev *);
	int (*post_f0_test)(struct fsm_dev *);
};
typedef struct fsm_ops fsm_ops_t;

struct fsm_config {
	uint8_t dev_count;
	uint16_t cur_angle;
	uint16_t next_angle;
	uint16_t next_scene;
	uint16_t test_type;
	uint16_t test_freq;
	uint16_t volume;
	uint16_t state     : 8;
	uint16_t wait_type : 8;
	unsigned int i2s_bclk;
	unsigned int i2s_srate;

	// flags
	uint32_t codec_inited : 3;
	uint32_t force_fw     : 1;
	uint32_t force_init   : 1;
	uint32_t force_scene  : 1;
	uint32_t force_calib  : 1;
	uint32_t store_otp    : 1;
	uint32_t force_mute   : 1;
	uint32_t vddd_on      : 1;
	uint32_t reset_chip   : 1;
	uint32_t stop_test    : 1;
	uint32_t skip_monitor : 1;
	uint32_t use_monitor  : 1;
	uint32_t stream_muted : 1;
	uint32_t speaker_on   : 1;
	//uint32_t bypass_dsp : 1;
	uint32_t dev_suspend  : 1;

	char *codec_name[FSM_DEV_MAX];
	char *codec_dai_name[FSM_DEV_MAX];
	char *fw_name;
	struct preset_file *preset;
};
typedef struct fsm_config fsm_config_t;

struct fsm_vbat_state {
	uint16_t last_batv;
	uint16_t cur_batv;
	uint16_t batvh_count;
};

struct fsm_calib {
	uint16_t pos  : 8;
	uint16_t addr : 8;
	uint16_t cal_zm;
	uint8_t count;
	int errcode;
	int re25;
	int f0;
	uint16_t f0_zm[F0_COUNT_MAX];
};

struct fsm_f0_data {
	uint16_t freq;
	uint16_t zmdata;
	uint16_t min_zm;
};

struct fsm_f0_test_data {
	uint16_t count;
	struct fsm_f0_data data[F0_COUNT_MAX];
};

struct fsm_cal_result {
	int ndev;
	uint16_t freq_start;
	uint16_t freq_end;
	uint16_t freq_step;
	struct fsm_calib data[FSM_DEV_MAX];
};

struct re25_data {
	uint16_t count;
	uint16_t pre_val;
	uint16_t min_val;
	uint16_t zmdata;
};

struct f0_data {
	uint16_t count   : 8;
	uint16_t min_idx : 8;
	uint16_t min_zm;
	uint16_t freq[F0_COUNT_MAX];
	uint16_t zmdata[F0_COUNT_MAX];
};

struct fsm_test_data {
	struct re25_data re25;
	struct f0_data f0;
	int test_re25;
	int test_f0;
};

#if !defined(CONFIG_I2C)
struct i2c_client {
	int addr;
};
#endif

struct fsm_msg {
	char *buf;
	uint32_t size;
};
typedef struct fsm_msg fsm_msg_t;

struct fsm_bpcoef {
	int freq;
	uint32_t coef[COEF_LEN];
};

struct fsm_state {
	uint16_t dev_inited : 1;
	uint16_t amp_mute   : 1;
	uint16_t bypass_dsp : 1;
	uint16_t calibrated : 1;
	uint16_t otp_stored : 1;
	uint16_t re25_runin : 1;
	uint16_t f0_runing  : 1;
};

struct fsm_compat {
	uint16_t preset_unit_len;
	uint16_t addr_excer_ram;
	uint16_t otp_max_count;
	uint16_t ACSEQA  : 8;
	uint16_t ACSEQWL : 8;
	uint16_t DACEQA  : 8;
	uint16_t DACEQWL : 8;
	int RS2RL_RATIO;
};

struct fsm_dev {
	struct i2c_client *i2c;
#if defined(CONFIG_FSM_REGMAP)
	struct regmap *regmap;
#endif
#if defined(CONFIG_FSM_STEREO)
	struct list_head list;
#endif
#if defined(__KERNEL__)
	struct workqueue_struct *fsm_wq;
	struct delayed_work init_work;
	struct delayed_work monitor_work;
	struct delayed_work interrupt_work;
	struct mutex i2c_lock;
	int irq_id;
#endif
	struct fsm_ops dev_ops;
	struct preset_file *preset;
	struct dev_list *dev_list;
	struct fsm_state state;
	struct fsm_compat compat;
	struct fsm_test_data *tdata;
	uint16_t bus  : 8;
	uint16_t addr : 8;
	uint16_t version;
	uint16_t ram_scene[FSM_EQ_RAM_MAX];
	uint16_t own_scene;
	uint16_t cur_scene;
	uint16_t cal_count;
	uint16_t tmax;
	uint16_t tcoef;
	uint16_t tsel;
	uint16_t rapp;
	uint16_t rstrim;
	uint16_t pos_mask  : 8; // position
	uint16_t acc_count : 8; // acc-key count

	uint16_t has_codec : 1;
	uint16_t load_fw   : 1;
	uint16_t has_sys   : 1;
	uint16_t use_irq   : 1;
	uint16_t is_1894s  : 1;

	int rst_gpio;
	int irq_gpio;
	int id;
	int spkr;
	int re25;
	int re25_dft;
	int f0;
	int errcode;
	char const *dev_name;
};
typedef struct fsm_dev fsm_dev_t;

#endif
