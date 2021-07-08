/*
 * @Codingstyle LinuxKernel
 * @Copyright   Copyright (c) Imagination Technologies Ltd. All Rights Reserved
 * @License     Dual MIT/GPLv2
 *
 * The contents of this file are subject to the MIT license as set out below.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License Version 2 ("GPL") in which case the provisions
 * of GPL are applicable instead of those above.
 *
 * If you wish to allow use of your version of this file only under the terms of
 * GPL, and not to allow others to use your version of this file under the terms
 * of the MIT license, indicate your decision by deleting the provisions above
 * and replace them with the notice and other provisions required by GPL as set
 * out in the file called "GPL-COPYING" included in this distribution. If you do
 * not delete the provisions above, a recipient may use your version of this file
 * under the terms of either the MIT license or GPL.
 *
 * This License is also included in this distribution in the file called
 * "MIT-COPYING".
 *
 * EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
 * PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

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
#include <linux/dmaengine.h>

#include "tc_drv_internal.h"
#include "tc_odin.h"
#include "tc_ion.h"

/* Odin (3rd gen TCF FPGA) */
#include "odin_defs.h"
#include "odin_regs.h"
#include "bonnie_tcf.h"
#include "tc_clocks.h"

/* Orion demo platform */
#include "orion_defs.h"
#include "orion_regs.h"

/* Odin/Orion common registers */
#include "tc_odin_common_regs.h"

/* Macros to set and get register fields */
#define REG_FIELD_GET(v, str) \
	(u32)(((v) & (str##_MASK)) >> (str##_SHIFT))
#define REG_FIELD_SET(v, f, str) \
	(v = (u32)(((v) & (u32)~(str##_MASK)) |		\
		   (u32)(((f) << (str##_SHIFT)) & (str##_MASK))))

#define SAI_STATUS_UNALIGNED 0
#define SAI_STATUS_ALIGNED   1
#define SAI_STATUS_ERROR     2

/* Odin/Orion shared masks */
static const u32 REVISION_MAJOR_MASK[] = {
	ODN_REVISION_MAJOR_MASK,
	SRS_REVISION_MAJOR_MASK
};
static const u32 REVISION_MAJOR_SHIFT[] = {
	ODN_REVISION_MAJOR_SHIFT,
	SRS_REVISION_MAJOR_SHIFT
};
static const u32 REVISION_MINOR_MASK[] = {
	ODN_REVISION_MINOR_MASK,
	SRS_REVISION_MINOR_MASK
};
static const u32 REVISION_MINOR_SHIFT[] = {
	ODN_REVISION_MINOR_SHIFT,
	SRS_REVISION_MINOR_SHIFT
};
static const u32 CHANGE_SET_SET_MASK[] = {
	ODN_CHANGE_SET_SET_MASK,
	SRS_CHANGE_SET_SET_MASK
};
static const u32 CHANGE_SET_SET_SHIFT[] = {
	ODN_CHANGE_SET_SET_SHIFT,
	SRS_CHANGE_SET_SET_SHIFT
};
static const u32 USER_ID_ID_MASK[] = {
	ODN_USER_ID_ID_MASK,
	SRS_USER_ID_ID_MASK
};
static const u32 USER_ID_ID_SHIFT[] = {
	ODN_USER_ID_ID_SHIFT,
	SRS_USER_ID_ID_SHIFT
};
static const u32 USER_BUILD_BUILD_MASK[] = {
	ODN_USER_BUILD_BUILD_MASK,
	SRS_USER_BUILD_BUILD_MASK
};
static const u32 USER_BUILD_BUILD_SHIFT[] = {
	ODN_USER_BUILD_BUILD_SHIFT,
	SRS_USER_BUILD_BUILD_SHIFT
};
static const u32 INPUT_CLOCK_SPEED_MIN[] = {
	ODN_INPUT_CLOCK_SPEED_MIN,
	SRS_INPUT_CLOCK_SPEED_MIN
};
static const u32 INPUT_CLOCK_SPEED_MAX[] = {
	ODN_INPUT_CLOCK_SPEED_MAX,
	SRS_INPUT_CLOCK_SPEED_MAX
};
static const u32 OUTPUT_CLOCK_SPEED_MIN[] = {
	ODN_OUTPUT_CLOCK_SPEED_MIN,
	SRS_OUTPUT_CLOCK_SPEED_MIN
};
static const u32 OUTPUT_CLOCK_SPEED_MAX[] = {
	ODN_OUTPUT_CLOCK_SPEED_MAX,
	SRS_OUTPUT_CLOCK_SPEED_MAX
};
static const u32 VCO_MIN[] = {
	ODN_VCO_MIN,
	SRS_VCO_MIN
};
static const u32 VCO_MAX[] = {
	ODN_VCO_MAX,
	SRS_VCO_MAX
};
static const u32 PFD_MIN[] = {
	ODN_PFD_MIN,
	SRS_PFD_MIN
};
static const u32 PFD_MAX[] = {
	ODN_PFD_MAX,
	SRS_PFD_MAX
};

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

/* Returns 1 for aligned, 0 for unaligned */
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
	return mca_status & 1; /* 'alignment found' status is in bit 1 */
}

/* Returns 1 for aligned, 0 for unaligned */
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
	u32 tcver = tc_odin_subvers(dev);
	u32 best_diff, d_best, m_best, o_best;
	u32 m_min, m_max, m_ideal;
	u32 d_cur, m_cur, o_cur;
	u32 d_min, d_max;

	/*
	 * Check specified input frequency is within range
	 */
	if (freq_input < INPUT_CLOCK_SPEED_MIN[tcver]) {
		dev_err(dev, "Input frequency (%u hz) below minimum supported value (%u hz)\n",
			freq_input, INPUT_CLOCK_SPEED_MIN[tcver]);
		return -EINVAL;
	}
	if (freq_input > INPUT_CLOCK_SPEED_MAX[tcver]) {
		dev_err(dev, "Input frequency (%u hz) above maximum supported value (%u hz)\n",
			freq_input, INPUT_CLOCK_SPEED_MAX[tcver]);
		return -EINVAL;
	}

	/*
	 * Check specified target frequency is within range
	 */
	if (freq_output < OUTPUT_CLOCK_SPEED_MIN[tcver]) {
		dev_err(dev, "Output frequency (%u hz) below minimum supported value (%u hz)\n",
			freq_input, OUTPUT_CLOCK_SPEED_MIN[tcver]);
		return -EINVAL;
	}
	if (freq_output > OUTPUT_CLOCK_SPEED_MAX[tcver]) {
		dev_err(dev, "Output frequency (%u hz) above maximum supported value (%u hz)\n",
			freq_output, OUTPUT_CLOCK_SPEED_MAX[tcver]);
		return -EINVAL;
	}

	/*
	 * Calculate min and max for Input Divider.
	 * Refer Xilinx 7 series FPGAs clocking resources user guide
	 * equation 3-6 and 3-7
	 */
	d_min = DIV_ROUND_UP(freq_input, PFD_MAX[tcver]);
	d_max = min(freq_input/PFD_MIN[tcver], (u32)ODN_DREG_VALUE_MAX);

	/*
	 * Calculate min and max for Input Divider.
	 * Refer Xilinx 7 series FPGAs clocking resources user guide.
	 * equation 3-8 and 3-9
	 */
	m_min = DIV_ROUND_UP((VCO_MIN[tcver] * d_min), freq_input);
	m_max = min(((VCO_MAX[tcver] * d_max) / freq_input),
		    (u32)ODN_MREG_VALUE_MAX);

	for (d_cur = d_min; d_cur <= d_max; d_cur++) {
		/*
		 * Refer Xilinx 7 series FPGAs clocking resources user guide.
		 * equation 3-10
		 */
		m_ideal = min(((d_cur * VCO_MAX[tcver])/freq_input), m_max);

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

	/*
	 * Failed to find exact optimal solution with high VCO. Brute-force find
	 * a suitable config, again prioritising high VCO, to get lowest jitter
	 */
	d_min = 1; d_max = (u32)ODN_DREG_VALUE_MAX;
	m_min = 1; m_max = (u32)ODN_MREG_VALUE_MAX;
	best_diff = 0xFFFFFFFF;

	for (d_cur = d_min; d_cur <= d_max; d_cur++) {
		for (m_cur = m_max; m_cur >= m_min; m_cur -= 1) {
			u32 pfd, vco, o_avg, o_min, o_max;

			pfd = freq_input / d_cur;
			vco = pfd * m_cur;

			if (pfd < PFD_MIN[tcver])
				continue;

			if (pfd > PFD_MAX[tcver])
				continue;

			if (vco < VCO_MIN[tcver])
				continue;

			if (vco > VCO_MAX[tcver])
				continue;

			/*
			 * A range of -1/+3 around o_avg gives us 100kHz granularity.
			 * It can be extended further.
			 */
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

	/* Read-modify-write the required fields to multiplier register 1 */
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

	/* Read-modify-write the required fields to multiplier register 1 */
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
	int dut_clk_info = 0;

#if defined(SUPPORT_FPGA_DUT_CLK_INFO)
	dut_clk_info = ioread32(tc->tcf.registers + ODN_CORE_DUT_CLK_INFO);
#endif

	if ((dut_clk_info != 0) && (dut_clk_info != 0xbaadface) && (dut_clk_info != 0xffffffff)) {
		dev_info(dev, "ODN_DUT_CLK_INFO = %08x\n", dut_clk_info);

		if (*core_clock == 0) {
			*core_clock = ((dut_clk_info & ODN_DUT_CLK_INFO_CORE_MASK)
					   >> ODN_DUT_CLK_INFO_CORE_SHIFT) * 1000000;
			dev_info(dev, "Using register DUT core clock value: %i\n",
						*core_clock);
		} else {
			dev_info(dev, "Using module param DUT core clock value: %i\n",
						*core_clock);
		}

		if (*mem_clock == 0) {
			*mem_clock = ((dut_clk_info & ODN_DUT_CLK_INFO_MEM_MASK)
				   >> ODN_DUT_CLK_INFO_MEM_SHIFT) * 1000000;
			dev_info(dev, "Using register DUT mem clock value: %i\n",
			 *mem_clock);
		} else {
			dev_info(dev, "Using module param DUT mem clock value: %i\n",
						*mem_clock);
		}

		return;
	}

	if (*core_clock == 0) {
		*core_clock = RGX_TC_CORE_CLOCK_SPEED;
		dev_info(dev, "Using default DUT core clock value: %i\n",
				 *core_clock);
	} else {
		dev_info(dev, "Using module param DUT core clock value: %i\n",
					*core_clock);
	}

	if (*mem_clock == 0) {
		*mem_clock = RGX_TC_MEM_CLOCK_SPEED;
		dev_info(dev, "Using default DUT mem clock value: %i\n",
				 *mem_clock);
	} else {
		dev_info(dev, "Using module param DUT mem clock value: %i\n",
					*mem_clock);
	}
}

static int odin_hard_reset_fpga(struct tc_device *tc,
				int *core_clock, int *mem_clock)
{
	int err = 0;

	odin_fpga_update_dut_clk_freq(tc, core_clock, mem_clock);

	err = odin_fpga_set_dut_core_clk(tc, ODN_INPUT_CLOCK_SPEED, *core_clock);
	if (err != 0)
		goto err_out;

	err = odin_fpga_set_dut_if_clk(tc, ODN_INPUT_CLOCK_SPEED, *mem_clock);

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

		/* Hold the DUT in reset for 50ms */
		msleep(50);

		/* Take the DUT out of reset */
		iowrite32(3, /* set bit 0 hi */
			tc->tcf.registers
			+ ODN_CORE_EXTERNAL_RESETN);
		reset_cnt++;

		/* Wait 200ms for the DUT to stabilise */
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
			"Memory latency register doesn't match requested value (actual: %#08x, expected: %#08x)\n",
			regval, mem_latency);
	}
}

static int orion_set_dut_core_clk(struct tc_device *tc,
				  u32 input_clk,
				  u32 output_clk)
{
	void __iomem *base = tc->tcf.registers;
	void __iomem *clk_blk_base = base + SRS_REG_BANK_ODN_CLK_BLK;
	struct device *dev = &tc->pdev->dev;
	u32 high_time, low_time, edge, no_count;
	u32 in_div, mul, out_div;
	u32 value;
	int err;

	err = odin_mmcm_counter_calc(dev, input_clk, output_clk, &in_div,
				     &mul, &out_div);
	if (err != 0)
		return err;

	/* Put DUT into reset */
	iowrite32(0, base + SRS_CORE_DUT_SOFT_RESETN);
	msleep(20);

	/* Put DUT Core MMCM into reset */
	iowrite32(SRS_CLK_GEN_RESET_DUT_CORE_MMCM_MASK,
		  base + SRS_CORE_CLK_GEN_RESET);
	msleep(20);

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

	/* Calculate the register fields for multiplier */
	odin_mmcm_reg_param_calc(mul, &high_time, &low_time,
				 &edge, &no_count);

	/* Read-modify-write the required fields to multiplier register 1 */
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

	/* Calculate the register fields for output divider */
	odin_mmcm_reg_param_calc(out_div, &high_time, &low_time,
				 &edge, &no_count);

	/*
	 * Read-modify-write the required fields to
	 * core output divider register 1
	 */
	value = ioread32(clk_blk_base + SRS_DUT_CORE_CLK_OUT_DIVIDER1);
	REG_FIELD_SET(value, high_time,
			SRS_DUT_CORE_CLK_OUT_DIVIDER1_HI_TIME);
	REG_FIELD_SET(value, low_time,
			SRS_DUT_CORE_CLK_OUT_DIVIDER1_LO_TIME);
	iowrite32(value, clk_blk_base + SRS_DUT_CORE_CLK_OUT_DIVIDER1);

	/*
	 * Read-modify-write the required fields to core output
	 * divider register 2
	 */
	value = ioread32(clk_blk_base + SRS_DUT_CORE_CLK_OUT_DIVIDER2);
	REG_FIELD_SET(value, edge,
			SRS_DUT_CORE_CLK_OUT_DIVIDER2_EDGE);
	REG_FIELD_SET(value, no_count,
			SRS_DUT_CORE_CLK_OUT_DIVIDER2_NOCOUNT);
	iowrite32(value, clk_blk_base + SRS_DUT_CORE_CLK_OUT_DIVIDER2);

	/*
	 * Read-modify-write the required fields to
	 * reference output divider register 1
	 */
	value = ioread32(clk_blk_base + SRS_DUT_REF_CLK_OUT_DIVIDER1);
	REG_FIELD_SET(value, high_time,
			SRS_DUT_CORE_CLK_OUT_DIVIDER1_HI_TIME);
	REG_FIELD_SET(value, low_time,
			SRS_DUT_CORE_CLK_OUT_DIVIDER1_LO_TIME);
	iowrite32(value, clk_blk_base + SRS_DUT_REF_CLK_OUT_DIVIDER1);

	/*
	 * Read-modify-write the required fields to
	 * reference output divider register 2
	 */
	value = ioread32(clk_blk_base + SRS_DUT_REF_CLK_OUT_DIVIDER2);
	REG_FIELD_SET(value, edge,
			SRS_DUT_REF_CLK_OUT_DIVIDER2_EDGE);
	REG_FIELD_SET(value, no_count,
			SRS_DUT_REF_CLK_OUT_DIVIDER2_NOCOUNT);
	iowrite32(value, clk_blk_base + SRS_DUT_REF_CLK_OUT_DIVIDER2);

	/* Bring DUT IF clock MMCM out of reset */
	iowrite32(0, tc->tcf.registers + SRS_CORE_CLK_GEN_RESET);

	err = tc_iopol32_nonzero(SRS_MMCM_LOCK_STATUS_DUT_CORE_MASK,
				 base + SRS_CORE_MMCM_LOCK_STATUS);
	if (err != 0) {
		dev_err(dev, "MMCM failed to lock for DUT core\n");
		return err;
	}

	/* Bring DUT out of reset */
	iowrite32(SRS_DUT_SOFT_RESETN_EXTERNAL_MASK,
		  tc->tcf.registers + SRS_CORE_DUT_SOFT_RESETN);
	msleep(20);

	dev_info(dev, "DUT core clock set-up successful\n");

	return err;
}

static int orion_set_dut_sys_mem_clk(struct tc_device *tc,
				     u32 input_clk,
				     u32 output_clk)
{
	void __iomem *base = tc->tcf.registers;
	void __iomem *clk_blk_base = base + SRS_REG_BANK_ODN_CLK_BLK;
	struct device *dev = &tc->pdev->dev;
	u32 high_time, low_time, edge, no_count;
	u32 in_div, mul, out_div;
	u32 value;
	int err;

	err = odin_mmcm_counter_calc(dev, input_clk, output_clk, &in_div,
				     &mul, &out_div);
	if (err != 0)
		return err;

	/* Put DUT into reset */
	iowrite32(0, base + SRS_CORE_DUT_SOFT_RESETN);
	msleep(20);

	/* Put DUT Core MMCM into reset */
	iowrite32(SRS_CLK_GEN_RESET_DUT_IF_MMCM_MASK,
		  base + SRS_CORE_CLK_GEN_RESET);
	msleep(20);

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

	/* Calculate the register fields for multiplier */
	odin_mmcm_reg_param_calc(mul, &high_time, &low_time,
				 &edge, &no_count);

	/* Read-modify-write the required fields to multiplier register 1 */
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

	/*
	 * New to Orion, registers undocumented in the TRM, assumed high_time,
	 * low_time, edge and no_count are in the same bit fields as the
	 * previous two registers Even though these registers seem to be
	 * undocumented, setting them is essential for the DUT not to show
	 * abnormal behaviour, like the firmware jumping to random addresses
	 */

	/*
	 * Read-modify-write the required fields to memory clock output divider
	 * register 1
	 */
	value = ioread32(clk_blk_base + SRS_DUT_MEM_CLK_OUT_DIVIDER1);
	REG_FIELD_SET(value, high_time,
			SRS_DUT_MEM_CLK_OUT_DIVIDER1_HI_TIME);
	REG_FIELD_SET(value, low_time,
			SRS_DUT_MEM_CLK_OUT_DIVIDER1_LO_TIME);
	iowrite32(value, clk_blk_base + SRS_DUT_MEM_CLK_OUT_DIVIDER1);

	/*
	 * Read-modify-write the required fields to memory clock output divider
	 * register 1
	 */
	value = ioread32(clk_blk_base + SRS_DUT_MEM_CLK_OUT_DIVIDER2);
	REG_FIELD_SET(value, edge,
			SRS_DUT_MEM_CLK_OUT_DIVIDER2_EDGE);
	REG_FIELD_SET(value, no_count,
			SRS_DUT_MEM_CLK_OUT_DIVIDER2_NOCOUNT);
	iowrite32(value, clk_blk_base + SRS_DUT_MEM_CLK_OUT_DIVIDER2);

	/* Bring DUT clock MMCM out of reset */
	iowrite32(0, tc->tcf.registers + SRS_CORE_CLK_GEN_RESET);

	err = tc_iopol32_nonzero(SRS_MMCM_LOCK_STATUS_DUT_IF_MASK,
				 base + SRS_CORE_MMCM_LOCK_STATUS);
	if (err != 0) {
		dev_err(dev, "MMCM failed to lock for DUT IF\n");
		return err;
	}

	/* Bring DUT out of reset */
	iowrite32(SRS_DUT_SOFT_RESETN_EXTERNAL_MASK,
		  tc->tcf.registers + SRS_CORE_DUT_SOFT_RESETN);
	msleep(20);

	dev_info(dev, "DUT IF clock set-up successful\n");

	return err;
}


static int orion_hard_reset(struct tc_device *tc, int *core_clock, int *mem_clock)
{
	int err;
	struct device *dev = &tc->pdev->dev;

	if (*core_clock == 0) {
		*core_clock = RGX_TC_CORE_CLOCK_SPEED;
		dev_info(dev, "Using default DUT core clock value: %i\n",
				 *core_clock);
	} else {
		dev_info(dev, "Using module param DUT core clock value: %i\n",
					*core_clock);
	}

	if (*mem_clock == 0) {
		*mem_clock = RGX_TC_MEM_CLOCK_SPEED;
		dev_info(dev, "Using default DUT mem clock value: %i\n",
				 *mem_clock);
	} else {
		dev_info(dev, "Using module param DUT mem clock value: %i\n",
					*mem_clock);
	}

	err = orion_set_dut_core_clk(tc, SRS_INPUT_CLOCK_SPEED, *core_clock);
	if (err != 0)
		goto err_out;

	err = orion_set_dut_sys_mem_clk(tc, SRS_INPUT_CLOCK_SPEED, *mem_clock);

err_out:
	return err;
}

#endif /* defined(SUPPORT_RGX) */

/* Do a hard reset on the DUT */
static int odin_hard_reset(struct tc_device *tc, int *core_clock, int *mem_clock)
{
#if defined(SUPPORT_RGX)
	if (tc->version == ODIN_VERSION_TCF_BONNIE)
		return odin_hard_reset_bonnie(tc);
	if (tc->version == ODIN_VERSION_FPGA)
		return odin_hard_reset_fpga(tc, core_clock, mem_clock);
	if (tc->version == ODIN_VERSION_ORION)
		return orion_hard_reset(tc, core_clock, mem_clock);

	dev_err(&tc->pdev->dev, "Invalid Odin version");
	return 1;
#else /* defined(SUPPORT_RGX) */
	return 0;
#endif /* defined(SUPPORT_RGX) */
}

static void odin_set_mem_mode_lma(struct tc_device *tc)
{
	u32 val;

	if (tc->version != ODIN_VERSION_FPGA)
		return;

	/* Enable memory offset to be applied to DUT and PDPs */
	iowrite32(0x80000A10, tc->tcf.registers + ODN_CORE_DUT_CTRL1);

	/* Apply memory offset to GPU and PDPs to point to DDR memory.
	 * Enable HDMI.
	 */
	val = (0x4 << ODN_CORE_CONTROL_DUT_OFFSET_SHIFT) |
	      (0x4 << ODN_CORE_CONTROL_PDP1_OFFSET_SHIFT) |
	      (0x4 << ODN_CORE_CONTROL_PDP2_OFFSET_SHIFT) |
	      (0x2 << ODN_CORE_CONTROL_HDMI_MODULE_EN_SHIFT) |
	      (0x1 << ODN_CORE_CONTROL_MCU_COMMUNICATOR_EN_SHIFT);
	iowrite32(val, tc->tcf.registers + ODN_CORE_CORE_CONTROL);
}

static int odin_set_mem_mode(struct tc_device *tc, int mem_mode)
{
	switch (mem_mode) {
	case TC_MEMORY_LOCAL:
		odin_set_mem_mode_lma(tc);
		dev_info(&tc->pdev->dev, "Memory mode: TC_MEMORY_LOCAL\n");
		break;
	default:
		dev_err(&tc->pdev->dev, "unsupported memory mode = %d\n",
			mem_mode);
		return -EINVAL;
	};

	tc->mem_mode = mem_mode;

	return 0;
}

static u64 odin_get_pdp_dma_mask(struct tc_device *tc)
{
	/* Does not access system memory, so there is no DMA limitation */
	if ((tc->mem_mode == TC_MEMORY_LOCAL) ||
	    (tc->mem_mode == TC_MEMORY_HYBRID))
		return DMA_BIT_MASK(64);

	return DMA_BIT_MASK(32);
}

#if defined(SUPPORT_RGX)
static u64 odin_get_rogue_dma_mask(struct tc_device *tc)
{
	/* Does not access system memory, so there is no DMA limitation */
	if (tc->mem_mode == TC_MEMORY_LOCAL)
		return DMA_BIT_MASK(64);

	return DMA_BIT_MASK(32);
}
#endif /* defined(SUPPORT_RGX) */

static void odin_set_fbc_bypass(struct tc_device *tc, bool fbc_bypass)
{
	u32 val;

	/* Register field is present whether TC has PFIM support or not */
	val = ioread32(tc->tcf.registers + ODN_CORE_DUT_CTRL1);
	REG_FIELD_SET(val, fbc_bypass ? 0x1 : 0x0,
		      ODN_DUT_CTRL1_FBDC_BYPASS);
	iowrite32(val, tc->tcf.registers + ODN_CORE_DUT_CTRL1);

	tc->fbc_bypass = fbc_bypass;
}

static int odin_hw_init(struct tc_device *tc, int *core_clock,
			int *mem_clock, int mem_latency,
			int mem_wresp_latency, int mem_mode,
			bool fbc_bypass)
{
	int err;

	err = odin_hard_reset(tc, core_clock, mem_clock);
	if (err) {
		dev_err(&tc->pdev->dev, "Failed to initialise Odin");
		goto err_out;
	}

	err = odin_set_mem_mode(tc, mem_mode);
	if (err)
		goto err_out;

	odin_set_fbc_bypass(tc, fbc_bypass);

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
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0))
	timer_setup(&tc->timer, tc_irq_fake_wrapper, 0);
#else
	setup_timer(&tc->timer, tc_irq_fake_wrapper, (unsigned long)tc);
#endif
	mod_timer(&tc->timer,
		jiffies + msecs_to_jiffies(FAKE_INTERRUPT_TIME_MS));
#else
	iowrite32(0, tc->tcf.registers +
		  common_reg_offset(tc, CORE_INTERRUPT_ENABLE));
	iowrite32(0xffffffff, tc->tcf.registers +
		  common_reg_offset(tc, CORE_INTERRUPT_CLR));

	dev_info(&tc->pdev->dev,
		"Registering IRQ %d for use by %s\n",
		 tc->pdev->irq,
		 odin_tc_name(tc));

	err = request_irq(tc->pdev->irq, odin_irq_handler,
		IRQF_SHARED, DRV_NAME, tc);

	if (err) {
		dev_err(&tc->pdev->dev,
			"Error - IRQ %d failed to register\n",
			tc->pdev->irq);
	} else {
		dev_info(&tc->pdev->dev,
			"IRQ %d was successfully registered for use by %s\n",
			 tc->pdev->irq,
			 odin_tc_name(tc));
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
			common_reg_offset(tc, CORE_INTERRUPT_ENABLE));
	iowrite32(0xffffffff, tc->tcf.registers +
			common_reg_offset(tc, CORE_INTERRUPT_CLR));

	free_irq(tc->pdev->irq, tc);
#endif
}

static enum tc_version_t
odin_detect_daughterboard_version(struct tc_device *tc)
{
	u32 reg = ioread32(tc->tcf.registers + ODN_REG_BANK_DB_TYPE_ID);
	u32 val = reg;

	if (tc->orion)
		return ODIN_VERSION_ORION;

	val = (val & ODN_REG_BANK_DB_TYPE_ID_TYPE_MASK) >>
		ODN_REG_BANK_DB_TYPE_ID_TYPE_SHIFT;

	switch (val) {
	default:
		dev_err(&tc->pdev->dev,
			"Unknown odin version ID type %#x (DB_TYPE_ID: %#08x)\n",
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
			"%s MEM region (bar %d) has size of %lu which is smaller than the requested PDP heap of %lu",
			odin_tc_name(tc),
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
			"Odin MEM region (bar %d) has size of %lu which is smaller than the requested PDP heap of %lu"
			" plus the requested secure heap size %lu",
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
			 "%s MEM region (bar 4) has size of %lu, with %lu pdp_mem_size only %lu bytes are left for "
			 "ext device, which looks too small",
			 odin_tc_name(tc),
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

	/* CDMA initialisation */
	val = ioread32(tc->tcf.registers + ODN_CORE_SUPPORTED_FEATURES);
	tc->dma_nchan = REG_FIELD_GET(val,
				       ODN_SUPPORTED_FEATURES_2X_CDMA_AND_IRQS);
	tc->dma_nchan++;
	dev_info(&tc->pdev->dev, "Odin RTL has %u DMA(s)\n", tc->dma_nchan);
	mutex_init(&tc->dma_mutex);

	val = ioread32(tc->tcf.registers +
		       common_reg_offset(tc, CORE_REVISION));
	dev_info(&pdev->dev, "%s = 0x%08x\n",
		 common_reg_name(tc, CORE_REVISION), val);

	val = ioread32(tc->tcf.registers +
		       common_reg_offset(tc, CORE_CHANGE_SET));
	dev_info(&pdev->dev, "%s = 0x%08x\n",
		 common_reg_name(tc, CORE_CHANGE_SET), val);

	val = ioread32(tc->tcf.registers +
		       common_reg_offset(tc, CORE_USER_ID));
	dev_info(&pdev->dev, "%s = 0x%08x\n",
		 common_reg_name(tc, CORE_USER_ID), val);

	val = ioread32(tc->tcf.registers +
		       common_reg_offset(tc, CORE_USER_BUILD));
	dev_info(&pdev->dev, "%s = 0x%08x\n",
		 common_reg_name(tc, CORE_USER_BUILD), val);

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
	case TC_INTERRUPT_PDP2:
		return ODN_INTERRUPT_ENABLE_PDP2;
	case TC_INTERRUPT_CDMA:
		return ODN_INTERRUPT_ENABLE_CDMA;
	case TC_INTERRUPT_CDMA2:
		return ODN_INTERRUPT_ENABLE_CDMA2;
	default:
		BUG();
	}
}

int odin_init(struct tc_device *tc, struct pci_dev *pdev,
	      int *core_clock, int *mem_clock,
	      int pdp_mem_size, int secure_mem_size,
	      int mem_latency, int mem_wresp_latency, int mem_mode,
	      bool fbc_bypass)
{
	int err = 0;

	err = odin_dev_init(tc, pdev, pdp_mem_size, secure_mem_size);
	if (err) {
		dev_err(&pdev->dev, "odin_dev_init failed\n");
		goto err_out;
	}

	err = odin_hw_init(tc, core_clock, mem_clock,
			   mem_latency, mem_wresp_latency, mem_mode,
			   fbc_bypass);
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
	/*
	 * Make sure we don't attempt to clean-up after an invalid device.
	 * We'll have already unmapped the PCI i/o space so cannot access
	 * anything now.
	 */
	if (tc->version != TC_INVALID_VERSION) {
		odin_disable_irq(tc);
		odin_dev_cleanup(tc);
	}

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
				ODN_PDP2_REGS_OFFSET, /* start */
				ODN_PDP2_REGS_SIZE, /* size */
				"pdp2-regs"),
		DEFINE_RES_MEM_NAMED(reg_start +
				ODN_SYS_REGS_OFFSET +
				common_reg_offset(tc, REG_BANK_ODN_CLK_BLK) +
				ODN_PDP_P_CLK_OUT_DIVIDER_REG1, /* start */
				ODN_PDP_P_CLK_IN_DIVIDER_REG -
				ODN_PDP_P_CLK_OUT_DIVIDER_REG1 + 4, /* size */
				"pll-regs"),
		DEFINE_RES_MEM_NAMED(reg_start +
				ODN_PDP2_PFIM_OFFSET, /* start */
				ODN_PDP2_PFIM_SIZE, /* size */
				"pfim-regs"),
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
		.dma_mask = odin_get_pdp_dma_mask(tc),
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
		.mem_mode = tc->mem_mode,
		.tc_memory_base = tc->tc_mem.base,
		.pdp_heap_memory_base = tc->pdp_heap_mem_base,
		.pdp_heap_memory_size = tc->pdp_heap_mem_size,
		.rogue_heap_memory_base = tc->ext_heap_mem_base,
		.rogue_heap_memory_size = tc->ext_heap_mem_size,
#if defined(SUPPORT_FAKE_SECURE_ION_HEAP)
		.secure_heap_memory_base = tc->secure_heap_mem_base,
		.secure_heap_memory_size = tc->secure_heap_mem_size,
#endif
		.tc_dma_tx_chan_name = ODIN_DMA_TX_CHAN_NAME,
		.tc_dma_rx_chan_name = ODIN_DMA_RX_CHAN_NAME,
	};
	struct platform_device_info odin_rogue_dev_info = {
		.parent = &tc->pdev->dev,
		.name = TC_DEVICE_NAME_ROGUE,
		.id = -2,
		.res = odin_rogue_resources,
		.num_res = ARRAY_SIZE(odin_rogue_resources),
		.data = &pdata,
		.size_data = sizeof(pdata),
		.dma_mask = odin_get_rogue_dma_mask(tc),
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

int odin_register_dma_device(struct tc_device *tc)
{
	resource_size_t reg_start = pci_resource_start(tc->pdev, ODN_SYS_BAR);
	int err = 0;

	struct resource odin_cdma_resources[] = {
		DEFINE_RES_MEM_NAMED(reg_start +
				     ODIN_DMA_REGS_OFFSET,     /* start */
				     ODIN_DMA_REGS_SIZE,       /* size */
				     "cdma-regs"),
		DEFINE_RES_IRQ_NAMED(TC_INTERRUPT_CDMA,
				     "cdma-irq"),
		DEFINE_RES_IRQ_NAMED(TC_INTERRUPT_CDMA2,
				     "cdma-irq2"),
	};

	struct tc_dma_platform_data pdata = {
		.addr_width = ODN_CDMA_ADDR_WIDTH,
		.num_dmas = tc->dma_nchan,
		.has_dre = true,
		.has_sg = true,
	};

	struct platform_device_info odin_cdma_dev_info = {
		.parent = &tc->pdev->dev,
		.name = ODN_DEVICE_NAME_CDMA,
		.id = -1,
		.res = odin_cdma_resources,
		.num_res = ARRAY_SIZE(odin_cdma_resources),
		.dma_mask = DMA_BIT_MASK(ODN_CDMA_ADDR_WIDTH),
		.data = &pdata,
		.size_data = sizeof(pdata),
	};

	tc->dma_dev
		= platform_device_register_full(&odin_cdma_dev_info);

	if (IS_ERR(tc->dma_dev)) {
		err = PTR_ERR(tc->dma_dev);
		dev_err(&tc->pdev->dev,
			"Failed to register CDMA device (%d)\n", err);
		tc->dma_dev = NULL;
	}

	return err;
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
	case TC_INTERRUPT_PDP2:
		dev_info(&tc->pdev->dev,
			"Enabling Odin PDP2 interrupts\n");
		break;
	case TC_INTERRUPT_CDMA:
		dev_info(&tc->pdev->dev,
			"Enabling Odin CDMA interrupts\n");
		break;
	case TC_INTERRUPT_CDMA2:
		dev_info(&tc->pdev->dev,
			"Enabling Odin CDMA2 interrupts\n");
		break;
	default:
		dev_err(&tc->pdev->dev,
			"Error - illegal interrupt id\n");
		return;
	}

	val = ioread32(tc->tcf.registers +
		       common_reg_offset(tc, CORE_INTERRUPT_ENABLE));
	flag = odin_interrupt_id_to_flag(interrupt_id);
	val |= flag;
	iowrite32(val, tc->tcf.registers +
		  common_reg_offset(tc, CORE_INTERRUPT_ENABLE));
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
	case TC_INTERRUPT_PDP2:
		dev_info(&tc->pdev->dev,
			"Disabling Odin PDP2 interrupts\n");
		break;
	case TC_INTERRUPT_CDMA:
		dev_info(&tc->pdev->dev,
			"Disabling Odin CDMA interrupts\n");
		break;
	case TC_INTERRUPT_CDMA2:
		dev_info(&tc->pdev->dev,
			"Disabling Odin CDMA2 interrupts\n");
		break;
	default:
		dev_err(&tc->pdev->dev,
			"Error - illegal interrupt id\n");
		return;
	}
	val = ioread32(tc->tcf.registers +
		       common_reg_offset(tc, CORE_INTERRUPT_ENABLE));
	val &= ~(odin_interrupt_id_to_flag(interrupt_id));
	iowrite32(val, tc->tcf.registers +
		  common_reg_offset(tc, CORE_INTERRUPT_ENABLE));
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
				    common_reg_offset(tc,
						      CORE_INTERRUPT_STATUS));
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
	if (interrupt_status & ODN_INTERRUPT_STATUS_PDP2) {
		struct tc_interrupt_handler *pdp_int =
			&tc->interrupt_handlers[TC_INTERRUPT_PDP2];

		if (pdp_int->enabled && pdp_int->handler_function) {
			pdp_int->handler_function(pdp_int->handler_data);
			interrupt_clear |= ODN_INTERRUPT_CLEAR_PDP2;
		}
		ret = IRQ_HANDLED;
	}

	if (interrupt_status & ODN_INTERRUPT_STATUS_CDMA) {
		struct tc_interrupt_handler *cdma_int =
			&tc->interrupt_handlers[TC_INTERRUPT_CDMA];
		if (cdma_int->enabled && cdma_int->handler_function) {
			cdma_int->handler_function(cdma_int->handler_data);
			interrupt_clear |= ODN_INTERRUPT_CLEAR_CDMA;
		}
		ret = IRQ_HANDLED;
	}

	if (interrupt_status & ODN_INTERRUPT_STATUS_CDMA2) {
		struct tc_interrupt_handler *cdma_int =
			&tc->interrupt_handlers[TC_INTERRUPT_CDMA2];
		if (cdma_int->enabled && cdma_int->handler_function) {
			cdma_int->handler_function(cdma_int->handler_data);
			interrupt_clear |= ODN_INTERRUPT_CLEAR_CDMA2;
		}
		ret = IRQ_HANDLED;
	}


	if (interrupt_clear)
		iowrite32(interrupt_clear,
			  tc->tcf.registers +
			  common_reg_offset(tc, CORE_INTERRUPT_CLR));

	/*
	 * Orion PDP interrupts are occasionally masked because, for unknown
	 * reasons, a vblank goes without being asserted for about 1000 ms. This
	 * feature is not present on Odin, and setting the
	 * INTERRUPT_TIMEOUT_THRESHOLD register to 0 does not seem to disable it
	 * either. This is probably caused by a bug in some versions of Sirius
	 * RTL. Also this bug seems to only affect PDP interrupts, but not the
	 * DUT. This might sometimes lead to a sudden jitter effect in the
	 * render. Further investigation is pending before this code can
	 * be safely removed.
	 */

	if (tc->orion) {
		if (REG_FIELD_GET(ioread32(tc->tcf.registers +
					   SRS_CORE_INTERRUPT_TIMEOUT_CLR),
				  SRS_INTERRUPT_TIMEOUT_CLR_INTERRUPT_MST_TIMEOUT)) {
			dev_warn(&tc->pdev->dev,
				 "Orion PDP interrupts were masked, clearing now\n");
			iowrite32(SRS_INTERRUPT_TIMEOUT_CLR_INTERRUPT_MST_TIMEOUT_CLR_MASK,
				  tc->tcf.registers + SRS_CORE_INTERRUPT_TIMEOUT_CLR);
		}
	}

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
	u32 tcver = tc_odin_subvers(&tc->pdev->dev);
	char temp_str[12];
	u32 val;

	/* Read the Odin major and minor revision ID register Rx-xx */
	val = ioread32(tc->tcf.registers +
		       common_reg_offset(tc, CORE_REVISION));

	snprintf(str_tcf_core_rev,
		 size_tcf_core_rev,
		 "%d.%d",
		 HEX2DEC((val & REVISION_MAJOR_MASK[tcver])
			 >> REVISION_MAJOR_SHIFT[tcver]),
		 HEX2DEC((val & REVISION_MINOR_MASK[tcver])
			 >> REVISION_MINOR_SHIFT[tcver]));

	dev_info(&tc->pdev->dev, "%s core revision %s\n",
		 odin_tc_name(tc), str_tcf_core_rev);

	/* Read the Odin register containing the Perforce changelist
	 * value that the FPGA build was generated from
	 */
	val = ioread32(tc->tcf.registers +
		       common_reg_offset(tc, CORE_CHANGE_SET));

	snprintf(str_tcf_core_target_build_id,
		 size_tcf_core_target_build_id,
		 "%d",
		 (val & CHANGE_SET_SET_MASK[tcver])
		 >> CHANGE_SET_SET_SHIFT[tcver]);

	/* Read the Odin User_ID register containing the User ID for
	 * identification of a modified build
	 */
	val = ioread32(tc->tcf.registers + common_reg_offset(tc, CORE_USER_ID));

	snprintf(temp_str,
		 sizeof(temp_str),
		 "%d",
		 HEX2DEC((val & USER_ID_ID_MASK[tcver])
			 >> USER_ID_ID_SHIFT[tcver]));

	/* Read the Odin User_Build register containing the User build
	 * number for identification of modified builds
	 */
	val = ioread32(tc->tcf.registers +
		       common_reg_offset(tc, CORE_USER_BUILD));

	snprintf(temp_str,
		 sizeof(temp_str),
		 "%d",
		 HEX2DEC((val & USER_BUILD_BUILD_MASK[tcver])
			 >> USER_BUILD_BUILD_SHIFT[tcver]));

	return 0;
}

const char *odin_tc_name(struct tc_device *tc)
{
	if (tc->odin)
		return "Odin";
	else if (tc->orion)
		return "Orion";
	else
		return "Unknown TC";
}

bool odin_pfim_compatible(struct tc_device *tc)
{
	u32 val;

	val = ioread32(tc->tcf.registers +
		       ODN_CORE_REVISION);

	return ((REG_FIELD_GET(val, ODN_REVISION_MAJOR)
		 >= ODIN_PFIM_RELNUM));
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)) && !defined(TC_XILINX_DMA)
static bool odin_dma_chan_filter(struct dma_chan *chan, void *param)
{
	return false;
}
#endif

struct dma_chan *odin_cdma_chan(struct tc_device *tc, char *name)
{
	struct dma_chan *chan;
	unsigned long chan_idx;
	int err;

	if (!(strcmp("rx", name)))
		chan_idx = ODN_DMA_CHAN_RX;
	else if (!(strcmp("tx", name))) {
		/*
		 * When Odin RTL has a single CDMA device, we simulate
		 * a second channel by always opening the first one.
		 * This is made possible because CDMA allows for
		 * transfers in both directions
		 */
		if (tc->dma_nchan == 1) {
			name = "rx";
			chan_idx = ODN_DMA_CHAN_RX;
		} else
			chan_idx = ODN_DMA_CHAN_TX;
	} else {
		dev_err(&tc->pdev->dev, "Wrong CDMA channel name\n");
		return NULL;
	}

	mutex_lock(&tc->dma_mutex);

	if (tc->dma_refcnt[chan_idx]) {
		tc->dma_refcnt[chan_idx]++;
	} else {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0))
		chan = dma_request_chan(&tc->dma_dev->dev, name);
#else
		dma_cap_mask_t mask;
		dma_cap_zero(mask);
		dma_cap_set(DMA_SLAVE, mask);
		chan = dma_request_channel(mask,
					   odin_dma_chan_filter,
					   (void *)chan_idx);
#endif
		if (IS_ERR(chan)) {
			err = PTR_ERR(chan);
			dev_err(&tc->pdev->dev,
				"dma channel request failed (%d)\n", err);
			mutex_unlock(&tc->dma_mutex);
			return NULL;
		}
		tc->dma_chans[chan_idx] = chan;
		tc->dma_refcnt[chan_idx] = 1;
	}

	mutex_unlock(&tc->dma_mutex);

	return tc->dma_chans[chan_idx];
}

void odin_cdma_chan_free(struct tc_device *tc,
			 void *chan_priv)
{
	struct dma_chan *dma_chan = (struct dma_chan *)chan_priv;
	u32 chan_idx;

	BUG_ON(dma_chan == NULL);

	mutex_lock(&tc->dma_mutex);

	if (dma_chan == tc->dma_chans[ODN_DMA_CHAN_RX])
		chan_idx = ODN_DMA_CHAN_RX;
	else if (dma_chan == tc->dma_chans[ODN_DMA_CHAN_TX])
		chan_idx = ODN_DMA_CHAN_TX;
	else
		goto cdma_chan_free_exit;

	tc->dma_refcnt[chan_idx]--;
	if (!tc->dma_refcnt[chan_idx]) {
		tc->dma_chans[chan_idx] = NULL;
		dma_release_channel(dma_chan);
	}

cdma_chan_free_exit:
	mutex_unlock(&tc->dma_mutex);
}
