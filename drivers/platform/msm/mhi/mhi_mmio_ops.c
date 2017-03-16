/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/completion.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/msm-bus.h>
#include <linux/cpu.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/pm_runtime.h>
#include "mhi_sys.h"
#include "mhi_hwio.h"
#include "mhi.h"

int mhi_test_for_device_reset(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	u32 pcie_word_val = 0;
	unsigned long flags;
	rwlock_t *pm_xfer_lock = &mhi_dev_ctxt->pm_xfer_lock;
	unsigned long timeout;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Waiting for MMIO RESET bit to be cleared.\n");

	timeout = jiffies +
		msecs_to_jiffies(mhi_dev_ctxt->poll_reset_timeout_ms);
	while (time_before(jiffies, timeout)) {
		read_lock_irqsave(pm_xfer_lock, flags);
		if (!MHI_REG_ACCESS_VALID(mhi_dev_ctxt->mhi_pm_state)) {
			read_unlock_irqrestore(pm_xfer_lock, flags);
			return -EIO;
		}
		pcie_word_val = mhi_reg_read(mhi_dev_ctxt->mmio_info.mmio_addr,
					     MHICTRL);
		read_unlock_irqrestore(&mhi_dev_ctxt->pm_xfer_lock, flags);
		if (pcie_word_val == 0xFFFFFFFF)
			return -ENOTCONN;
		MHI_READ_FIELD(pcie_word_val,
			       MHICTRL_RESET_MASK,
			       MHICTRL_RESET_SHIFT);

		if (!pcie_word_val)
			return 0;

		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"MHI still in Reset sleeping\n");
		msleep(MHI_THREAD_SLEEP_TIMEOUT_MS);
	}
	mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
		"Timeout waiting for reset to be cleared\n");
	return -ETIMEDOUT;
}

int mhi_test_for_device_ready(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	u32 pcie_word_val = 0;
	unsigned long flags;
	rwlock_t *pm_xfer_lock = &mhi_dev_ctxt->pm_xfer_lock;
	unsigned long timeout;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Waiting for MMIO Ready bit to be set\n");

	timeout = jiffies +
		msecs_to_jiffies(mhi_dev_ctxt->poll_reset_timeout_ms);
	while (time_before(jiffies, timeout)) {
		/* Read MMIO and poll for READY bit to be set */
		read_lock_irqsave(pm_xfer_lock, flags);
		if (!MHI_REG_ACCESS_VALID(mhi_dev_ctxt->mhi_pm_state)) {
			read_unlock_irqrestore(pm_xfer_lock, flags);
			return -EIO;
		}

		pcie_word_val = mhi_reg_read(mhi_dev_ctxt->mmio_info.mmio_addr,
					     MHISTATUS);
		read_unlock_irqrestore(pm_xfer_lock, flags);
		if (pcie_word_val == 0xFFFFFFFF)
			return -ENOTCONN;
		MHI_READ_FIELD(pcie_word_val,
			       MHISTATUS_READY_MASK,
			       MHISTATUS_READY_SHIFT);
		if (pcie_word_val == MHI_STATE_READY)
			return 0;
		mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
			"Device is not ready, sleeping and retrying.\n");
		msleep(MHI_THREAD_SLEEP_TIMEOUT_MS);
	}
	mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
		"Device timed out waiting for ready\n");
	return -ETIMEDOUT;
}

int mhi_init_mmio(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	u64 pcie_dword_val = 0;
	u32 pcie_word_val = 0;
	u32 i = 0;
	int ret_val;

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"~~~ Initializing MMIO ~~~\n");
	mhi_dev_ctxt->mmio_info.mmio_addr = mhi_dev_ctxt->core.bar0_base;

	mhi_dev_ctxt->mmio_info.mmio_len = mhi_reg_read(
					mhi_dev_ctxt->mmio_info.mmio_addr,
							 MHIREGLEN);

	if (0 == mhi_dev_ctxt->mmio_info.mmio_len) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_ERROR,
			"Received mmio length as zero\n");
		return -EIO;
	}

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO, "Testing MHI Ver\n");
	mhi_dev_ctxt->core.mhi_ver = mhi_reg_read(
				mhi_dev_ctxt->mmio_info.mmio_addr, MHIVER);
	if (mhi_dev_ctxt->core.mhi_ver != MHI_VERSION) {
		mhi_log(mhi_dev_ctxt, MHI_MSG_CRITICAL,
			"Bad MMIO version, 0x%x\n", mhi_dev_ctxt->core.mhi_ver);
			return ret_val;
	}

	/* Enable the channels */
	for (i = 0; i < MHI_MAX_CHANNELS; ++i) {
			struct mhi_chan_ctxt *chan_ctxt =
				&mhi_dev_ctxt->dev_space.ring_ctxt.cc_list[i];
		if (VALID_CHAN_NR(i))
			chan_ctxt->chstate = MHI_CHAN_STATE_ENABLED;
		else
			chan_ctxt->chstate = MHI_CHAN_STATE_DISABLED;
	}
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Read back MMIO Ready bit successfully. Moving on..\n");
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Reading channel doorbell offset\n");

	mhi_dev_ctxt->mmio_info.chan_db_addr =
					mhi_dev_ctxt->mmio_info.mmio_addr;
	mhi_dev_ctxt->mmio_info.event_db_addr =
					mhi_dev_ctxt->mmio_info.mmio_addr;

	mhi_dev_ctxt->mmio_info.chan_db_addr += mhi_reg_read_field(
					mhi_dev_ctxt->mmio_info.mmio_addr,
					CHDBOFF, CHDBOFF_CHDBOFF_MASK,
					CHDBOFF_CHDBOFF_SHIFT);

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Reading event doorbell offset\n");
	mhi_dev_ctxt->mmio_info.event_db_addr += mhi_reg_read_field(
					mhi_dev_ctxt->mmio_info.mmio_addr,
					ERDBOFF, ERDBOFF_ERDBOFF_MASK,
					ERDBOFF_ERDBOFF_SHIFT);

	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO,
		"Setting all MMIO values.\n");

	mhi_reg_write_field(mhi_dev_ctxt, mhi_dev_ctxt->mmio_info.mmio_addr,
				MHICFG,
				MHICFG_NER_MASK, MHICFG_NER_SHIFT,
				mhi_dev_ctxt->mmio_info.nr_event_rings);
	mhi_reg_write_field(mhi_dev_ctxt, mhi_dev_ctxt->mmio_info.mmio_addr,
			    MHICFG,
			    MHICFG_NHWER_MASK,
			    MHICFG_NHWER_SHIFT,
			    mhi_dev_ctxt->mmio_info.nr_hw_event_rings);

	pcie_dword_val = mhi_dev_ctxt->dev_space.ring_ctxt.dma_cc_list;
	pcie_word_val = HIGH_WORD(pcie_dword_val);
	mhi_reg_write_field(mhi_dev_ctxt,
			    mhi_dev_ctxt->mmio_info.mmio_addr, CCABAP_HIGHER,
			    CCABAP_HIGHER_CCABAP_HIGHER_MASK,
			CCABAP_HIGHER_CCABAP_HIGHER_SHIFT, pcie_word_val);
	pcie_word_val = LOW_WORD(pcie_dword_val);

	mhi_reg_write_field(mhi_dev_ctxt, mhi_dev_ctxt->mmio_info.mmio_addr,
				CCABAP_LOWER,
				CCABAP_LOWER_CCABAP_LOWER_MASK,
				CCABAP_LOWER_CCABAP_LOWER_SHIFT,
				pcie_word_val);

	/* Write the Event Context Base Address Register High and Low parts */
	pcie_dword_val = mhi_dev_ctxt->dev_space.ring_ctxt.dma_ec_list;
	pcie_word_val = HIGH_WORD(pcie_dword_val);
	mhi_reg_write_field(mhi_dev_ctxt,
			mhi_dev_ctxt->mmio_info.mmio_addr, ECABAP_HIGHER,
			ECABAP_HIGHER_ECABAP_HIGHER_MASK,
			ECABAP_HIGHER_ECABAP_HIGHER_SHIFT, pcie_word_val);
	pcie_word_val = LOW_WORD(pcie_dword_val);

	mhi_reg_write_field(mhi_dev_ctxt, mhi_dev_ctxt->mmio_info.mmio_addr,
			ECABAP_LOWER,
			ECABAP_LOWER_ECABAP_LOWER_MASK,
			ECABAP_LOWER_ECABAP_LOWER_SHIFT, pcie_word_val);

	/* Write the Command Ring Control Register High and Low parts */
	pcie_dword_val = mhi_dev_ctxt->dev_space.ring_ctxt.dma_cmd_ctxt;
	pcie_word_val = HIGH_WORD(pcie_dword_val);
	mhi_reg_write_field(mhi_dev_ctxt,
				mhi_dev_ctxt->mmio_info.mmio_addr,
				CRCBAP_HIGHER,
				CRCBAP_HIGHER_CRCBAP_HIGHER_MASK,
				CRCBAP_HIGHER_CRCBAP_HIGHER_SHIFT,
				pcie_word_val);
	pcie_word_val = LOW_WORD(pcie_dword_val);
	mhi_reg_write_field(mhi_dev_ctxt,
			mhi_dev_ctxt->mmio_info.mmio_addr, CRCBAP_LOWER,
			CRCBAP_LOWER_CRCBAP_LOWER_MASK,
			CRCBAP_LOWER_CRCBAP_LOWER_SHIFT,
			pcie_word_val);

	mhi_dev_ctxt->mmio_info.cmd_db_addr =
			mhi_dev_ctxt->mmio_info.mmio_addr + CRDB_LOWER;
	/* Set the control and data segments device MMIO */
	pcie_dword_val = mhi_dev_ctxt->dev_space.start_win_addr;
	pcie_word_val = HIGH_WORD(pcie_dword_val);
	mhi_reg_write_field(mhi_dev_ctxt,
			mhi_dev_ctxt->mmio_info.mmio_addr, MHICTRLBASE_HIGHER,
			MHICTRLBASE_HIGHER_MHICTRLBASE_HIGHER_MASK,
			MHICTRLBASE_HIGHER_MHICTRLBASE_HIGHER_SHIFT,
			pcie_word_val);
	mhi_reg_write_field(mhi_dev_ctxt,
			mhi_dev_ctxt->mmio_info.mmio_addr, MHIDATABASE_HIGHER,
			MHIDATABASE_HIGHER_MHIDATABASE_HIGHER_MASK,
			MHIDATABASE_HIGHER_MHIDATABASE_HIGHER_SHIFT,
			pcie_word_val);

	pcie_word_val = LOW_WORD(pcie_dword_val);
	mhi_reg_write_field(mhi_dev_ctxt,
			mhi_dev_ctxt->mmio_info.mmio_addr, MHICTRLBASE_LOWER,
			MHICTRLBASE_LOWER_MHICTRLBASE_LOWER_MASK,
			MHICTRLBASE_LOWER_MHICTRLBASE_LOWER_SHIFT,
			pcie_word_val);
	mhi_reg_write_field(mhi_dev_ctxt,
			mhi_dev_ctxt->mmio_info.mmio_addr, MHIDATABASE_LOWER,
			MHIDATABASE_LOWER_MHIDATABASE_LOWER_MASK,
			MHIDATABASE_LOWER_MHIDATABASE_LOWER_SHIFT,
			pcie_word_val);

	pcie_dword_val = mhi_dev_ctxt->dev_space.end_win_addr;

	pcie_word_val = HIGH_WORD(pcie_dword_val);
	mhi_reg_write_field(mhi_dev_ctxt,
			mhi_dev_ctxt->mmio_info.mmio_addr, MHICTRLLIMIT_HIGHER,
			MHICTRLLIMIT_HIGHER_MHICTRLLIMIT_HIGHER_MASK,
			MHICTRLLIMIT_HIGHER_MHICTRLLIMIT_HIGHER_SHIFT,
			pcie_word_val);
	mhi_reg_write_field(mhi_dev_ctxt,
			mhi_dev_ctxt->mmio_info.mmio_addr, MHIDATALIMIT_HIGHER,
			MHIDATALIMIT_HIGHER_MHIDATALIMIT_HIGHER_MASK,
			MHIDATALIMIT_HIGHER_MHIDATALIMIT_HIGHER_SHIFT,
			pcie_word_val);

	pcie_word_val = LOW_WORD(pcie_dword_val);

	mhi_reg_write_field(mhi_dev_ctxt,
			mhi_dev_ctxt->mmio_info.mmio_addr, MHICTRLLIMIT_LOWER,
			MHICTRLLIMIT_LOWER_MHICTRLLIMIT_LOWER_MASK,
			MHICTRLLIMIT_LOWER_MHICTRLLIMIT_LOWER_SHIFT,
			pcie_word_val);
	mhi_reg_write_field(mhi_dev_ctxt,
			    mhi_dev_ctxt->mmio_info.mmio_addr,
			    MHIDATALIMIT_LOWER,
			MHIDATALIMIT_LOWER_MHIDATALIMIT_LOWER_MASK,
			MHIDATALIMIT_LOWER_MHIDATALIMIT_LOWER_SHIFT,
			pcie_word_val);
	mhi_log(mhi_dev_ctxt, MHI_MSG_INFO, "Done..\n");
	return 0;
}

