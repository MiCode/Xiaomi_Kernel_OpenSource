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
 * This is a device driver for the apollo testchip framework. It creates
 * platform devices for the pdp and ext sub-devices, and exports functions to
 * manage the shared interrupt handling
 */

#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/thermal.h>

#include "tc_drv_internal.h"
#include "tc_apollo.h"
#include "tc_ion.h"

#include "apollo_regs.h"
#include "tcf_clk_ctrl.h"
#include "tcf_pll.h"

#if defined(SUPPORT_APOLLO_FPGA)
#include "tc_apollo_debugfs.h"
#endif /* defined(SUPPORT_APOLLO_FPGA) */

#define TC_INTERRUPT_FLAG_PDP      (1 << PDP1_INT_SHIFT)
#define TC_INTERRUPT_FLAG_EXT      (1 << EXT_INT_SHIFT)

#define PCI_VENDOR_ID_POWERVR      0x1010
#define DEVICE_ID_PCI_APOLLO_FPGA  0x1CF1
#define DEVICE_ID_PCIE_APOLLO_FPGA 0x1CF2

#define APOLLO_MEM_PCI_BASENUM	   (2)

static struct {
	struct thermal_zone_device *thermal_zone;

#if defined(SUPPORT_APOLLO_FPGA)
	struct tc_io_region fpga;
	struct apollo_debugfs_fpga_entries fpga_entries;
#endif
} apollo_pdata;

#if defined(SUPPORT_APOLLO_FPGA)

#define APOLLO_DEVICE_NAME_FPGA "apollo_fpga"

struct apollo_fpga_platform_data {
	/* The testchip memory mode (LMA, HOST or HYBRID) */
	int mem_mode;

	resource_size_t tc_memory_base;

	resource_size_t pdp_heap_memory_base;
	resource_size_t pdp_heap_memory_size;
};

#endif /* defined(SUPPORT_APOLLO_FPGA) */

static void spi_write(struct tc_device *tc, u32 off, u32 val)
{
	iowrite32(off, tc->tcf.registers
		  + TCF_CLK_CTRL_TCF_SPI_MST_ADDR_RDNWR);
	iowrite32(val, tc->tcf.registers
		  + TCF_CLK_CTRL_TCF_SPI_MST_WDATA);
	iowrite32(TCF_SPI_MST_GO_MASK, tc->tcf.registers
		  + TCF_CLK_CTRL_TCF_SPI_MST_GO);
	udelay(1000);
}

static int spi_read(struct tc_device *tc, u32 off, u32 *val)
{
	int cnt = 0;
	u32 spi_mst_status;

	iowrite32(0x40000 | off, tc->tcf.registers
		  + TCF_CLK_CTRL_TCF_SPI_MST_ADDR_RDNWR);
	iowrite32(TCF_SPI_MST_GO_MASK, tc->tcf.registers
		  + TCF_CLK_CTRL_TCF_SPI_MST_GO);

	udelay(100);

	do {
		spi_mst_status = ioread32(tc->tcf.registers
					  + TCF_CLK_CTRL_TCF_SPI_MST_STATUS);

		if (cnt++ > 10000) {
			dev_err(&tc->pdev->dev,
				"%s: Time out reading SPI reg (0x%x)\n",
				__func__, off);
			return -1;
		}

	} while (spi_mst_status != 0x08);

	*val = ioread32(tc->tcf.registers
			+ TCF_CLK_CTRL_TCF_SPI_MST_RDATA);

	return 0;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 3, 0))
static int apollo_thermal_get_temp(struct thermal_zone_device *thermal,
				   unsigned long *t)
#else
static int apollo_thermal_get_temp(struct thermal_zone_device *thermal,
				   int *t)
#endif
{
	struct tc_device *tc;
	int err = -ENODEV;
	u32 tmp;

	if (!thermal)
		goto err_out;

	tc = (struct tc_device *)thermal->devdata;

	if (!tc)
		goto err_out;

	if (spi_read(tc, TCF_TEMP_SENSOR_SPI_OFFSET, &tmp)) {
		dev_err(&tc->pdev->dev,
				"Failed to read apollo temperature sensor\n");

		goto err_out;
	}

	/* Report this in millidegree Celsius */
	*t = TCF_TEMP_SENSOR_TO_C(tmp) * 1000;

	err = 0;

err_out:
	return err;
}

static struct thermal_zone_device_ops apollo_thermal_dev_ops = {
	.get_temp = apollo_thermal_get_temp,
};

#if defined(SUPPORT_RGX)

static void pll_write_reg(struct tc_device *tc,
	resource_size_t reg_offset, u32 reg_value)
{
	BUG_ON(reg_offset < TCF_PLL_PLL_CORE_CLK0);
	BUG_ON(reg_offset > tc->tcf_pll.region.size +
		TCF_PLL_PLL_CORE_CLK0 - 4);

	/* Tweak the offset because we haven't mapped the full pll region */
	iowrite32(reg_value, tc->tcf_pll.registers +
		reg_offset - TCF_PLL_PLL_CORE_CLK0);
}

static u32 sai_read_es2(struct tc_device *tc, u32 addr)
{
	iowrite32(0x200 | addr, tc->tcf.registers + 0x300);
	iowrite32(0x1 | addr, tc->tcf.registers + 0x318);
	return ioread32(tc->tcf.registers + 0x310);
}

static int apollo_align_interface_es2(struct tc_device *tc)
{
	u32 reg = 0;
	u32 reg_reset_n;
	int reset_cnt = 0;
	int err = -EFAULT;
	bool aligned = false;

	/* Try to enable the core clock PLL */
	spi_write(tc, 0x1, 0x0);
	reg  = ioread32(tc->tcf.registers + 0x320);
	reg |= 0x1;
	iowrite32(reg, tc->tcf.registers + 0x320);
	reg &= 0xfffffffe;
	iowrite32(reg, tc->tcf.registers + 0x320);
	msleep(1000);

	if (spi_read(tc, 0x2, &reg)) {
		dev_err(&tc->pdev->dev,
				"Unable to read PLL status\n");
		goto err_out;
	}

	if (reg == 0x1) {
		/* Select DUT PLL as core clock */
		reg  = ioread32(tc->tcf.registers +
			TCF_CLK_CTRL_DUT_CONTROL_1);
		reg &= 0xfffffff7;
		iowrite32(reg, tc->tcf.registers +
			TCF_CLK_CTRL_DUT_CONTROL_1);
	} else {
		dev_err(&tc->pdev->dev,
			"PLL has failed to lock, status = %x\n", reg);
		goto err_out;
	}

	reg_reset_n = ioread32(tc->tcf.registers +
		TCF_CLK_CTRL_CLK_AND_RST_CTRL);

	while (!aligned && reset_cnt < 10 &&
			tc->version != APOLLO_VERSION_TCF_5) {
		int bank;
		u32 eyes;
		u32 clk_taps;
		u32 train_ack;

		++reset_cnt;

		/* Reset the DUT to allow the SAI to retrain */
		reg_reset_n &= ~(0x1 << DUT_RESETN_SHIFT);
		iowrite32(reg_reset_n, tc->tcf.registers +
			  TCF_CLK_CTRL_CLK_AND_RST_CTRL);
		udelay(100);
		reg_reset_n |= (0x1 << DUT_RESETN_SHIFT);
		iowrite32(reg_reset_n, tc->tcf.registers +
			  TCF_CLK_CTRL_CLK_AND_RST_CTRL);
		udelay(100);

		/* Assume alignment passed, if any bank fails on either DUT or
		 * FPGA we will set this to false and try again for a max of 10
		 * times.
		 */
		aligned = true;

		/* For each of the banks */
		for (bank = 0; bank < 10; bank++) {
			int bank_aligned = 0;
			/* Check alignment on the DUT */
			u32 bank_base = 0x7000 + (0x1000 * bank);

			spi_read(tc, bank_base + 0x4, &eyes);
			spi_read(tc, bank_base + 0x3, &clk_taps);
			spi_read(tc, bank_base + 0x6, &train_ack);

			bank_aligned = tc_is_interface_aligned(
					eyes, clk_taps, train_ack);
			if (!bank_aligned) {
				dev_warn(&tc->pdev->dev,
					"Alignment check failed, retrying\n");
				aligned = false;
				break;
			}

			/* Check alignment on the FPGA */
			bank_base = 0xb0 + (0x10 * bank);

			eyes = sai_read_es2(tc, bank_base + 0x4);
			clk_taps = sai_read_es2(tc, bank_base + 0x3);
			train_ack = sai_read_es2(tc, bank_base + 0x6);

			bank_aligned = tc_is_interface_aligned(
					eyes, clk_taps, train_ack);

			if (!bank_aligned) {
				dev_warn(&tc->pdev->dev,
					"Alignment check failed, retrying\n");
				aligned = false;
				break;
			}
		}
	}

	if (!aligned) {
		dev_err(&tc->pdev->dev, "Unable to initialise the testchip (interface alignment failure), please restart the system.\n");
		/* We are not returning an error here, cause VP doesn't
		 * implement the necessary registers although they claim to be
		 * TC compatible. */
	}

	if (reset_cnt > 1) {
		dev_dbg(&tc->pdev->dev, "Note: The testchip required more than one reset to find a good interface alignment!\n");
		dev_dbg(&tc->pdev->dev, "      This should be harmless, but if you do suspect foul play, please reset the machine.\n");
		dev_dbg(&tc->pdev->dev, "      If you continue to see this message you may want to report it to PowerVR Verification Platforms.\n");
	}

	err = 0;
err_out:
	return err;
}

static void apollo_set_clocks(struct tc_device *tc,
			      int core_clock, int mem_clock, int sys_clock)
{
	u32 val;

	/* This is disabled for TCF2 since the current FPGA builds do not
	 * like their core clocks being set (it takes apollo down).
	 */
	if (tc->version != APOLLO_VERSION_TCF_2) {
		val = core_clock / 1000000;
		pll_write_reg(tc, TCF_PLL_PLL_CORE_CLK0, val);

		val = 0x1 << PLL_CORE_DRP_GO_SHIFT;
		pll_write_reg(tc, TCF_PLL_PLL_CORE_DRP_GO, val);
	}

	val = mem_clock / 1000000;
	pll_write_reg(tc, TCF_PLL_PLL_MEMIF_CLK0, val);

	val = 0x1 << PLL_MEM_DRP_GO_SHIFT;
	pll_write_reg(tc, TCF_PLL_PLL_MEM_DRP_GO, val);

	if (tc->version == APOLLO_VERSION_TCF_5) {
		val = sys_clock / 1000000;
		pll_write_reg(tc, TCF_PLL_PLL_SYSIF_CLK0, val);

		val = 0x1 << PLL_MEM_DRP_GO_SHIFT;
		pll_write_reg(tc, TCF_PLL_PLL_SYS_DRP_GO, val);
	}

	dev_info(&tc->pdev->dev, "Setting clocks to %uMHz/%uMHz\n",
			 core_clock / 1000000,
			 mem_clock / 1000000);
	udelay(400);
}

static void apollo_set_mem_latency(struct tc_device *tc,
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

static void apollo_fpga_update_dut_clk_freq(struct tc_device *tc,
					    int *core_clock, int *mem_clock)
{
	struct device *dev = &tc->pdev->dev;

#if defined(SUPPORT_FPGA_DUT_CLK_INFO)
	u32 reg;

	/* DUT_CLK_INFO available only if SW_IF_VERSION >= 1 */
	reg = ioread32(tc->tcf.registers + TCF_CLK_CTRL_SW_IF_VERSION);
	reg = (reg & VERSION_MASK) >> VERSION_SHIFT;

	if (reg >= 1) {
		reg = ioread32(tc->tcf.registers + TCF_CLK_CTRL_DUT_CLK_INFO);

		if ((reg != 0) && (reg != 0xbaadface) && (reg != 0xffffffff)) {
			dev_info(dev, "TCF_CLK_CTRL_DUT_CLK_INFO = %08x\n", reg);
			dev_info(dev, "Overriding provided DUT clock values: "
				 "core %i, mem %i\n",
				 *core_clock, *mem_clock);

			*core_clock = ((reg & CORE_MASK) >> CORE_SHIFT) * 1000000;
			*mem_clock = ((reg & MEM_MASK) >> MEM_SHIFT) * 1000000;
		}
	}
#endif

	dev_info(dev, "DUT clock values: core %i, mem %i\n",
		 *core_clock, *mem_clock);
}

#endif /* defined(SUPPORT_RGX) */

static int apollo_hard_reset(struct tc_device *tc,
			     int core_clock, int mem_clock, int sys_clock)
{
	u32 reg;
	u32 reg_reset_n = 0;

	int err = 0;

	/* This is required for SPI reset which is not yet implemented. */
	/*u32 aux_reset_n;*/

	if (tc->version == APOLLO_VERSION_TCF_2) {
		/* Power down */
		reg = ioread32(tc->tcf.registers +
			TCF_CLK_CTRL_DUT_CONTROL_1);
		reg &= ~DUT_CTRL_VCC_0V9EN;
		reg &= ~DUT_CTRL_VCC_1V8EN;
		reg |= DUT_CTRL_VCC_IO_INH;
		reg |= DUT_CTRL_VCC_CORE_INH;
		iowrite32(reg, tc->tcf.registers +
			TCF_CLK_CTRL_DUT_CONTROL_1);
		msleep(500);
	}

	/* Put everything into reset */
	iowrite32(reg_reset_n, tc->tcf.registers +
		TCF_CLK_CTRL_CLK_AND_RST_CTRL);

	/* Take PDP1 and PDP2 out of reset */
	reg_reset_n |= (0x1 << PDP1_RESETN_SHIFT);
	reg_reset_n |= (0x1 << PDP2_RESETN_SHIFT);

	iowrite32(reg_reset_n, tc->tcf.registers +
		TCF_CLK_CTRL_CLK_AND_RST_CTRL);
	msleep(100);

	/* Take DDR out of reset */
	reg_reset_n |= (0x1 << DDR_RESETN_SHIFT);
	iowrite32(reg_reset_n, tc->tcf.registers +
		  TCF_CLK_CTRL_CLK_AND_RST_CTRL);

#if defined(SUPPORT_RGX)
	if (tc->version == APOLLO_VERSION_TCF_5)
		apollo_fpga_update_dut_clk_freq(tc, &core_clock, &mem_clock);

	/* Set clock speed here, before reset. */
	apollo_set_clocks(tc, core_clock, mem_clock, sys_clock);

	/* Put take GLB_CLKG and SCB out of reset */
	reg_reset_n |= (0x1 << GLB_CLKG_EN_SHIFT);
	reg_reset_n |= (0x1 << SCB_RESETN_SHIFT);
	iowrite32(reg_reset_n, tc->tcf.registers +
		  TCF_CLK_CTRL_CLK_AND_RST_CTRL);
	msleep(100);

	if (tc->version == APOLLO_VERSION_TCF_2) {
		/* Enable the voltage control regulators on DUT */
		reg = ioread32(tc->tcf.registers +
			TCF_CLK_CTRL_DUT_CONTROL_1);
		reg |= DUT_CTRL_VCC_0V9EN;
		reg |= DUT_CTRL_VCC_1V8EN;
		reg &= ~DUT_CTRL_VCC_IO_INH;
		reg &= ~DUT_CTRL_VCC_CORE_INH;
		iowrite32(reg, tc->tcf.registers +
			TCF_CLK_CTRL_DUT_CONTROL_1);
		msleep(300);
	}

	/* Take DUT_DCM out of reset */
	reg_reset_n |= (0x1 << DUT_DCM_RESETN_SHIFT);
	iowrite32(reg_reset_n, tc->tcf.registers +
		  TCF_CLK_CTRL_CLK_AND_RST_CTRL);
	msleep(100);


	err = tc_iopol32_nonzero(DCM_LOCK_STATUS_MASK,
		tc->tcf.registers + TCF_CLK_CTRL_DCM_LOCK_STATUS);

	if (err != 0)
		goto err_out;

	if (tc->version == APOLLO_VERSION_TCF_2) {
		/* Set ODT to a specific value that seems to provide the most
		 * stable signals.
		 */
		spi_write(tc, 0x11, 0x413130);
	}

	/* Take DUT out of reset */
	reg_reset_n |= (0x1 << DUT_RESETN_SHIFT);
	iowrite32(reg_reset_n, tc->tcf.registers +
		  TCF_CLK_CTRL_CLK_AND_RST_CTRL);
	msleep(100);

	if (tc->version != APOLLO_VERSION_TCF_5) {
		err = apollo_align_interface_es2(tc);
		if (err)
			goto err_out;
	}

#endif /* defined(SUPPORT_RGX) */

	if (tc->version == APOLLO_VERSION_TCF_2) {
		/* Enable the temperature sensor */
		spi_write(tc, 0xc, 0); /* power up */
		spi_write(tc, 0xc, 2); /* reset */
		spi_write(tc, 0xc, 6); /* init & run */

		/* Register a new thermal zone */
		apollo_pdata.thermal_zone =
			thermal_zone_device_register("apollo", 0, 0, tc,
						     &apollo_thermal_dev_ops,
						     NULL, 0, 0);
		if (IS_ERR(apollo_pdata.thermal_zone)) {
			dev_warn(&tc->pdev->dev, "Couldn't register thermal zone");
			apollo_pdata.thermal_zone = NULL;
		}
	}

	reg = ioread32(tc->tcf.registers + TCF_CLK_CTRL_SW_IF_VERSION);
	reg = (reg & VERSION_MASK) >> VERSION_SHIFT;

	if (reg == 0) {
		u32 build_inc;
		u32 build_owner;

		/* Check the build */
		reg = ioread32(tc->tcf.registers + TCF_CLK_CTRL_FPGA_DES_REV_1);
		build_inc = (reg >> 12) & 0xff;
		build_owner = (reg >> 20) & 0xf;

		if (build_inc) {
			dev_alert(&tc->pdev->dev,
				"BE WARNED: You are not running a tagged release of the FPGA!\n");

			dev_alert(&tc->pdev->dev, "Owner: 0x%01x, Inc: 0x%02x\n",
				  build_owner, build_inc);
		}

		dev_info(&tc->pdev->dev, "FPGA Release: %u.%02u\n",
			 reg >> 8 & 0xf, reg & 0xff);
	}

#if defined(SUPPORT_RGX)
err_out:
#endif /* defined(SUPPORT_RGX) */
	return err;
}

static void apollo_set_mem_mode_lma(struct tc_device *tc)
{
	u32 val;

	val = ioread32(tc->tcf.registers + TCF_CLK_CTRL_TEST_CTRL);
	val &= ~(ADDRESS_FORCE_MASK | PCI_TEST_MODE_MASK | HOST_ONLY_MODE_MASK
		| HOST_PHY_MODE_MASK);
	val |= (0x1 << ADDRESS_FORCE_SHIFT);
	iowrite32(val, tc->tcf.registers + TCF_CLK_CTRL_TEST_CTRL);
}

static void apollo_set_mem_mode_hybrid(struct tc_device *tc)
{
	u32 val;

	val = ioread32(tc->tcf.registers + TCF_CLK_CTRL_TEST_CTRL);
	val &= ~(ADDRESS_FORCE_MASK | PCI_TEST_MODE_MASK | HOST_ONLY_MODE_MASK
		| HOST_PHY_MODE_MASK);
	val |= ((0x1 << HOST_ONLY_MODE_SHIFT) | (0x1 << HOST_PHY_MODE_SHIFT));
	iowrite32(val, tc->tcf.registers + TCF_CLK_CTRL_TEST_CTRL);

	/* Setup apollo to pass 1GB window of address space to the local memory.
	 * This is a sub-mode of the host only mode, meaning that the apollo TC
	 * can address the system memory with a 1GB window of address space
	 * routed to the device local memory. The simplest approach is to mirror
	 * the CPU physical address space, by moving the device local memory
	 * window where it is mapped in the CPU physical address space.
	 */
	iowrite32(tc->tc_mem.base,
		  tc->tcf.registers + TCF_CLK_CTRL_HOST_PHY_OFFSET);
}

static int apollo_set_mem_mode(struct tc_device *tc, int mem_mode)
{
	switch (mem_mode) {
	case TC_MEMORY_HYBRID:
		apollo_set_mem_mode_hybrid(tc);
		dev_info(&tc->pdev->dev, "Memory mode: TC_MEMORY_HYBRID\n");
		break;
	case TC_MEMORY_LOCAL:
		apollo_set_mem_mode_lma(tc);
		dev_info(&tc->pdev->dev, "Memory mode: TC_MEMORY_LOCAL\n");
		break;
	default:
		dev_err(&tc->pdev->dev, "unsupported memory mode = %d\n",
			mem_mode);
		return -ENOSYS;
	};

	tc->mem_mode = mem_mode;

	return 0;
}

static bool apollo_pdp_export_host_addr(struct tc_device *tc)
{
	return tc->mem_mode == TC_MEMORY_HYBRID;
}

static u64 apollo_get_pdp_dma_mask(struct tc_device *tc)
{
	/* The PDP does not access system memory, so there is no
	 * DMA limitation.
	 */
	if ((tc->mem_mode == TC_MEMORY_LOCAL) ||
	    (tc->mem_mode == TC_MEMORY_HYBRID))
		return DMA_BIT_MASK(64);

	return DMA_BIT_MASK(32);
}

#if defined(SUPPORT_RGX)
static u64 apollo_get_rogue_dma_mask(struct tc_device *tc)
#else /* SUPPORT_APOLLO_FPGA */
static u64 apollo_get_fpga_dma_mask(struct tc_device *tc)
#endif /* defined(SUPPORT_RGX) */
{
	/* Does not access system memory, so there is no DMA limitation */
	if (tc->mem_mode == TC_MEMORY_LOCAL)
		return DMA_BIT_MASK(64);

	return DMA_BIT_MASK(32);
}

static int apollo_hw_init(struct tc_device *tc,
			  int core_clock, int mem_clock, int sys_clock,
			  int mem_latency, int mem_wresp_latency, int mem_mode)
{
	int err = 0;

	err = apollo_hard_reset(tc, core_clock, mem_clock, sys_clock);
	if (err)
		goto err_out;

	err = apollo_set_mem_mode(tc, mem_mode);
	if (err)
		goto err_out;

#if defined(SUPPORT_RGX)
	if (tc->version == APOLLO_VERSION_TCF_BONNIE) {
		u32 reg;
		/* Enable ASTC via SPI */
		if (spi_read(tc, 0xf, &reg)) {
			dev_err(&tc->pdev->dev,
				"Failed to read apollo ASTC register\n");
			err = -ENODEV;
			goto err_out;
		}

		reg |= 0x1 << 4;
		spi_write(tc, 0xf, reg);
	} else if (tc->version == APOLLO_VERSION_TCF_5) {
		apollo_set_mem_latency(tc, mem_latency, mem_wresp_latency);
	}
#endif /* defined(SUPPORT_RGX) */

err_out:
	return err;
}

static int apollo_enable_irq(struct tc_device *tc)
{
	int err = 0;

#if defined(TC_FAKE_INTERRUPTS)
	setup_timer(&tc->timer, tc_irq_fake_wrapper,
		(unsigned long)tc);
	mod_timer(&tc->timer,
		jiffies + msecs_to_jiffies(FAKE_INTERRUPT_TIME_MS));
#else
	{
		u32 val;

		iowrite32(0, tc->tcf.registers +
			TCF_CLK_CTRL_INTERRUPT_ENABLE);
		iowrite32(0xffffffff, tc->tcf.registers +
			TCF_CLK_CTRL_INTERRUPT_CLEAR);

		/* Set sense to active high */
		val = ioread32(tc->tcf.registers +
			TCF_CLK_CTRL_INTERRUPT_OP_CFG) & ~(INT_SENSE_MASK);
		iowrite32(val, tc->tcf.registers +
			TCF_CLK_CTRL_INTERRUPT_OP_CFG);

		err = request_irq(tc->pdev->irq, apollo_irq_handler,
			IRQF_SHARED, DRV_NAME, tc);
	}
#endif
	return err;
}

static void apollo_disable_irq(struct tc_device *tc)
{
#if defined(TC_FAKE_INTERRUPTS)
	del_timer_sync(&tc->timer);
#else
	iowrite32(0, tc->tcf.registers +
		TCF_CLK_CTRL_INTERRUPT_ENABLE);
	iowrite32(0xffffffff, tc->tcf.registers +
		TCF_CLK_CTRL_INTERRUPT_CLEAR);

	free_irq(tc->pdev->irq, tc);
#endif
}

static enum tc_version_t
apollo_detect_tc_version(struct tc_device *tc)
{
	u32 val = ioread32(tc->tcf.registers +
		       TCF_CLK_CTRL_TCF_CORE_TARGET_BUILD_CFG);

	switch (val) {
	default:
		dev_err(&tc->pdev->dev,
			"Unknown TCF core target build ID (0x%x) - assuming Hood ES2 - PLEASE REPORT TO ANDROID TEAM\n",
			val);
		/* Fall-through */
	case 5:
		dev_err(&tc->pdev->dev, "Looks like a Hood ES2 TC\n");
		return APOLLO_VERSION_TCF_2;
	case 1:
		dev_err(&tc->pdev->dev, "Looks like a TCF5\n");
		return APOLLO_VERSION_TCF_5;
	case 6:
		dev_err(&tc->pdev->dev, "Looks like a Bonnie TC\n");
		return APOLLO_VERSION_TCF_BONNIE;
	}
}

static u32 apollo_interrupt_id_to_flag(int interrupt_id)
{
	switch (interrupt_id) {
	case TC_INTERRUPT_PDP:
		return TC_INTERRUPT_FLAG_PDP;
	case TC_INTERRUPT_EXT:
		return TC_INTERRUPT_FLAG_EXT;
	default:
		BUG();
	}
}

static int apollo_dev_init(struct tc_device *tc, struct pci_dev *pdev,
			   int pdp_mem_size, int secure_mem_size)
{
	int err;

	/* Reserve and map the tcf_clk / "sys" registers */
	err = setup_io_region(pdev, &tc->tcf,
		SYS_APOLLO_REG_PCI_BASENUM,
		SYS_APOLLO_REG_SYS_OFFSET, SYS_APOLLO_REG_SYS_SIZE);
	if (err)
		goto err_out;

	/* Reserve and map the tcf_pll registers */
	err = setup_io_region(pdev, &tc->tcf_pll,
		SYS_APOLLO_REG_PCI_BASENUM,
		SYS_APOLLO_REG_PLL_OFFSET + TCF_PLL_PLL_CORE_CLK0,
		TCF_PLL_PLL_DRP_STATUS - TCF_PLL_PLL_CORE_CLK0 + 4);
	if (err)
		goto err_unmap_sys_registers;

#if defined(SUPPORT_APOLLO_FPGA)
#define FPGA_REGISTERS_SIZE 4
	/* If this is a special 'fgpa' build, have the apollo driver manage
	 * the second register bar.
	 */
	err = setup_io_region(pdev, &apollo_pdata.fpga,
		SYS_RGX_REG_PCI_BASENUM, 0, FPGA_REGISTERS_SIZE);
	if (err)
		goto err_unmap_pll_registers;
#endif

	/* Detect testchip version */
	tc->version = apollo_detect_tc_version(tc);

	/* Setup card memory */
	tc->tc_mem.base =
		pci_resource_start(pdev, APOLLO_MEM_PCI_BASENUM);
	tc->tc_mem.size =
		pci_resource_len(pdev, APOLLO_MEM_PCI_BASENUM);

	if (tc->tc_mem.size < pdp_mem_size) {
		dev_err(&pdev->dev,
			"Apollo MEM region (bar %d) has size of %lu which is smaller than the requested PDP heap of %lu",
			APOLLO_MEM_PCI_BASENUM,
			(unsigned long)tc->tc_mem.size,
			(unsigned long)pdp_mem_size);
		err = -EIO;
		goto err_unmap_fpga_registers;
	}

#if defined(SUPPORT_FAKE_SECURE_ION_HEAP)
	if (tc->tc_mem.size <
	    (pdp_mem_size + secure_mem_size)) {
		dev_err(&pdev->dev,
			"Apollo MEM region (bar %d) has size of %lu which is smaller than the requested PDP heap of %lu plus the requested secure heap size %lu",
			APOLLO_MEM_PCI_BASENUM,
			(unsigned long)tc->tc_mem.size,
			(unsigned long)pdp_mem_size,
			(unsigned long)secure_mem_size);
		err = -EIO;
		goto err_unmap_fpga_registers;
	}
#endif

	err = tc_mtrr_setup(tc);
	if (err)
		goto err_unmap_fpga_registers;

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
			"Apollo MEM region (bar %d) has size of %lu, with %lu pdp_mem_size only %lu bytes are left for ext device, which looks too small",
			APOLLO_MEM_PCI_BASENUM,
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
	err = tc_ion_init(tc, APOLLO_MEM_PCI_BASENUM);
	if (err) {
		dev_err(&pdev->dev, "Failed to initialise ION\n");
		goto err_unmap_fpga_registers;
	}
#endif

#if defined(SUPPORT_APOLLO_FPGA)
	apollo_debugfs_add_fpga_entries(tc, &apollo_pdata.fpga,
					&apollo_pdata.fpga_entries);
#endif /* defined(SUPPORT_APOLLO_FPGA) */

err_out:
	return err;
err_unmap_fpga_registers:
#if defined(SUPPORT_APOLLO_FPGA)
	iounmap(apollo_pdata.fpga.registers);
	release_pci_io_addr(pdev, SYS_RGX_REG_PCI_BASENUM,
		apollo_pdata.fpga.region.base, apollo_pdata.fpga.region.size);
err_unmap_pll_registers:
#endif /* defined(SUPPORT_APOLLO_FPGA) */
	iounmap(tc->tcf_pll.registers);
	release_pci_io_addr(pdev, SYS_APOLLO_REG_PCI_BASENUM,
		tc->tcf_pll.region.base, tc->tcf_pll.region.size);
err_unmap_sys_registers:
	iounmap(tc->tcf.registers);
	release_pci_io_addr(pdev, SYS_APOLLO_REG_PCI_BASENUM,
		tc->tcf.region.base, tc->tcf.region.size);
	goto err_out;
}

static void apollo_dev_cleanup(struct tc_device *tc)
{
#if defined(SUPPORT_APOLLO_FPGA)
	apollo_debugfs_remove_fpga_entries(&apollo_pdata.fpga_entries);
#endif

#if defined(SUPPORT_ION)
	tc_ion_deinit(tc, APOLLO_MEM_PCI_BASENUM);
#endif

	tc_mtrr_cleanup(tc);

#if defined(SUPPORT_APOLLO_FPGA)
	iounmap(apollo_pdata.fpga.registers);
	release_pci_io_addr(tc->pdev, SYS_RGX_REG_PCI_BASENUM,
		apollo_pdata.fpga.region.base, apollo_pdata.fpga.region.size);
#endif

	iounmap(tc->tcf_pll.registers);
	release_pci_io_addr(tc->pdev, SYS_APOLLO_REG_PCI_BASENUM,
		tc->tcf_pll.region.base, tc->tcf_pll.region.size);

	iounmap(tc->tcf.registers);
	release_pci_io_addr(tc->pdev, SYS_APOLLO_REG_PCI_BASENUM,
		tc->tcf.region.base, tc->tcf.region.size);

	if (apollo_pdata.thermal_zone)
		thermal_zone_device_unregister(apollo_pdata.thermal_zone);
}

int apollo_init(struct tc_device *tc, struct pci_dev *pdev,
		int core_clock, int mem_clock, int sys_clock,
		int pdp_mem_size, int secure_mem_size,
		int mem_latency, int mem_wresp_latency, int mem_mode)
{
	int err = 0;

	err = apollo_dev_init(tc, pdev, pdp_mem_size, secure_mem_size);
	if (err) {
		dev_err(&pdev->dev, "apollo_dev_init failed\n");
		goto err_out;
	}

	err = apollo_hw_init(tc, core_clock, mem_clock, sys_clock,
			     mem_latency, mem_wresp_latency, mem_mode);
	if (err) {
		dev_err(&pdev->dev, "apollo_hw_init failed\n");
		goto err_dev_cleanup;
	}

	err = apollo_enable_irq(tc);
	if (err) {
		dev_err(&pdev->dev,
			"Failed to initialise IRQ\n");
		goto err_dev_cleanup;
	}

err_out:
	return err;

err_dev_cleanup:
	apollo_dev_cleanup(tc);
	goto err_out;
}

int apollo_cleanup(struct tc_device *tc)
{
	apollo_disable_irq(tc);
	apollo_dev_cleanup(tc);

	return 0;
}

int apollo_register_pdp_device(struct tc_device *tc)
{
	int err = 0;
	resource_size_t reg_start =
		pci_resource_start(tc->pdev,
				   SYS_APOLLO_REG_PCI_BASENUM);
	struct resource pdp_resources_es2[] = {
		DEFINE_RES_MEM_NAMED(reg_start + SYS_APOLLO_REG_PDP1_OFFSET,
				SYS_APOLLO_REG_PDP1_SIZE, "pdp-regs"),
		DEFINE_RES_MEM_NAMED(reg_start +
				SYS_APOLLO_REG_PLL_OFFSET +
				TCF_PLL_PLL_PDP_CLK0,
				TCF_PLL_PLL_PDP2_DRP_GO -
				TCF_PLL_PLL_PDP_CLK0 + 4, "pll-regs"),
	};
	struct resource pdp_resources_tcf5[] = {
		DEFINE_RES_MEM_NAMED(reg_start + SYS_APOLLO_REG_PDP1_OFFSET,
				SYS_APOLLO_REG_PDP1_SIZE, "pdp-regs"),
		DEFINE_RES_MEM_NAMED(reg_start +
				SYS_APOLLO_REG_PLL_OFFSET +
				TCF_PLL_PLL_PDP_CLK0,
				TCF_PLL_PLL_PDP2_DRP_GO -
				TCF_PLL_PLL_PDP_CLK0 + 4, "pll-regs"),
		DEFINE_RES_MEM_NAMED(pci_resource_start(tc->pdev,
				TC5_SYS_APOLLO_REG_PCI_BASENUM)
				+ TC5_SYS_APOLLO_REG_PDP2_OFFSET,
			TC5_SYS_APOLLO_REG_PDP2_SIZE, "tc5-pdp2-regs"),

		DEFINE_RES_MEM_NAMED(pci_resource_start(tc->pdev,
				TC5_SYS_APOLLO_REG_PCI_BASENUM)
				+ TC5_SYS_APOLLO_REG_PDP2_FBDC_OFFSET,
				TC5_SYS_APOLLO_REG_PDP2_FBDC_SIZE,
				"tc5-pdp2-fbdc-regs"),

		DEFINE_RES_MEM_NAMED(pci_resource_start(tc->pdev,
				TC5_SYS_APOLLO_REG_PCI_BASENUM)
				+ TC5_SYS_APOLLO_REG_HDMI_OFFSET,
				TC5_SYS_APOLLO_REG_HDMI_SIZE,
				"tc5-adv5711-regs"),
	};

	struct tc_pdp_platform_data pdata = {
#if defined(SUPPORT_ION) && (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
		.ion_device = tc->ion_device,
		.ion_heap_id = ION_HEAP_TC_PDP,
#endif
		.memory_base = tc->tc_mem.base,
		.pdp_heap_memory_base = tc->pdp_heap_mem_base,
		.pdp_heap_memory_size = tc->pdp_heap_mem_size,
		.dma_map_export_host_addr = apollo_pdp_export_host_addr(tc),
	};
	struct platform_device_info pdp_device_info = {
		.parent = &tc->pdev->dev,
		.name = APOLLO_DEVICE_NAME_PDP,
		.id = -2,
		.data = &pdata,
		.size_data = sizeof(pdata),
		.dma_mask = apollo_get_pdp_dma_mask(tc),
	};

	if (tc->version == APOLLO_VERSION_TCF_5) {
		pdp_device_info.res = pdp_resources_tcf5;
		pdp_device_info.num_res = ARRAY_SIZE(pdp_resources_tcf5);
	} else if (tc->version == APOLLO_VERSION_TCF_2 ||
			tc->version == APOLLO_VERSION_TCF_BONNIE) {
		pdp_device_info.res = pdp_resources_es2;
		pdp_device_info.num_res = ARRAY_SIZE(pdp_resources_es2);
	} else {
		dev_err(&tc->pdev->dev,
			"Unable to set PDP resource info for unknown apollo device\n");
	}

	tc->pdp_dev = platform_device_register_full(&pdp_device_info);
	if (IS_ERR(tc->pdp_dev)) {
		err = PTR_ERR(tc->pdp_dev);
		dev_err(&tc->pdev->dev,
			"Failed to register PDP device (%d)\n", err);
		tc->pdp_dev = NULL;
		goto err;
	}
err:
	return err;
}

#if defined(SUPPORT_RGX)

int apollo_register_ext_device(struct tc_device *tc)
{
	int err = 0;
	struct resource rogue_resources[] = {
		DEFINE_RES_MEM_NAMED(pci_resource_start(tc->pdev,
				SYS_RGX_REG_PCI_BASENUM),
			 SYS_RGX_REG_REGION_SIZE, "rogue-regs"),
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
	};
	struct platform_device_info rogue_device_info = {
		.parent = &tc->pdev->dev,
		.name = TC_DEVICE_NAME_ROGUE,
		.id = -2,
		.res = rogue_resources,
		.num_res = ARRAY_SIZE(rogue_resources),
		.data = &pdata,
		.size_data = sizeof(pdata),
		.dma_mask = apollo_get_rogue_dma_mask(tc),
	};

	tc->ext_dev
		= platform_device_register_full(&rogue_device_info);

	if (IS_ERR(tc->ext_dev)) {
		err = PTR_ERR(tc->ext_dev);
		dev_err(&tc->pdev->dev,
			"Failed to register rogue device (%d)\n", err);
		tc->ext_dev = NULL;
	}
	return err;
}

#elif defined(SUPPORT_APOLLO_FPGA)

int apollo_register_ext_device(struct tc_device *tc)
{
	int err = 0;
	struct resource fpga_resources[] = {
		/* FIXME: Don't overload SYS_RGX_REG_xxx for FPGA */
		DEFINE_RES_MEM_NAMED(pci_resource_start(tc->pdev,
				SYS_RGX_REG_PCI_BASENUM),
			 SYS_RGX_REG_REGION_SIZE, "fpga-regs"),
	};
	struct apollo_fpga_platform_data pdata = {
		.mem_mode = tc->mem_mode,
		.tc_memory_base = tc->tc_mem.base,
		.pdp_heap_memory_base = tc->pdp_heap_mem_base,
		.pdp_heap_memory_size = tc->pdp_heap_mem_size,
	};
	struct platform_device_info fpga_device_info = {
		.parent = &tc->pdev->dev,
		.name = APOLLO_DEVICE_NAME_FPGA,
		.id = -1,
		.res = fpga_resources,
		.num_res = ARRAY_SIZE(fpga_resources),
		.data = &pdata,
		.size_data = sizeof(pdata),
		.dma_mask = apollo_get_fpga_dma_mask(tc),
	};

	tc->ext_dev = platform_device_register_full(&fpga_device_info);
	if (IS_ERR(tc->ext_dev)) {
		err = PTR_ERR(tc->ext_dev);
		dev_err(&tc->pdev->dev,
			"Failed to register fpga device (%d)\n", err);
		tc->ext_dev = NULL;
		/* Fall through */
	}

	return err;
}

#else /* defined(SUPPORT_APOLLO_FPGA) */

int apollo_register_ext_device(struct tc_device *tc)
{
	return 0;
}

#endif /* defined(SUPPORT_RGX) */

void apollo_enable_interrupt_register(struct tc_device *tc,
				      int interrupt_id)
{
	u32 val;

	if (interrupt_id == TC_INTERRUPT_PDP ||
		interrupt_id == TC_INTERRUPT_EXT) {
		val = ioread32(
			tc->tcf.registers + TCF_CLK_CTRL_INTERRUPT_ENABLE);
		val |= apollo_interrupt_id_to_flag(interrupt_id);
		iowrite32(val,
			tc->tcf.registers + TCF_CLK_CTRL_INTERRUPT_ENABLE);
	}
}

void apollo_disable_interrupt_register(struct tc_device *tc,
				       int interrupt_id)
{
	u32 val;

	if (interrupt_id == TC_INTERRUPT_PDP ||
		interrupt_id == TC_INTERRUPT_EXT) {
		val = ioread32(
			tc->tcf.registers + TCF_CLK_CTRL_INTERRUPT_ENABLE);
		val &= ~(apollo_interrupt_id_to_flag(interrupt_id));
		iowrite32(val,
			tc->tcf.registers + TCF_CLK_CTRL_INTERRUPT_ENABLE);
	}
}

irqreturn_t apollo_irq_handler(int irq, void *data)
{
	u32 interrupt_status;
	u32 interrupt_clear = 0;
	unsigned long flags;
	irqreturn_t ret = IRQ_NONE;
	struct tc_device *tc = (struct tc_device *)data;

	spin_lock_irqsave(&tc->interrupt_handler_lock, flags);

#if defined(TC_FAKE_INTERRUPTS)
	/* If we're faking interrupts pretend we got both ext and PDP ints */
	interrupt_status = TC_INTERRUPT_FLAG_EXT
		| TC_INTERRUPT_FLAG_PDP;
#else
	interrupt_status = ioread32(tc->tcf.registers
			+ TCF_CLK_CTRL_INTERRUPT_STATUS);
#endif

	if (interrupt_status & TC_INTERRUPT_FLAG_EXT) {
		struct tc_interrupt_handler *ext_int =
			&tc->interrupt_handlers[TC_INTERRUPT_EXT];

		if (ext_int->enabled && ext_int->handler_function) {
			ext_int->handler_function(ext_int->handler_data);
			interrupt_clear |= TC_INTERRUPT_FLAG_EXT;
		}
		ret = IRQ_HANDLED;
	}
	if (interrupt_status & TC_INTERRUPT_FLAG_PDP) {
		struct tc_interrupt_handler *pdp_int =
			&tc->interrupt_handlers[TC_INTERRUPT_PDP];

		if (pdp_int->enabled && pdp_int->handler_function) {
			pdp_int->handler_function(pdp_int->handler_data);
			interrupt_clear |= TC_INTERRUPT_FLAG_PDP;
		}
		ret = IRQ_HANDLED;
	}

	if (tc->version == APOLLO_VERSION_TCF_5) {
		/* On TC5 the interrupt is not  by the TC framework, but
		 * by the PDP itself. So we always have to callback to the tc5
		 * pdp code regardless of the interrupt status of the TCF.
		 */
		struct tc_interrupt_handler *pdp_int =
			&tc->interrupt_handlers[TC_INTERRUPT_TC5_PDP];

		if (pdp_int->enabled && pdp_int->handler_function) {
			pdp_int->handler_function(pdp_int->handler_data);
			ret = IRQ_HANDLED;
		}
	}

	if (interrupt_clear)
		iowrite32(0xffffffff,
			tc->tcf.registers + TCF_CLK_CTRL_INTERRUPT_CLEAR);

	spin_unlock_irqrestore(&tc->interrupt_handler_lock, flags);

	return ret;
}

int apollo_sys_info(struct tc_device *tc, u32 *tmp, u32 *pll)
{
	int err = 0;

	*tmp = 0;
	*pll = 0;

	if (tc->version == APOLLO_VERSION_TCF_5)
		/* Not implemented on TCF5 */
		goto err_out;
	else if (tc->version == APOLLO_VERSION_TCF_2) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 3, 0))
		unsigned long t;
#else
		int t;
#endif

		err = apollo_thermal_get_temp(apollo_pdata.thermal_zone, &t);
		if (err)
			goto err_out;
		*tmp = t / 1000;
	}

	if (spi_read(tc, 0x2, pll)) {
		dev_err(&tc->pdev->dev, "Failed to read PLL status\n");
		err = -ENODEV;
		goto err_out;
	}

err_out:
	return err;
}

int apollo_sys_strings(struct tc_device *tc,
		       char *str_fpga_rev, size_t size_fpga_rev,
		       char *str_tcf_core_rev, size_t size_tcf_core_rev,
		       char *str_tcf_core_target_build_id,
		       size_t size_tcf_core_target_build_id,
		       char *str_pci_ver, size_t size_pci_ver,
		       char *str_macro_ver, size_t size_macro_ver)
{
	int err = 0;
	u32 val;
	resource_size_t host_fpga_base;
	void __iomem *host_fpga_registers;

	/* To get some of the version information we need to read from a
	 * register that we don't normally have mapped. Map it temporarily
	 * (without trying to reserve it) to get the information we need.
	 */
	host_fpga_base =
		pci_resource_start(tc->pdev, SYS_APOLLO_REG_PCI_BASENUM)
				+ 0x40F0;

	host_fpga_registers = ioremap(host_fpga_base, 0x04);
	if (!host_fpga_registers) {
		dev_err(&tc->pdev->dev,
			"Failed to map host fpga registers\n");
		err = -EIO;
		goto err_out;
	}

	/* Create the components of the PCI and macro versions */
	val = ioread32(host_fpga_registers);
	snprintf(str_pci_ver, size_pci_ver, "%d",
		 HEX2DEC((val & 0x00FF0000) >> 16));
	snprintf(str_macro_ver, size_macro_ver, "%d.%d",
		 (val & 0x00000F00) >> 8,
		 HEX2DEC((val & 0x000000FF) >> 0));

	/* Unmap the register now that we no longer need it */
	iounmap(host_fpga_registers);

	/*
	 * Check bits 7:0 of register 0x28 (TCF_CORE_REV_REG or SW_IF_VERSION
	 * depending on its own value) to find out how the driver should
	 * generate the strings for FPGA and core revision.
	 */
	val = ioread32(tc->tcf.registers + TCF_CLK_CTRL_SW_IF_VERSION);
	val = (val & VERSION_MASK) >> VERSION_SHIFT;

	if (val == 0) {
		/* Create the components of the TCF core revision number */
		val = ioread32(tc->tcf.registers + TCF_CLK_CTRL_TCF_CORE_REV_REG);
		snprintf(str_tcf_core_rev, size_tcf_core_rev, "%d.%d.%d",
			 HEX2DEC((val & TCF_CORE_REV_REG_MAJOR_MASK)
				 >> TCF_CORE_REV_REG_MAJOR_SHIFT),
			 HEX2DEC((val & TCF_CORE_REV_REG_MINOR_MASK)
				 >> TCF_CORE_REV_REG_MINOR_SHIFT),
			 HEX2DEC((val & TCF_CORE_REV_REG_MAINT_MASK)
				 >> TCF_CORE_REV_REG_MAINT_SHIFT));

		/* Create the components of the FPGA revision number */
		val = ioread32(tc->tcf.registers + TCF_CLK_CTRL_FPGA_REV_REG);
		snprintf(str_fpga_rev, size_fpga_rev, "%d.%d.%d",
			 HEX2DEC((val & FPGA_REV_REG_MAJOR_MASK)
				 >> FPGA_REV_REG_MAJOR_SHIFT),
			 HEX2DEC((val & FPGA_REV_REG_MINOR_MASK)
				 >> FPGA_REV_REG_MINOR_SHIFT),
			 HEX2DEC((val & FPGA_REV_REG_MAINT_MASK)
				 >> FPGA_REV_REG_MAINT_SHIFT));
	} else if (val == 1) {
		/* Create the components of the TCF core revision number */
		snprintf(str_tcf_core_rev, size_tcf_core_rev, "%d", val);

		/* Create the components of the FPGA revision number */
		val = ioread32(tc->tcf.registers + TCF_CLK_CTRL_REL);
		snprintf(str_fpga_rev, size_fpga_rev, "%d.%d",
			 HEX2DEC((val & MAJOR_MASK) >> MAJOR_SHIFT),
			 HEX2DEC((val & MINOR_MASK) >> MINOR_SHIFT));
	} else {
		dev_warn(&tc->pdev->dev,
			 "%s: unrecognised SW_IF_VERSION %#08x\n",
			 __func__, val);

		/* Create the components of the TCF core revision number */
		snprintf(str_tcf_core_rev, size_tcf_core_rev, "%d", val);

		/* Create the components of the FPGA revision number */
		snprintf(str_fpga_rev, size_fpga_rev, "N/A");
	}

	/* Create the component of the TCF core target build ID */
	val = ioread32(tc->tcf.registers +
		       TCF_CLK_CTRL_TCF_CORE_TARGET_BUILD_CFG);
	snprintf(str_tcf_core_target_build_id, size_tcf_core_target_build_id,
		"%d",
		(val & TCF_CORE_TARGET_BUILD_ID_MASK)
		>> TCF_CORE_TARGET_BUILD_ID_SHIFT);

err_out:
	return err;
}
