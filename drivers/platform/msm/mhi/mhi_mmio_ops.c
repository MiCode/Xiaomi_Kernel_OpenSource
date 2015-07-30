/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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
#include "mhi_sys.h"
#include "mhi_hwio.h"
#include "mhi.h"

enum MHI_STATUS mhi_test_for_device_reset(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	u32 pcie_word_val = 0;
	u32 expiry_counter;

	mhi_log(MHI_MSG_INFO, "Waiting for MMIO RESET bit to be cleared.\n");
	pcie_word_val = mhi_reg_read(mhi_dev_ctxt->mmio_info.mmio_addr,
					MHISTATUS);
	MHI_READ_FIELD(pcie_word_val,
			MHICTRL_RESET_MASK,
			MHICTRL_RESET_SHIFT);
	if (pcie_word_val == 0xFFFFFFFF)
		return MHI_STATUS_LINK_DOWN;
	while (MHI_STATE_RESET != pcie_word_val && expiry_counter < 100) {
		expiry_counter++;
		mhi_log(MHI_MSG_ERROR,
			"Device is not RESET, sleeping and retrying.\n");
		msleep(MHI_READY_STATUS_TIMEOUT_MS);
		pcie_word_val = mhi_reg_read(mhi_dev_ctxt->mmio_info.mmio_addr,
							MHICTRL);
		MHI_READ_FIELD(pcie_word_val,
				MHICTRL_RESET_MASK,
				MHICTRL_RESET_SHIFT);
	}

	if (MHI_STATE_READY != pcie_word_val)
		return MHI_STATUS_DEVICE_NOT_READY;
	return MHI_STATUS_SUCCESS;
}

enum MHI_STATUS mhi_test_for_device_ready(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	u32 pcie_word_val = 0;
	u32 expiry_counter;

	mhi_log(MHI_MSG_INFO, "Waiting for MMIO Ready bit to be set\n");

	/* Read MMIO and poll for READY bit to be set */
	pcie_word_val = mhi_reg_read(
			mhi_dev_ctxt->mmio_info.mmio_addr, MHISTATUS);
	MHI_READ_FIELD(pcie_word_val,
			MHISTATUS_READY_MASK,
			MHISTATUS_READY_SHIFT);

	if (pcie_word_val == 0xFFFFFFFF)
		return MHI_STATUS_LINK_DOWN;
	expiry_counter = 0;
	while (MHI_STATE_READY != pcie_word_val && expiry_counter < 50) {
		expiry_counter++;
		mhi_log(MHI_MSG_ERROR,
			"Device is not ready, sleeping and retrying.\n");
		msleep(MHI_READY_STATUS_TIMEOUT_MS);
		pcie_word_val = mhi_reg_read(mhi_dev_ctxt->mmio_info.mmio_addr,
					     MHISTATUS);
		MHI_READ_FIELD(pcie_word_val,
				MHISTATUS_READY_MASK, MHISTATUS_READY_SHIFT);
	}

	if (pcie_word_val != MHI_STATE_READY)
		return MHI_STATUS_DEVICE_NOT_READY;
	return MHI_STATUS_SUCCESS;
}

enum MHI_STATUS mhi_init_mmio(struct mhi_device_ctxt *mhi_dev_ctxt)
{
	u64 pcie_dword_val = 0;
	u32 pcie_word_val = 0;
	u32 i = 0;
	enum MHI_STATUS ret_val;

	mhi_log(MHI_MSG_INFO, "~~~ Initializing MMIO ~~~\n");
	mhi_dev_ctxt->mmio_info.mmio_addr = mhi_dev_ctxt->dev_props->bar0_base;

	mhi_log(MHI_MSG_INFO, "Bar 0 address is at: 0x%p\n",
			mhi_dev_ctxt->mmio_info.mmio_addr);

	mhi_dev_ctxt->mmio_info.mmio_len = mhi_reg_read(
					mhi_dev_ctxt->mmio_info.mmio_addr,
							 MHIREGLEN);

	if (0 == mhi_dev_ctxt->mmio_info.mmio_len) {
		mhi_log(MHI_MSG_ERROR, "Received mmio length as zero\n");
		return MHI_STATUS_ERROR;
	}

	mhi_log(MHI_MSG_INFO, "Testing MHI Ver\n");
	mhi_dev_ctxt->dev_props->mhi_ver = mhi_reg_read(
				mhi_dev_ctxt->mmio_info.mmio_addr, MHIVER);
	if (MHI_VERSION != mhi_dev_ctxt->dev_props->mhi_ver) {
		mhi_log(MHI_MSG_CRITICAL, "Bad MMIO version, 0x%x\n",
					mhi_dev_ctxt->dev_props->mhi_ver);

		if (mhi_dev_ctxt->dev_props->mhi_ver == 0xFFFFFFFF)
			ret_val = mhi_wait_for_mdm(mhi_dev_ctxt);
		if (ret_val)
			return MHI_STATUS_ERROR;
	}
	/* Enable the channels */
	for (i = 0; i < MHI_MAX_CHANNELS; ++i) {
			struct mhi_chan_ctxt *chan_ctxt =
				&mhi_dev_ctxt->dev_space.ring_ctxt.cc_list[i];
		if (VALID_CHAN_NR(i))
			chan_ctxt->mhi_chan_state = MHI_CHAN_STATE_ENABLED;
		else
			chan_ctxt->mhi_chan_state = MHI_CHAN_STATE_DISABLED;
	}
	mhi_log(MHI_MSG_INFO,
			"Read back MMIO Ready bit successfully. Moving on..\n");
	mhi_log(MHI_MSG_INFO, "Reading channel doorbell offset\n");

	mhi_dev_ctxt->mmio_info.chan_db_addr =
					mhi_dev_ctxt->mmio_info.mmio_addr;
	mhi_dev_ctxt->mmio_info.event_db_addr =
					mhi_dev_ctxt->mmio_info.mmio_addr;

	mhi_dev_ctxt->mmio_info.chan_db_addr += mhi_reg_read_field(
					mhi_dev_ctxt->mmio_info.mmio_addr,
					CHDBOFF, CHDBOFF_CHDBOFF_MASK,
					CHDBOFF_CHDBOFF_SHIFT);

	mhi_log(MHI_MSG_INFO, "Reading event doorbell offset\n");
	mhi_dev_ctxt->mmio_info.event_db_addr += mhi_reg_read_field(
					mhi_dev_ctxt->mmio_info.mmio_addr,
					ERDBOFF, ERDBOFF_ERDBOFF_MASK,
					ERDBOFF_ERDBOFF_SHIFT);

	mhi_log(MHI_MSG_INFO, "Setting all MMIO values.\n");

	mhi_reg_write_field(mhi_dev_ctxt, mhi_dev_ctxt->mmio_info.mmio_addr,
				MHICFG,
				MHICFG_NER_MASK, MHICFG_NER_SHIFT,
				mhi_dev_ctxt->mmio_info.nr_event_rings);

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
	mhi_log(MHI_MSG_INFO, "Done..\n");
	return MHI_STATUS_SUCCESS;
}

