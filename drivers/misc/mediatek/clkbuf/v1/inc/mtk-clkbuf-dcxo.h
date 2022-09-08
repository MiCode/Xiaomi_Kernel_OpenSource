/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: ren-ting.wang <ren-ting.wang@mediatek.com>
 */
#ifndef CLKBUF_DCXO_H
#define CLKBUF_DCXO_H

#include "mtk_clkbuf_common.h"

struct platform_device;

struct xo_buf_t {
	const char *xo_name;
	struct list_head xo_buf_ctl_head;
	struct reg_t _xo_mode;
	struct reg_t _xo_en;
	struct reg_t _xo_en_auxout;
	struct reg_t _hwbblpm_msk;
	struct reg_t _impedance;
	struct reg_t _de_sense;
	struct reg_t _drv_curr;
	struct reg_t _xo_drv_curr_auxout;
	struct reg_t _rc_voter;
	struct xo_buf_ctl_t xo_voter_ctrl_op;
	u16 init_rc_voter;
	struct xo_buf_ctl_t xo_buf_ctrl_op;
	u8 init_mode : 2;
	u8 dct_sta : 2;
	u8 init_en : 1;
	u8 support : 1;
	u8 controllable : 1;
	u8 sw_status : 1;
	u8 in_use : 1;
	u8 hwbblpm_msk : 1;
	u8 hwbblpm_bypass : 1;
	u8 dct_impedance : 3;
	u8 dct_desense : 3;
	u8 dct_drv_curr : 2;
	u32 xo_drv_curr_auxout_sel;
	u32 xo_en_auxout_sel;
};

struct dcxo_op {
	int (*dcxo_dump_reg_log)(char *buf);
	int (*dcxo_dump_misc_log)(char *buf);
	int (*dcxo_misc_store)(const char *obj, const char *arg);
	int (*dcxo_pmic_store)(const u8 xo_id, const char *cmd);
};

struct pmic_pmrc_en {
	struct base_hw hw;
	struct reg_t _pmrc_en;
};

struct dcxo_hw {
	struct base_hw hw;
	u8 xo_num;
	u8 xo_mode_num;
	u8 pmrc_en_num;
	u32 bblpm_auxout_sel;
	bool bblpm_support;
	bool hwbblpm_support;
	bool impedance_support;
	bool de_sense_support;
	bool drv_curr_support;
	bool voter_support;
	bool pmrc_en_support;
	bool do_init_in_k;
	bool spmi_rw;
	struct mutex lock;
	struct xo_buf_t *xo_bufs;
	struct reg_t _static_aux_sel;
	struct reg_t _bblpm_auxout;
	struct reg_t _swbblpm_en;
	struct reg_t _hwbblpm_sel;
	struct reg_t _srclken_i3;
	struct reg_t _dcxo_pmrc_en;
	struct reg_t _xo_cdac_fpm;
	struct reg_t _xo_aac_fpm_swen;
	struct reg_t _xo_heater_sel;
	struct pmic_pmrc_en *pmrc_en;
	struct dcxo_op ops;
	const char * const *valid_dcxo_cmd;
};

int clkbuf_dcxo_init(struct platform_device *pdev);
int clkbuf_dcxo_post_init(void);
int clkbuf_xo_sanity_check(u8 xo_idx);
int clkbuf_dcxo_get_xo_en(u8 idx, u32 *en);
bool clkbuf_dcxo_get_xo_support(u8 idx);
bool clkbuf_dcxo_get_xo_controllable(u8 idx);
bool clkbuf_dcxo_is_xo_in_use(u8 idx);
int clkbuf_dcxo_set_xo_sw_en(u8 idx, u8 en);
bool clkbuf_dcxo_get_xo_sw_en(u8 idx);
int clkbuf_dcxo_register_op(u8 idx, struct xo_buf_ctl_t *xo_buf_ctl);
int clkbuf_dcxo_notify(u8 xo_idx, struct xo_buf_ctl_cmd_t *ctl_cmd);
int clkbuf_dcxo_get_xo_num(void);
const char *clkbuf_dcxo_get_xo_name(u8 idx);
int clkbuf_dcxo_get_xo_id_by_name(const char *xo_name);
int clkbuf_dcxo_pmic_store(const char *cmd, const char *arg1, const char *arg2);
int clkbuf_dcxo_debug_store(const char *cmd, const u8 xo_idx);
int clkbuf_dcxo_get_hwbblpm_sel(u32 *en);
int clkbuf_dcxo_get_bblpm_en(u32 *bblpm_en);
int clkbuf_dcxo_get_xo_mode(u8 xo_idx, u32 *mode);
int clkbuf_dcxo_dump_rc_voter_log(char *buf);
int clkbuf_dcxo_dump_reg_log(char *buf);
int clkbuf_dcxo_dump_misc_log(char *buf);
int clkbuf_dcxo_dump_dws(char *buf);
int clkbuf_dcxo_dump_pmrc_en(char *buf);
int clkbuf_dcxo_set_capid_pre(void);
int clkbuf_dcxo_get_capid(u32 *capid);
int clkbuf_dcxo_set_capid(u32 capid);
int clkbuf_dcxo_get_heater(bool *on);
int clkbuf_dcxo_set_heater(bool on);

/* Get platform dcxo structures */
extern struct dcxo_hw mt6359p_dcxo;
extern struct dcxo_hw mt6685_dcxo;
extern struct dcxo_hw mt6366_dcxo;
extern struct dcxo_hw mt6358_dcxo;
extern struct dcxo_hw mt6357_dcxo;
#endif /* CLKBUF_DCXO_H */
