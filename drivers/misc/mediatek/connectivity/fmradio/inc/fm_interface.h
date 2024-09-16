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

#ifndef __FM_INTERFACE_H__
#define __FM_INTERFACE_H__

#include <linux/cdev.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>

#include "fm_typedef.h"
#include "fm_rds.h"
#include "fm_utils.h"

/******************************************************************************
 * STRUCTURE DEFINITIONS
 *****************************************************************************/
enum fm_op_state {
	FM_STA_STOP = 0,
	FM_STA_PLAY = 1,
	FM_STA_TUNE = 2,
	FM_STA_SEEK = 3,
	FM_STA_SCAN = 4,
	FM_STA_RAMPDOWN = 5,
	FM_STA_UNKNOWN = 100,
	FM_STA_MAX
};

enum fm_pwr_state {
	FM_PWR_OFF = 0,
	FM_PWR_RX_ON = 1,
	FM_PWR_TX_ON = 2,
	FM_PWR_MAX
};

enum fm_antenna_type {
	FM_ANA_LONG = 0,	/* long antenna */
	FM_ANA_SHORT = 1,	/* short antenna */
	FM_ANA_MAX
};

enum fm_gps_desense_t {
	FM_GPS_DESE_ENABLE,
	FM_GPS_DESE_DISABLE
};

struct fm_hw_info {
	signed int chip_id;		/* chip ID, eg. 6620 */
	signed int eco_ver;		/* chip ECO version, eg. E3 */
	signed int rom_ver;		/* FM DSP rom code version, eg. V2 */
	signed int patch_ver;	/* FM DSP patch version, eg. 1.11 */
	signed int reserve;
};

struct fm_i2s_setting {
	signed int onoff;
	signed int mode;
	signed int sample;
};

enum fm_i2s_state_e {
	FM_I2S_ON = 0,
	FM_I2S_OFF,
	FM_I2S_STATE_ERR
};

enum fm_i2s_mode_e {
	FM_I2S_MASTER = 0,
	FM_I2S_SLAVE,
	FM_I2S_MODE_ERR
};

enum fm_i2s_sample_e {
	FM_I2S_32K = 0,
	FM_I2S_44K,
	FM_I2S_48K,
	FM_I2S_SR_ERR
};

struct fm_i2s_info {
	signed int status;		/*0:FM_I2S_ON, 1:FM_I2S_OFF,2:error */
	signed int mode;		/*0:FM_I2S_MASTER, 1:FM_I2S_SLAVE,2:error */
	signed int rate;		/*0:FM_I2S_32K:32000,1:FM_I2S_44K:44100,2:FM_I2S_48K:48000,3:error */
};

enum fm_audio_path_e {
	FM_AUD_ANALOG = 0,
	FM_AUD_I2S = 1,
	FM_AUD_MRGIF = 2,
	FM_AUD_ERR
};

enum fm_i2s_pad_sel_e {
	FM_I2S_PAD_CONN = 0,	/* sco fm chip: e.g.6627 */
	FM_I2S_PAD_IO = 1,	/* combo fm chip: e.g.6628 */
	FM_I2S_PAD_ERR
};

struct fm_audio_info_t {
	enum fm_audio_path_e aud_path;
	struct fm_i2s_info i2s_info;
	enum fm_i2s_pad_sel_e i2s_pad;
};

struct fm_platform {
	struct cdev cdev;
	dev_t dev_t;
	struct class *cls;
	struct device *dev;
};

struct fm {
	/* chip info */
	signed int projectid;
	unsigned short chip_id;		/* chip id, such as 6616/6620/6626/6628 */
	unsigned short device_id;	/* chip version */
	/* basic run time info */
	signed int ref;		/* fm driver can be multi opened */
	bool chipon;		/* Chip power state */
	enum fm_pwr_state pwr_sta;	/* FM module power state */
	enum fm_op_state op_sta;	/* current operation state: tune, seek, scan ... */
	/* enum fm_audio_path aud_path;    //I2S or Analog */
	signed int vol;		/* current audio volume from chip side */
	bool mute;		/* true: mute, false: playing */
	bool rds_on;		/* true: on, false: off */
	enum fm_antenna_type ana_type;	/* long/short antenna */
	bool via_bt;		/* true: fm over bt controller; false: fm over host */
	unsigned short min_freq;	/* for UE, 875KHz */
	unsigned short max_freq;	/* for UE, 1080KHz */
	unsigned short cur_freq;	/* current frequency */
	unsigned char band;		/* UE/JAPAN/JPANWD */
	/*FM Tx */
	unsigned int vcoon;		/* TX VCO tracking ON duiration(ms) */
	unsigned int vcooff;		/* TX RTC VCO tracking interval(s) */
	unsigned int txpwrctl;	/* TX power contrl interval(s) */
	unsigned int tx_pwr;
	bool rdstx_on;	/* false:rds tx off, true:rds tx on */
	bool wholechiprst;
	/* RDS data */
	struct fm_flag_event *rds_event;	/* pointer to rds event */
	struct rds_t *pstRDSData;	/* rds spec data buffer */
	/* platform data */
	struct fm_platform platform;	/* platform related members */

	struct fm_workthread *eint_wkthd;
	struct fm_workthread *timer_wkthd;
	struct fm_work *eint_wk;
	struct fm_work *rds_wk;
	struct fm_work *rst_wk;	/* work for subsystem reset */
	struct fm_work *pwroff_wk;
	struct fm_work *ch_valid_check_wk;
	/* Tx */
	struct fm_work *fm_tx_desense_wifi_work;
	struct fm_work *fm_tx_power_ctrl_work;

};

struct fm_callback {
	/* call backs */
	unsigned short (*cur_freq_get)(void);
	signed int (*cur_freq_set)(unsigned short new_freq);
	signed int (*projectid_get)(void);
/* unsigned short(*chan_para_get)(unsigned short freq);    //get channel parameter, HL side/ FA / ATJ */
};

struct fm_basic_interface {
	/* mt66x6 lib interfaces */
	signed int (*low_pwr_wa)(signed int onoff);
	signed int (*pwron)(signed int data);
	signed int (*pwroff)(signed int data);
	signed int (*pmic_read)(unsigned char addr, unsigned int *val);
	signed int (*pmic_write)(unsigned char addr, unsigned int val);
	unsigned short (*chipid_get)(void);
	signed int (*mute)(bool mute);
	signed int (*rampdown)(void);
	signed int (*pwrupseq)(unsigned short *chip_id, unsigned short *device_id);
	signed int (*pwrdownseq)(void);
	bool (*setfreq)(unsigned short freq);
	bool (*seek)(unsigned short min_freq, unsigned short max_freq, unsigned short *freq, unsigned short dir,
				unsigned short space);
	signed int (*seekstop)(void);
	bool (*scan)(unsigned short min_freq, unsigned short max_freq, unsigned short *freq, unsigned short *tbl,
				unsigned short *tblsize, unsigned short dir, unsigned short space);
	bool (*jammer_scan)(unsigned short min_freq, unsigned short max_freq, unsigned short *freq, unsigned short *tbl,
				unsigned short *tblsize, unsigned short dir, unsigned short space);
	signed int (*cqi_get)(signed char *buf, signed int buf_len);
	signed int (*scanstop)(void);
	signed int (*rssiget)(signed int *rssi);
	signed int (*volset)(unsigned char vol);
	signed int (*volget)(unsigned char *vol);
	signed int (*dumpreg)(void);
	bool (*msget)(unsigned short *ms);	/* mono/stereo indicator get */
	signed int (*msset)(signed int ms);	/* mono/stereo force set */
	bool (*pamdget)(unsigned short *pamd);
	bool (*em)(unsigned short group, unsigned short item, unsigned int val);
	signed int (*anaswitch)(signed int ana);
	signed int (*anaget)(void);
	signed int (*caparray_get)(signed int *ca);
	signed int (*i2s_set)(signed int onoff, signed int mode, signed int sample);
	signed int (*i2s_get)(signed int *ponoff, signed int *pmode, signed int *psample);
	signed int (*hwinfo_get)(struct fm_hw_info *req);
	/* check if this is a de-sense channel */
	signed int (*is_dese_chan)(unsigned short freq);
	signed int (*softmute_tune)(unsigned short freq, signed int *rssi, signed int *valid);
	signed int (*pre_search)(void);
	signed int (*restore_search)(void);
	/* check if this is a valid channel */
	signed int (*desense_check)(unsigned short freq, signed int rssi);
	signed int (*get_freq_cqi)(unsigned short freq, signed int *cqi);
	/* cqi log tool */
	signed int (*cqi_log)(signed int min_freq, signed int max_freq, signed int space, signed int cnt);
	signed int (*fm_via_bt)(bool flag);	/* fm over BT:1:enable,0:disable */
	signed int (*set_search_th)(signed int idx, signed int val, signed int reserve);
	signed int (*get_aud_info)(struct fm_audio_info_t *data);
	/*tx function */
	signed int (*tx_support)(signed int *sup);
	signed int (*rdstx_enable)(signed int *flag);
	bool (*tune_tx)(unsigned short freq);
	signed int (*pwrupseq_tx)(void);
	signed int (*pwrdownseq_tx)(void);
	signed int (*tx_pwr_ctrl)(unsigned short freq, signed int *ctr);
	signed int (*rtc_drift_ctrl)(unsigned short freq, signed int *ctr);
	signed int (*tx_desense_wifi)(unsigned short freq, signed int *ctr);
	signed int (*tx_scan)(unsigned short min_freq, unsigned short max_freq, unsigned short *pFreq,
							unsigned short *pScanTBL, unsigned short *ScanTBLsize,
							unsigned short scandir, unsigned short space);
	signed int (*rds_tx_adapter)(unsigned short pi, unsigned short *ps, unsigned short *other_rds,
							unsigned char other_rds_cnt);
	bool (*is_valid_freq)(unsigned short freq);

};

struct fm_rds_interface {
	/* rds lib interfaces */
	signed int (*rds_blercheck)(struct rds_t *dst);
	bool (*rds_onoff)(struct rds_t *dst, bool onoff);
	signed int (*rds_parser)(struct rds_t *rds_dst, struct rds_rx_t *rds_raw,
							signed int rds_size, unsigned short(*getfreq) (void));
	unsigned short (*rds_gbc_get)(void);	/* good block counter */
	unsigned short (*rds_bbc_get)(void);	/* bad block counter */
	unsigned char (*rds_bbr_get)(void);	/* bad block ratio */
	signed int (*rds_bc_reset)(void);	/* reset block counter */
	unsigned int (*rds_bci_get)(void);	/* bler check interval */
	signed int (*rds_log_get)(struct rds_rx_t *dst, signed int *dst_len);
	signed int (*rds_gc_get)(struct rds_group_cnt_t *dst, struct rds_t *rdsp);
	signed int (*rds_gc_reset)(struct rds_t *rdsp);
	/*Tx */
	signed int (*rds_tx)(unsigned short pi, unsigned short *ps, unsigned short *other_rds,
							unsigned char other_rds_cnt);
	signed int (*rds_tx_enable)(void);
	signed int (*rds_tx_disable)(void);
	signed int (*rdstx_support)(signed int *sup);
};

struct fm_lowlevel_ops {
	struct fm_callback cb;
	struct fm_basic_interface bi;
	struct fm_rds_interface ri;
};

extern signed int fm_low_ops_register(struct fm_callback *cb, struct fm_basic_interface *bi);
extern signed int fm_low_ops_unregister(struct fm_basic_interface *bi);
extern signed int fm_rds_ops_register(struct fm_basic_interface *bi, struct fm_rds_interface *ri);
extern signed int fm_rds_ops_unregister(struct fm_rds_interface *ri);
extern signed int fm_wcn_ops_register(void);
extern signed int fm_wcn_ops_unregister(void);
extern int fm_register_irq(struct platform_driver *drv);

/*
 * fm_get_channel_space - get the spcace of gived channel
 * @freq - value in 760~1080 or 7600~10800
 *
 * Return 0, if 760~1080; return 1, if 7600 ~ 10800, else err code < 0
 */

extern signed int fm_get_channel_space(int freq);
#endif /* __FM_INTERFACE_H__ */
