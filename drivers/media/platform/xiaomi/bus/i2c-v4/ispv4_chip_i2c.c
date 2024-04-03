// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, Xiaomi, Inc. All rights reserved.
 */

#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/xm_regops.h>
#include <linux/delay.h>

// TODO: change to use FIELD()
#define ISPV4_FPGA_CORE_CLK 40000000
#define ISPV4_I2C0_BASE 0x02090000
#define I2CIP_ENABLE_REG_OFFSET 0x6c
#define I2CIP_ENABLE_SHIFT 0
#define I2CIP_ENABLE_MASK ((0x1) << I2CIP_ENABLE_SHIFT)
#define I2CIP_CTRL_REG_OFFSET 0x0
#define I2CIP_TARGET_ADDRESS_REG_OFFSET 0x04
#define I2CIP_TARGET_ADDRESS_SHIFT 0
#define I2CIP_TARGET_ADDRESS_MASK ((0x3ff) << I2CIP_TARGET_ADDRESS_SHIFT)
#define I2CIP_TARGET_ADDRESS_IC_10BITADDR_MASTER_SHIFT 12
#define I2CIP_TARGET_ADDRESS_IC_10BITADDR_MASTER_MASK                          \
	(1 << I2CIP_TARGET_ADDRESS_IC_10BITADDR_MASTER_SHIFT)
#define I2CIP_TARGET_ADDRESS_SPECIAL_BIT_SHIFT 11
#define I2CIP_TARGET_ADDRESS_SPECIAL_BIT_MASK                                  \
	((0x1) << I2CIP_TARGET_ADDRESS_SPECIAL_BIT_SHIFT)
#define I2CIP_TARGET_ADDRESS_GC_OR_START_SHIFT 10
#define I2CIP_TARGET_ADDRESS_GC_OR_START_MASK                                  \
	((0x1) << I2CIP_TARGET_ADDRESS_GC_OR_START_SHIFT)
#define HAL_I2C_10BITADDR_MASK (1 << 15)
#define I2CIP_MASTER_MODE_SHIFT (0)
#define I2CIP_MASTER_MODE_MASK ((0x1) << I2CIP_MASTER_MODE_SHIFT)
#define I2CIP_10BITADDR_MASTER_SHIFT (4)
#define I2CIP_10BITADDR_MASTER_MASK ((0x1) << I2CIP_10BITADDR_MASTER_SHIFT)
#define I2CIP_10BITADDR_SLAVE_SHIFT (3)
#define I2CIP_10BITADDR_SLAVE_MASK ((0x1) << I2CIP_10BITADDR_SLAVE_SHIFT)
#define I2CIP_SLAVE_DISABLE_SHIFT (6)
#define I2CIP_SLAVE_DISABLE_MASK ((0x1) << I2CIP_SLAVE_DISABLE_SHIFT)
#define I2CIP_RESTART_ENABLE_SHIFT (5)
#define I2CIP_RESTART_ENABLE_MASK ((0x1) << I2CIP_RESTART_ENABLE_SHIFT)
#define I2CIP_IC_HS_SPKLEN_REG_OFFSET 0xA4
#define I2CIP_HS_SCL_HCNT_REG_OFFSET 0x24
#define I2CIP_HS_SCL_HCNT_SHIFT (0)
#define I2CIP_HS_SCL_HCNT_MASK ((0xffff) << I2CIP_HS_SCL_HCNT_SHIFT)
#define I2CIP_HS_SCL_LCNT_REG_OFFSET 0x28
#define I2CIP_HS_SCL_LCNT_SHIFT (0)
#define I2CIP_HS_SCL_LCNT_MASK ((0xffff) << I2CIP_HS_SCL_LCNT_SHIFT)
#define I2CIP_SS_SCL_LCNT_REG_OFFSET 0x18
#define I2CIP_SS_SCL_LCNT_SHIFT (0)
#define I2CIP_SS_SCL_LCNT_MASK ((0xffff) << I2CIP_SS_SCL_LCNT_SHIFT)
#define I2CIP_HIGH_SPEED_MASK ((0x3) << I2CIP_HIGH_SPEED_SHIFT)
#define I2CIP_SPEED_MASK ((0x3) << I2CIP_SPEED_SHIFT)
#define I2CIP_SPEED_SHIFT (1)
#define I2CIP_FAST_SPEED_SHIFT (1)
#define I2CIP_FAST_SPEED_MASK ((0x2) << I2CIP_HIGH_SPEED_SHIFT)
#define I2CIP_STANDARD_SPEED_SHIFT (1)
#define I2CIP_STANDARD_SPEED_MASK ((0x1) << I2CIP_STANDARD_SPEED_SHIFT)
#define I2CIP_SDA_HOLD_REG_OFFSET 0x7c
#define I2CIP_STATUS_REG_OFFSET 0x70
#define I2CIP_STATUS_ACT_SHIFT (0)
#define I2CIP_STATUS_ACT_MASK ((0x1) << I2CIP_STATUS_ACT_SHIFT)
#define I2CIP_RX_FIFO_LEVEL_REG_OFFSET 0x78
#define I2CIP_CMD_DATA_REG_OFFSET 0x10
#define I2CIP_TX_FIFO_LEVEL_REG_OFFSET 0x74
#define I2CIP_RX_FIFO_LEVEL_REG_OFFSET 0x78
#define I2CIP_TX_FIFO_DEPTH (8)
#define I2CIP_RX_FIFO_DEPTH (8)
#define I2CIP_STATUS_TFNF_SHIFT (1)
#define I2CIP_STATUS_TFNF_MASK ((0x1) << I2CIP_STATUS_TFNF_SHIFT)
#define I2CIP_CMD_DATA_RESTART_SHIFT (10)
#define I2CIP_CMD_DATA_RESTART_MASK ((0x1) << I2CIP_CMD_DATA_RESTART_SHIFT)
#define I2CIP_CMD_DATA_STOP_SHIFT (9)
#define I2CIP_CMD_DATA_STOP_MASK ((0x1) << I2CIP_CMD_DATA_STOP_SHIFT)
#define I2CIP_CMD_DATA_CMD_READ_SHIFT (8)
#define I2CIP_CMD_DATA_CMD_READ_MASK ((0x1) << I2CIP_CMD_DATA_CMD_READ_SHIFT)
#define I2CIP_STATUS_RFNE_SHIFT (3)
#define I2CIP_STATUS_RFNE_MASK ((0x1) << I2CIP_STATUS_RFNE_SHIFT)
#define I2CIP_RAW_INT_STATUS_REG_OFFSET 0x34
#define I2CIP_CMD_DATA_CMD_WRITE_SHIFT (8)
#define I2CIP_CMD_DATA_CMD_WRITE_MASK ((0x0) << I2CIP_CMD_DATA_CMD_WRITE_SHIFT)
#define I2CIP_STATUS_TFE_SHIFT (2)
#define I2CIP_STATUS_TFE_MASK ((0x1) << I2CIP_STATUS_TFE_SHIFT)

#define i2cip_read32(b, a) (getreg32(((b) + (a))))
#define i2cip_write32(v, b, a) (putreg32(v, ((b) + (a))))

static inline uint8_t i2cip_w_enable(uint32_t reg_base, uint8_t enable)
{
	uint32_t val = 0;
	val = i2cip_read32(reg_base, I2CIP_ENABLE_REG_OFFSET);
	if (enable)
		val |= I2CIP_ENABLE_MASK;
	else
		val &= ~I2CIP_ENABLE_MASK;
	i2cip_write32(val, reg_base, I2CIP_ENABLE_REG_OFFSET);
	return 0;
}

static inline uint8_t i2cip_w_target_address(uint32_t reg_base, uint32_t addr)
{
	uint32_t val = 0;
	val = i2cip_read32(reg_base, I2CIP_TARGET_ADDRESS_REG_OFFSET);
	val &= ~I2CIP_TARGET_ADDRESS_MASK;
	val |= (addr << I2CIP_TARGET_ADDRESS_SHIFT) & I2CIP_TARGET_ADDRESS_MASK;
	if (addr & HAL_I2C_10BITADDR_MASK) {
		val |= I2CIP_TARGET_ADDRESS_IC_10BITADDR_MASTER_MASK;
	} else {
		val &= ~I2CIP_TARGET_ADDRESS_IC_10BITADDR_MASTER_MASK;
	}
	i2cip_write32(val, reg_base, I2CIP_TARGET_ADDRESS_REG_OFFSET);
	return 0;
}

static inline uint8_t i2cip_w_clear_ctrl(uint32_t reg_base)
{
	i2cip_write32(0, reg_base, I2CIP_CTRL_REG_OFFSET);
	return 0;
}

static inline uint8_t i2cip_w_master_mode(uint32_t reg_base)
{
	uint32_t val = 0;
	val = i2cip_read32(reg_base, I2CIP_CTRL_REG_OFFSET);
	val |= I2CIP_MASTER_MODE_MASK;
	val |= I2CIP_SLAVE_DISABLE_MASK;
	i2cip_write32(val, reg_base, I2CIP_CTRL_REG_OFFSET);
	return 0;
}

static inline uint8_t i2cip_w_restart(uint32_t reg_base, uint8_t restart)
{
	uint32_t val = 0;
	val = i2cip_read32(reg_base, I2CIP_CTRL_REG_OFFSET);
	if (restart)
		val |= I2CIP_RESTART_ENABLE_MASK;
	else
		val &= ~I2CIP_RESTART_ENABLE_MASK;
	i2cip_write32(val, reg_base, I2CIP_CTRL_REG_OFFSET);
	return 0;
}

static inline uint32_t i2cip_w_hs_spklen(uint32_t reg_base, uint32_t val)
{
	i2cip_write32(val, reg_base, I2CIP_IC_HS_SPKLEN_REG_OFFSET);
	return 0;
}

static inline uint32_t i2cip_r_status(uint32_t reg_base)
{
	return i2cip_read32(reg_base, I2CIP_STATUS_REG_OFFSET);
}

static inline uint32_t i2cip_r_rx_fifo_level(uint32_t reg_base)
{
	return i2cip_read32(reg_base, I2CIP_RX_FIFO_LEVEL_REG_OFFSET);
}

static inline uint32_t i2cip_r_cmd_data(uint32_t reg_base)
{
	return i2cip_read32(reg_base, I2CIP_CMD_DATA_REG_OFFSET);
}

static inline uint32_t i2cip_r_tx_fifo_level(uint32_t reg_base)
{
	return i2cip_read32(reg_base, I2CIP_TX_FIFO_LEVEL_REG_OFFSET);
}

static inline uint8_t i2cip_w_high_speed_hcnt(uint32_t reg_base, uint32_t hcnt)
{
	uint32_t val = 0;

	val |= hcnt << I2CIP_HS_SCL_HCNT_SHIFT;
	i2cip_write32(val, reg_base, I2CIP_HS_SCL_HCNT_REG_OFFSET);

	return 0;
}
static inline uint8_t i2cip_w_standard_speed_lcnt(uint32_t reg_base,
						  uint32_t lcnt)
{
	uint32_t val = 0;

	val |= lcnt << I2CIP_SS_SCL_LCNT_SHIFT;
	i2cip_write32(val, reg_base, I2CIP_SS_SCL_LCNT_REG_OFFSET);

	return 0;
}

static inline uint8_t i2cip_w_speed(uint32_t reg_base, uint32_t speed)
{
	uint32_t val = 0;

	val = i2cip_read32(reg_base, I2CIP_CTRL_REG_OFFSET);
	val &= ~I2CIP_SPEED_MASK;
	val |= speed;
	i2cip_write32(val, reg_base, I2CIP_CTRL_REG_OFFSET);

	return 0;
}

static inline uint32_t i2cip_w_sda_hold_time(uint32_t reg_base, uint32_t val)
{
	i2cip_write32(val, reg_base, I2CIP_SDA_HOLD_REG_OFFSET);
	return 0;
}

static inline uint32_t i2cip_r_target_address_reg(uint32_t reg_base)
{
	return i2cip_read32(reg_base, I2CIP_TARGET_ADDRESS_REG_OFFSET);
}

static inline uint8_t i2cip_w_cmd_data(uint32_t reg_base, uint32_t cmd_data)
{
	i2cip_write32(cmd_data, reg_base, I2CIP_CMD_DATA_REG_OFFSET);
	return 0;
}

static inline uint32_t i2cip_r_raw_int_status(uint32_t reg_base)
{
	return i2cip_read32(reg_base, I2CIP_RAW_INT_STATUS_REG_OFFSET);
}

static uint32_t _i2c_adjust_period_cnt(uint32_t period_cnt, uint16_t trising_ns,
				       uint16_t tfalling_ns, uint16_t pclk_mhz)
{
	uint16_t rising_falling_cycle;
	rising_falling_cycle = ((trising_ns + tfalling_ns) * pclk_mhz) / 1000;
	if (period_cnt > rising_falling_cycle) {
		period_cnt -= rising_falling_cycle;
	} else {
		period_cnt = 0;
	}
	return period_cnt;
}

static void _i2c_get_clk_cnt(uint32_t period_cnt, uint16_t tlow_ns,
			     uint16_t thigh_ns, uint16_t spklen,
			     uint16_t pclk_mhz, uint16_t *plcnt,
			     uint16_t *phcnt)
{
#define IC_SCL_LOW_CYCLE_ADD (1)
/* NOTE: H/w spec says that (6 + spklen) cycles is added for SCL high interval, but tests show that 1 more cycle is needed*/
#define IC_SCL_HIGH_CYCLE_ADD (6 + spklen + 1)

#define MIN_IC_SCL_LCNT (7 + spklen + IC_SCL_LOW_CYCLE_ADD)
#define MIN_IC_SCL_HCNT (5 + spklen + IC_SCL_HIGH_CYCLE_ADD)

	uint32_t lcnt, hcnt;
	uint16_t min_lcnt, min_hcnt;

	min_lcnt = (tlow_ns * pclk_mhz + 1000 - 1) / 1000;
	if (min_lcnt < MIN_IC_SCL_LCNT) {
		min_lcnt = MIN_IC_SCL_LCNT;
	}
	min_hcnt = (thigh_ns * pclk_mhz + 1000 - 1) / 1000;
	if (min_hcnt < MIN_IC_SCL_HCNT) {
		min_hcnt = MIN_IC_SCL_HCNT;
	}

	if (min_lcnt + min_hcnt > period_cnt) {
		lcnt = min_lcnt;
		hcnt = min_hcnt;
	} else {
		lcnt = (period_cnt + 1) / 2;
		if (min_lcnt >= min_hcnt) {
			if (lcnt < min_lcnt) {
				lcnt = min_lcnt;
			}
			hcnt = period_cnt - lcnt;
			if (hcnt < min_hcnt) {
				hcnt = min_hcnt;
			}
		} else {
			hcnt = lcnt;
			if (hcnt < min_hcnt) {
				hcnt = min_hcnt;
			}
			lcnt = period_cnt - hcnt;
			if (lcnt < min_lcnt) {
				lcnt = min_lcnt;
			}
		}
	}

	lcnt -= IC_SCL_LOW_CYCLE_ADD;
	hcnt -= IC_SCL_HIGH_CYCLE_ADD;

	*plcnt = lcnt;
	*phcnt = hcnt;
}

__maybe_unused static uint32_t _i2c_check_raw_int_status(uint32_t reg_base,
							 uint32_t mask)
{
	uint32_t regval = i2cip_r_raw_int_status(reg_base);
	return regval & mask;
}

static void _i2c_set_speed(int speed)
{
#define MAX_HS_SPK_NS 10
#define MAX_FSP_SPK_NS 50
#define MAX_FS_SPK_NS 50

#define SS_THOLD_NS 1800
#define FS_THOLD_NS 600
#define FSP_THOLD_NS 300
#define HS_100PF_THOLD_NS 30
#define HS_400PF_THOLD_NS 70

#define SS_TRISING_NS 300
#define FS_TRISING_NS 50
#define FSP_TRISING_NS 30
#define HS_100PF_TRISING_NS 30
#define HS_400PF_TRISING_NS 30

#define SS_TFALLING_NS 30
#define FS_TFALLING_NS 30
#define FSP_TFALLING_NS 30
#define HS_100PF_TFALLING_NS 30
#define HS_400PF_TFALLING_NS 30

#define MIN_SS_TLOW_NS 4700
#define MIN_SS_THIGH_NS 4000
#define MIN_FS_TLOW_NS 1300
#define MIN_FS_THIGH_NS 600
#define MIN_FSP_TLOW_NS 500
#define MIN_FSP_THIGH_NS 260
#define MIN_HS_100PF_TLOW_NS 160
#define MIN_HS_100PF_THIGH_NS 60
#define MIN_HS_400PF_TLOW_NS 320
#define MIN_HS_400PF_THIGH_NS 120

// Round down the spike suppression limit value
#define GET_SPKLEN_VAL(s) ((s)*pclk_mhz / 1000)

	uint32_t reg_base = ISPV4_I2C0_BASE;
	uint32_t pclk, period_cnt;
	uint16_t lcnt, hcnt, hold_cycle, spklen;
	uint16_t tlow_ns, thigh_ns, thold_ns, trising_ns, tfalling_ns;
	uint8_t spk_ns;
	uint16_t pclk_mhz;

	pclk = ISPV4_FPGA_CORE_CLK;

	pclk_mhz = pclk / 1000000;
	period_cnt = (pclk + speed - 1) / speed;

	spk_ns = MAX_FS_SPK_NS;
	tlow_ns = MIN_SS_TLOW_NS;
	thigh_ns = MIN_SS_THIGH_NS;
	thold_ns = SS_THOLD_NS;
	trising_ns = SS_TRISING_NS;
	tfalling_ns = SS_TFALLING_NS;

	spklen = GET_SPKLEN_VAL(spk_ns);
	if (spklen == 0)
		spklen = 1;

	i2cip_w_hs_spklen(reg_base, spklen);

	period_cnt = _i2c_adjust_period_cnt(period_cnt, trising_ns, tfalling_ns,
					    pclk_mhz);
	_i2c_get_clk_cnt(period_cnt, tlow_ns, thigh_ns, spklen, pclk_mhz, &lcnt,
			 &hcnt);

	i2cip_w_standard_speed_lcnt(reg_base, hcnt);
	i2cip_w_standard_speed_lcnt(reg_base, lcnt);
	i2cip_w_speed(reg_base, I2CIP_STANDARD_SPEED_MASK);

	/* Master mode: min = 1; slave mode: min = (spklen + 7)*/
	hold_cycle = (thold_ns * pclk_mhz + 1000 - 1) / 1000;
	i2cip_w_sda_hold_time(reg_base, hold_cycle);
}

struct ispv4_i2c_dev {
	struct device *dev;
	struct i2c_adapter adap;
};

#define I2C_SPEED 100 * 1000

int ispv4_i2c_init(struct platform_device *pdev)
{
	u32 reg_base = ISPV4_I2C0_BASE;
	// Disable i2c.
	i2cip_w_enable(reg_base, 0);
	// Set target address, only for pmic.
	i2cip_w_target_address(reg_base, 0x18);
	i2cip_w_clear_ctrl(reg_base);
	i2cip_w_master_mode(reg_base);
	_i2c_set_speed(I2C_SPEED);
	i2cip_w_restart(reg_base, 1);
	i2cip_w_enable(reg_base, 1);
	return 0;
}
EXPORT_SYMBOL_GPL(ispv4_i2c_init);

int ispv4_i2c_deinit(struct platform_device *pdev)
{
	u32 reg_base = ISPV4_I2C0_BASE;
	i2cip_w_enable(reg_base, 0);
	return 0;
}
EXPORT_SYMBOL_GPL(ispv4_i2c_deinit);

#if 1
static int ispv4_i2c_config_target_addr(u32 target_addr)
{
	u32 cur_addr = 0;
	u32 reg_base = ISPV4_I2C0_BASE;
	int tout = 0;

	cur_addr = i2cip_r_target_address_reg(reg_base);

	if (cur_addr == target_addr)
		return 0;

	tout = 10;
	while ((i2cip_r_status(reg_base) & I2CIP_STATUS_ACT_MASK) && tout--)
		udelay(1000);
	if (tout == 0)
		return -EIO;

	i2cip_w_enable(reg_base, 0);
	i2cip_w_target_address(reg_base, target_addr);
	i2cip_w_enable(reg_base, 1);

	return 0;
}
#else
static int ispv4_i2c_config_target_addr(u32 target_addr)
{
	return 0;
}
#endif

static int ispv4_i2c_read(struct i2c_msg msg)
{
	int tmp = 0, i = 0, ret = 0;
	u32 rdcnt = 0, wrcnt = 0;
	u32 rx_ongoing, tx_limit, res, sto;
	u32 reg_base = ISPV4_I2C0_BASE;
	int len = msg.len;
	int tout = 0;

	ret = ispv4_i2c_config_target_addr(msg.addr);
	if (ret != 0)
		return ret;

	// Clear FIFO
	tmp = i2cip_r_rx_fifo_level(reg_base);
	for (i = 0; i < tmp; i++)
		i2cip_r_cmd_data(reg_base);

	tout = 10 * len;
	while (rdcnt < len && tout--) {
		// Send reading cmd
		rx_ongoing = i2cip_r_tx_fifo_level(reg_base) +
			     i2cip_r_rx_fifo_level(reg_base) + 1;
		if (rx_ongoing < I2CIP_RX_FIFO_DEPTH) {
			tx_limit = I2CIP_RX_FIFO_DEPTH - rx_ongoing;
		} else {
			tx_limit = 0;
		}

		for (i = 0; i < tx_limit && wrcnt < len; i++, wrcnt++) {
			// if TX FIFO is full
			if (!(i2cip_r_status(reg_base) &
			      I2CIP_STATUS_TFNF_MASK)) {
				break;
			}
			if (wrcnt == 0) {
				res = !(msg.flags & I2C_M_NOSTART) ?
					      I2CIP_CMD_DATA_RESTART_MASK :
					      0;
			} else {
				res = 0;
			}
			if (wrcnt == len - 1) {
				sto = (msg.flags & I2C_M_STOP) ?
					      I2CIP_CMD_DATA_STOP_MASK :
					      0;
			} else {
				sto = 0;
			}

			i2cip_w_cmd_data(reg_base,
					 I2CIP_CMD_DATA_CMD_READ_MASK | res |
						 sto);
		}
		if (i2cip_r_status(reg_base) & I2CIP_STATUS_RFNE_MASK) {
			tmp = i2cip_r_cmd_data(reg_base);
			msg.buf[rdcnt++] = tmp;
		} else {
			udelay(1000);
		}
	}

	if (tout == 0)
		return -EIO;

	tout = 10;
	if (msg.flags & I2C_M_STOP)
		while ((i2cip_r_status(reg_base) & I2CIP_STATUS_ACT_MASK) &&
		       tout--)
			udelay(1000);

	if (tout == 0)
		return -EIO;

	return 0;
}

static int ispv4_i2c_write(struct i2c_msg msg)
{
	int i = 0, ret = 0;
	u32 wrcnt = 0, res, sto;
	u32 reg_base = ISPV4_I2C0_BASE;
	int len = msg.len;
	int tout = 0;

	ret = ispv4_i2c_config_target_addr(msg.addr);
	if (ret != 0)
		return ret;

	for (i = 0; i < len; i++) {
		if (i == 0) {
			res = !(msg.flags & I2C_M_NOSTART) ?
				      I2CIP_CMD_DATA_RESTART_MASK :
				      0;
		} else {
			res = 0;
		}
		if (i == (len - 1)) {
			sto = (msg.flags & I2C_M_STOP) ?
				      I2CIP_CMD_DATA_STOP_MASK :
				      0;
		} else {
			sto = 0;
		}

		tout = 10;
		while ((!(i2cip_r_status(reg_base) & I2CIP_STATUS_TFNF_MASK)) &&
		       tout--)
			udelay(1000);

		if (tout == 0)
			return -EIO;

		i2cip_w_cmd_data(reg_base,
				 msg.buf[i] | res | sto |
					 I2CIP_CMD_DATA_CMD_WRITE_MASK);

		wrcnt++;
	}

	tout = 10;
	if (msg.flags & I2C_M_STOP)
		while (((i2cip_r_status(reg_base) &
			 (I2CIP_STATUS_TFE_MASK | I2CIP_STATUS_ACT_MASK)) !=
			I2CIP_STATUS_TFE_MASK) &&
		       tout--)
			udelay(1000);
	else
		while ((!(i2cip_r_status(reg_base) & I2CIP_STATUS_TFE_MASK)) &&
		       tout--)
			udelay(1000);

	if (tout == 0)
		return -EIO;

	return 0;
}

static int ispv4_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[],
			  int num)
{
	// struct ispv4_i2c_dev *v4i2c = i2c_get_adapdata(adap);
	int ret = 0, i;

	for (i = 0; i < num; i++) {
		if (msgs[i].flags & I2C_M_RD)
			ret = ispv4_i2c_read(msgs[i]);
		else
			ret = ispv4_i2c_write(msgs[i]);

		if (ret != 0)
			return ret;
	}

	return ret;
}

static u32 ispv4_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C;
}

static const struct i2c_algorithm ispv4_i2c_algo = {
	.master_xfer = ispv4_i2c_xfer,
	.functionality = ispv4_i2c_func,
};

static int ispv4_i2c_probe(struct platform_device *pdev)
{
	struct ispv4_i2c_dev *v4i2c;
	int ret;

	v4i2c = devm_kzalloc(&pdev->dev, sizeof(*v4i2c), GFP_KERNEL);
	if (!v4i2c)
		return -ENOMEM;

	v4i2c->dev = &pdev->dev;
	v4i2c->adap.algo = &ispv4_i2c_algo;
	platform_set_drvdata(pdev, v4i2c);
	i2c_set_adapdata(&v4i2c->adap, v4i2c);
	v4i2c->adap.dev.parent = v4i2c->dev;
	strlcpy(v4i2c->adap.name, "ISPV4-I2C", sizeof(v4i2c->adap.name));
	ret = i2c_add_adapter(&v4i2c->adap);
	if (ret) {
		dev_err(v4i2c->dev, "Add adapter failed, ret=%d\n", ret);
		return ret;
	}

	dev_info(v4i2c->dev, "I2C probed\n");
	return 0;
}

static int ispv4_i2c_remove(struct platform_device *pdev)
{
	struct ispv4_i2c_dev *v4i2c = platform_get_drvdata(pdev);

	i2c_del_adapter(&v4i2c->adap);
	return 0;
}

static struct platform_driver ispv4_i2c_driver = {
	.probe  = ispv4_i2c_probe,
	.remove = ispv4_i2c_remove,
	.driver = {
		.name = "ispv4-i2c",
	},
};

static int __init i2c_dev_init(void)
{
	return platform_driver_register(&ispv4_i2c_driver);
}

static void __exit i2c_dev_exit(void)
{
	platform_driver_unregister(&ispv4_i2c_driver);
}

module_init(i2c_dev_init);
module_exit(i2c_dev_exit);
MODULE_LICENSE("GPL v2");
