/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: ren-ting.wang <ren-ting.wang@mediatek.com>
 */
#ifndef CLKBUF_COMMON_H
#define CLKBUF_COMMON_H

#include <linux/compiler_types.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/list.h>

#include "mtk_clkbuf_err.h"

#if defined(pr_fmt)
#undef pr_fmt
#endif /* defined(pr_fmt) */

#define pr_fmt(fmt) "[clkbuf] [%s]:%d: " fmt, __func__, __LINE__

#define CLKBUF_CTL_PHANDLE_NAME			"clkbuf_ctl"
#define ENABLE_PROP_NAME			"mediatek,enable"
#define CLKBUF_STATUS_INFO_SIZE			2048

struct reg_t {
	u32 ofs;
	u32 mask;
	u32 shift;
};

struct base_hw {
	union {
		void __iomem *addr;
		struct regmap *map;
	} base;
	bool enable;
	bool is_pmic;
};

#define SET_REG(reg, offset, msk, bit)			\
	._##reg = {						\
		.ofs = offset,					\
		.mask = msk,					\
		.shift = bit,					\
	},

#define REG_DEFINE_BY_NAME(reg, name)			\
	SET_REG(reg, name ## _ADDR, name ## _MASK, name ## _SHIFT)

#define SET_REG_BY_NAME(reg, name)			\
	SET_REG(reg, name ## _ADDR, name ## _MASK, name ## _SHIFT)

#define SET_REG_BY_VERSION(reg, name, ver)		\
	SET_REG_BY_NAME(reg, name ## _V ## ver)

#define EXTRACT_REG_VAL(val, mask, shift)			\
	(((val) >> (shift)) & (mask))

#if IS_ENABLED(CONFIG_PM)
#define DEFINE_ATTR_RO(_name)					\
static struct kobj_attribute _name##_attr = {			\
	.attr	= {						\
		.name = #_name,					\
		.mode = 0444,					\
	},							\
	.show	= _name##_show,					\
}

#define DEFINE_ATTR_RW(_name)					\
static struct kobj_attribute _name##_attr = {			\
	.attr	= {						\
		.name = #_name,					\
		.mode = 0644,					\
	},							\
	.show	= _name##_show,					\
	.store	= _name##_store,				\
}

#define DEFINE_ATTR_WO(_name)					\
static struct kobj_attribute _name##_attr = {			\
	.attr	= {						\
		.name = #_name,					\
		.mode = 0222,					\
	},							\
	.store	= _name##_store,				\
}

#define __ATTR_OF(_name)	(&_name##_attr.attr)
#endif /* CONFIG_PM */

enum CLKBUF_CHIP_ID {
	MT6765,
	MT6768,
	MT6789,
	MT6833,
	MT6855,
	MT6873,
	MT6879,
	MT6893,
	MT6895,
	MT6983,
	CLKBUF_CHIP_ID_MAX,
};

enum CLKBUF_PMIC_ID {
	MT6357,
	MT6358,
	MT6359P,
	MT6366,
	MT6685,
	CLKBUF_PMIC_ID_MAX,
};

enum DCXO_MODE {
	DCXO_SW_MODE,
	DCXO_HW1_MODE,
	DCXO_HW2_MODE,
	DCXO_CO_BUF_MODE,
	DCXO_MODE_MAX,
};

enum SRCLKEN_RC_REQ {
	RC_NONE_REQ,
	RC_FPM_REQ,
	RC_LPM_REQ,
	RC_LPM_VOTE_REQ,
	RC_MAX_REQ,
};

enum CLKBUF_HW {
	CLKBUF_DCXO,
	CLKBUF_PMIF,
	CLKBUF_RC_VOTER,
	CLKBUF_RC_SUBSYS,
	CLKBUF_HW_ALL,
	CLKBUF_HW_MAX,
};

enum CLKBUF_CTL_CMD {
	CLKBUF_CMD_OFF,
	CLKBUF_CMD_ON,
	CLKBUF_CMD_SW,
	CLKBUF_CMD_HW,
	CLKBUF_CMD_INIT,
	CLKBUF_CMD_SHOW,
	CLKBUF_CMD_NOOP,
	CLKBUF_CMD_MAX,
};

struct xo_buf_ctl_cmd_t {
	enum CLKBUF_CTL_CMD cmd;
	enum CLKBUF_HW hw_id;
	enum DCXO_MODE mode;
	enum SRCLKEN_RC_REQ rc_req;
	u32 xo_voter_mask;
	char *buf;
};

struct xo_buf_ctl_t {
	struct list_head xo_buf_ctl_list;
	int (*clk_buf_ctrl)(u8 xo_idx, struct xo_buf_ctl_cmd_t *ctl_cmd,
			struct xo_buf_ctl_t *xo_buf_ctl);
	int (*clk_buf_on_ctrl)(u8 xo_idx, struct xo_buf_ctl_cmd_t *ctl_cmd,
			struct xo_buf_ctl_t *xo_buf_ctl);
	int (*clk_buf_off_ctrl)(u8 xo_idx, struct xo_buf_ctl_cmd_t *ctl_cmd,
			struct xo_buf_ctl_t *xo_buf_ctl);
	int (*clk_buf_sw_ctrl)(u8 xo_idx, struct xo_buf_ctl_cmd_t *ctl_cmd,
			struct xo_buf_ctl_t *xo_buf_ctl);
	int (*clk_buf_hw_ctrl)(u8 xo_idx, struct xo_buf_ctl_cmd_t *ctl_cmd,
			struct xo_buf_ctl_t *xo_buf_ctl);
	int (*clk_buf_init_ctrl)(u8 xo_idx, struct xo_buf_ctl_cmd_t *ctl_cmd,
			struct xo_buf_ctl_t *xo_buf_ctl);
	int (*clk_buf_show_ctrl)(u8 xo_idx, struct xo_buf_ctl_cmd_t *ctl_cmd,
			struct xo_buf_ctl_t *xo_buf_ctl);
};

int clk_buf_read_with_ofs(struct base_hw *hw, struct reg_t *reg,
	u32 *val, u32 ofs);
int clk_buf_write_with_ofs(struct base_hw *hw, struct reg_t *reg,
	u32 val, u32 ofs);

static inline int clk_buf_read(struct base_hw *hw, struct reg_t *reg, u32 *val)
{
	return clk_buf_read_with_ofs(hw, reg, val, 0);
}

static inline int clk_buf_write(struct base_hw *hw, struct reg_t *reg, u32 val)
{
	return clk_buf_write_with_ofs(hw, reg, val, 0);
}

int clk_buf_register_xo_ctl_op(const char *xo_name,
	struct xo_buf_ctl_t *xo_buf_ctl);

extern const char *clk_buf_get_xo_name(u8 idx);
extern u8 clk_buf_get_xo_num(void);

enum CLKBUF_CHIP_ID clk_buf_get_chip_id(void);
enum CLKBUF_PMIC_ID clk_buf_get_pmic_id(struct platform_device *clkbuf_pdev);

#endif /* CLKBUF_COMMON_H */
