/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __FM_MAIN_H__
#define __FM_MAIN_H__

#include "fm_typedef.h"
#include "fm_dbg.h"
#include "fm_err.h"
#include "fm_rds.h"
#include "fm_eint.h"
#include "fm_link.h"
#include "fm_interface.h"
#include "fm_stdlib.h"
#include "fm_config.h"

#define FM_NAME             "fm"
#define FM_DEVICE_NAME      "/dev/fm"

#define FM_VOL_MAX           0x2B	/* 43 volume(0-15) */
#define FM_TIMER_TIMEOUT_DEFAULT 1000
#define FM_TIMER_TIMEOUT_MIN 1000
#define FM_TIMER_TIMEOUT_MAX 1000000
/* FM Tx */
#define FM_TX_PWR_LEVEL_MAX  120	/* FM transmitter power level, rang: 85db~120db, default 120db */

#define FM_TX_PWR_CTRL_INVAL_DEFAULT 10
#define FM_TX_PWR_CTRL_INVAL_MIN 5
#define FM_TX_PWR_CTRL_INVAL_MAX 10000

#define FM_TX_VCO_OFF_DEFAULT 5
#define FM_TX_VCO_OFF_MIN 1
#define FM_TX_VCO_OFF_MAX 10000

#define FM_TX_VCO_ON_DEFAULT 100
#define FM_TX_VCO_ON_MIN 10
#define FM_TX_VCO_ON_MAX 10000

#define FM_GPS_RTC_AGE_TH       2
#define FM_GPS_RTC_DRIFT_TH     0
#define FM_GPS_RTC_TIME_DIFF_TH 10
#define FM_GPS_RTC_RETRY_CNT    1
#define FM_GPS_RTC_DRIFT_MAX 5000
enum {
	FM_GPS_RTC_INFO_OLD = 0,
	FM_GPS_RTC_INFO_NEW = 1,
	FM_GPS_RTC_INFO_MAX
};

enum fm_over_bt_enable_state {
	FM_OVER_BT_DISABLE = 0,
	FM_OVER_BT_ENABLE
};

#define FM_RDS_ENABLE		0x01	/* 1: enable RDS, 0:disable RDS */
#define FM_RDS_DATA_READY   (1 << 0)

/* errno */
#define FM_SUCCESS      0
#define FM_FAILED       1
#define FM_EPARM        2
#define FM_BADSTATUS    3
#define FM_TUNE_FAILED  4
#define FM_SEEK_FAILED  5
#define FM_BUSY         6
#define FM_SCAN_FAILED  7

struct fm_tune_parm {
	unsigned char err;
	unsigned char band;
	unsigned char space;
	unsigned char hilo;
	unsigned char deemphasis;
	unsigned short freq;		/* IN/OUT parameter */
};

struct fm_tune_parm_old {
	unsigned char err;
	unsigned char band;
	unsigned char space;
	unsigned char hilo;
	unsigned short freq;		/* IN/OUT parameter */
};

struct fm_seek_parm {
	unsigned char err;
	unsigned char band;
	unsigned char space;
	unsigned char hilo;
	unsigned char seekdir;
	unsigned char seekth;
	unsigned short freq;		/* IN/OUT parameter */
};

struct fm_scan_parm {
	unsigned char err;
	unsigned char band;
	unsigned char space;
	unsigned char hilo;
	unsigned short freq;		/* OUT parameter */
	unsigned short ScanTBL[26];	/* need no less than the chip */
	unsigned short ScanTBLSize;	/* IN/OUT parameter */
};

struct fm_cqi {
	signed int ch;
	signed int rssi;
	signed int reserve;
};

struct fm_cqi_req {
	unsigned short ch_num;
	signed int buf_size;
	signed char *cqi_buf;
};

struct fm_ch_rssi {
	unsigned short freq;
	signed int rssi;
};

enum fm_scan_cmd_t {
	FM_SCAN_CMD_INIT = 0,
	FM_SCAN_CMD_START,
	FM_SCAN_CMD_GET_NUM,
	FM_SCAN_CMD_GET_CH,
	FM_SCAN_CMD_GET_RSSI,
	FM_SCAN_CMD_GET_CH_RSSI,
	FM_SCAN_CMD_MAX
};

struct fm_scan_t {
	enum fm_scan_cmd_t cmd;
	signed int ret;		/* 0, success; else error code */
	unsigned short lower;		/* lower band, Eg, 7600 -> 76.0Mhz */
	unsigned short upper;		/* upper band, Eg, 10800 -> 108.0Mhz */
	signed int space;		/* 5: 50KHz, 10: 100Khz, 20: 200Khz */
	signed int num;		/* valid channel number */
	void *priv;
	signed int sr_size;		/* scan result buffer size in bytes */
	union {
		unsigned short *ch_buf;	/* channel buffer */
		signed int *rssi_buf;	/* rssi buffer */
		struct fm_ch_rssi *ch_rssi_buf;	/* channel and RSSI buffer */
	} sr;
};

struct fm_seek_t {
	signed int ret;		/* 0, success; else error code */
	unsigned short freq;
	unsigned short lower;		/* lower band, Eg, 7600 -> 76.0Mhz */
	unsigned short upper;		/* upper band, Eg, 10800 -> 108.0Mhz */
	signed int space;		/* 5: 50KHz, 10: 100Khz, 20: 200Khz */
	signed int dir;		/* 0: up; 1: down */
	signed int th;		/* seek threshold in dbm(Eg, -95dbm) */
	void *priv;
};

struct fm_tune_t {
	signed int ret;		/* 0, success; else error code */
	unsigned short freq;
	unsigned short lower;		/* lower band, Eg, 7600 -> 76.0Mhz */
	unsigned short upper;		/* upper band, Eg, 10800 -> 108.0Mhz */
	signed int space;		/* 5: 50KHz, 10: 100Khz, 20: 200Khz */
	void *priv;
};

struct fm_rssi_req {
	unsigned short num;
	unsigned short read_cnt;
	struct fm_ch_rssi cr[26 * 16];
};

struct fm_rds_tx_parm {
	unsigned char err;
	unsigned short pi;
	unsigned short ps[12];		/* 4 ps */
	unsigned short other_rds[87];	/* 0~29 other groups */
	unsigned char other_rds_cnt;	/* # of other group */
};

struct fm_rds_tx_req {
	unsigned char pty;	/* 0~31 integer */
	unsigned char rds_rbds;	/* 0:RDS, 1:RBDS */
	unsigned char dyn_pty;	/* 0:static, 1:dynamic */
	unsigned short pi_code;	/* 2-byte hex */
	unsigned char ps_buf[8];	/* hex buf of PS */
	unsigned char ps_len;	/* length of PS, must be 0 / 8" */
	unsigned char af;	/* 0~204, 0:not used, 1~204:(87.5+0.1*af)MHz */
	unsigned char ah;	/* Artificial head, 0:no, 1:yes */
	unsigned char stereo;	/* 0:mono, 1:stereo */
	unsigned char compress;	/* Audio compress, 0:no, 1:yes */
	unsigned char tp;	/* traffic program, 0:no, 1:yes */
	unsigned char ta;	/* traffic announcement, 0:no, 1:yes */
	unsigned char speech;	/* 0:music, 1:speech */
};

#define TX_SCAN_MAX 10
#define TX_SCAN_MIN 1

struct fm_tx_scan_parm {
	unsigned char err;
	unsigned char band;		/* 87.6~108MHz */
	unsigned char space;
	unsigned char hilo;
	unsigned short freq;		/* start freq, if less than band min freq, then will use band min freq */
	unsigned char scandir;
	unsigned short ScanTBL[TX_SCAN_MAX];	/* need no less than the chip */
	unsigned short ScanTBLSize;	/* IN: desired size, OUT: scan result size */
};

struct fm_gps_rtc_info {
	signed int err;		/* error number, 0: success, other: err code */
	signed int retryCnt;	/* GPS mnl can decide retry times */
	signed int ageThd;		/* GPS 3D fix time diff threshold */
	signed int driftThd;	/* GPS RTC drift threshold */
	struct timeval tvThd;	/* time value diff threshold */
	signed int age;		/* GPS 3D fix time diff */
	signed int drift;		/* GPS RTC drift */
	union {
		unsigned long stamp;	/* time stamp in jiffies */
		struct timeval tv;	/* time stamp value in RTC */
	};
	signed int flag;		/* rw flag */
};

struct fm_desense_check_t {
	signed int freq;
	signed int rssi;
};

struct fm_full_cqi_log_t {
	uint16_t lower;		/* lower band, Eg, 7600 -> 76.0Mhz */
	uint16_t upper;		/* upper band, Eg, 10800 -> 108.0Mhz */
	int space;		/* 0x1: 50KHz, 0x2: 100Khz, 0x4: 200Khz */
	int cycle;		/* repeat times */
};

enum {
	FM_RX = 0,
	FM_TX = 1
};

struct fm_ctl_parm {
	unsigned char err;
	unsigned char addr;
	unsigned short val;
	unsigned short rw_flag;		/* 0:write, 1:read */
};
struct fm_em_parm {
	unsigned short group_idx;
	unsigned short item_idx;
	unsigned int item_value;
};
struct fm_top_rw_parm {
	unsigned char err;
	unsigned char rw_flag;		/* 0:write, 1:read */
	unsigned short addr;
	unsigned int val;
};
struct fm_host_rw_parm {
	unsigned char err;
	unsigned char rw_flag;		/* 0:write, 1:read */
	unsigned int addr;
	unsigned int val;
};
struct fm_pmic_rw_parm {
	unsigned char err;
	unsigned char rw_flag;		/* 0:write, 1:read */
	unsigned char addr;
	unsigned int val;
};
enum {
	FM_SUBSYS_RST_OFF,
	FM_SUBSYS_RST_START,
	FM_SUBSYS_RST_END,
	FM_SUBSYS_RST_MAX
};
enum {
	FM_TX_PWR_CTRL_DISABLE,
	FM_TX_PWR_CTRL_ENABLE,
	FM_TX_PWR_CTRL_MAX
};

enum {
	FM_TX_RTC_CTRL_DISABLE,
	FM_TX_RTC_CTRL_ENABLE,
	FM_TX_RTC_CTRL_MAX
};

enum {
	FM_TX_DESENSE_DISABLE,
	FM_TX_DESENSE_ENABLE,
	FM_TX_DESENSE_MAX
};

struct fm_softmute_tune_t {
	signed int rssi;		/* RSSI of current channel */
	unsigned short freq;		/* current frequency */
	signed int valid;		/* current channel is valid(true) or not(false) */
};
struct fm_search_threshold_t {
	signed int th_type;		/* 0, RSSI. 1,desense RSSI. 2,SMG. */
	signed int th_val;		/* threshold value */
	signed int reserve;
};

struct fm_status_t {
	int which;
	bool stat;
};

struct fm_chip_mapping {
	unsigned short con_chip;
	unsigned short fm_chip;
	enum fm_cfg_chip_type type;
};

/* init and deinit APIs */
extern signed int fm_env_setup(void);
extern signed int fm_env_destroy(void);
extern struct fm *fm_dev_init(unsigned int arg);
extern signed int fm_dev_destroy(struct fm *fm);

/* fm main basic APIs */
extern enum fm_pwr_state fm_pwr_state_get(struct fm *fmp);
extern enum fm_pwr_state fm_pwr_state_set(struct fm *fmp, enum fm_pwr_state sta);
extern signed int fm_open(struct fm *fmp);
extern signed int fm_close(struct fm *fmp);
extern signed int fm_rds_read(struct fm *fmp, signed char *dst, signed int len);
extern signed int fm_powerup(struct fm *fm, struct fm_tune_parm *parm);
extern signed int fm_powerdown(struct fm *fm, int type);
extern signed int fm_cqi_get(struct fm *fm, signed int ch_num, signed char *buf, signed int buf_size);
extern signed int fm_get_hw_info(struct fm *pfm, struct fm_hw_info *req);
extern signed int fm_hwscan_stop(struct fm *fm);
extern signed int fm_ana_switch(struct fm *fm, signed int antenna);
extern signed int fm_setvol(struct fm *fm, unsigned int vol);
extern signed int fm_getvol(struct fm *fm, unsigned int *vol);
extern signed int fm_mute(struct fm *fm, unsigned int bmute);
extern signed int fm_getrssi(struct fm *fm, signed int *rssi);
extern signed int fm_read(struct fm *fm, unsigned char addr, unsigned short *val);
extern signed int fm_write(struct fm *fm, unsigned char addr, unsigned short val);
extern signed int fm_top_read(struct fm *fm, unsigned short addr, unsigned int *val);
extern signed int fm_top_write(struct fm *fm, unsigned short addr, unsigned int val);
extern signed int fm_host_read(struct fm *fm, unsigned int addr, unsigned int *val);
extern signed int fm_host_write(struct fm *fm, unsigned int addr, unsigned int val);
extern signed int fm_pmic_read(struct fm *fm, unsigned char addr, unsigned int *val);
extern signed int fm_pmic_write(struct fm *fm, unsigned char addr, unsigned int val);
extern signed int fm_chipid_get(struct fm *fm, unsigned short *chipid);
extern signed int fm_monostereo_get(struct fm *fm, unsigned short *ms);
extern signed int fm_monostereo_set(struct fm *fm, signed int ms);
extern signed int fm_pamd_get(struct fm *fm, unsigned short *pamd);
extern signed int fm_caparray_get(struct fm *fm, signed int *ca);
extern signed int fm_em_test(struct fm *fm, unsigned short group, unsigned short item, unsigned int val);
extern signed int fm_rds_onoff(struct fm *fm, unsigned short rdson_off);
extern signed int fm_rds_good_bc_get(struct fm *fm, unsigned short *gbc);
extern signed int fm_rds_bad_bc_get(struct fm *fm, unsigned short *bbc);
extern signed int fm_rds_bler_ratio_get(struct fm *fm, unsigned short *bbr);
extern signed int fm_rds_group_cnt_get(struct fm *fm, struct rds_group_cnt_t *dst);
extern signed int fm_rds_group_cnt_reset(struct fm *fm);
extern signed int fm_rds_log_get(struct fm *fm, struct rds_rx_t *dst, signed int *dst_len);
extern signed int fm_rds_block_cnt_reset(struct fm *fm);
extern signed int fm_i2s_set(struct fm *fm, signed int onoff, signed int mode, signed int sample);
extern signed int fm_get_i2s_info(struct fm *pfm, struct fm_i2s_info *req);
extern signed int fm_tune(struct fm *fm, struct fm_tune_parm *parm);
extern signed int fm_is_dese_chan(struct fm *pfm, unsigned short freq);
extern signed int fm_desense_check(struct fm *pfm, unsigned short freq, signed int rssi);
extern signed int fm_sys_state_get(struct fm *fmp);
extern signed int fm_sys_state_set(struct fm *fmp, signed int sta);
extern signed int fm_set_stat(struct fm *fmp, int which, bool stat);
extern signed int fm_get_stat(struct fm *fmp, int which, bool *stat);
extern signed int fm_subsys_reset(struct fm *fm);
extern signed int fm_cqi_log(void);
extern signed int fm_soft_mute_tune(struct fm *fm, struct fm_softmute_tune_t *parm);
extern signed int fm_pre_search(struct fm *fm);
extern signed int fm_restore_search(struct fm *fm);

extern signed int fm_dump_reg(void);
extern signed int fm_get_gps_rtc_info(struct fm_gps_rtc_info *src);
extern signed int fm_over_bt(struct fm *fm, signed int flag);
extern signed int fm_set_search_th(struct fm *fm, struct fm_search_threshold_t parm);
extern signed int fm_get_aud_info(struct fm_audio_info_t *data);
/*tx function*/
extern signed int fm_tx_support(struct fm *fm, signed int *support);

extern signed int fm_powerup_tx(struct fm *fm, struct fm_tune_parm *parm);
extern signed int fm_tune_tx(struct fm *fm, struct fm_tune_parm *parm);
extern signed int fm_powerdowntx(struct fm *fm);
extern signed int fm_rds_tx(struct fm *fm, struct fm_rds_tx_parm *parm);
extern signed int fm_rdstx_support(struct fm *fm, signed int *support);
extern signed int fm_rdstx_enable(struct fm *fm, signed int enable);
extern signed int fm_tx_scan(struct fm *fm, struct fm_tx_scan_parm *parm);
signed int fm_full_cqi_logger(struct fm_full_cqi_log_t *setting);
signed int fm_rds_parser(struct rds_rx_t *rds_raw, signed int rds_size);

#endif /* __FM_MAIN_H__ */
