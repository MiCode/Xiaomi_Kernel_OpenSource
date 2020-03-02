/* -*- mode: c; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* vi: set ts=8 sw=8 sts=8: */
/*************************************************************************/ /*!
@Codingstyle    LinuxKernel
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

/*
 * This is a device driver for the odin testchip framework. It creates
 * platform devices for the pdp and ext sub-devices, and exports functions
 * to manage the shared interrupt handling
 */

#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/delay.h>

#include "tc_drv_internal.h"
#include "tc_odin.h"
#include "tc_ion.h"

/* Odin (3rd gen TCF FPGA) */
#include "odin_defs.h"
#include "odin_regs.h"
#include "bonnie_tcf.h"

/* Macro's to set and get register fields */
#define REG_FIELD_GET(v, str) \
	(u32)(((v) & (s##_MASK)) >> (s##_SHIFT))
#define REG_FIELD_SET(v, f, str) \
	v = (u32)(((v) & (u32)~(str##_MASK)) | \
		  (u32)(((f) << (str##_SHIFT)) & (str##_MASK)))

#define SAI_STATUS_UNALIGNED 0
#define SAI_STATUS_ALIGNED   1
#define SAI_STATUS_ERROR     2

#if defined(SUPPORT_RGX)

static void spi_write(struct tc_device *tc, u32 off, u32 val)
{
	iowrite32(off, tc->tcf.registers
		  + ODN_REG_BANK_TCF_SPI_MASTER
		  + ODN_SPI_MST_ADDR_RDNWR);
	iowrite32(val, tc->tcf.registers
		  + ODN_REG_BANK_TCF_SPI_MASTER
		  + ODN_SPI_MST_WDATA);
	iowrite32(0x1, tc->tcf.registers
		  + ODN_REG_BANK_TCF_SPI_MASTER
		  + ODN_SPI_MST_GO);
	udelay(1000);
}

static int spi_read(struct tc_device *tc, u32 off, u32 *val)
{
	int cnt = 0;
	u32 spi_mst_status;

	iowrite32(0x40000 | off, tc->tcf.registers
		  + ODN_REG_BANK_TCF_SPI_MASTER
		  + ODN_SPI_MST_ADDR_RDNWR);
	iowrite32(0x1, tc->tcf.registers
		  + ODN_REG_BANK_TCF_SPI_MASTER
		  + ODN_SPI_MST_GO);
	udelay(100);

	do {
		spi_mst_status = ioread32(tc->tcf.registers
					  + ODN_REG_BANK_TCF_SPI_MASTER
					  + ODN_SPI_MST_STATUS);

		if (cnt++ > 10000) {
			dev_err(&tc->pdev->dev,
				"%s: Time out reading SPI reg (0x%x)\n",
				__func__, off);
			return -1;
		}

	} while (spi_mst_status != 0x08);

	*val = ioread32(tc->tcf.registers
			+ ODN_REG_BANK_TCF_SPI_MASTER
			+ ODN_SPI_MST_RDATA);

	return 0;
}

/* returns 1 for aligned, 0 for unaligned */
static int get_odin_sai_status(struct tc_device *tc, int bank)
{
	void __iomem *bank_addr = tc->tcf.registers
					+ ODN_REG_BANK_SAI_RX_DDR(bank);
	void __iomem *reg_addr;
	u32 eyes;
	u32 clk_taps;
	u32 train_ack;

	reg_addr = bank_addr + ODN_SAI_RX_DEBUG_SAI_EYES;
	eyes = ioread32(reg_addr);

	reg_addr = bank_addr + ODN_SAI_RX_DEBUG_SAI_CLK_TAPS;
	clk_taps = ioread32(reg_addr);

	reg_addr = bank_addr + ODN_SAI_RX_DEBUG_SAI_TRAIN_ACK;
	train_ack = ioread32(reg_addr);

#if 0 /* enable this to get debug info if the board is not aligning */
	dev_info(&tc->pdev->dev,
		"odin bank %d align: eyes=%08x clk_taps=%08x train_ack=%08x\n",
		bank, eyes, clk_taps, train_ack);
#endif

	if (tc_is_interface_aligned(eyes, clk_taps, train_ack))
		return SAI_STATUS_ALIGNED;

	dev_warn(&tc->pdev->dev, "odin bank %d is unaligned\n", bank);
	return SAI_STATUS_UNALIGNED;
}

/* Read the odin multi clocked bank align status.
 * Returns 1 for aligned, 0 for unaligned
 */
static int read_odin_mca_status(struct tc_device *tc)
{
	void __iomem *bank_addr = tc->tcf.registers
					+ ODN_REG_BANK_MULTI_CLK_ALIGN;
	void __iomem *reg_addr = bank_addr + ODN_MCA_DEBUG_MCA_STATUS;
	u32 mca_status;

	mca_status = ioread32(reg_addr);

#if 0 /* Enable this if there are alignment issues */
	dev_info(&tc->pdev->dev,
		"Odin MCA_STATUS = %08x\n", mca_status);
#endif
	return mca_status & ODN_ALIGNMENT_FOUND_MASK;
}

/* Read the DUT multi clocked bank align status.
 * Returns 1 for aligned, 0 for unaligned
 */
static int read_dut_mca_status(struct tc_device *tc)
{
	u32 mca_status;
	const int mca_status_register_offset = 1; /* not in bonnie_tcf.h */
	int spi_address = DWORD_OFFSET(BONNIE_TCF_OFFSET_MULTI_CLK_ALIGN);

	spi_address = DWORD_OFFSET(BONNIE_TCF_OFFSET_MULTI_CLK_ALIGN)
			+ mca_status_register_offset;

	spi_read(tc, spi_address, &mca_status);

#if 0 /* Enable this if there are alignment issues */
	dev_info(&tc->pdev->dev,
		"DUT MCA_STATUS = %08x\n", mca_status);
#endif
	return mca_status & 1;  /* 'alignment found' status is in bit 1 */
}

/* returns 1 for aligned, 0 for unaligned */
static int get_dut_sai_status(struct tc_device *tc, int bank)
{
	u32 eyes;
	u32 clk_taps;
	u32 train_ack;
	const u32 bank_base = DWORD_OFFSET(BONNIE_TCF_OFFSET_SAI_RX_1
				+ (BONNIE_TCF_OFFSET_SAI_RX_DELTA * bank));
	int spi_timeout;

	spi_timeout = spi_read(tc, bank_base
		+ DWORD_OFFSET(BONNIE_TCF_OFFSET_SAI_EYES), &eyes);
	if (spi_timeout)
		return SAI_STATUS_ERROR;

	spi_read(tc, bank_base
		+ DWORD_OFFSET(BONNIE_TCF_OFFSET_SAI_CLK_TAPS), &clk_taps);
	spi_read(tc, bank_base
		+ DWORD_OFFSET(BONNIE_TCF_OFFSET_SAI_TRAIN_ACK), &train_ack);

#if 0 /* enable this to get debug info if the board is not aligning */
	dev_info(&tc->pdev->dev,
		"dut  bank %d align: eyes=%08x clk_taps=%08x train_ack=%08x\n",
		bank, eyes, clk_taps, train_ack);
#endif

	if (tc_is_interface_aligned(eyes, clk_taps, train_ack))
		return SAI_STATUS_ALIGNED;

	dev_warn(&tc->pdev->dev, "dut bank %d is unaligned\n", bank);
	return SAI_STATUS_UNALIGNED;
}

/*
 * Returns the divider group register fields for the specified counter value.
 * See Xilinx Application Note xapp888.
 */
static void odin_mmcm_reg_param_calc(u32 value, u32 *low, u32 *high,
				     u32 *edge, u32 *no_count)
{
	if (value == 1U) {
		*no_count = 1U;
		*edge = 0;
		*high = 0;
		*low = 0;
	} else {
		*no_count = 0;
		*edge = value % 2U;
		*high = value >> 1;
		*low = (value + *edge) >> 1U;
	}
}

/*
 * Returns the MMCM Input Divider, FB Multiplier and Output Divider values for
 * the specified input frequency and target output frequency.
 * Function doesn't support fractional values for multiplier and output divider
 * As per Xilinx 7 series FPGAs clocking resources user guide, aims for highest
 * VCO and smallest D and M.
 * Configured for Xilinx Virtex7 speed grade 2.
 */
static int odin_mmcm_counter_calc(struct device *dev,
				  u32 freq_input, u32 freq_output,
				  u32 *d, u32 *m, u32 *o)
{
	u32 d_min, d_max;
	u32 m_min, m_max, m_ideal;
	u32 d_cur, m_cur, o_cur;
	u32 best_diff, d_best, m_best, o_best;

	/*
	 * Check specified input frequency is within range
	 */
	if (freq_input < ODN_INPUT_CLOCK_SPEED_MIN) {
		dev_err(dev, "Input frequency (%u hz) below minimum supported value (%u hz)\n",
			freq_input, ODN_INPUT_CLOCK_SPEED_MIN);
		return -EINVAL;
	}
	if (freq_input > ODN_INPUT_CLOCK_SPEED_MAX) {
		dev_err(dev, "Input frequency (%u hz) above maximum supported value (%u hz)\n",
			freq_input, ODN_INPUT_CLOCK_SPEED_MAX);
		return -EINVAL;
	}

	/*
	 * Check specified target frequency is within range
	 */
	if (freq_output < ODN_OUTPUT_CLOCK_SPEED_MIN) {
		dev_err(dev, "Output frequency (%u hz) below minimum supported value (%u hz)\n",
			freq_input, ODN_OUTPUT_CLOCK_SPEED_MIN);
		return -EINVAL;
	}
	if (freq_output > ODN_OUTPUT_CLOCK_SPEED_MAX) {
		dev_err(dev, "Output frequency (%u hz) above maximum supported value (%u hz)\n",
			freq_output, ODN_OUTPUT_CLOCK_SPEED_MAX);
		return -EINVAL;
	}

	/*
	 * Calculate min and max for Input Divider.
	 * Refer Xilinx 7 series FPGAs clocking resources user guide
	 * equation 3-6 and 3-7
	 */
	d_min = DIV_ROUND_UP(freq_input, ODN_PFD_MAX);
	d_max = min(freq_input/ODN_PFD_MIN, (u32)ODN_DREG_VALUE_MAX);

	/*
	 * Calculate min and max for Input Divider.
	 * Refer Xilinx 7 series FPGAs clocking resources user guide.
	 * equation 3-8 and 3-9
	 */
	m_min = DIV_ROUND_UP((ODN_VCO_MIN * d_min), freq_input);
	m_max = min(((ODN_VCO_MAX * d_max) / freq_input),
		    (u32)ODN_MREG_VALUE_MAX);

	for (d_cur = d_min; d_cur <= d_max; d_cur++) {
		/*
		 * Refer Xilinx 7 series FPGAs clocking resources user guide.
		 * equation 3-10
		 */
		m_ideal = min(((d_cur * ODN_VCO_MAX)/freq_input), m_max);

		for (m_cur = m_ideal; m_cur >= m_min; m_cur -= 1) {
			/**
			 * Skip if VCO for given 'm' and 'd' value is not an
			 * integer since fractional component is not supported
			 */
			if (((freq_input * m_cur) % d_cur) != 0)
				continue;

			/**
			 * Skip if divider for given 'm' and 'd' value is not
			 * an integer since fractional component is not
			 * supported
			 */
			if ((freq_input * m_cur) % (d_cur * freq_output) != 0)
				continue;

			/**
			 * Calculate output divider value.
			 */
			o_cur = (freq_input * m_cur)/(d_cur * freq_output);

			*d = d_cur;
			*m = m_cur;
			*o = o_cur;
			return 0;
		}
	}

	/* Failed to find exact optimal solution with high VCO. Brute-force find a suitable config,
	 * again prioritising high VCO, to get lowest jitter */
	d_min = 1; d_max = (u32)ODN_DREG_VALUE_MAX;
	m_min = 1; m_max = (u32)ODN_MREG_VALUE_MAX;
	best_diff = 0xFFFFFFFF;

	for (d_cur = d_min; d_cur <= d_max; d_cur++) {
		for (m_cur = m_max; m_cur >= m_min; m_cur -= 1) {
			u32 pfd, vco, o_avg, o_min, o_max;

			pfd = freq_input / d_cur;
			vco = pfd * m_cur;

			if (pfd < ODN_PFD_MIN)
				continue;

			if (pfd > ODN_PFD_MAX)
				continue;

			if (vco < ODN_VCO_MIN)
				continue;

			if (vco > ODN_VCO_MAX)
				continue;

			/* A range of -1/+3 around o_avg gives us 100kHz granularity. It can be extended further. */
			o_avg = vco / freq_output;
			o_min = (o_avg >= 2) ? (o_avg - 1) : 1;
			o_max = o_avg + 3;
			if (o_max > (u32)ODN_OREG_VALUE_MAX)
				o_max = (u32)ODN_OREG_VALUE_MAX;

			for (o_cur = o_min; o_cur <= o_max; o_cur++) {
				u32 freq_cur, diff_cur;

				freq_cur = vco / o_cur;

				if (freq_cur > freq_output)
					continue;

				diff_cur = freq_output - freq_cur;

				if (diff_cur == 0) {
					/* Found an exact match */
					*d = d_cur;
					*m = m_cur;
					*o = o_cur;
					return 0;
				}

				if (diff_cur < best_diff) {
					best_diff = diff_cur;
					d_best = d_cur;
					m_best = m_cur;
					o_best = o_cur;
				}
			}
		}
	}

	if (best_diff != 0xFFFFFFFF) {
		dev_warn(dev, "Odin: Found similar freq of %u Hz\n", freq_output - best_diff);
		*d = d_best;
		*m = m_best;
		*o = o_best;
		return 0;
	}

	dev_err(dev, "Odin: Unable to find integer values for d, m and o for requested frequency (%u)\n",
		freq_output);

	return -ERANGE;
}

static int odin_fpga_set_dut_core_clk(struct tc_device *tc,
				      u32 input_clk, u32 output_clk)
{
	int err = 0;
	u32 in_div, mul, out_div;
	u32 high_time, low_time, edge, no_count;
	u32 value;
	void __iomem *base = tc->tcf.registers;
	void __iomem *clk_blk_base = base + ODN_REG_BANK_ODN_CLK_BLK;
	struct device *dev = &tc->pdev->dev;

	err = odin_mmcm_counter_calc(dev, input_clk, output_clk, &in_div,
				     &mul, &out_div);
	if (err != 0)
		return err;

	/* Put DUT into reset */
	iowrite32(ODN_EXTERNAL_RESETN_DUT_SPI_MASK,
		  base + ODN_CORE_EXTERNAL_RESETN);
	msleep(20);

	/* Put DUT Core MMCM into reset */
	iowrite32(ODN_CLK_GEN_RESET_DUT_CORE_MMCM_MASK,
		  base + ODN_CORE_CLK_GEN_RESET);
	msleep(20);

	/* Calculate the register fields for output divider */
	odin_mmcm_reg_param_calc(out_div, &high_time, &low_time,
				 &edge, &no_count);

	/* Read-modify-write the required fields to output divider register 1 */
	value = ioread32(clk_blk_base + ODN_DUT_CORE_CLK_OUT_DIVIDER1);
	REG_FIELD_SET(value, high_time,
			ODN_DUT_CORE_CLK_OUT_DIVIDER1_HI_TIME);
	REG_FIELD_SET(value, low_time,
			ODN_DUT_CORE_CLK_OUT_DIVIDER1_LO_TIME);
	iowrite32(value, clk_blk_base + ODN_DUT_CORE_CLK_OUT_DIVIDER1);

	/* Read-modify-write the required fields to output divider register 2 */
	value = ioread32(clk_blk_base + ODN_DUT_CORE_CLK_OUT_DIVIDER2);
	REG_FIELD_SET(value, edge,
			ODN_DUT_CORE_CLK_OUT_DIVIDER2_EDGE);
	REG_FIELD_SET(value, no_count,
			ODN_DUT_CORE_CLK_OUT_DIVIDER2_NOCOUNT);
	iowrite32(value, clk_blk_base + ODN_DUT_CORE_CLK_OUT_DIVIDER2);

	/* Calculate the register fields for multiplier */
	odin_mmcm_reg_param_calc(mul, &high_time, &low_time,
				 &edge, &no_count);

	/* Read-modify-write the required fields to multiplier register 1*/
	value = ioread32(clk_blk_base + ODN_DUT_CORE_CLK_MULTIPLIER1);
	REG_FIELD_SET(value, high_time,
			ODN_DUT_CORE_CLK_MULTIPLIER1_HI_TIME);
	REG_FIELD_SET(value, low_time,
			ODN_DUT_CORE_CLK_MULTIPLIER1_LO_TIME);
	iowrite32(value, clk_blk_base + ODN_DUT_CORE_CLK_MULTIPLIER1);

	/* Read-modify-write the required fields to multiplier register 2 */
	value = ioread32(clk_blk_base + ODN_DUT_CORE_CLK_MULTIPLIER2);
	REG_FIELD_SET(value, edge,
			ODN_DUT_CORE_CLK_MULTIPLIER2_EDGE);
	REG_FIELD_SET(value, no_count,
			ODN_DUT_CORE_CLK_MULTIPLIER2_NOCOUNT);
	iowrite32(value, clk_blk_base + ODN_DUT_CORE_CLK_MULTIPLIER2);

	/* Calculate the register fields for input divider */
	odin_mmcm_reg_param_calc(in_div, &high_time, &low_time,
				 &edge, &no_count);

	/* Read-modify-write the required fields to input divider register 1 */
	value = ioread32(clk_blk_base + ODN_DUT_CORE_CLK_IN_DIVIDER1);
	REG_FIELD_SET(value, high_time,
			 ODN_DUT_CORE_CLK_IN_DIVIDER1_HI_TIME);
	REG_FIELD_SET(value, low_time,
			 ODN_DUT_CORE_CLK_IN_DIVIDER1_LO_TIME);
	REG_FIELD_SET(value, edge,
			 ODN_DUT_CORE_CLK_IN_DIVIDER1_EDGE);
	REG_FIELD_SET(value, no_count,
			 ODN_DUT_CORE_CLK_IN_DIVIDER1_NOCOUNT);
	iowrite32(value, clk_blk_base + ODN_DUT_CORE_CLK_IN_DIVIDER1);

	/* Bring DUT clock MMCM out of reset */
	iowrite32(0, tc->tcf.registers + ODN_CORE_CLK_GEN_RESET);

	err = tc_iopol32_nonzero(ODN_MMCM_LOCK_STATUS_DUT_CORE,
				 base + ODN_CORE_MMCM_LOCK_STATUS);
	if (err != 0) {
		dev_err(dev, "MMCM failed to lock for DUT core\n");
		return err;
	}

	/* Bring DUT out of reset */
	iowrite32(ODN_EXTERNAL_RESETN_DUT_SPI_MASK |
		  ODN_EXTERNAL_RESETN_DUT_MASK,
		  tc->tcf.registers + ODN_CORE_EXTERNAL_RESETN);
	msleep(20);

	dev_info(dev, "DUT core clock set-up successful\n");

	return err;
}

static int odin_fpga_set_dut_if_clk(struct tc_device *tc,
				    u32 input_clk, u32 output_clk)
{
	int err = 0;
	u32 in_div, mul, out_div;
	u32 high_time, low_time, edge, no_count;
	u32 value;
	void __iomem *base = tc->tcf.registers;
	void __iomem *clk_blk_base = base + ODN_REG_BANK_ODN_CLK_BLK;
	struct device *dev = &tc->pdev->dev;

	err = odin_mmcm_counter_calc(dev, input_clk, output_clk,
				     &in_div, &mul, &out_div);
	if (err != 0)
		return err;

	/* Put DUT into reset */
	iowrite32(ODN_EXTERNAL_RESETN_DUT_SPI_MASK,
		  base + ODN_CORE_EXTERNAL_RESETN);
	msleep(20);

	/* Put DUT Core MMCM into reset */
	iowrite32(ODN_CLK_GEN_RESET_DUT_IF_MMCM_MASK,
		  base + ODN_CORE_CLK_GEN_RESET);
	msleep(20);

	/* Calculate the register fields for output divider */
	odin_mmcm_reg_param_calc(out_div, &high_time, &low_time,
				 &edge, &no_count);

	/* Read-modify-write the required fields to output divider register 1 */
	value = ioread32(clk_blk_base + ODN_DUT_IFACE_CLK_OUT_DIVIDER1);
	REG_FIELD_SET(value, high_time,
			ODN_DUT_IFACE_CLK_OUT_DIVIDER1_HI_TIME);
	REG_FIELD_SET(value, low_time,
			ODN_DUT_IFACE_CLK_OUT_DIVIDER1_LO_TIME);
	iowrite32(value, clk_blk_base + ODN_DUT_IFACE_CLK_OUT_DIVIDER1);

	/* Read-modify-write the required fields to output divider register 2 */
	value = ioread32(clk_blk_base + ODN_DUT_IFACE_CLK_OUT_DIVIDER2);
	REG_FIELD_SET(value, edge,
			ODN_DUT_IFACE_CLK_OUT_DIVIDER2_EDGE);
	REG_FIELD_SET(value, no_count,
			ODN_DUT_IFACE_CLK_OUT_DIVIDER2_NOCOUNT);
	iowrite32(value, clk_blk_base + ODN_DUT_IFACE_CLK_OUT_DIVIDER2);

	/* Calculate the register fields for multiplier */
	odin_mmcm_reg_param_calc(mul, &high_time, &low_time, &edge, &no_count);

	/* Read-modify-write the required fields to multiplier register 1*/
	value = ioread32(clk_blk_base + ODN_DUT_IFACE_CLK_MULTIPLIER1);
	REG_FIELD_SET(value, high_time,
			ODN_DUT_IFACE_CLK_MULTIPLIER1_HI_TIME);
	REG_FIELD_SET(value, low_time,
			ODN_DUT_IFACE_CLK_MULTIPLIER1_LO_TIME);
	iowrite32(value, clk_blk_base + ODN_DUT_IFACE_CLK_MULTIPLIER1);

	/* Read-modify-write the required fields to multiplier register 2 */
	value = ioread32(clk_blk_base + ODN_DUT_IFACE_CLK_MULTIPLIER2);
	REG_FIELD_SET(value, edge,
			ODN_DUT_IFACE_CLK_MULTIPLIER2_EDGE);
	REG_FIELD_SET(value, no_count,
			ODN_DUT_IFACE_CLK_MULTIPLIER2_NOCOUNT);
	iowrite32(value, clk_blk_base + ODN_DUT_IFACE_CLK_MULTIPLIER2);

	/* Calculate the register fields for input divider */
	odin_mmcm_reg_param_calc(in_div, &high_time, &low_time,
				 &edge, &no_count);

	/* Read-modify-write the required fields to input divider register 1 */
	value = ioread32(clk_blk_base + ODN_DUT_IFACE_CLK_IN_DIVIDER1);
	REG_FIELD_SET(value, high_time,
			 ODN_DUT_IFACE_CLK_IN_DIVIDER1_HI_TIME);
	REG_FIELD_SET(value, low_time,
			 ODN_DUT_IFACE_CLK_IN_DIVIDER1_LO_TIME);
	REG_FIELD_SET(value, edge,
			 ODN_DUT_IFACE_CLK_IN_DIVIDER1_EDGE);
	REG_FIELD_SET(value, no_count,
			 ODN_DUT_IFACE_CLK_IN_DIVIDER1_NOCOUNT);
	iowrite32(value, clk_blk_base + ODN_DUT_IFACE_CLK_IN_DIVIDER1);

	/* Bring DUT interface clock MMCM out of reset */
	iowrite32(0, tc->tcf.registers + ODN_CORE_CLK_GEN_RESET);

	err = tc_iopol32_nonzero(ODN_MMCM_LOCK_STATUS_DUT_IF,
				 base + ODN_CORE_MMCM_LOCK_STATUS);
	if (err != 0) {
		dev_err(dev, "MMCM failed to lock for DUT IF\n");
		return err;
	}

	/* Bring DUT out of reset */
	iowrite32(ODN_EXTERNAL_RESETN_DUT_SPI_MASK |
		  ODN_EXTERNAL_RESETN_DUT_MASK,
		  tc->tcf.registers + ODN_CORE_EXTERNAL_RESETN);
	msleep(20);

	dev_info(dev, "DUT IF clock set-up successful\n");

	return err;
}

static void odin_fpga_update_dut_clk_freq(struct tc_device *tc,
					  int *core_clock, int *mem_clock)
{
	struct device *dev = &tc->pdev->dev;

#if defined(SUPPORT_FPGA_DUT_CLK_INFO)
	int dut_clk_info = ioread32(tc->tcf.registers + ODN_CORE_DUT_CLK_INFO);

	if ((dut_clk_info != 0) && (dut_clk_info != 0xbaadface) && (dut_clk_info != 0xffffffff)) {
		dev_info(dev, "ODN_DUT_CLK_INFO = %08x\n", dut_clk_info);
		dev_info(dev, "Overriding provided DUT clock values: core %i, mem %i\n",
			 *core_clock, *mem_clock);

		*core_clock = ((dut_clk_info & ODN_DUT_CLK_INFO_CORE_MASK)
			       >> ODN_DUT_CLK_INFO_CORE_SHIFT) * 1000000;

		*mem_clock = ((dut_clk_info & ODN_DUT_CLK_INFO_MEM_MASK)
			       >> ODN_DUT_CLK_INFO_MEM_SHIFT) * 1000000;
	}
#endif

	dev_info(dev, "DUT clock values: core %i, mem %i\n",
		 *core_clock, *mem_clock);
}

static int odin_hard_reset_fpga(struct tc_device *tc,
				int core_clock, int mem_clock)
{
	int err = 0;

	odin_fpga_update_dut_clk_freq(tc, &core_clock, &mem_clock);

	err = odin_fpga_set_dut_core_clk(tc, ODN_INPUT_CLOCK_SPEED, core_clock);
	if (err != 0)
		goto err_out;

	err = odin_fpga_set_dut_if_clk(tc, ODN_INPUT_CLOCK_SPEED, mem_clock);

err_out:
	return err;
}

static int odin_hard_reset_bonnie(struct tc_device *tc)
{
	int reset_cnt = 0;
	bool aligned = false;
	int alignment_found;

	msleep(100);

	/* It is essential to do an SPI reset once on power-up before
	 * doing any DUT reads via the SPI interface.
	 */
	iowrite32(1, tc->tcf.registers		/* set bit 1 low */
			+ ODN_CORE_EXTERNAL_RESETN);
	msleep(20);

	iowrite32(3, tc->tcf.registers		/* set bit 1 high */
			+ ODN_CORE_EXTERNAL_RESETN);
	msleep(20);

	while (!aligned && (reset_cnt < 20)) {

		int bank;

		/* Reset the DUT to allow the SAI to retrain */
		iowrite32(2, /* set bit 0 low */
			tc->tcf.registers
			+ ODN_CORE_EXTERNAL_RESETN);

		/* Hold the DUT in reset for 50mS */
		msleep(50);

		/* Take the DUT out of reset */
		iowrite32(3, /* set bit 0 hi */
			tc->tcf.registers
			+ ODN_CORE_EXTERNAL_RESETN);
		reset_cnt++;

		/* Wait 200mS for the DUT to stabilise */
		msleep(200);

		/* Check the odin Multi Clocked bank Align status */
		alignment_found = read_odin_mca_status(tc);
		dev_info(&tc->pdev->dev,
				"Odin mca_status indicates %s\n",
				(alignment_found)?"aligned":"UNALIGNED");

		/* Check the DUT MCA status */
		alignment_found = read_dut_mca_status(tc);
		dev_info(&tc->pdev->dev,
				"DUT mca_status indicates %s\n",
				(alignment_found)?"aligned":"UNALIGNED");

		/* If all banks have aligned then the reset was successful */
		for (bank = 0; bank < 10; bank++) {

			int dut_aligned = 0;
			int odin_aligned = 0;

			odin_aligned = get_odin_sai_status(tc, bank);
			dut_aligned = get_dut_sai_status(tc, bank);

			if (dut_aligned == SAI_STATUS_ERROR)
				return SAI_STATUS_ERROR;

			if (!dut_aligned || !odin_aligned) {
				aligned = false;
				break;
			}
			aligned = true;
		}

		if (aligned) {
			dev_info(&tc->pdev->dev,
				"all banks have aligned\n");
			break;
		}

		dev_warn(&tc->pdev->dev,
			"Warning- not all banks have aligned. Trying again.\n");
	}

	if (!aligned)
		dev_warn(&tc->pdev->dev, "odin_hard_reset failed\n");

	return (aligned) ? 0 : 1; /* return 0 for success */
}

static void odin_set_mem_latency(struct tc_device *tc,
				 int mem_latency, int mem_wresp_latency)
{
	u32 regval = 0;

	if (mem_latency <= 4) {
		/* The total memory read latency cannot be lower than the
		 * amount of cycles consumed by the hardware to do a read.
		 * Set the memory read latency to 0 cycles.
		 */
		mem_latency = 0;
	} else {
		mem_latency -= 4;

		dev_info(&tc->pdev->dev,
			 "Setting memory read latency to %i cycles\n",
			 mem_latency);
	}

	if (mem_wresp_latency <= 2) {
		/* The total memory write latency cannot be lower than the
		 * amount of cycles consumed by the hardware to do a write.
		 * Set the memory write latency to 0 cycles.
		 */
		mem_wresp_latency = 0;
	} else {
		mem_wresp_latency -= 2;

		dev_info(&tc->pdev->dev,
			 "Setting memory write response latency to %i cycles\n",
			 mem_wresp_latency);
	}

	mem_latency |= mem_wresp_latency << 16;

	spi_write(tc, 0x1009, mem_latency);

	if (spi_read(tc, 0x1009, &regval) != 0) {
		dev_err(&tc->pdev->dev,
			"Failed to read back memory latency register");
		return;
	}

	if (mem_latency != regval) {
		dev_err(&tc->pdev->dev,
			"Memory latency register doesn't match requested value"
			" (actual: %#08x, expected: %#08x)\n",
			regval, mem_latency);
	}
}

#endif /* defined(SUPPORT_RGX) */

static void odin_set_mem_mode(struct tc_device *tc)
{
	u32 val;

	if (tc->version != ODIN_VERSION_FPGA)
		return;

	/* Enable memory offset to be applied to DUT and PDP1 */
	iowrite32(0x80000A10, tc->tcf.registers + ODN_CORE_DUT_CTRL1);

	/* Apply memory offset to GPU and PDP1 to point to DDR memory.
	 * Enable HDMI.
	 */
	val = (0x4 << ODN_CORE_CONTROL_DUT_OFFSET_SHIFT) |
	      (0x4 << ODN_CORE_CONTROL_PDP1_OFFSET_SHIFT) |
	      (0x2 << ODN_CORE_CONTROL_HDMI_MODULE_EN_SHIFT) |
	      (0x1 << ODN_CORE_CONTROL_MCU_COMMUNICATOR_EN_SHIFT);
	iowrite32(val, tc->tcf.registers + ODN_CORE_CORE_CONTROL);
}

/* Do a hard reset on the DUT */
static int odin_hard_reset(struct tc_device *tc, int core_clock, int mem_clock)
{
#if defined(SUPPORT_RGX)
	if (tc->version == ODIN_VERSION_TCF_BONNIE)
		return odin_hard_reset_bonnie(tc);
	if (tc->version == ODIN_VERSION_FPGA)
		return odin_hard_reset_fpga(tc, core_clock, mem_clock);

	dev_err(&tc->pdev->dev, "Invalid Odin version");
	return 1;
#else /* defined(SUPPORT_RGX) */
	return 0;
#endif /* defined(SUPPORT_RGX) */
}

static int odin_hw_init(struct tc_device *tc, int core_clock, int mem_clock,
			int mem_latency, int mem_wresp_latency)
{
	int err;

	err = odin_hard_reset(tc, core_clock, mem_clock);
	if (err) {
		dev_err(&tc->pdev->dev, "Failed to initialise Odin");
		goto err_out;
	}

	odin_set_mem_mode(tc);

#if defined(SUPPORT_RGX)
	if (tc->version == ODIN_VERSION_FPGA)
		odin_set_mem_latency(tc, mem_latency, mem_wresp_latency);
#endif /* defined(SUPPORT_RGX) */

err_out:
	return err;
}

static int odin_enable_irq(struct tc_device *tc)
{
	int err = 0;

#if defined(TC_FAKE_INTERRUPTS)
	setup_timer(&tc->timer, tc_irq_fake_wrapper,
		(unsigned long)tc);
	mod_timer(&tc->timer,
		jiffies + msecs_to_jiffies(FAKE_INTERRUPT_TIME_MS));
#else
	iowrite32(0, tc->tcf.registers +
		ODN_CORE_INTERRUPT_ENABLE);
	iowrite32(0xffffffff, tc->tcf.registers +
		ODN_CORE_INTERRUPT_CLR);

	dev_info(&tc->pdev->dev,
		"Registering IRQ %d for use by Odin\n",
		tc->pdev->irq);

	err = request_irq(tc->pdev->irq, odin_irq_handler,
		IRQF_SHARED, DRV_NAME, tc);

	if (err) {
		dev_err(&tc->pdev->dev,
			"Error - IRQ %d failed to register\n",
			tc->pdev->irq);
	} else {
		dev_info(&tc->pdev->dev,
			"IRQ %d was successfully registered for use by Odin\n",
			tc->pdev->irq);
	}
#endif
	return err;
}

static void odin_disable_irq(struct tc_device *tc)
{
#if defined(TC_FAKE_INTERRUPTS)
	del_timer_sync(&tc->timer);
#else
	iowrite32(0, tc->tcf.registers +
			ODN_CORE_INTERRUPT_ENABLE);
	iowrite32(0xffffffff, tc->tcf.registers +
			ODN_CORE_INTERRUPT_CLR);

	free_irq(tc->pdev->irq, tc);
#endif
}

static enum tc_version_t
odin_detect_daughterboard_version(struct tc_device *tc)
{
	u32 reg = ioread32(tc->tcf.registers + ODN_REG_BANK_DB_TYPE_ID);
	u32 val = reg;

	val = (val & ODN_REG_BANK_DB_TYPE_ID_TYPE_MASK) >>
		ODN_REG_BANK_DB_TYPE_ID_TYPE_SHIFT;

	switch (val) {
	default:
		dev_err(&tc->pdev->dev,
			"Unknown odin version ID type %#x "
			"(DB_TYPE_ID: %#08x)\n",
			val, reg);
		return TC_INVALID_VERSION;
	case 1:
		dev_info(&tc->pdev->dev, "DUT: Bonnie TC\n");
		return ODIN_VERSION_TCF_BONNIE;
	case 2:
	case 3:
		dev_info(&tc->pdev->dev, "DUT: FPGA\n");
		return ODIN_VERSION_FPGA;
	}
}

static int odin_dev_init(struct tc_device *tc, struct pci_dev *pdev,
			 int pdp_mem_size, int secure_mem_size)
{
	int err;
	u32 val;

	/* Reserve and map the tcf system registers */
	err = setup_io_region(pdev, &tc->tcf,
		ODN_SYS_BAR, ODN_SYS_REGS_OFFSET, ODN_SYS_REGS_SIZE);
	if (err)
		goto err_out;

	tc->version = odin_detect_daughterboard_version(tc);
	if (tc->version == TC_INVALID_VERSION) {
		err = -EIO;
		goto err_odin_unmap_sys_registers;
	}

	/* Setup card memory */
	tc->tc_mem.base = pci_resource_start(pdev, ODN_DDR_BAR);
	tc->tc_mem.size = pci_resource_len(pdev, ODN_DDR_BAR);

	if (tc->tc_mem.size < pdp_mem_size) {
		dev_err(&pdev->dev,
			"Odin MEM region (bar %d) has size of %lu which is smaller than the requested PDP heap of %lu",
			ODN_DDR_BAR,
			(unsigned long)tc->tc_mem.size,
			(unsigned long)pdp_mem_size);

		err = -EIO;
		goto err_odin_unmap_sys_registers;
	}

#if defined(SUPPORT_FAKE_SECURE_ION_HEAP)
	if (tc->tc_mem.size <
	    (pdp_mem_size + secure_mem_size)) {
		dev_err(&pdev->dev,
			"Odin MEM region (bar %d) has size of %lu which is smaller than the requested PDP heap of %lu plus the requested secure heap size %lu",
			ODN_DDR_BAR,
			(unsigned long)tc->tc_mem.size,
			(unsigned long)pdp_mem_size,
			(unsigned long)secure_mem_size);
		err = -EIO;
		goto err_odin_unmap_sys_registers;
	}
#endif

	err = tc_mtrr_setup(tc);
	if (err)
		goto err_odin_unmap_sys_registers;

	/* Setup ranges for the device heaps */
	tc->pdp_heap_mem_size = pdp_mem_size;

	/* We know ext_heap_mem_size won't underflow as we've compared
	 * tc_mem.size against the pdp_mem_size value earlier
	 */
	tc->ext_heap_mem_size =
		tc->tc_mem.size - tc->pdp_heap_mem_size;

#if defined(SUPPORT_FAKE_SECURE_ION_HEAP)
	tc->ext_heap_mem_size -= secure_mem_size;
#endif

	if (tc->ext_heap_mem_size < TC_EXT_MINIMUM_MEM_SIZE) {
		dev_warn(&pdev->dev,
			"Odin MEM region (bar 4) has size of %lu, with %lu pdp_mem_size only %lu bytes are left for ext device, which looks too small",
			(unsigned long)tc->tc_mem.size,
			(unsigned long)pdp_mem_size,
			(unsigned long)tc->ext_heap_mem_size);
		/* Continue as this is only a 'helpful warning' not a hard
		 * requirement
		 */
	}
	tc->ext_heap_mem_base = tc->tc_mem.base;
	tc->pdp_heap_mem_base =
		tc->tc_mem.base + tc->ext_heap_mem_size;
#if defined(SUPPORT_FAKE_SECURE_ION_HEAP)
	tc->secure_heap_mem_base = tc->pdp_heap_mem_base +
		tc->pdp_heap_mem_size;
	tc->secure_heap_mem_size = secure_mem_size;
#endif

#if defined(SUPPORT_ION)
	err = tc_ion_init(tc, ODN_DDR_BAR);
	if (err) {
		dev_err(&pdev->dev, "Failed to initialise ION\n");
		goto err_odin_unmap_sys_registers;
	}
#endif

	val = ioread32(tc->tcf.registers + ODN_CORE_REVISION);
	dev_info(&pdev->dev, "ODN_CORE_REVISION = %08x\n", val);

	val = ioread32(tc->tcf.registers + ODN_CORE_CHANGE_SET);
	dev_info(&pdev->dev, "ODN_CORE_CHANGE_SET = %08x\n", val);

	val = ioread32(tc->tcf.registers + ODN_CORE_USER_ID);
	dev_info(&pdev->dev, "ODN_CORE_USER_ID = %08x\n", val);

	val = ioread32(tc->tcf.registers + ODN_CORE_USER_BUILD);
	dev_info(&pdev->dev, "ODN_CORE_USER_BUILD = %08x\n", val);

err_out:
	return err;

err_odin_unmap_sys_registers:
	dev_info(&pdev->dev,
		 "%s: failed - unmapping the io regions.\n", __func__);

	iounmap(tc->tcf.registers);
	release_pci_io_addr(pdev, ODN_SYS_BAR,
			 tc->tcf.region.base, tc->tcf.region.size);
	goto err_out;
}

static void odin_dev_cleanup(struct tc_device *tc)
{
#if defined(SUPPORT_ION)
	tc_ion_deinit(tc, ODN_DDR_BAR);
#endif

	tc_mtrr_cleanup(tc);

	iounmap(tc->tcf.registers);

	release_pci_io_addr(tc->pdev,
			ODN_SYS_BAR,
			tc->tcf.region.base,
			tc->tcf.region.size);
}

static u32 odin_interrupt_id_to_flag(int interrupt_id)
{
	switch (interrupt_id) {
	case TC_INTERRUPT_PDP:
		return ODN_INTERRUPT_ENABLE_PDP1;
	case TC_INTERRUPT_EXT:
		return ODN_INTERRUPT_ENABLE_DUT;
	default:
		BUG();
	}
}

int odin_init(struct tc_device *tc, struct pci_dev *pdev,
	      int core_clock, int mem_clock,
	      int pdp_mem_size, int secure_mem_size,
	      int mem_latency, int mem_wresp_latency)
{
	int err = 0;

	err = odin_dev_init(tc, pdev, pdp_mem_size, secure_mem_size);
	if (err) {
		dev_err(&pdev->dev, "odin_dev_init failed\n");
		goto err_out;
	}

	err = odin_hw_init(tc, core_clock, mem_clock,
			   mem_latency, mem_wresp_latency);
	if (err) {
		dev_err(&pdev->dev, "odin_hw_init failed\n");
		goto err_dev_cleanup;
	}

	err = odin_enable_irq(tc);
	if (err) {
		dev_err(&pdev->dev,
			"Failed to initialise IRQ\n");
		goto err_dev_cleanup;
	}

err_out:
	return err;

err_dev_cleanup:
	odin_dev_cleanup(tc);
	goto err_out;
}

int odin_cleanup(struct tc_device *tc)
{
	odin_disable_irq(tc);
	odin_dev_cleanup(tc);

	return 0;
}

int odin_register_pdp_device(struct tc_device *tc)
{
	int err = 0;
	resource_size_t reg_start = pci_resource_start(tc->pdev, ODN_SYS_BAR);
	struct resource pdp_resources_odin[] = {
		DEFINE_RES_MEM_NAMED(reg_start +
				ODN_PDP_REGS_OFFSET, /* start */
				ODN_PDP_REGS_SIZE, /* size */
				"pdp-regs"),
		DEFINE_RES_MEM_NAMED(reg_start +
				ODN_SYS_REGS_OFFSET +
				ODN_REG_BANK_ODN_CLK_BLK +
				ODN_PDP_P_CLK_OUT_DIVIDER_REG1, /* start */
				ODN_PDP_P_CLK_IN_DIVIDER_REG -
				ODN_PDP_P_CLK_OUT_DIVIDER_REG1 + 4, /* size */
				"pll-regs"),
		DEFINE_RES_MEM_NAMED(reg_start +
				ODN_SYS_REGS_OFFSET +
				ODN_REG_BANK_CORE, /* start */
				ODN_CORE_MMCM_LOCK_STATUS + 4, /* size */
				"odn-core"),
	};

	struct tc_pdp_platform_data pdata = {
#if defined(SUPPORT_ION) && (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
		.ion_device = tc->ion_device,
		.ion_heap_id = ION_HEAP_TC_PDP,
#endif
		.memory_base = tc->tc_mem.base,
		.pdp_heap_memory_base = tc->pdp_heap_mem_base,
		.pdp_heap_memory_size = tc->pdp_heap_mem_size,
	};
	struct platform_device_info pdp_device_info = {
		.parent = &tc->pdev->dev,
		.name = ODN_DEVICE_NAME_PDP,
		.id = -2,
		.data = &pdata,
		.size_data = sizeof(pdata),
#if (TC_MEMORY_CONFIG == TC_MEMORY_LOCAL) || \
	(TC_MEMORY_CONFIG == TC_MEMORY_HYBRID)
		/*
		 * The PDP cannot address system memory, so there is no
		 * DMA limitation.
		 */
		.dma_mask = DMA_BIT_MASK(64),
#else
		.dma_mask = DMA_BIT_MASK(32),
#endif
	};

	pdp_device_info.res = pdp_resources_odin;
	pdp_device_info.num_res = ARRAY_SIZE(pdp_resources_odin);

	tc->pdp_dev = platform_device_register_full(&pdp_device_info);
	if (IS_ERR(tc->pdp_dev)) {
		err = PTR_ERR(tc->pdp_dev);
		dev_err(&tc->pdev->dev,
			"Failed to register PDP device (%d)\n", err);
		tc->pdp_dev = NULL;
		goto err_out;
	}

err_out:
	return err;
}

int odin_register_ext_device(struct tc_device *tc)
{
#if defined(SUPPORT_RGX)
	int err = 0;
	struct resource odin_rogue_resources[] = {
		DEFINE_RES_MEM_NAMED(pci_resource_start(tc->pdev,
							ODN_DUT_SOCIF_BAR),
				     ODN_DUT_SOCIF_SIZE, "rogue-regs"),
	};
	struct tc_rogue_platform_data pdata = {
#if defined(SUPPORT_ION) && (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
		.ion_device = tc->ion_device,
		.ion_heap_id = ION_HEAP_TC_ROGUE,
#endif
		.tc_memory_base = tc->tc_mem.base,
		.pdp_heap_memory_base = tc->pdp_heap_mem_base,
		.pdp_heap_memory_size = tc->pdp_heap_mem_size,
		.rogue_heap_memory_base = tc->ext_heap_mem_base,
		.rogue_heap_memory_size = tc->ext_heap_mem_size,
#if defined(SUPPORT_FAKE_SECURE_ION_HEAP)
		.secure_heap_memory_base = tc->secure_heap_mem_base,
		.secure_heap_memory_size = tc->secure_heap_mem_size,
#endif
	};
	struct platform_device_info odin_rogue_dev_info = {
		.parent = &tc->pdev->dev,
		.name = TC_DEVICE_NAME_ROGUE,
		.id = -2,
		.res = odin_rogue_resources,
		.num_res = ARRAY_SIZE(odin_rogue_resources),
		.data = &pdata,
		.size_data = sizeof(pdata),
#if (TC_MEMORY_CONFIG == TC_MEMORY_LOCAL)
		/*
		 * The FPGA cannot address system memory, so there is no DMA
		 * limitation.
		 */
		.dma_mask = DMA_BIT_MASK(64),
#else
		.dma_mask = DMA_BIT_MASK(32),
#endif
	};

	tc->ext_dev
		= platform_device_register_full(&odin_rogue_dev_info);

	if (IS_ERR(tc->ext_dev)) {
		err = PTR_ERR(tc->ext_dev);
		dev_err(&tc->pdev->dev,
			"Failed to register rogue device (%d)\n", err);
		tc->ext_dev = NULL;
	}
	return err;
#else /* defined(SUPPORT_RGX) */
	return 0;
#endif /* defined(SUPPORT_RGX) */
}

void odin_enable_interrupt_register(struct tc_device *tc,
				    int interrupt_id)
{
	u32 val;
	u32 flag;

	switch (interrupt_id) {
	case TC_INTERRUPT_PDP:
		dev_info(&tc->pdev->dev,
			"Enabling Odin PDP interrupts\n");
		break;
	case TC_INTERRUPT_EXT:
		dev_info(&tc->pdev->dev,
			"Enabling Odin DUT interrupts\n");
		break;
	default:
		dev_err(&tc->pdev->dev,
			"Error - illegal interrupt id\n");
		return;
	}

	val = ioread32(tc->tcf.registers +
		       ODN_CORE_INTERRUPT_ENABLE);
	flag = odin_interrupt_id_to_flag(interrupt_id);
	val |= flag;
	iowrite32(val, tc->tcf.registers +
		  ODN_CORE_INTERRUPT_ENABLE);
}

void odin_disable_interrupt_register(struct tc_device *tc,
				     int interrupt_id)
{
	u32 val;

	switch (interrupt_id) {
	case TC_INTERRUPT_PDP:
		dev_info(&tc->pdev->dev,
			"Disabling Odin PDP interrupts\n");
		break;
	case TC_INTERRUPT_EXT:
		dev_info(&tc->pdev->dev,
			"Disabling Odin DUT interrupts\n");
		break;
	default:
		dev_err(&tc->pdev->dev,
			"Error - illegal interrupt id\n");
		return;
	}
	val = ioread32(tc->tcf.registers +
		       ODN_CORE_INTERRUPT_ENABLE);
	val &= ~(odin_interrupt_id_to_flag(interrupt_id));
	iowrite32(val, tc->tcf.registers +
		  ODN_CORE_INTERRUPT_ENABLE);
}

irqreturn_t odin_irq_handler(int irq, void *data)
{
	u32 interrupt_status;
	u32 interrupt_clear = 0;
	unsigned long flags;
	irqreturn_t ret = IRQ_NONE;
	struct tc_device *tc = (struct tc_device *)data;

	spin_lock_irqsave(&tc->interrupt_handler_lock, flags);

#if defined(TC_FAKE_INTERRUPTS)
	/* If we're faking interrupts pretend we got both ext and PDP ints */
	interrupt_status = ODN_INTERRUPT_STATUS_DUT
		| ODN_INTERRUPT_STATUS_PDP1;
#else
	interrupt_status = ioread32(tc->tcf.registers +
				    ODN_CORE_INTERRUPT_STATUS);
#endif

	if (interrupt_status & ODN_INTERRUPT_STATUS_DUT) {
		struct tc_interrupt_handler *ext_int =
			&tc->interrupt_handlers[TC_INTERRUPT_EXT];

		if (ext_int->enabled && ext_int->handler_function) {
			ext_int->handler_function(ext_int->handler_data);
			interrupt_clear |= ODN_INTERRUPT_CLEAR_DUT;
		}
		ret = IRQ_HANDLED;
	}
	if (interrupt_status & ODN_INTERRUPT_STATUS_PDP1) {
		struct tc_interrupt_handler *pdp_int =
			&tc->interrupt_handlers[TC_INTERRUPT_PDP];

		if (pdp_int->enabled && pdp_int->handler_function) {
			pdp_int->handler_function(pdp_int->handler_data);
			interrupt_clear |= ODN_INTERRUPT_CLEAR_PDP1;
		}
		ret = IRQ_HANDLED;
	}

	if (interrupt_clear)
		iowrite32(interrupt_clear,
			  tc->tcf.registers + ODN_CORE_INTERRUPT_CLR);

	spin_unlock_irqrestore(&tc->interrupt_handler_lock, flags);

	return ret;
}

int odin_sys_info(struct tc_device *tc, u32 *tmp, u32 *pll)
{
	*tmp = 0;
	*pll = 0;
	return 0;
}

int odin_sys_strings(struct tc_device *tc,
		     char *str_fpga_rev, size_t size_fpga_rev,
		     char *str_tcf_core_rev, size_t size_tcf_core_rev,
		     char *str_tcf_core_target_build_id,
		     size_t size_tcf_core_target_build_id,
		     char *str_pci_ver, size_t size_pci_ver,
		     char *str_macro_ver, size_t size_macro_ver)
{
	u32 val;
	char temp_str[12];

	/* Read the Odin major and minor revision ID register Rx-xx */
	val = ioread32(tc->tcf.registers + ODN_CORE_REVISION);

	snprintf(str_tcf_core_rev,
		 size_tcf_core_rev,
		 "%d.%d",
		 HEX2DEC((val & ODN_REVISION_MAJOR_MASK)
			 >> ODN_REVISION_MAJOR_SHIFT),
		 HEX2DEC((val & ODN_REVISION_MINOR_MASK)
			 >> ODN_REVISION_MINOR_SHIFT));

	dev_info(&tc->pdev->dev, "Odin core revision %s\n",
		 str_tcf_core_rev);

	/* Read the Odin register containing the Perforce changelist
	 * value that the FPGA build was generated from
	 */
	val = ioread32(tc->tcf.registers + ODN_CORE_CHANGE_SET);

	snprintf(str_tcf_core_target_build_id,
		 size_tcf_core_target_build_id,
		 "%d",
		 (val & ODN_CHANGE_SET_SET_MASK)
		 >> ODN_CHANGE_SET_SET_SHIFT);

	/* Read the Odin User_ID register containing the User ID for
	 * identification of a modified build
	 */
	val = ioread32(tc->tcf.registers + ODN_CORE_USER_ID);

	snprintf(temp_str,
		 sizeof(temp_str),
		 "%d",
		 HEX2DEC((val & ODN_USER_ID_ID_MASK)
			 >> ODN_USER_ID_ID_SHIFT));

	/* Read the Odin User_Build register containing the User build
	 * number for identification of modified builds
	 */
	val = ioread32(tc->tcf.registers + ODN_CORE_USER_BUILD);

	snprintf(temp_str,
		 sizeof(temp_str),
		 "%d",
		 HEX2DEC((val & ODN_USER_BUILD_BUILD_MASK)
			 >> ODN_USER_BUILD_BUILD_SHIFT));

	return 0;
}
