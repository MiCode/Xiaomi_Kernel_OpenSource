// SPDX-License-Identifier: GPL-2.0-only
/* Atlantic Network Driver
 *
 * Copyright (C) 2020 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "macsec_api.h"
#include <linux/mdio.h>
#include "MSS_Ingress_registers.h"
#include "MSS_Egress_registers.h"
#include "atl_mdio.h"

#define AQ_API_CALL_SAFE(func, ...)                                            \
({                                                                             \
	int ret;                                                               \
	do {                                                                   \
		ret = atl_mdio_hwsem_get(hw);                                  \
		if (unlikely(ret))                                             \
			break;                                                 \
									       \
		ret = func(__VA_ARGS__);                                       \
									       \
		atl_mdio_hwsem_put(hw);                                        \
	} while (0);                                                           \
	ret;                                                                   \
})

/*******************************************************************************
 *                          MACSEC config and status
 ******************************************************************************/

static int set_raw_ingress_record(struct atl_hw *hw, u16 *packed_record,
				  u8 num_words, u8 table_id,
				  u16 table_index)
{
	struct mss_ingress_lut_addr_ctl_register lut_sel_reg;
	struct mss_ingress_lut_ctl_register lut_op_reg;

	unsigned int i;

	/* NOTE: MSS registers must always be read/written as adjacent pairs.
	 * For instance, to write either or both 1E.80A0 and 80A1, we have to:
	 * 1. Write 1E.80A0 first
	 * 2. Then write 1E.80A1
	 *
	 * For HHD devices: These writes need to be performed consecutively, and
	 * to ensure this we use the PIF mailbox to delegate the reads/writes to
	 * the FW.
	 *
	 * For EUR devices: Not need to use the PIF mailbox; it is safe to
	 * write to the registers directly.
	 */

	/* Write the packed record words to the data buffer registers. */
	for (i = 0; i < num_words; i += 2) {
		__atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
				 MSS_INGRESS_LUT_DATA_CTL_REGISTER_ADDR + i,
				 packed_record[i]);
		__atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
				 MSS_INGRESS_LUT_DATA_CTL_REGISTER_ADDR + i + 1,
				 packed_record[i + 1]);
	}

	/* Clear out the unused data buffer registers. */
	for (i = num_words; i < 24; i += 2) {
		__atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
				 MSS_INGRESS_LUT_DATA_CTL_REGISTER_ADDR + i, 0);
		__atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
				 MSS_INGRESS_LUT_DATA_CTL_REGISTER_ADDR + i + 1,
				 0);
	}

	/* Select the table and row index to write to */
	lut_sel_reg.bits_0.lut_select = table_id;
	lut_sel_reg.bits_0.lut_addr = table_index;

	lut_op_reg.bits_0.lut_read = 0;
	lut_op_reg.bits_0.lut_write = 1;

	__atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
			 MSS_INGRESS_LUT_ADDR_CTL_REGISTER_ADDR,
			 lut_sel_reg.word_0);
	__atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
			 MSS_INGRESS_LUT_CTL_REGISTER_ADDR,
			 lut_op_reg.word_0);

	return 0;
}

/*! Read the specified Ingress LUT table row.
 *  packed_record - [OUT] The table row data (raw).
 */
static int get_raw_ingress_record(struct atl_hw *hw, u16 *packed_record,
				  u8 num_words, u8 table_id,
				  u16 table_index)
{
	struct mss_ingress_lut_addr_ctl_register lut_sel_reg;
	struct mss_ingress_lut_ctl_register lut_op_reg;
	int ret;

	unsigned int i;

	/* Select the table and row index to read */
	lut_sel_reg.bits_0.lut_select = table_id;
	lut_sel_reg.bits_0.lut_addr = table_index;

	lut_op_reg.bits_0.lut_read = 1;
	lut_op_reg.bits_0.lut_write = 0;

	ret = __atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
			       MSS_INGRESS_LUT_ADDR_CTL_REGISTER_ADDR,
			       lut_sel_reg.word_0);
	if (unlikely(ret))
		return ret;
	ret = __atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
			       MSS_INGRESS_LUT_CTL_REGISTER_ADDR,
			       lut_op_reg.word_0);
	if (unlikely(ret))
		return ret;

	memset(packed_record, 0, sizeof(u16) * num_words);

	for (i = 0; i < num_words; i += 2) {
		ret = __atl_mdio_read(hw, 0, MDIO_MMD_VEND1,
				      MSS_INGRESS_LUT_DATA_CTL_REGISTER_ADDR +
					      i,
				      &packed_record[i]);
		if (unlikely(ret))
			return ret;
		ret = __atl_mdio_read(hw, 0, MDIO_MMD_VEND1,
				      MSS_INGRESS_LUT_DATA_CTL_REGISTER_ADDR +
					      i + 1,
				      &packed_record[i + 1]);
		if (unlikely(ret))
			return ret;
	}

	return 0;
}

/*! Write packed_record to the specified Egress LUT table row. */
static int set_raw_egress_record(struct atl_hw *hw, u16 *packed_record,
				 u8 num_words, u8 table_id,
				 u16 table_index)
{
	struct mss_egress_lut_addr_ctl_register lut_sel_reg;
	struct mss_egress_lut_ctl_register lut_op_reg;

	unsigned int i;

	/* Write the packed record words to the data buffer registers. */
	for (i = 0; i < num_words; i += 2) {
		__atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
				 MSS_EGRESS_LUT_DATA_CTL_REGISTER_ADDR + i,
				 packed_record[i]);
		__atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
				 MSS_EGRESS_LUT_DATA_CTL_REGISTER_ADDR + i + 1,
				 packed_record[i + 1]);
	}

	/* Clear out the unused data buffer registers. */
	for (i = num_words; i < 28; i += 2) {
		__atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
				 MSS_EGRESS_LUT_DATA_CTL_REGISTER_ADDR + i, 0);
		__atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
				 MSS_EGRESS_LUT_DATA_CTL_REGISTER_ADDR + i + 1,
				 0);
	}

	/* Select the table and row index to write to */
	lut_sel_reg.bits_0.lut_select = table_id;
	lut_sel_reg.bits_0.lut_addr = table_index;

	lut_op_reg.bits_0.lut_read = 0;
	lut_op_reg.bits_0.lut_write = 1;

	__atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
			 MSS_EGRESS_LUT_ADDR_CTL_REGISTER_ADDR,
			 lut_sel_reg.word_0);
	__atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
			 MSS_EGRESS_LUT_CTL_REGISTER_ADDR,
			 lut_op_reg.word_0);

	return 0;
}

static int get_raw_egress_record(struct atl_hw *hw, u16 *packed_record,
				 u8 num_words, u8 table_id,
				 u16 table_index)
{
	struct mss_egress_lut_addr_ctl_register lut_sel_reg;
	struct mss_egress_lut_ctl_register lut_op_reg;
	int ret;

	unsigned int i;

	/* Select the table and row index to read */
	lut_sel_reg.bits_0.lut_select = table_id;
	lut_sel_reg.bits_0.lut_addr = table_index;

	lut_op_reg.bits_0.lut_read = 1;
	lut_op_reg.bits_0.lut_write = 0;

	ret = __atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
			       MSS_EGRESS_LUT_ADDR_CTL_REGISTER_ADDR,
			       lut_sel_reg.word_0);
	if (unlikely(ret))
		return ret;
	ret = __atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
			       MSS_EGRESS_LUT_CTL_REGISTER_ADDR,
			       lut_op_reg.word_0);
	if (unlikely(ret))
		return ret;

	memset(packed_record, 0, sizeof(u16) * num_words);

	for (i = 0; i < num_words; i += 2) {
		ret = __atl_mdio_read(hw, 0, MDIO_MMD_VEND1,
				      MSS_EGRESS_LUT_DATA_CTL_REGISTER_ADDR + i,
				      &packed_record[i]);
		if (unlikely(ret))
			return ret;
		ret = __atl_mdio_read(hw, 0, MDIO_MMD_VEND1,
				      MSS_EGRESS_LUT_DATA_CTL_REGISTER_ADDR +
					      i + 1,
				      &packed_record[i + 1]);
		if (unlikely(ret))
			return ret;
	}

	return 0;
}

static int
set_ingress_prectlf_record(struct atl_hw *hw,
			   const struct aq_mss_ingress_prectlf_record *rec,
			   u16 table_index)
{
	u16 packed_record[6];

	if (table_index >= NUMROWS_INGRESSPRECTLFRECORD)
		return -EINVAL;

	memset(packed_record, 0, sizeof(u16) * 6);

	packed_record[0] = (packed_record[0] & 0x0000) |
			   (((rec->sa_da[0] >> 0) & 0xFFFF) << 0);
	packed_record[1] = (packed_record[1] & 0x0000) |
			   (((rec->sa_da[0] >> 16) & 0xFFFF) << 0);
	packed_record[2] = (packed_record[2] & 0x0000) |
			   (((rec->sa_da[1] >> 0) & 0xFFFF) << 0);
	packed_record[3] = (packed_record[3] & 0x0000) |
			   (((rec->eth_type >> 0) & 0xFFFF) << 0);
	packed_record[4] = (packed_record[4] & 0x0000) |
			   (((rec->match_mask >> 0) & 0xFFFF) << 0);
	packed_record[5] = (packed_record[5] & 0xFFF0) |
			   (((rec->match_type >> 0) & 0xF) << 0);
	packed_record[5] =
		(packed_record[5] & 0xFFEF) | (((rec->action >> 0) & 0x1) << 4);

	return set_raw_ingress_record(hw, packed_record, 6, 0,
				      ROWOFFSET_INGRESSPRECTLFRECORD +
					      table_index);
}

int aq_mss_set_ingress_prectlf_record(
	struct atl_hw *hw, const struct aq_mss_ingress_prectlf_record *rec,
	u16 table_index)
{
	return AQ_API_CALL_SAFE(set_ingress_prectlf_record, hw, rec,
				table_index);
}

static int get_ingress_prectlf_record(struct atl_hw *hw,
				      struct aq_mss_ingress_prectlf_record *rec,
				      u16 table_index)
{
	u16 packed_record[6];
	int ret;

	if (table_index >= NUMROWS_INGRESSPRECTLFRECORD)
		return -EINVAL;

	/* If the row that we want to read is odd, first read the previous even
	 * row, throw that value away, and finally read the desired row.
	 * This is a workaround for EUR devices that allows us to read
	 * odd-numbered rows.  For HHD devices: this workaround will not work,
	 * so don't bother; odd-numbered rows are not readable.
	 */
	if ((table_index % 2) > 0) {
		ret = get_raw_ingress_record(hw, packed_record, 6, 0,
					     ROWOFFSET_INGRESSPRECTLFRECORD +
						     table_index - 1);
		if (unlikely(ret))
			return ret;
	}

	ret = get_raw_ingress_record(hw, packed_record, 6, 0,
				     ROWOFFSET_INGRESSPRECTLFRECORD +
					     table_index);
	if (unlikely(ret))
		return ret;

	rec->sa_da[0] = (rec->sa_da[0] & 0xFFFF0000) |
			(((packed_record[0] >> 0) & 0xFFFF) << 0);
	rec->sa_da[0] = (rec->sa_da[0] & 0x0000FFFF) |
			(((packed_record[1] >> 0) & 0xFFFF) << 16);

	rec->sa_da[1] = (rec->sa_da[1] & 0xFFFF0000) |
			(((packed_record[2] >> 0) & 0xFFFF) << 0);

	rec->eth_type = (rec->eth_type & 0xFFFF0000) |
			(((packed_record[3] >> 0) & 0xFFFF) << 0);

	rec->match_mask = (rec->match_mask & 0xFFFF0000) |
			  (((packed_record[4] >> 0) & 0xFFFF) << 0);

	rec->match_type = (rec->match_type & 0xFFFFFFF0) |
			  (((packed_record[5] >> 0) & 0xF) << 0);

	rec->action = (rec->action & 0xFFFFFFFE) |
		      (((packed_record[5] >> 4) & 0x1) << 0);

	return 0;
}

int aq_mss_get_ingress_prectlf_record(struct atl_hw *hw,
				      struct aq_mss_ingress_prectlf_record *rec,
				      u16 table_index)
{
	memset(rec, 0, sizeof(*rec));

	return AQ_API_CALL_SAFE(get_ingress_prectlf_record, hw, rec,
				table_index);
}

static int
set_ingress_preclass_record(struct atl_hw *hw,
			    const struct aq_mss_ingress_preclass_record *rec,
			    u16 table_index)
{
	u16 packed_record[20];

	if (table_index >= NUMROWS_INGRESSPRECLASSRECORD)
		return -EINVAL;

	memset(packed_record, 0, sizeof(u16) * 20);

	packed_record[0] = (packed_record[0] & 0x0000) |
			   (((rec->sci[0] >> 0) & 0xFFFF) << 0);
	packed_record[1] = (packed_record[1] & 0x0000) |
			   (((rec->sci[0] >> 16) & 0xFFFF) << 0);

	packed_record[2] = (packed_record[2] & 0x0000) |
			   (((rec->sci[1] >> 0) & 0xFFFF) << 0);
	packed_record[3] = (packed_record[3] & 0x0000) |
			   (((rec->sci[1] >> 16) & 0xFFFF) << 0);

	packed_record[4] =
		(packed_record[4] & 0xFF00) | (((rec->tci >> 0) & 0xFF) << 0);

	packed_record[4] = (packed_record[4] & 0x00FF) |
			   (((rec->encr_offset >> 0) & 0xFF) << 8);

	packed_record[5] = (packed_record[5] & 0x0000) |
			   (((rec->eth_type >> 0) & 0xFFFF) << 0);

	packed_record[6] = (packed_record[6] & 0x0000) |
			   (((rec->snap[0] >> 0) & 0xFFFF) << 0);
	packed_record[7] = (packed_record[7] & 0x0000) |
			   (((rec->snap[0] >> 16) & 0xFFFF) << 0);

	packed_record[8] = (packed_record[8] & 0xFF00) |
			   (((rec->snap[1] >> 0) & 0xFF) << 0);

	packed_record[8] =
		(packed_record[8] & 0x00FF) | (((rec->llc >> 0) & 0xFF) << 8);
	packed_record[9] =
		(packed_record[9] & 0x0000) | (((rec->llc >> 8) & 0xFFFF) << 0);

	packed_record[10] = (packed_record[10] & 0x0000) |
			    (((rec->mac_sa[0] >> 0) & 0xFFFF) << 0);
	packed_record[11] = (packed_record[11] & 0x0000) |
			    (((rec->mac_sa[0] >> 16) & 0xFFFF) << 0);

	packed_record[12] = (packed_record[12] & 0x0000) |
			    (((rec->mac_sa[1] >> 0) & 0xFFFF) << 0);

	packed_record[13] = (packed_record[13] & 0x0000) |
			    (((rec->mac_da[0] >> 0) & 0xFFFF) << 0);
	packed_record[14] = (packed_record[14] & 0x0000) |
			    (((rec->mac_da[0] >> 16) & 0xFFFF) << 0);

	packed_record[15] = (packed_record[15] & 0x0000) |
			    (((rec->mac_da[1] >> 0) & 0xFFFF) << 0);

	packed_record[16] = (packed_record[16] & 0xFFFE) |
			    (((rec->lpbk_packet >> 0) & 0x1) << 0);

	packed_record[16] = (packed_record[16] & 0xFFF9) |
			    (((rec->an_mask >> 0) & 0x3) << 1);

	packed_record[16] = (packed_record[16] & 0xFE07) |
			    (((rec->tci_mask >> 0) & 0x3F) << 3);

	packed_record[16] = (packed_record[16] & 0x01FF) |
			    (((rec->sci_mask >> 0) & 0x7F) << 9);
	packed_record[17] = (packed_record[17] & 0xFFFE) |
			    (((rec->sci_mask >> 7) & 0x1) << 0);

	packed_record[17] = (packed_record[17] & 0xFFF9) |
			    (((rec->eth_type_mask >> 0) & 0x3) << 1);

	packed_record[17] = (packed_record[17] & 0xFF07) |
			    (((rec->snap_mask >> 0) & 0x1F) << 3);

	packed_record[17] = (packed_record[17] & 0xF8FF) |
			    (((rec->llc_mask >> 0) & 0x7) << 8);

	packed_record[17] = (packed_record[17] & 0xF7FF) |
			    (((rec->_802_2_encapsulate >> 0) & 0x1) << 11);

	packed_record[17] = (packed_record[17] & 0x0FFF) |
			    (((rec->sa_mask >> 0) & 0xF) << 12);
	packed_record[18] = (packed_record[18] & 0xFFFC) |
			    (((rec->sa_mask >> 4) & 0x3) << 0);

	packed_record[18] = (packed_record[18] & 0xFF03) |
			    (((rec->da_mask >> 0) & 0x3F) << 2);

	packed_record[18] = (packed_record[18] & 0xFEFF) |
			    (((rec->lpbk_mask >> 0) & 0x1) << 8);

	packed_record[18] = (packed_record[18] & 0xC1FF) |
			    (((rec->sc_idx >> 0) & 0x1F) << 9);

	packed_record[18] = (packed_record[18] & 0xBFFF) |
			    (((rec->proc_dest >> 0) & 0x1) << 14);

	packed_record[18] = (packed_record[18] & 0x7FFF) |
			    (((rec->action >> 0) & 0x1) << 15);
	packed_record[19] = (packed_record[19] & 0xFFFE) |
			    (((rec->action >> 1) & 0x1) << 0);

	packed_record[19] = (packed_record[19] & 0xFFFD) |
			    (((rec->ctrl_unctrl >> 0) & 0x1) << 1);

	packed_record[19] = (packed_record[19] & 0xFFFB) |
			    (((rec->sci_from_table >> 0) & 0x1) << 2);

	packed_record[19] = (packed_record[19] & 0xFF87) |
			    (((rec->reserved >> 0) & 0xF) << 3);

	packed_record[19] =
		(packed_record[19] & 0xFF7F) | (((rec->valid >> 0) & 0x1) << 7);

	return set_raw_ingress_record(hw, packed_record, 20, 1,
				      ROWOFFSET_INGRESSPRECLASSRECORD +
					      table_index);
}

int aq_mss_set_ingress_preclass_record(
	struct atl_hw *hw, const struct aq_mss_ingress_preclass_record *rec,
	u16 table_index)
{
	return AQ_API_CALL_SAFE(set_ingress_preclass_record, hw, rec,
				table_index);
}

static int
get_ingress_preclass_record(struct atl_hw *hw,
			    struct aq_mss_ingress_preclass_record *rec,
			    u16 table_index)
{
	u16 packed_record[20];
	int ret;

	if (table_index >= NUMROWS_INGRESSPRECLASSRECORD)
		return -EINVAL;

	/* If the row that we want to read is odd, first read the previous even
	 * row, throw that value away, and finally read the desired row.
	 */
	if ((table_index % 2) > 0) {
		ret = get_raw_ingress_record(hw, packed_record, 20, 1,
					     ROWOFFSET_INGRESSPRECLASSRECORD +
						     table_index - 1);
		if (unlikely(ret))
			return ret;
	}

	ret = get_raw_ingress_record(hw, packed_record, 20, 1,
				     ROWOFFSET_INGRESSPRECLASSRECORD +
					     table_index);
	if (unlikely(ret))
		return ret;

	rec->sci[0] = (rec->sci[0] & 0xFFFF0000) |
		      (((packed_record[0] >> 0) & 0xFFFF) << 0);
	rec->sci[0] = (rec->sci[0] & 0x0000FFFF) |
		      (((packed_record[1] >> 0) & 0xFFFF) << 16);

	rec->sci[1] = (rec->sci[1] & 0xFFFF0000) |
		      (((packed_record[2] >> 0) & 0xFFFF) << 0);
	rec->sci[1] = (rec->sci[1] & 0x0000FFFF) |
		      (((packed_record[3] >> 0) & 0xFFFF) << 16);

	rec->tci = (rec->tci & 0xFFFFFF00) |
		   (((packed_record[4] >> 0) & 0xFF) << 0);

	rec->encr_offset = (rec->encr_offset & 0xFFFFFF00) |
			   (((packed_record[4] >> 8) & 0xFF) << 0);

	rec->eth_type = (rec->eth_type & 0xFFFF0000) |
			(((packed_record[5] >> 0) & 0xFFFF) << 0);

	rec->snap[0] = (rec->snap[0] & 0xFFFF0000) |
		       (((packed_record[6] >> 0) & 0xFFFF) << 0);
	rec->snap[0] = (rec->snap[0] & 0x0000FFFF) |
		       (((packed_record[7] >> 0) & 0xFFFF) << 16);

	rec->snap[1] = (rec->snap[1] & 0xFFFFFF00) |
		       (((packed_record[8] >> 0) & 0xFF) << 0);

	rec->llc = (rec->llc & 0xFFFFFF00) |
		   (((packed_record[8] >> 8) & 0xFF) << 0);
	rec->llc = (rec->llc & 0xFF0000FF) |
		   (((packed_record[9] >> 0) & 0xFFFF) << 8);

	rec->mac_sa[0] = (rec->mac_sa[0] & 0xFFFF0000) |
			 (((packed_record[10] >> 0) & 0xFFFF) << 0);
	rec->mac_sa[0] = (rec->mac_sa[0] & 0x0000FFFF) |
			 (((packed_record[11] >> 0) & 0xFFFF) << 16);

	rec->mac_sa[1] = (rec->mac_sa[1] & 0xFFFF0000) |
			 (((packed_record[12] >> 0) & 0xFFFF) << 0);

	rec->mac_da[0] = (rec->mac_da[0] & 0xFFFF0000) |
			 (((packed_record[13] >> 0) & 0xFFFF) << 0);
	rec->mac_da[0] = (rec->mac_da[0] & 0x0000FFFF) |
			 (((packed_record[14] >> 0) & 0xFFFF) << 16);

	rec->mac_da[1] = (rec->mac_da[1] & 0xFFFF0000) |
			 (((packed_record[15] >> 0) & 0xFFFF) << 0);

	rec->lpbk_packet = (rec->lpbk_packet & 0xFFFFFFFE) |
			   (((packed_record[16] >> 0) & 0x1) << 0);

	rec->an_mask = (rec->an_mask & 0xFFFFFFFC) |
		       (((packed_record[16] >> 1) & 0x3) << 0);

	rec->tci_mask = (rec->tci_mask & 0xFFFFFFC0) |
			(((packed_record[16] >> 3) & 0x3F) << 0);

	rec->sci_mask = (rec->sci_mask & 0xFFFFFF80) |
			(((packed_record[16] >> 9) & 0x7F) << 0);
	rec->sci_mask = (rec->sci_mask & 0xFFFFFF7F) |
			(((packed_record[17] >> 0) & 0x1) << 7);

	rec->eth_type_mask = (rec->eth_type_mask & 0xFFFFFFFC) |
			     (((packed_record[17] >> 1) & 0x3) << 0);

	rec->snap_mask = (rec->snap_mask & 0xFFFFFFE0) |
			 (((packed_record[17] >> 3) & 0x1F) << 0);

	rec->llc_mask = (rec->llc_mask & 0xFFFFFFF8) |
			(((packed_record[17] >> 8) & 0x7) << 0);

	rec->_802_2_encapsulate = (rec->_802_2_encapsulate & 0xFFFFFFFE) |
				  (((packed_record[17] >> 11) & 0x1) << 0);

	rec->sa_mask = (rec->sa_mask & 0xFFFFFFF0) |
		       (((packed_record[17] >> 12) & 0xF) << 0);
	rec->sa_mask = (rec->sa_mask & 0xFFFFFFCF) |
		       (((packed_record[18] >> 0) & 0x3) << 4);

	rec->da_mask = (rec->da_mask & 0xFFFFFFC0) |
		       (((packed_record[18] >> 2) & 0x3F) << 0);

	rec->lpbk_mask = (rec->lpbk_mask & 0xFFFFFFFE) |
			 (((packed_record[18] >> 8) & 0x1) << 0);

	rec->sc_idx = (rec->sc_idx & 0xFFFFFFE0) |
		      (((packed_record[18] >> 9) & 0x1F) << 0);

	rec->proc_dest = (rec->proc_dest & 0xFFFFFFFE) |
			 (((packed_record[18] >> 14) & 0x1) << 0);

	rec->action = (rec->action & 0xFFFFFFFE) |
		      (((packed_record[18] >> 15) & 0x1) << 0);
	rec->action = (rec->action & 0xFFFFFFFD) |
		      (((packed_record[19] >> 0) & 0x1) << 1);

	rec->ctrl_unctrl = (rec->ctrl_unctrl & 0xFFFFFFFE) |
			   (((packed_record[19] >> 1) & 0x1) << 0);

	rec->sci_from_table = (rec->sci_from_table & 0xFFFFFFFE) |
			      (((packed_record[19] >> 2) & 0x1) << 0);

	rec->reserved = (rec->reserved & 0xFFFFFFF0) |
			(((packed_record[19] >> 3) & 0xF) << 0);

	rec->valid = (rec->valid & 0xFFFFFFFE) |
		     (((packed_record[19] >> 7) & 0x1) << 0);

	return 0;
}

int aq_mss_get_ingress_preclass_record(
	struct atl_hw *hw, struct aq_mss_ingress_preclass_record *rec,
	u16 table_index)
{
	memset(rec, 0, sizeof(*rec));

	return AQ_API_CALL_SAFE(get_ingress_preclass_record, hw, rec,
				table_index);
}

static int set_ingress_sc_record(struct atl_hw *hw,
				 const struct aq_mss_ingress_sc_record *rec,
				 u16 table_index)
{
	u16 packed_record[8];

	if (table_index >= NUMROWS_INGRESSSCRECORD)
		return -EINVAL;

	memset(packed_record, 0, sizeof(u16) * 8);

	packed_record[0] = (packed_record[0] & 0x0000) |
			   (((rec->stop_time >> 0) & 0xFFFF) << 0);
	packed_record[1] = (packed_record[1] & 0x0000) |
			   (((rec->stop_time >> 16) & 0xFFFF) << 0);

	packed_record[2] = (packed_record[2] & 0x0000) |
			   (((rec->start_time >> 0) & 0xFFFF) << 0);
	packed_record[3] = (packed_record[3] & 0x0000) |
			   (((rec->start_time >> 16) & 0xFFFF) << 0);

	packed_record[4] = (packed_record[4] & 0xFFFC) |
			   (((rec->validate_frames >> 0) & 0x3) << 0);

	packed_record[4] = (packed_record[4] & 0xFFFB) |
			   (((rec->replay_protect >> 0) & 0x1) << 2);

	packed_record[4] = (packed_record[4] & 0x0007) |
			   (((rec->anti_replay_window >> 0) & 0x1FFF) << 3);
	packed_record[5] = (packed_record[5] & 0x0000) |
			   (((rec->anti_replay_window >> 13) & 0xFFFF) << 0);
	packed_record[6] = (packed_record[6] & 0xFFF8) |
			   (((rec->anti_replay_window >> 29) & 0x7) << 0);

	packed_record[6] = (packed_record[6] & 0xFFF7) |
			   (((rec->receiving >> 0) & 0x1) << 3);

	packed_record[6] =
		(packed_record[6] & 0xFFEF) | (((rec->fresh >> 0) & 0x1) << 4);

	packed_record[6] =
		(packed_record[6] & 0xFFDF) | (((rec->an_rol >> 0) & 0x1) << 5);

	packed_record[6] = (packed_record[6] & 0x003F) |
			   (((rec->reserved >> 0) & 0x3FF) << 6);
	packed_record[7] = (packed_record[7] & 0x8000) |
			   (((rec->reserved >> 10) & 0x7FFF) << 0);

	packed_record[7] =
		(packed_record[7] & 0x7FFF) | (((rec->valid >> 0) & 0x1) << 15);

	return set_raw_ingress_record(hw, packed_record, 8, 3,
				      ROWOFFSET_INGRESSSCRECORD + table_index);
}

int aq_mss_set_ingress_sc_record(struct atl_hw *hw,
				 const struct aq_mss_ingress_sc_record *rec,
				 u16 table_index)
{
	return AQ_API_CALL_SAFE(set_ingress_sc_record, hw, rec, table_index);
}

static int get_ingress_sc_record(struct atl_hw *hw,
				 struct aq_mss_ingress_sc_record *rec,
				 u16 table_index)
{
	u16 packed_record[8];
	int ret;

	if (table_index >= NUMROWS_INGRESSSCRECORD)
		return -EINVAL;

	ret = get_raw_ingress_record(hw, packed_record, 8, 3,
				     ROWOFFSET_INGRESSSCRECORD + table_index);
	if (unlikely(ret))
		return ret;

	rec->stop_time = (rec->stop_time & 0xFFFF0000) |
			 (((packed_record[0] >> 0) & 0xFFFF) << 0);
	rec->stop_time = (rec->stop_time & 0x0000FFFF) |
			 (((packed_record[1] >> 0) & 0xFFFF) << 16);

	rec->start_time = (rec->start_time & 0xFFFF0000) |
			  (((packed_record[2] >> 0) & 0xFFFF) << 0);
	rec->start_time = (rec->start_time & 0x0000FFFF) |
			  (((packed_record[3] >> 0) & 0xFFFF) << 16);

	rec->validate_frames = (rec->validate_frames & 0xFFFFFFFC) |
			       (((packed_record[4] >> 0) & 0x3) << 0);

	rec->replay_protect = (rec->replay_protect & 0xFFFFFFFE) |
			      (((packed_record[4] >> 2) & 0x1) << 0);

	rec->anti_replay_window = (rec->anti_replay_window & 0xFFFFE000) |
				  (((packed_record[4] >> 3) & 0x1FFF) << 0);
	rec->anti_replay_window = (rec->anti_replay_window & 0xE0001FFF) |
				  (((packed_record[5] >> 0) & 0xFFFF) << 13);
	rec->anti_replay_window = (rec->anti_replay_window & 0x1FFFFFFF) |
				  (((packed_record[6] >> 0) & 0x7) << 29);

	rec->receiving = (rec->receiving & 0xFFFFFFFE) |
			 (((packed_record[6] >> 3) & 0x1) << 0);

	rec->fresh = (rec->fresh & 0xFFFFFFFE) |
		     (((packed_record[6] >> 4) & 0x1) << 0);

	rec->an_rol = (rec->an_rol & 0xFFFFFFFE) |
		      (((packed_record[6] >> 5) & 0x1) << 0);

	rec->reserved = (rec->reserved & 0xFFFFFC00) |
			(((packed_record[6] >> 6) & 0x3FF) << 0);
	rec->reserved = (rec->reserved & 0xFE0003FF) |
			(((packed_record[7] >> 0) & 0x7FFF) << 10);

	rec->valid = (rec->valid & 0xFFFFFFFE) |
		     (((packed_record[7] >> 15) & 0x1) << 0);

	return 0;
}

int aq_mss_get_ingress_sc_record(struct atl_hw *hw,
				 struct aq_mss_ingress_sc_record *rec,
				 u16 table_index)
{
	memset(rec, 0, sizeof(*rec));

	return AQ_API_CALL_SAFE(get_ingress_sc_record, hw, rec, table_index);
}

static int set_ingress_sa_record(struct atl_hw *hw,
				 const struct aq_mss_ingress_sa_record *rec,
				 u16 table_index)
{
	u16 packed_record[8];

	if (table_index >= NUMROWS_INGRESSSARECORD)
		return -EINVAL;

	memset(packed_record, 0, sizeof(u16) * 8);

	packed_record[0] = (packed_record[0] & 0x0000) |
			   (((rec->stop_time >> 0) & 0xFFFF) << 0);
	packed_record[1] = (packed_record[1] & 0x0000) |
			   (((rec->stop_time >> 16) & 0xFFFF) << 0);

	packed_record[2] = (packed_record[2] & 0x0000) |
			   (((rec->start_time >> 0) & 0xFFFF) << 0);
	packed_record[3] = (packed_record[3] & 0x0000) |
			   (((rec->start_time >> 16) & 0xFFFF) << 0);

	packed_record[4] = (packed_record[4] & 0x0000) |
			   (((rec->next_pn >> 0) & 0xFFFF) << 0);
	packed_record[5] = (packed_record[5] & 0x0000) |
			   (((rec->next_pn >> 16) & 0xFFFF) << 0);

	packed_record[6] = (packed_record[6] & 0xFFFE) |
			   (((rec->sat_nextpn >> 0) & 0x1) << 0);

	packed_record[6] =
		(packed_record[6] & 0xFFFD) | (((rec->in_use >> 0) & 0x1) << 1);

	packed_record[6] =
		(packed_record[6] & 0xFFFB) | (((rec->fresh >> 0) & 0x1) << 2);

	packed_record[6] = (packed_record[6] & 0x0007) |
			   (((rec->reserved >> 0) & 0x1FFF) << 3);
	packed_record[7] = (packed_record[7] & 0x8000) |
			   (((rec->reserved >> 13) & 0x7FFF) << 0);

	packed_record[7] =
		(packed_record[7] & 0x7FFF) | (((rec->valid >> 0) & 0x1) << 15);

	return set_raw_ingress_record(hw, packed_record, 8, 3,
				      ROWOFFSET_INGRESSSARECORD + table_index);
}

int aq_mss_set_ingress_sa_record(struct atl_hw *hw,
				 const struct aq_mss_ingress_sa_record *rec,
				 u16 table_index)
{
	return AQ_API_CALL_SAFE(set_ingress_sa_record, hw, rec, table_index);
}

static int get_ingress_sa_record(struct atl_hw *hw,
				 struct aq_mss_ingress_sa_record *rec,
				 u16 table_index)
{
	u16 packed_record[8];
	int ret;

	if (table_index >= NUMROWS_INGRESSSARECORD)
		return -EINVAL;

	ret = get_raw_ingress_record(hw, packed_record, 8, 3,
				     ROWOFFSET_INGRESSSARECORD + table_index);
	if (unlikely(ret))
		return ret;

	rec->stop_time = (rec->stop_time & 0xFFFF0000) |
			 (((packed_record[0] >> 0) & 0xFFFF) << 0);
	rec->stop_time = (rec->stop_time & 0x0000FFFF) |
			 (((packed_record[1] >> 0) & 0xFFFF) << 16);

	rec->start_time = (rec->start_time & 0xFFFF0000) |
			  (((packed_record[2] >> 0) & 0xFFFF) << 0);
	rec->start_time = (rec->start_time & 0x0000FFFF) |
			  (((packed_record[3] >> 0) & 0xFFFF) << 16);

	rec->next_pn = (rec->next_pn & 0xFFFF0000) |
		       (((packed_record[4] >> 0) & 0xFFFF) << 0);
	rec->next_pn = (rec->next_pn & 0x0000FFFF) |
		       (((packed_record[5] >> 0) & 0xFFFF) << 16);

	rec->sat_nextpn = (rec->sat_nextpn & 0xFFFFFFFE) |
			  (((packed_record[6] >> 0) & 0x1) << 0);

	rec->in_use = (rec->in_use & 0xFFFFFFFE) |
		      (((packed_record[6] >> 1) & 0x1) << 0);

	rec->fresh = (rec->fresh & 0xFFFFFFFE) |
		     (((packed_record[6] >> 2) & 0x1) << 0);

	rec->reserved = (rec->reserved & 0xFFFFE000) |
			(((packed_record[6] >> 3) & 0x1FFF) << 0);
	rec->reserved = (rec->reserved & 0xF0001FFF) |
			(((packed_record[7] >> 0) & 0x7FFF) << 13);

	rec->valid = (rec->valid & 0xFFFFFFFE) |
		     (((packed_record[7] >> 15) & 0x1) << 0);

	return 0;
}

int aq_mss_get_ingress_sa_record(struct atl_hw *hw,
				 struct aq_mss_ingress_sa_record *rec,
				 u16 table_index)
{
	memset(rec, 0, sizeof(*rec));

	return AQ_API_CALL_SAFE(get_ingress_sa_record, hw, rec, table_index);
}

static int
set_ingress_sakey_record(struct atl_hw *hw,
			 const struct aq_mss_ingress_sakey_record *rec,
			 u16 table_index)
{
	u16 packed_record[18];

	if (table_index >= NUMROWS_INGRESSSAKEYRECORD)
		return -EINVAL;

	memset(packed_record, 0, sizeof(u16) * 18);

	packed_record[0] = (packed_record[0] & 0x0000) |
			   (((rec->key[0] >> 0) & 0xFFFF) << 0);
	packed_record[1] = (packed_record[1] & 0x0000) |
			   (((rec->key[0] >> 16) & 0xFFFF) << 0);

	packed_record[2] = (packed_record[2] & 0x0000) |
			   (((rec->key[1] >> 0) & 0xFFFF) << 0);
	packed_record[3] = (packed_record[3] & 0x0000) |
			   (((rec->key[1] >> 16) & 0xFFFF) << 0);

	packed_record[4] = (packed_record[4] & 0x0000) |
			   (((rec->key[2] >> 0) & 0xFFFF) << 0);
	packed_record[5] = (packed_record[5] & 0x0000) |
			   (((rec->key[2] >> 16) & 0xFFFF) << 0);

	packed_record[6] = (packed_record[6] & 0x0000) |
			   (((rec->key[3] >> 0) & 0xFFFF) << 0);
	packed_record[7] = (packed_record[7] & 0x0000) |
			   (((rec->key[3] >> 16) & 0xFFFF) << 0);

	packed_record[8] = (packed_record[8] & 0x0000) |
			   (((rec->key[4] >> 0) & 0xFFFF) << 0);
	packed_record[9] = (packed_record[9] & 0x0000) |
			   (((rec->key[4] >> 16) & 0xFFFF) << 0);

	packed_record[10] = (packed_record[10] & 0x0000) |
			    (((rec->key[5] >> 0) & 0xFFFF) << 0);
	packed_record[11] = (packed_record[11] & 0x0000) |
			    (((rec->key[5] >> 16) & 0xFFFF) << 0);

	packed_record[12] = (packed_record[12] & 0x0000) |
			    (((rec->key[6] >> 0) & 0xFFFF) << 0);
	packed_record[13] = (packed_record[13] & 0x0000) |
			    (((rec->key[6] >> 16) & 0xFFFF) << 0);

	packed_record[14] = (packed_record[14] & 0x0000) |
			    (((rec->key[7] >> 0) & 0xFFFF) << 0);
	packed_record[15] = (packed_record[15] & 0x0000) |
			    (((rec->key[7] >> 16) & 0xFFFF) << 0);

	packed_record[16] = (packed_record[16] & 0xFFFC) |
			    (((rec->key_len >> 0) & 0x3) << 0);

	return set_raw_ingress_record(hw, packed_record, 18, 2,
				      ROWOFFSET_INGRESSSAKEYRECORD +
					      table_index);
}

int aq_mss_set_ingress_sakey_record(
	struct atl_hw *hw, const struct aq_mss_ingress_sakey_record *rec,
	u16 table_index)
{
	return AQ_API_CALL_SAFE(set_ingress_sakey_record, hw, rec, table_index);
}

static int get_ingress_sakey_record(struct atl_hw *hw,
				    struct aq_mss_ingress_sakey_record *rec,
				    u16 table_index)
{
	u16 packed_record[18];
	int ret;

	if (table_index >= NUMROWS_INGRESSSAKEYRECORD)
		return -EINVAL;

	ret = get_raw_ingress_record(hw, packed_record, 18, 2,
				     ROWOFFSET_INGRESSSAKEYRECORD +
					     table_index);
	if (unlikely(ret))
		return ret;

	rec->key[0] = (rec->key[0] & 0xFFFF0000) |
		      (((packed_record[0] >> 0) & 0xFFFF) << 0);
	rec->key[0] = (rec->key[0] & 0x0000FFFF) |
		      (((packed_record[1] >> 0) & 0xFFFF) << 16);

	rec->key[1] = (rec->key[1] & 0xFFFF0000) |
		      (((packed_record[2] >> 0) & 0xFFFF) << 0);
	rec->key[1] = (rec->key[1] & 0x0000FFFF) |
		      (((packed_record[3] >> 0) & 0xFFFF) << 16);

	rec->key[2] = (rec->key[2] & 0xFFFF0000) |
		      (((packed_record[4] >> 0) & 0xFFFF) << 0);
	rec->key[2] = (rec->key[2] & 0x0000FFFF) |
		      (((packed_record[5] >> 0) & 0xFFFF) << 16);

	rec->key[3] = (rec->key[3] & 0xFFFF0000) |
		      (((packed_record[6] >> 0) & 0xFFFF) << 0);
	rec->key[3] = (rec->key[3] & 0x0000FFFF) |
		      (((packed_record[7] >> 0) & 0xFFFF) << 16);

	rec->key[4] = (rec->key[4] & 0xFFFF0000) |
		      (((packed_record[8] >> 0) & 0xFFFF) << 0);
	rec->key[4] = (rec->key[4] & 0x0000FFFF) |
		      (((packed_record[9] >> 0) & 0xFFFF) << 16);

	rec->key[5] = (rec->key[5] & 0xFFFF0000) |
		      (((packed_record[10] >> 0) & 0xFFFF) << 0);
	rec->key[5] = (rec->key[5] & 0x0000FFFF) |
		      (((packed_record[11] >> 0) & 0xFFFF) << 16);

	rec->key[6] = (rec->key[6] & 0xFFFF0000) |
		      (((packed_record[12] >> 0) & 0xFFFF) << 0);
	rec->key[6] = (rec->key[6] & 0x0000FFFF) |
		      (((packed_record[13] >> 0) & 0xFFFF) << 16);

	rec->key[7] = (rec->key[7] & 0xFFFF0000) |
		      (((packed_record[14] >> 0) & 0xFFFF) << 0);
	rec->key[7] = (rec->key[7] & 0x0000FFFF) |
		      (((packed_record[15] >> 0) & 0xFFFF) << 16);

	rec->key_len = (rec->key_len & 0xFFFFFFFC) |
		       (((packed_record[16] >> 0) & 0x3) << 0);

	return 0;
}

int aq_mss_get_ingress_sakey_record(struct atl_hw *hw,
				    struct aq_mss_ingress_sakey_record *rec,
				    u16 table_index)
{
	memset(rec, 0, sizeof(*rec));

	return AQ_API_CALL_SAFE(get_ingress_sakey_record, hw, rec, table_index);
}

static int
set_ingress_postclass_record(struct atl_hw *hw,
			     const struct aq_mss_ingress_postclass_record *rec,
			     u16 table_index)
{
	u16 packed_record[8];

	if (table_index >= NUMROWS_INGRESSPOSTCLASSRECORD)
		return -EINVAL;

	memset(packed_record, 0, sizeof(u16) * 8);

	packed_record[0] =
		(packed_record[0] & 0xFF00) | (((rec->byte0 >> 0) & 0xFF) << 0);

	packed_record[0] =
		(packed_record[0] & 0x00FF) | (((rec->byte1 >> 0) & 0xFF) << 8);

	packed_record[1] =
		(packed_record[1] & 0xFF00) | (((rec->byte2 >> 0) & 0xFF) << 0);

	packed_record[1] =
		(packed_record[1] & 0x00FF) | (((rec->byte3 >> 0) & 0xFF) << 8);

	packed_record[2] = (packed_record[2] & 0x0000) |
			   (((rec->eth_type >> 0) & 0xFFFF) << 0);

	packed_record[3] = (packed_record[3] & 0xFFFE) |
			   (((rec->eth_type_valid >> 0) & 0x1) << 0);

	packed_record[3] = (packed_record[3] & 0xE001) |
			   (((rec->vlan_id >> 0) & 0xFFF) << 1);

	packed_record[3] = (packed_record[3] & 0x1FFF) |
			   (((rec->vlan_up >> 0) & 0x7) << 13);

	packed_record[4] = (packed_record[4] & 0xFFFE) |
			   (((rec->vlan_valid >> 0) & 0x1) << 0);

	packed_record[4] =
		(packed_record[4] & 0xFFC1) | (((rec->sai >> 0) & 0x1F) << 1);

	packed_record[4] = (packed_record[4] & 0xFFBF) |
			   (((rec->sai_hit >> 0) & 0x1) << 6);

	packed_record[4] = (packed_record[4] & 0xF87F) |
			   (((rec->eth_type_mask >> 0) & 0xF) << 7);

	packed_record[4] = (packed_record[4] & 0x07FF) |
			   (((rec->byte3_location >> 0) & 0x1F) << 11);
	packed_record[5] = (packed_record[5] & 0xFFFE) |
			   (((rec->byte3_location >> 5) & 0x1) << 0);

	packed_record[5] = (packed_record[5] & 0xFFF9) |
			   (((rec->byte3_mask >> 0) & 0x3) << 1);

	packed_record[5] = (packed_record[5] & 0xFE07) |
			   (((rec->byte2_location >> 0) & 0x3F) << 3);

	packed_record[5] = (packed_record[5] & 0xF9FF) |
			   (((rec->byte2_mask >> 0) & 0x3) << 9);

	packed_record[5] = (packed_record[5] & 0x07FF) |
			   (((rec->byte1_location >> 0) & 0x1F) << 11);
	packed_record[6] = (packed_record[6] & 0xFFFE) |
			   (((rec->byte1_location >> 5) & 0x1) << 0);

	packed_record[6] = (packed_record[6] & 0xFFF9) |
			   (((rec->byte1_mask >> 0) & 0x3) << 1);

	packed_record[6] = (packed_record[6] & 0xFE07) |
			   (((rec->byte0_location >> 0) & 0x3F) << 3);

	packed_record[6] = (packed_record[6] & 0xF9FF) |
			   (((rec->byte0_mask >> 0) & 0x3) << 9);

	packed_record[6] = (packed_record[6] & 0xE7FF) |
			   (((rec->eth_type_valid_mask >> 0) & 0x3) << 11);

	packed_record[6] = (packed_record[6] & 0x1FFF) |
			   (((rec->vlan_id_mask >> 0) & 0x7) << 13);
	packed_record[7] = (packed_record[7] & 0xFFFE) |
			   (((rec->vlan_id_mask >> 3) & 0x1) << 0);

	packed_record[7] = (packed_record[7] & 0xFFF9) |
			   (((rec->vlan_up_mask >> 0) & 0x3) << 1);

	packed_record[7] = (packed_record[7] & 0xFFE7) |
			   (((rec->vlan_valid_mask >> 0) & 0x3) << 3);

	packed_record[7] = (packed_record[7] & 0xFF9F) |
			   (((rec->sai_mask >> 0) & 0x3) << 5);

	packed_record[7] = (packed_record[7] & 0xFE7F) |
			   (((rec->sai_hit_mask >> 0) & 0x3) << 7);

	packed_record[7] = (packed_record[7] & 0xFDFF) |
			   (((rec->firstlevel_actions >> 0) & 0x1) << 9);

	packed_record[7] = (packed_record[7] & 0xFBFF) |
			   (((rec->secondlevel_actions >> 0) & 0x1) << 10);

	packed_record[7] = (packed_record[7] & 0x87FF) |
			   (((rec->reserved >> 0) & 0xF) << 11);

	packed_record[7] =
		(packed_record[7] & 0x7FFF) | (((rec->valid >> 0) & 0x1) << 15);

	return set_raw_ingress_record(hw, packed_record, 8, 4,
				      ROWOFFSET_INGRESSPOSTCLASSRECORD +
					      table_index);
}

int aq_mss_set_ingress_postclass_record(
	struct atl_hw *hw, const struct aq_mss_ingress_postclass_record *rec,
	u16 table_index)
{
	return AQ_API_CALL_SAFE(set_ingress_postclass_record, hw, rec,
				table_index);
}

static int
get_ingress_postclass_record(struct atl_hw *hw,
			     struct aq_mss_ingress_postclass_record *rec,
			     u16 table_index)
{
	u16 packed_record[8];
	int ret;

	if (table_index >= NUMROWS_INGRESSPOSTCLASSRECORD)
		return -EINVAL;

	/* If the row that we want to read is odd, first read the previous even
	 * row, throw that value away, and finally read the desired row.
	 */
	if ((table_index % 2) > 0) {
		ret = get_raw_ingress_record(hw, packed_record, 8, 4,
					     ROWOFFSET_INGRESSPOSTCLASSRECORD +
						     table_index - 1);
		if (unlikely(ret))
			return ret;
	}

	ret = get_raw_ingress_record(hw, packed_record, 8, 4,
				     ROWOFFSET_INGRESSPOSTCLASSRECORD +
					     table_index);
	if (unlikely(ret))
		return ret;

	rec->byte0 = (rec->byte0 & 0xFFFFFF00) |
		     (((packed_record[0] >> 0) & 0xFF) << 0);

	rec->byte1 = (rec->byte1 & 0xFFFFFF00) |
		     (((packed_record[0] >> 8) & 0xFF) << 0);

	rec->byte2 = (rec->byte2 & 0xFFFFFF00) |
		     (((packed_record[1] >> 0) & 0xFF) << 0);

	rec->byte3 = (rec->byte3 & 0xFFFFFF00) |
		     (((packed_record[1] >> 8) & 0xFF) << 0);

	rec->eth_type = (rec->eth_type & 0xFFFF0000) |
			(((packed_record[2] >> 0) & 0xFFFF) << 0);

	rec->eth_type_valid = (rec->eth_type_valid & 0xFFFFFFFE) |
			      (((packed_record[3] >> 0) & 0x1) << 0);

	rec->vlan_id = (rec->vlan_id & 0xFFFFF000) |
		       (((packed_record[3] >> 1) & 0xFFF) << 0);

	rec->vlan_up = (rec->vlan_up & 0xFFFFFFF8) |
		       (((packed_record[3] >> 13) & 0x7) << 0);

	rec->vlan_valid = (rec->vlan_valid & 0xFFFFFFFE) |
			  (((packed_record[4] >> 0) & 0x1) << 0);

	rec->sai = (rec->sai & 0xFFFFFFE0) |
		   (((packed_record[4] >> 1) & 0x1F) << 0);

	rec->sai_hit = (rec->sai_hit & 0xFFFFFFFE) |
		       (((packed_record[4] >> 6) & 0x1) << 0);

	rec->eth_type_mask = (rec->eth_type_mask & 0xFFFFFFF0) |
			     (((packed_record[4] >> 7) & 0xF) << 0);

	rec->byte3_location = (rec->byte3_location & 0xFFFFFFE0) |
			      (((packed_record[4] >> 11) & 0x1F) << 0);
	rec->byte3_location = (rec->byte3_location & 0xFFFFFFDF) |
			      (((packed_record[5] >> 0) & 0x1) << 5);

	rec->byte3_mask = (rec->byte3_mask & 0xFFFFFFFC) |
			  (((packed_record[5] >> 1) & 0x3) << 0);

	rec->byte2_location = (rec->byte2_location & 0xFFFFFFC0) |
			      (((packed_record[5] >> 3) & 0x3F) << 0);

	rec->byte2_mask = (rec->byte2_mask & 0xFFFFFFFC) |
			  (((packed_record[5] >> 9) & 0x3) << 0);

	rec->byte1_location = (rec->byte1_location & 0xFFFFFFE0) |
			      (((packed_record[5] >> 11) & 0x1F) << 0);
	rec->byte1_location = (rec->byte1_location & 0xFFFFFFDF) |
			      (((packed_record[6] >> 0) & 0x1) << 5);

	rec->byte1_mask = (rec->byte1_mask & 0xFFFFFFFC) |
			  (((packed_record[6] >> 1) & 0x3) << 0);

	rec->byte0_location = (rec->byte0_location & 0xFFFFFFC0) |
			      (((packed_record[6] >> 3) & 0x3F) << 0);

	rec->byte0_mask = (rec->byte0_mask & 0xFFFFFFFC) |
			  (((packed_record[6] >> 9) & 0x3) << 0);

	rec->eth_type_valid_mask = (rec->eth_type_valid_mask & 0xFFFFFFFC) |
				   (((packed_record[6] >> 11) & 0x3) << 0);

	rec->vlan_id_mask = (rec->vlan_id_mask & 0xFFFFFFF8) |
			    (((packed_record[6] >> 13) & 0x7) << 0);
	rec->vlan_id_mask = (rec->vlan_id_mask & 0xFFFFFFF7) |
			    (((packed_record[7] >> 0) & 0x1) << 3);

	rec->vlan_up_mask = (rec->vlan_up_mask & 0xFFFFFFFC) |
			    (((packed_record[7] >> 1) & 0x3) << 0);

	rec->vlan_valid_mask = (rec->vlan_valid_mask & 0xFFFFFFFC) |
			       (((packed_record[7] >> 3) & 0x3) << 0);

	rec->sai_mask = (rec->sai_mask & 0xFFFFFFFC) |
			(((packed_record[7] >> 5) & 0x3) << 0);

	rec->sai_hit_mask = (rec->sai_hit_mask & 0xFFFFFFFC) |
			    (((packed_record[7] >> 7) & 0x3) << 0);

	rec->firstlevel_actions = (rec->firstlevel_actions & 0xFFFFFFFE) |
				  (((packed_record[7] >> 9) & 0x1) << 0);

	rec->secondlevel_actions = (rec->secondlevel_actions & 0xFFFFFFFE) |
				   (((packed_record[7] >> 10) & 0x1) << 0);

	rec->reserved = (rec->reserved & 0xFFFFFFF0) |
			(((packed_record[7] >> 11) & 0xF) << 0);

	rec->valid = (rec->valid & 0xFFFFFFFE) |
		     (((packed_record[7] >> 15) & 0x1) << 0);

	return 0;
}

int aq_mss_get_ingress_postclass_record(
	struct atl_hw *hw, struct aq_mss_ingress_postclass_record *rec,
	u16 table_index)
{
	memset(rec, 0, sizeof(*rec));

	return AQ_API_CALL_SAFE(get_ingress_postclass_record, hw, rec,
				table_index);
}

static int
set_ingress_postctlf_record(struct atl_hw *hw,
			    const struct aq_mss_ingress_postctlf_record *rec,
			    u16 table_index)
{
	u16 packed_record[6];

	if (table_index >= NUMROWS_INGRESSPOSTCTLFRECORD)
		return -EINVAL;

	memset(packed_record, 0, sizeof(u16) * 6);

	packed_record[0] = (packed_record[0] & 0x0000) |
			   (((rec->sa_da[0] >> 0) & 0xFFFF) << 0);
	packed_record[1] = (packed_record[1] & 0x0000) |
			   (((rec->sa_da[0] >> 16) & 0xFFFF) << 0);

	packed_record[2] = (packed_record[2] & 0x0000) |
			   (((rec->sa_da[1] >> 0) & 0xFFFF) << 0);

	packed_record[3] = (packed_record[3] & 0x0000) |
			   (((rec->eth_type >> 0) & 0xFFFF) << 0);

	packed_record[4] = (packed_record[4] & 0x0000) |
			   (((rec->match_mask >> 0) & 0xFFFF) << 0);

	packed_record[5] = (packed_record[5] & 0xFFF0) |
			   (((rec->match_type >> 0) & 0xF) << 0);

	packed_record[5] =
		(packed_record[5] & 0xFFEF) | (((rec->action >> 0) & 0x1) << 4);

	return set_raw_ingress_record(hw, packed_record, 6, 5,
				      ROWOFFSET_INGRESSPOSTCTLFRECORD +
					      table_index);
}

int aq_mss_set_ingress_postctlf_record(
	struct atl_hw *hw, const struct aq_mss_ingress_postctlf_record *rec,
	u16 table_index)
{
	return AQ_API_CALL_SAFE(set_ingress_postctlf_record, hw, rec,
				table_index);
}

static int
get_ingress_postctlf_record(struct atl_hw *hw,
			    struct aq_mss_ingress_postctlf_record *rec,
			    u16 table_index)
{
	u16 packed_record[6];
	int ret;

	if (table_index >= NUMROWS_INGRESSPOSTCTLFRECORD)
		return -EINVAL;

	/* If the row that we want to read is odd, first read the previous even
	 * row, throw that value away, and finally read the desired row.
	 */
	if ((table_index % 2) > 0) {
		ret = get_raw_ingress_record(hw, packed_record, 6, 5,
					     ROWOFFSET_INGRESSPOSTCTLFRECORD +
						     table_index - 1);
		if (unlikely(ret))
			return ret;
	}

	ret = get_raw_ingress_record(hw, packed_record, 6, 5,
				     ROWOFFSET_INGRESSPOSTCTLFRECORD +
					     table_index);
	if (unlikely(ret))
		return ret;

	rec->sa_da[0] = (rec->sa_da[0] & 0xFFFF0000) |
			(((packed_record[0] >> 0) & 0xFFFF) << 0);
	rec->sa_da[0] = (rec->sa_da[0] & 0x0000FFFF) |
			(((packed_record[1] >> 0) & 0xFFFF) << 16);

	rec->sa_da[1] = (rec->sa_da[1] & 0xFFFF0000) |
			(((packed_record[2] >> 0) & 0xFFFF) << 0);

	rec->eth_type = (rec->eth_type & 0xFFFF0000) |
			(((packed_record[3] >> 0) & 0xFFFF) << 0);

	rec->match_mask = (rec->match_mask & 0xFFFF0000) |
			  (((packed_record[4] >> 0) & 0xFFFF) << 0);

	rec->match_type = (rec->match_type & 0xFFFFFFF0) |
			  (((packed_record[5] >> 0) & 0xF) << 0);

	rec->action = (rec->action & 0xFFFFFFFE) |
		      (((packed_record[5] >> 4) & 0x1) << 0);

	return 0;
}

int aq_mss_get_ingress_postctlf_record(
	struct atl_hw *hw, struct aq_mss_ingress_postctlf_record *rec,
	u16 table_index)
{
	memset(rec, 0, sizeof(*rec));

	return AQ_API_CALL_SAFE(get_ingress_postctlf_record, hw, rec,
				table_index);
}

static int set_egress_ctlf_record(struct atl_hw *hw,
				  const struct aq_mss_egress_ctlf_record *rec,
				  u16 table_index)
{
	u16 packed_record[6];

	if (table_index >= NUMROWS_EGRESSCTLFRECORD)
		return -EINVAL;

	memset(packed_record, 0, sizeof(u16) * 6);

	packed_record[0] = (packed_record[0] & 0x0000) |
			   (((rec->sa_da[0] >> 0) & 0xFFFF) << 0);
	packed_record[1] = (packed_record[1] & 0x0000) |
			   (((rec->sa_da[0] >> 16) & 0xFFFF) << 0);

	packed_record[2] = (packed_record[2] & 0x0000) |
			   (((rec->sa_da[1] >> 0) & 0xFFFF) << 0);

	packed_record[3] = (packed_record[3] & 0x0000) |
			   (((rec->eth_type >> 0) & 0xFFFF) << 0);

	packed_record[4] = (packed_record[4] & 0x0000) |
			   (((rec->match_mask >> 0) & 0xFFFF) << 0);

	packed_record[5] = (packed_record[5] & 0xFFF0) |
			   (((rec->match_type >> 0) & 0xF) << 0);

	packed_record[5] =
		(packed_record[5] & 0xFFEF) | (((rec->action >> 0) & 0x1) << 4);

	return set_raw_egress_record(hw, packed_record, 6, 0,
				     ROWOFFSET_EGRESSCTLFRECORD + table_index);
}

int aq_mss_set_egress_ctlf_record(struct atl_hw *hw,
				  const struct aq_mss_egress_ctlf_record *rec,
				  u16 table_index)
{
	return AQ_API_CALL_SAFE(set_egress_ctlf_record, hw, rec, table_index);
}

static int get_egress_ctlf_record(struct atl_hw *hw,
				  struct aq_mss_egress_ctlf_record *rec,
				  u16 table_index)
{
	u16 packed_record[6];
	int ret;

	if (table_index >= NUMROWS_EGRESSCTLFRECORD)
		return -EINVAL;

	/* If the row that we want to read is odd, first read the previous even
	 * row, throw that value away, and finally read the desired row.
	 */
	if ((table_index % 2) > 0) {
		ret = get_raw_egress_record(hw, packed_record, 6, 0,
					    ROWOFFSET_EGRESSCTLFRECORD +
						    table_index - 1);
		if (unlikely(ret))
			return ret;
	}

	ret = get_raw_egress_record(hw, packed_record, 6, 0,
				    ROWOFFSET_EGRESSCTLFRECORD + table_index);
	if (unlikely(ret))
		return ret;

	rec->sa_da[0] = (rec->sa_da[0] & 0xFFFF0000) |
			(((packed_record[0] >> 0) & 0xFFFF) << 0);
	rec->sa_da[0] = (rec->sa_da[0] & 0x0000FFFF) |
			(((packed_record[1] >> 0) & 0xFFFF) << 16);

	rec->sa_da[1] = (rec->sa_da[1] & 0xFFFF0000) |
			(((packed_record[2] >> 0) & 0xFFFF) << 0);

	rec->eth_type = (rec->eth_type & 0xFFFF0000) |
			(((packed_record[3] >> 0) & 0xFFFF) << 0);

	rec->match_mask = (rec->match_mask & 0xFFFF0000) |
			  (((packed_record[4] >> 0) & 0xFFFF) << 0);

	rec->match_type = (rec->match_type & 0xFFFFFFF0) |
			  (((packed_record[5] >> 0) & 0xF) << 0);

	rec->action = (rec->action & 0xFFFFFFFE) |
		      (((packed_record[5] >> 4) & 0x1) << 0);

	return 0;
}

int aq_mss_get_egress_ctlf_record(struct atl_hw *hw,
				  struct aq_mss_egress_ctlf_record *rec,
				  u16 table_index)
{
	memset(rec, 0, sizeof(*rec));

	return AQ_API_CALL_SAFE(get_egress_ctlf_record, hw, rec, table_index);
}

static int set_egress_class_record(struct atl_hw *hw,
				   const struct aq_mss_egress_class_record *rec,
				   u16 table_index)
{
	u16 packed_record[28];

	if (table_index >= NUMROWS_EGRESSCLASSRECORD)
		return -EINVAL;

	memset(packed_record, 0, sizeof(u16) * 28);

	packed_record[0] = (packed_record[0] & 0xF000) |
			   (((rec->vlan_id >> 0) & 0xFFF) << 0);

	packed_record[0] = (packed_record[0] & 0x8FFF) |
			   (((rec->vlan_up >> 0) & 0x7) << 12);

	packed_record[0] = (packed_record[0] & 0x7FFF) |
			   (((rec->vlan_valid >> 0) & 0x1) << 15);

	packed_record[1] =
		(packed_record[1] & 0xFF00) | (((rec->byte3 >> 0) & 0xFF) << 0);

	packed_record[1] =
		(packed_record[1] & 0x00FF) | (((rec->byte2 >> 0) & 0xFF) << 8);

	packed_record[2] =
		(packed_record[2] & 0xFF00) | (((rec->byte1 >> 0) & 0xFF) << 0);

	packed_record[2] =
		(packed_record[2] & 0x00FF) | (((rec->byte0 >> 0) & 0xFF) << 8);

	packed_record[3] =
		(packed_record[3] & 0xFF00) | (((rec->tci >> 0) & 0xFF) << 0);

	packed_record[3] = (packed_record[3] & 0x00FF) |
			   (((rec->sci[0] >> 0) & 0xFF) << 8);
	packed_record[4] = (packed_record[4] & 0x0000) |
			   (((rec->sci[0] >> 8) & 0xFFFF) << 0);
	packed_record[5] = (packed_record[5] & 0xFF00) |
			   (((rec->sci[0] >> 24) & 0xFF) << 0);

	packed_record[5] = (packed_record[5] & 0x00FF) |
			   (((rec->sci[1] >> 0) & 0xFF) << 8);
	packed_record[6] = (packed_record[6] & 0x0000) |
			   (((rec->sci[1] >> 8) & 0xFFFF) << 0);
	packed_record[7] = (packed_record[7] & 0xFF00) |
			   (((rec->sci[1] >> 24) & 0xFF) << 0);

	packed_record[7] = (packed_record[7] & 0x00FF) |
			   (((rec->eth_type >> 0) & 0xFF) << 8);
	packed_record[8] = (packed_record[8] & 0xFF00) |
			   (((rec->eth_type >> 8) & 0xFF) << 0);

	packed_record[8] = (packed_record[8] & 0x00FF) |
			   (((rec->snap[0] >> 0) & 0xFF) << 8);
	packed_record[9] = (packed_record[9] & 0x0000) |
			   (((rec->snap[0] >> 8) & 0xFFFF) << 0);
	packed_record[10] = (packed_record[10] & 0xFF00) |
			    (((rec->snap[0] >> 24) & 0xFF) << 0);

	packed_record[10] = (packed_record[10] & 0x00FF) |
			    (((rec->snap[1] >> 0) & 0xFF) << 8);

	packed_record[11] = (packed_record[11] & 0x0000) |
			    (((rec->llc >> 0) & 0xFFFF) << 0);
	packed_record[12] =
		(packed_record[12] & 0xFF00) | (((rec->llc >> 16) & 0xFF) << 0);

	packed_record[12] = (packed_record[12] & 0x00FF) |
			    (((rec->mac_sa[0] >> 0) & 0xFF) << 8);
	packed_record[13] = (packed_record[13] & 0x0000) |
			    (((rec->mac_sa[0] >> 8) & 0xFFFF) << 0);
	packed_record[14] = (packed_record[14] & 0xFF00) |
			    (((rec->mac_sa[0] >> 24) & 0xFF) << 0);

	packed_record[14] = (packed_record[14] & 0x00FF) |
			    (((rec->mac_sa[1] >> 0) & 0xFF) << 8);
	packed_record[15] = (packed_record[15] & 0xFF00) |
			    (((rec->mac_sa[1] >> 8) & 0xFF) << 0);

	packed_record[15] = (packed_record[15] & 0x00FF) |
			    (((rec->mac_da[0] >> 0) & 0xFF) << 8);
	packed_record[16] = (packed_record[16] & 0x0000) |
			    (((rec->mac_da[0] >> 8) & 0xFFFF) << 0);
	packed_record[17] = (packed_record[17] & 0xFF00) |
			    (((rec->mac_da[0] >> 24) & 0xFF) << 0);

	packed_record[17] = (packed_record[17] & 0x00FF) |
			    (((rec->mac_da[1] >> 0) & 0xFF) << 8);
	packed_record[18] = (packed_record[18] & 0xFF00) |
			    (((rec->mac_da[1] >> 8) & 0xFF) << 0);

	packed_record[18] =
		(packed_record[18] & 0x00FF) | (((rec->pn >> 0) & 0xFF) << 8);
	packed_record[19] =
		(packed_record[19] & 0x0000) | (((rec->pn >> 8) & 0xFFFF) << 0);
	packed_record[20] =
		(packed_record[20] & 0xFF00) | (((rec->pn >> 24) & 0xFF) << 0);

	packed_record[20] = (packed_record[20] & 0xC0FF) |
			    (((rec->byte3_location >> 0) & 0x3F) << 8);

	packed_record[20] = (packed_record[20] & 0xBFFF) |
			    (((rec->byte3_mask >> 0) & 0x1) << 14);

	packed_record[20] = (packed_record[20] & 0x7FFF) |
			    (((rec->byte2_location >> 0) & 0x1) << 15);
	packed_record[21] = (packed_record[21] & 0xFFE0) |
			    (((rec->byte2_location >> 1) & 0x1F) << 0);

	packed_record[21] = (packed_record[21] & 0xFFDF) |
			    (((rec->byte2_mask >> 0) & 0x1) << 5);

	packed_record[21] = (packed_record[21] & 0xF03F) |
			    (((rec->byte1_location >> 0) & 0x3F) << 6);

	packed_record[21] = (packed_record[21] & 0xEFFF) |
			    (((rec->byte1_mask >> 0) & 0x1) << 12);

	packed_record[21] = (packed_record[21] & 0x1FFF) |
			    (((rec->byte0_location >> 0) & 0x7) << 13);
	packed_record[22] = (packed_record[22] & 0xFFF8) |
			    (((rec->byte0_location >> 3) & 0x7) << 0);

	packed_record[22] = (packed_record[22] & 0xFFF7) |
			    (((rec->byte0_mask >> 0) & 0x1) << 3);

	packed_record[22] = (packed_record[22] & 0xFFCF) |
			    (((rec->vlan_id_mask >> 0) & 0x3) << 4);

	packed_record[22] = (packed_record[22] & 0xFFBF) |
			    (((rec->vlan_up_mask >> 0) & 0x1) << 6);

	packed_record[22] = (packed_record[22] & 0xFF7F) |
			    (((rec->vlan_valid_mask >> 0) & 0x1) << 7);

	packed_record[22] = (packed_record[22] & 0x00FF) |
			    (((rec->tci_mask >> 0) & 0xFF) << 8);

	packed_record[23] = (packed_record[23] & 0xFF00) |
			    (((rec->sci_mask >> 0) & 0xFF) << 0);

	packed_record[23] = (packed_record[23] & 0xFCFF) |
			    (((rec->eth_type_mask >> 0) & 0x3) << 8);

	packed_record[23] = (packed_record[23] & 0x83FF) |
			    (((rec->snap_mask >> 0) & 0x1F) << 10);

	packed_record[23] = (packed_record[23] & 0x7FFF) |
			    (((rec->llc_mask >> 0) & 0x1) << 15);
	packed_record[24] = (packed_record[24] & 0xFFFC) |
			    (((rec->llc_mask >> 1) & 0x3) << 0);

	packed_record[24] = (packed_record[24] & 0xFF03) |
			    (((rec->sa_mask >> 0) & 0x3F) << 2);

	packed_record[24] = (packed_record[24] & 0xC0FF) |
			    (((rec->da_mask >> 0) & 0x3F) << 8);

	packed_record[24] = (packed_record[24] & 0x3FFF) |
			    (((rec->pn_mask >> 0) & 0x3) << 14);
	packed_record[25] = (packed_record[25] & 0xFFFC) |
			    (((rec->pn_mask >> 2) & 0x3) << 0);

	packed_record[25] = (packed_record[25] & 0xFFFB) |
			    (((rec->eight02dot2 >> 0) & 0x1) << 2);

	packed_record[25] = (packed_record[25] & 0xFFF7) |
			    (((rec->tci_sc >> 0) & 0x1) << 3);

	packed_record[25] = (packed_record[25] & 0xFFEF) |
			    (((rec->tci_87543 >> 0) & 0x1) << 4);

	packed_record[25] = (packed_record[25] & 0xFFDF) |
			    (((rec->exp_sectag_en >> 0) & 0x1) << 5);

	packed_record[25] = (packed_record[25] & 0xF83F) |
			    (((rec->sc_idx >> 0) & 0x1F) << 6);

	packed_record[25] = (packed_record[25] & 0xE7FF) |
			    (((rec->sc_sa >> 0) & 0x3) << 11);

	packed_record[25] = (packed_record[25] & 0xDFFF) |
			    (((rec->debug >> 0) & 0x1) << 13);

	packed_record[25] = (packed_record[25] & 0x3FFF) |
			    (((rec->action >> 0) & 0x3) << 14);

	packed_record[26] =
		(packed_record[26] & 0xFFF7) | (((rec->valid >> 0) & 0x1) << 3);

	return set_raw_egress_record(hw, packed_record, 28, 1,
				     ROWOFFSET_EGRESSCLASSRECORD + table_index);
}

int aq_mss_set_egress_class_record(struct atl_hw *hw,
				   const struct aq_mss_egress_class_record *rec,
				   u16 table_index)
{
	return AQ_API_CALL_SAFE(set_egress_class_record, hw, rec, table_index);
}

static int get_egress_class_record(struct atl_hw *hw,
				   struct aq_mss_egress_class_record *rec,
				   u16 table_index)
{
	u16 packed_record[28];
	int ret;

	if (table_index >= NUMROWS_EGRESSCLASSRECORD)
		return -EINVAL;

	/* If the row that we want to read is odd, first read the previous even
	 * row, throw that value away, and finally read the desired row.
	 */
	if ((table_index % 2) > 0) {
		ret = get_raw_egress_record(hw, packed_record, 28, 1,
					    ROWOFFSET_EGRESSCLASSRECORD +
						    table_index - 1);
		if (unlikely(ret))
			return ret;
	}

	ret = get_raw_egress_record(hw, packed_record, 28, 1,
				    ROWOFFSET_EGRESSCLASSRECORD + table_index);
	if (unlikely(ret))
		return ret;

	rec->vlan_id = (rec->vlan_id & 0xFFFFF000) |
		       (((packed_record[0] >> 0) & 0xFFF) << 0);

	rec->vlan_up = (rec->vlan_up & 0xFFFFFFF8) |
		       (((packed_record[0] >> 12) & 0x7) << 0);

	rec->vlan_valid = (rec->vlan_valid & 0xFFFFFFFE) |
			  (((packed_record[0] >> 15) & 0x1) << 0);

	rec->byte3 = (rec->byte3 & 0xFFFFFF00) |
		     (((packed_record[1] >> 0) & 0xFF) << 0);

	rec->byte2 = (rec->byte2 & 0xFFFFFF00) |
		     (((packed_record[1] >> 8) & 0xFF) << 0);

	rec->byte1 = (rec->byte1 & 0xFFFFFF00) |
		     (((packed_record[2] >> 0) & 0xFF) << 0);

	rec->byte0 = (rec->byte0 & 0xFFFFFF00) |
		     (((packed_record[2] >> 8) & 0xFF) << 0);

	rec->tci = (rec->tci & 0xFFFFFF00) |
		   (((packed_record[3] >> 0) & 0xFF) << 0);

	rec->sci[0] = (rec->sci[0] & 0xFFFFFF00) |
		      (((packed_record[3] >> 8) & 0xFF) << 0);
	rec->sci[0] = (rec->sci[0] & 0xFF0000FF) |
		      (((packed_record[4] >> 0) & 0xFFFF) << 8);
	rec->sci[0] = (rec->sci[0] & 0x00FFFFFF) |
		      (((packed_record[5] >> 0) & 0xFF) << 24);

	rec->sci[1] = (rec->sci[1] & 0xFFFFFF00) |
		      (((packed_record[5] >> 8) & 0xFF) << 0);
	rec->sci[1] = (rec->sci[1] & 0xFF0000FF) |
		      (((packed_record[6] >> 0) & 0xFFFF) << 8);
	rec->sci[1] = (rec->sci[1] & 0x00FFFFFF) |
		      (((packed_record[7] >> 0) & 0xFF) << 24);

	rec->eth_type = (rec->eth_type & 0xFFFFFF00) |
			(((packed_record[7] >> 8) & 0xFF) << 0);
	rec->eth_type = (rec->eth_type & 0xFFFF00FF) |
			(((packed_record[8] >> 0) & 0xFF) << 8);

	rec->snap[0] = (rec->snap[0] & 0xFFFFFF00) |
		       (((packed_record[8] >> 8) & 0xFF) << 0);
	rec->snap[0] = (rec->snap[0] & 0xFF0000FF) |
		       (((packed_record[9] >> 0) & 0xFFFF) << 8);
	rec->snap[0] = (rec->snap[0] & 0x00FFFFFF) |
		       (((packed_record[10] >> 0) & 0xFF) << 24);

	rec->snap[1] = (rec->snap[1] & 0xFFFFFF00) |
		       (((packed_record[10] >> 8) & 0xFF) << 0);

	rec->llc = (rec->llc & 0xFFFF0000) |
		   (((packed_record[11] >> 0) & 0xFFFF) << 0);
	rec->llc = (rec->llc & 0xFF00FFFF) |
		   (((packed_record[12] >> 0) & 0xFF) << 16);

	rec->mac_sa[0] = (rec->mac_sa[0] & 0xFFFFFF00) |
			 (((packed_record[12] >> 8) & 0xFF) << 0);
	rec->mac_sa[0] = (rec->mac_sa[0] & 0xFF0000FF) |
			 (((packed_record[13] >> 0) & 0xFFFF) << 8);
	rec->mac_sa[0] = (rec->mac_sa[0] & 0x00FFFFFF) |
			 (((packed_record[14] >> 0) & 0xFF) << 24);

	rec->mac_sa[1] = (rec->mac_sa[1] & 0xFFFFFF00) |
			 (((packed_record[14] >> 8) & 0xFF) << 0);
	rec->mac_sa[1] = (rec->mac_sa[1] & 0xFFFF00FF) |
			 (((packed_record[15] >> 0) & 0xFF) << 8);

	rec->mac_da[0] = (rec->mac_da[0] & 0xFFFFFF00) |
			 (((packed_record[15] >> 8) & 0xFF) << 0);
	rec->mac_da[0] = (rec->mac_da[0] & 0xFF0000FF) |
			 (((packed_record[16] >> 0) & 0xFFFF) << 8);
	rec->mac_da[0] = (rec->mac_da[0] & 0x00FFFFFF) |
			 (((packed_record[17] >> 0) & 0xFF) << 24);

	rec->mac_da[1] = (rec->mac_da[1] & 0xFFFFFF00) |
			 (((packed_record[17] >> 8) & 0xFF) << 0);
	rec->mac_da[1] = (rec->mac_da[1] & 0xFFFF00FF) |
			 (((packed_record[18] >> 0) & 0xFF) << 8);

	rec->pn = (rec->pn & 0xFFFFFF00) |
		  (((packed_record[18] >> 8) & 0xFF) << 0);
	rec->pn = (rec->pn & 0xFF0000FF) |
		  (((packed_record[19] >> 0) & 0xFFFF) << 8);
	rec->pn = (rec->pn & 0x00FFFFFF) |
		  (((packed_record[20] >> 0) & 0xFF) << 24);

	rec->byte3_location = (rec->byte3_location & 0xFFFFFFC0) |
			      (((packed_record[20] >> 8) & 0x3F) << 0);

	rec->byte3_mask = (rec->byte3_mask & 0xFFFFFFFE) |
			  (((packed_record[20] >> 14) & 0x1) << 0);

	rec->byte2_location = (rec->byte2_location & 0xFFFFFFFE) |
			      (((packed_record[20] >> 15) & 0x1) << 0);
	rec->byte2_location = (rec->byte2_location & 0xFFFFFFC1) |
			      (((packed_record[21] >> 0) & 0x1F) << 1);

	rec->byte2_mask = (rec->byte2_mask & 0xFFFFFFFE) |
			  (((packed_record[21] >> 5) & 0x1) << 0);

	rec->byte1_location = (rec->byte1_location & 0xFFFFFFC0) |
			      (((packed_record[21] >> 6) & 0x3F) << 0);

	rec->byte1_mask = (rec->byte1_mask & 0xFFFFFFFE) |
			  (((packed_record[21] >> 12) & 0x1) << 0);

	rec->byte0_location = (rec->byte0_location & 0xFFFFFFF8) |
			      (((packed_record[21] >> 13) & 0x7) << 0);
	rec->byte0_location = (rec->byte0_location & 0xFFFFFFC7) |
			      (((packed_record[22] >> 0) & 0x7) << 3);

	rec->byte0_mask = (rec->byte0_mask & 0xFFFFFFFE) |
			  (((packed_record[22] >> 3) & 0x1) << 0);

	rec->vlan_id_mask = (rec->vlan_id_mask & 0xFFFFFFFC) |
			    (((packed_record[22] >> 4) & 0x3) << 0);

	rec->vlan_up_mask = (rec->vlan_up_mask & 0xFFFFFFFE) |
			    (((packed_record[22] >> 6) & 0x1) << 0);

	rec->vlan_valid_mask = (rec->vlan_valid_mask & 0xFFFFFFFE) |
			       (((packed_record[22] >> 7) & 0x1) << 0);

	rec->tci_mask = (rec->tci_mask & 0xFFFFFF00) |
			(((packed_record[22] >> 8) & 0xFF) << 0);

	rec->sci_mask = (rec->sci_mask & 0xFFFFFF00) |
			(((packed_record[23] >> 0) & 0xFF) << 0);

	rec->eth_type_mask = (rec->eth_type_mask & 0xFFFFFFFC) |
			     (((packed_record[23] >> 8) & 0x3) << 0);

	rec->snap_mask = (rec->snap_mask & 0xFFFFFFE0) |
			 (((packed_record[23] >> 10) & 0x1F) << 0);

	rec->llc_mask = (rec->llc_mask & 0xFFFFFFFE) |
			(((packed_record[23] >> 15) & 0x1) << 0);
	rec->llc_mask = (rec->llc_mask & 0xFFFFFFF9) |
			(((packed_record[24] >> 0) & 0x3) << 1);

	rec->sa_mask = (rec->sa_mask & 0xFFFFFFC0) |
		       (((packed_record[24] >> 2) & 0x3F) << 0);

	rec->da_mask = (rec->da_mask & 0xFFFFFFC0) |
		       (((packed_record[24] >> 8) & 0x3F) << 0);

	rec->pn_mask = (rec->pn_mask & 0xFFFFFFFC) |
		       (((packed_record[24] >> 14) & 0x3) << 0);
	rec->pn_mask = (rec->pn_mask & 0xFFFFFFF3) |
		       (((packed_record[25] >> 0) & 0x3) << 2);

	rec->eight02dot2 = (rec->eight02dot2 & 0xFFFFFFFE) |
			   (((packed_record[25] >> 2) & 0x1) << 0);

	rec->tci_sc = (rec->tci_sc & 0xFFFFFFFE) |
		      (((packed_record[25] >> 3) & 0x1) << 0);

	rec->tci_87543 = (rec->tci_87543 & 0xFFFFFFFE) |
			 (((packed_record[25] >> 4) & 0x1) << 0);

	rec->exp_sectag_en = (rec->exp_sectag_en & 0xFFFFFFFE) |
			     (((packed_record[25] >> 5) & 0x1) << 0);

	rec->sc_idx = (rec->sc_idx & 0xFFFFFFE0) |
		      (((packed_record[25] >> 6) & 0x1F) << 0);

	rec->sc_sa = (rec->sc_sa & 0xFFFFFFFC) |
		     (((packed_record[25] >> 11) & 0x3) << 0);

	rec->debug = (rec->debug & 0xFFFFFFFE) |
		     (((packed_record[25] >> 13) & 0x1) << 0);

	rec->action = (rec->action & 0xFFFFFFFC) |
		      (((packed_record[25] >> 14) & 0x3) << 0);

	rec->valid = (rec->valid & 0xFFFFFFFE) |
		     (((packed_record[26] >> 3) & 0x1) << 0);

	return 0;
}

int aq_mss_get_egress_class_record(struct atl_hw *hw,
				   struct aq_mss_egress_class_record *rec,
				   u16 table_index)
{
	memset(rec, 0, sizeof(*rec));

	return AQ_API_CALL_SAFE(get_egress_class_record, hw, rec, table_index);
}

static int set_egress_sc_record(struct atl_hw *hw,
				const struct aq_mss_egress_sc_record *rec,
				u16 table_index)
{
	u16 packed_record[8];

	if (table_index >= NUMROWS_EGRESSSCRECORD)
		return -EINVAL;

	memset(packed_record, 0, sizeof(u16) * 8);

	packed_record[0] = (packed_record[0] & 0x0000) |
			   (((rec->start_time >> 0) & 0xFFFF) << 0);
	packed_record[1] = (packed_record[1] & 0x0000) |
			   (((rec->start_time >> 16) & 0xFFFF) << 0);

	packed_record[2] = (packed_record[2] & 0x0000) |
			   (((rec->stop_time >> 0) & 0xFFFF) << 0);
	packed_record[3] = (packed_record[3] & 0x0000) |
			   (((rec->stop_time >> 16) & 0xFFFF) << 0);

	packed_record[4] = (packed_record[4] & 0xFFFC) |
			   (((rec->curr_an >> 0) & 0x3) << 0);

	packed_record[4] = (packed_record[4] & 0xFFFB) |
			   (((rec->an_roll >> 0) & 0x1) << 2);

	packed_record[4] =
		(packed_record[4] & 0xFE07) | (((rec->tci >> 0) & 0x3F) << 3);

	packed_record[4] = (packed_record[4] & 0x01FF) |
			   (((rec->enc_off >> 0) & 0x7F) << 9);
	packed_record[5] = (packed_record[5] & 0xFFFE) |
			   (((rec->enc_off >> 7) & 0x1) << 0);

	packed_record[5] = (packed_record[5] & 0xFFFD) |
			   (((rec->protect >> 0) & 0x1) << 1);

	packed_record[5] =
		(packed_record[5] & 0xFFFB) | (((rec->recv >> 0) & 0x1) << 2);

	packed_record[5] =
		(packed_record[5] & 0xFFF7) | (((rec->fresh >> 0) & 0x1) << 3);

	packed_record[5] = (packed_record[5] & 0xFFCF) |
			   (((rec->sak_len >> 0) & 0x3) << 4);

	packed_record[7] =
		(packed_record[7] & 0x7FFF) | (((rec->valid >> 0) & 0x1) << 15);

	return set_raw_egress_record(hw, packed_record, 8, 2,
				     ROWOFFSET_EGRESSSCRECORD + table_index);
}

int aq_mss_set_egress_sc_record(struct atl_hw *hw,
				const struct aq_mss_egress_sc_record *rec,
				u16 table_index)
{
	return AQ_API_CALL_SAFE(set_egress_sc_record, hw, rec, table_index);
}

static int get_egress_sc_record(struct atl_hw *hw,
				struct aq_mss_egress_sc_record *rec,
				u16 table_index)
{
	u16 packed_record[8];
	int ret;

	if (table_index >= NUMROWS_EGRESSSCRECORD)
		return -EINVAL;

	ret = get_raw_egress_record(hw, packed_record, 8, 2,
				    ROWOFFSET_EGRESSSCRECORD + table_index);
	if (unlikely(ret))
		return ret;

	rec->start_time = (rec->start_time & 0xFFFF0000) |
			  (((packed_record[0] >> 0) & 0xFFFF) << 0);
	rec->start_time = (rec->start_time & 0x0000FFFF) |
			  (((packed_record[1] >> 0) & 0xFFFF) << 16);

	rec->stop_time = (rec->stop_time & 0xFFFF0000) |
			 (((packed_record[2] >> 0) & 0xFFFF) << 0);
	rec->stop_time = (rec->stop_time & 0x0000FFFF) |
			 (((packed_record[3] >> 0) & 0xFFFF) << 16);

	rec->curr_an = (rec->curr_an & 0xFFFFFFFC) |
		       (((packed_record[4] >> 0) & 0x3) << 0);

	rec->an_roll = (rec->an_roll & 0xFFFFFFFE) |
		       (((packed_record[4] >> 2) & 0x1) << 0);

	rec->tci = (rec->tci & 0xFFFFFFC0) |
		   (((packed_record[4] >> 3) & 0x3F) << 0);

	rec->enc_off = (rec->enc_off & 0xFFFFFF80) |
		       (((packed_record[4] >> 9) & 0x7F) << 0);
	rec->enc_off = (rec->enc_off & 0xFFFFFF7F) |
		       (((packed_record[5] >> 0) & 0x1) << 7);

	rec->protect = (rec->protect & 0xFFFFFFFE) |
		       (((packed_record[5] >> 1) & 0x1) << 0);

	rec->recv = (rec->recv & 0xFFFFFFFE) |
		    (((packed_record[5] >> 2) & 0x1) << 0);

	rec->fresh = (rec->fresh & 0xFFFFFFFE) |
		     (((packed_record[5] >> 3) & 0x1) << 0);

	rec->sak_len = (rec->sak_len & 0xFFFFFFFC) |
		       (((packed_record[5] >> 4) & 0x3) << 0);

	rec->valid = (rec->valid & 0xFFFFFFFE) |
		     (((packed_record[7] >> 15) & 0x1) << 0);

	return 0;
}

int aq_mss_get_egress_sc_record(struct atl_hw *hw,
				struct aq_mss_egress_sc_record *rec,
				u16 table_index)
{
	memset(rec, 0, sizeof(*rec));

	return AQ_API_CALL_SAFE(get_egress_sc_record, hw, rec, table_index);
}

static int set_egress_sa_record(struct atl_hw *hw,
				const struct aq_mss_egress_sa_record *rec,
				u16 table_index)
{
	u16 packed_record[8];

	if (table_index >= NUMROWS_EGRESSSARECORD)
		return -EINVAL;

	memset(packed_record, 0, sizeof(u16) * 8);

	packed_record[0] = (packed_record[0] & 0x0000) |
			   (((rec->start_time >> 0) & 0xFFFF) << 0);
	packed_record[1] = (packed_record[1] & 0x0000) |
			   (((rec->start_time >> 16) & 0xFFFF) << 0);

	packed_record[2] = (packed_record[2] & 0x0000) |
			   (((rec->stop_time >> 0) & 0xFFFF) << 0);
	packed_record[3] = (packed_record[3] & 0x0000) |
			   (((rec->stop_time >> 16) & 0xFFFF) << 0);

	packed_record[4] = (packed_record[4] & 0x0000) |
			   (((rec->next_pn >> 0) & 0xFFFF) << 0);
	packed_record[5] = (packed_record[5] & 0x0000) |
			   (((rec->next_pn >> 16) & 0xFFFF) << 0);

	packed_record[6] =
		(packed_record[6] & 0xFFFE) | (((rec->sat_pn >> 0) & 0x1) << 0);

	packed_record[6] =
		(packed_record[6] & 0xFFFD) | (((rec->fresh >> 0) & 0x1) << 1);

	packed_record[7] =
		(packed_record[7] & 0x7FFF) | (((rec->valid >> 0) & 0x1) << 15);

	return set_raw_egress_record(hw, packed_record, 8, 2,
				     ROWOFFSET_EGRESSSARECORD + table_index);
}

int aq_mss_set_egress_sa_record(struct atl_hw *hw,
				const struct aq_mss_egress_sa_record *rec,
				u16 table_index)
{
	return AQ_API_CALL_SAFE(set_egress_sa_record, hw, rec, table_index);
}

static int get_egress_sa_record(struct atl_hw *hw,
				struct aq_mss_egress_sa_record *rec,
				u16 table_index)
{
	u16 packed_record[8];
	int ret;

	if (table_index >= NUMROWS_EGRESSSARECORD)
		return -EINVAL;

	ret = get_raw_egress_record(hw, packed_record, 8, 2,
				    ROWOFFSET_EGRESSSARECORD + table_index);
	if (unlikely(ret))
		return ret;

	rec->start_time = (rec->start_time & 0xFFFF0000) |
			  (((packed_record[0] >> 0) & 0xFFFF) << 0);
	rec->start_time = (rec->start_time & 0x0000FFFF) |
			  (((packed_record[1] >> 0) & 0xFFFF) << 16);

	rec->stop_time = (rec->stop_time & 0xFFFF0000) |
			 (((packed_record[2] >> 0) & 0xFFFF) << 0);
	rec->stop_time = (rec->stop_time & 0x0000FFFF) |
			 (((packed_record[3] >> 0) & 0xFFFF) << 16);

	rec->next_pn = (rec->next_pn & 0xFFFF0000) |
		       (((packed_record[4] >> 0) & 0xFFFF) << 0);
	rec->next_pn = (rec->next_pn & 0x0000FFFF) |
		       (((packed_record[5] >> 0) & 0xFFFF) << 16);

	rec->sat_pn = (rec->sat_pn & 0xFFFFFFFE) |
		      (((packed_record[6] >> 0) & 0x1) << 0);

	rec->fresh = (rec->fresh & 0xFFFFFFFE) |
		     (((packed_record[6] >> 1) & 0x1) << 0);

	rec->valid = (rec->valid & 0xFFFFFFFE) |
		     (((packed_record[7] >> 15) & 0x1) << 0);

	return 0;
}

int aq_mss_get_egress_sa_record(struct atl_hw *hw,
				struct aq_mss_egress_sa_record *rec,
				u16 table_index)
{
	memset(rec, 0, sizeof(*rec));

	return AQ_API_CALL_SAFE(get_egress_sa_record, hw, rec, table_index);
}

static int set_egress_sakey_record(struct atl_hw *hw,
				   const struct aq_mss_egress_sakey_record *rec,
				   u16 table_index)
{
	u16 packed_record[16];
	int ret;

	if (table_index >= NUMROWS_EGRESSSAKEYRECORD)
		return -EINVAL;

	memset(packed_record, 0, sizeof(u16) * 16);

	packed_record[0] = (packed_record[0] & 0x0000) |
			   (((rec->key[0] >> 0) & 0xFFFF) << 0);
	packed_record[1] = (packed_record[1] & 0x0000) |
			   (((rec->key[0] >> 16) & 0xFFFF) << 0);

	packed_record[2] = (packed_record[2] & 0x0000) |
			   (((rec->key[1] >> 0) & 0xFFFF) << 0);
	packed_record[3] = (packed_record[3] & 0x0000) |
			   (((rec->key[1] >> 16) & 0xFFFF) << 0);

	packed_record[4] = (packed_record[4] & 0x0000) |
			   (((rec->key[2] >> 0) & 0xFFFF) << 0);
	packed_record[5] = (packed_record[5] & 0x0000) |
			   (((rec->key[2] >> 16) & 0xFFFF) << 0);

	packed_record[6] = (packed_record[6] & 0x0000) |
			   (((rec->key[3] >> 0) & 0xFFFF) << 0);
	packed_record[7] = (packed_record[7] & 0x0000) |
			   (((rec->key[3] >> 16) & 0xFFFF) << 0);

	packed_record[8] = (packed_record[8] & 0x0000) |
			   (((rec->key[4] >> 0) & 0xFFFF) << 0);
	packed_record[9] = (packed_record[9] & 0x0000) |
			   (((rec->key[4] >> 16) & 0xFFFF) << 0);

	packed_record[10] = (packed_record[10] & 0x0000) |
			    (((rec->key[5] >> 0) & 0xFFFF) << 0);
	packed_record[11] = (packed_record[11] & 0x0000) |
			    (((rec->key[5] >> 16) & 0xFFFF) << 0);

	packed_record[12] = (packed_record[12] & 0x0000) |
			    (((rec->key[6] >> 0) & 0xFFFF) << 0);
	packed_record[13] = (packed_record[13] & 0x0000) |
			    (((rec->key[6] >> 16) & 0xFFFF) << 0);

	packed_record[14] = (packed_record[14] & 0x0000) |
			    (((rec->key[7] >> 0) & 0xFFFF) << 0);
	packed_record[15] = (packed_record[15] & 0x0000) |
			    (((rec->key[7] >> 16) & 0xFFFF) << 0);

	ret = set_raw_egress_record(hw, packed_record, 8, 2,
				    ROWOFFSET_EGRESSSAKEYRECORD + table_index);
	if (unlikely(ret))
		return ret;
	ret = set_raw_egress_record(hw, packed_record + 8, 8, 2,
				    ROWOFFSET_EGRESSSAKEYRECORD + table_index -
					    32);
	if (unlikely(ret))
		return ret;

	return 0;
}

int aq_mss_set_egress_sakey_record(struct atl_hw *hw,
				   const struct aq_mss_egress_sakey_record *rec,
				   u16 table_index)
{
	return AQ_API_CALL_SAFE(set_egress_sakey_record, hw, rec, table_index);
}

static int get_egress_sakey_record(struct atl_hw *hw,
				   struct aq_mss_egress_sakey_record *rec,
				   u16 table_index)
{
	u16 packed_record[16];
	int ret;

	if (table_index >= NUMROWS_EGRESSSAKEYRECORD)
		return -EINVAL;

	ret = get_raw_egress_record(hw, packed_record, 8, 2,
				    ROWOFFSET_EGRESSSAKEYRECORD + table_index);
	if (unlikely(ret))
		return ret;
	ret = get_raw_egress_record(hw, packed_record + 8, 8, 2,
				    ROWOFFSET_EGRESSSAKEYRECORD + table_index -
					    32);
	if (unlikely(ret))
		return ret;

	rec->key[0] = (rec->key[0] & 0xFFFF0000) |
		      (((packed_record[0] >> 0) & 0xFFFF) << 0);
	rec->key[0] = (rec->key[0] & 0x0000FFFF) |
		      (((packed_record[1] >> 0) & 0xFFFF) << 16);

	rec->key[1] = (rec->key[1] & 0xFFFF0000) |
		      (((packed_record[2] >> 0) & 0xFFFF) << 0);
	rec->key[1] = (rec->key[1] & 0x0000FFFF) |
		      (((packed_record[3] >> 0) & 0xFFFF) << 16);

	rec->key[2] = (rec->key[2] & 0xFFFF0000) |
		      (((packed_record[4] >> 0) & 0xFFFF) << 0);
	rec->key[2] = (rec->key[2] & 0x0000FFFF) |
		      (((packed_record[5] >> 0) & 0xFFFF) << 16);

	rec->key[3] = (rec->key[3] & 0xFFFF0000) |
		      (((packed_record[6] >> 0) & 0xFFFF) << 0);
	rec->key[3] = (rec->key[3] & 0x0000FFFF) |
		      (((packed_record[7] >> 0) & 0xFFFF) << 16);

	rec->key[4] = (rec->key[4] & 0xFFFF0000) |
		      (((packed_record[8] >> 0) & 0xFFFF) << 0);
	rec->key[4] = (rec->key[4] & 0x0000FFFF) |
		      (((packed_record[9] >> 0) & 0xFFFF) << 16);

	rec->key[5] = (rec->key[5] & 0xFFFF0000) |
		      (((packed_record[10] >> 0) & 0xFFFF) << 0);
	rec->key[5] = (rec->key[5] & 0x0000FFFF) |
		      (((packed_record[11] >> 0) & 0xFFFF) << 16);

	rec->key[6] = (rec->key[6] & 0xFFFF0000) |
		      (((packed_record[12] >> 0) & 0xFFFF) << 0);
	rec->key[6] = (rec->key[6] & 0x0000FFFF) |
		      (((packed_record[13] >> 0) & 0xFFFF) << 16);

	rec->key[7] = (rec->key[7] & 0xFFFF0000) |
		      (((packed_record[14] >> 0) & 0xFFFF) << 0);
	rec->key[7] = (rec->key[7] & 0x0000FFFF) |
		      (((packed_record[15] >> 0) & 0xFFFF) << 16);

	return 0;
}

int aq_mss_get_egress_sakey_record(struct atl_hw *hw,
				   struct aq_mss_egress_sakey_record *rec,
				   u16 table_index)
{
	memset(rec, 0, sizeof(*rec));

	return AQ_API_CALL_SAFE(get_egress_sakey_record, hw, rec, table_index);
}

static int get_egress_sc_counters(struct atl_hw *hw,
				  struct aq_mss_egress_sc_counters *counters,
				  u16 sc_index)
{
	u16 packed_record[4];
	int ret;

	if (sc_index >= NUMROWS_EGRESSSCRECORD)
		return -EINVAL;

	ret = get_raw_egress_record(hw, packed_record, 4, 3, sc_index * 8 + 4);
	if (unlikely(ret))
		return ret;
	counters->sc_protected_pkts[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->sc_protected_pkts[1] =
		packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_egress_record(hw, packed_record, 4, 3, sc_index * 8 + 5);
	if (unlikely(ret))
		return ret;
	counters->sc_encrypted_pkts[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->sc_encrypted_pkts[1] =
		packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_egress_record(hw, packed_record, 4, 3, sc_index * 8 + 6);
	if (unlikely(ret))
		return ret;
	counters->sc_protected_octets[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->sc_protected_octets[1] =
		packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_egress_record(hw, packed_record, 4, 3, sc_index * 8 + 7);
	if (unlikely(ret))
		return ret;
	counters->sc_encrypted_octets[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->sc_encrypted_octets[1] =
		packed_record[2] | (packed_record[3] << 16);

	return 0;
}

int aq_mss_get_egress_sc_counters(struct atl_hw *hw,
				  struct aq_mss_egress_sc_counters *counters,
				  u16 sc_index)
{
	memset(counters, 0, sizeof(*counters));

	return AQ_API_CALL_SAFE(get_egress_sc_counters, hw, counters, sc_index);
}

static int get_egress_sa_counters(struct atl_hw *hw,
				  struct aq_mss_egress_sa_counters *counters,
				  u16 sa_index)
{
	u16 packed_record[4];
	int ret;

	if (sa_index >= NUMROWS_EGRESSSARECORD)
		return -EINVAL;

	ret = get_raw_egress_record(hw, packed_record, 4, 3, sa_index * 8 + 0);
	if (unlikely(ret))
		return ret;
	counters->sa_hit_drop_redirect[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->sa_hit_drop_redirect[1] =
		packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_egress_record(hw, packed_record, 4, 3, sa_index * 8 + 1);
	if (unlikely(ret))
		return ret;
	counters->sa_protected2_pkts[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->sa_protected2_pkts[1] =
		packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_egress_record(hw, packed_record, 4, 3, sa_index * 8 + 2);
	if (unlikely(ret))
		return ret;
	counters->sa_protected_pkts[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->sa_protected_pkts[1] =
		packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_egress_record(hw, packed_record, 4, 3, sa_index * 8 + 3);
	if (unlikely(ret))
		return ret;
	counters->sa_encrypted_pkts[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->sa_encrypted_pkts[1] =
		packed_record[2] | (packed_record[3] << 16);

	return 0;
}

int aq_mss_get_egress_sa_counters(struct atl_hw *hw,
				  struct aq_mss_egress_sa_counters *counters,
				  u16 sa_index)
{
	memset(counters, 0, sizeof(*counters));

	return AQ_API_CALL_SAFE(get_egress_sa_counters, hw, counters, sa_index);
}

static int
get_egress_common_counters(struct atl_hw *hw,
			   struct aq_mss_egress_common_counters *counters)
{
	u16 packed_record[4];
	int ret;

	ret = get_raw_egress_record(hw, packed_record, 4, 3, 256 + 0);
	if (unlikely(ret))
		return ret;
	counters->ctl_pkt[0] = packed_record[0] | (packed_record[1] << 16);
	counters->ctl_pkt[1] = packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_egress_record(hw, packed_record, 4, 3, 256 + 1);
	if (unlikely(ret))
		return ret;
	counters->unknown_sa_pkts[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->unknown_sa_pkts[1] =
		packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_egress_record(hw, packed_record, 4, 3, 256 + 2);
	if (unlikely(ret))
		return ret;
	counters->untagged_pkts[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->untagged_pkts[1] =
		packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_egress_record(hw, packed_record, 4, 3, 256 + 3);
	if (unlikely(ret))
		return ret;
	counters->too_long[0] = packed_record[0] | (packed_record[1] << 16);
	counters->too_long[1] = packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_egress_record(hw, packed_record, 4, 3, 256 + 4);
	if (unlikely(ret))
		return ret;
	counters->ecc_error_pkts[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->ecc_error_pkts[1] =
		packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_egress_record(hw, packed_record, 4, 3, 256 + 5);
	if (unlikely(ret))
		return ret;
	counters->unctrl_hit_drop_redir[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->unctrl_hit_drop_redir[1] =
		packed_record[2] | (packed_record[3] << 16);

	return 0;
}

int aq_mss_get_egress_common_counters(
	struct atl_hw *hw, struct aq_mss_egress_common_counters *counters)
{
	memset(counters, 0, sizeof(*counters));

	return AQ_API_CALL_SAFE(get_egress_common_counters, hw, counters);
}

static int clear_egress_counters(struct atl_hw *hw)
{
	struct mss_egress_ctl_register ctl_reg;
	int ret;

	memset(&ctl_reg, 0, sizeof(ctl_reg));

	ret = __atl_mdio_read(hw, 0, MDIO_MMD_VEND1,
			      MSS_EGRESS_CTL_REGISTER_ADDR,
			      &ctl_reg.word_0);
	if (unlikely(ret))
		return ret;
	ret = __atl_mdio_read(hw, 0, MDIO_MMD_VEND1,
			      MSS_EGRESS_CTL_REGISTER_ADDR + 4,
			      &ctl_reg.word_1);
	if (unlikely(ret))
		return ret;

	/* Toggle the Egress MIB clear bit 0->1->0 */
	ctl_reg.bits_0.clear_counter = 0;
	ret = __atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
			       MSS_EGRESS_CTL_REGISTER_ADDR,
			       ctl_reg.word_0);
	if (unlikely(ret))
		return ret;
	ret = __atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
			       MSS_EGRESS_CTL_REGISTER_ADDR + 4,
			       ctl_reg.word_1);
	if (unlikely(ret))
		return ret;

	ctl_reg.bits_0.clear_counter = 1;
	ret = __atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
			       MSS_EGRESS_CTL_REGISTER_ADDR,
			       ctl_reg.word_0);
	if (unlikely(ret))
		return ret;
	ret = __atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
			       MSS_EGRESS_CTL_REGISTER_ADDR + 4,
			       ctl_reg.word_1);
	if (unlikely(ret))
		return ret;

	ctl_reg.bits_0.clear_counter = 0;
	ret = __atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
			       MSS_EGRESS_CTL_REGISTER_ADDR,
			       ctl_reg.word_0);
	if (unlikely(ret))
		return ret;
	ret = __atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
			       MSS_EGRESS_CTL_REGISTER_ADDR + 4,
			       ctl_reg.word_1);
	if (unlikely(ret))
		return ret;

	return 0;
}

int aq_mss_clear_egress_counters(struct atl_hw *hw)
{
	return AQ_API_CALL_SAFE(clear_egress_counters, hw);
}

static int get_ingress_sa_counters(struct atl_hw *hw,
				   struct aq_mss_ingress_sa_counters *counters,
				   u16 sa_index)
{
	u16 packed_record[4];
	int ret;

	if (sa_index >= NUMROWS_INGRESSSARECORD)
		return -EINVAL;

	ret = get_raw_ingress_record(hw, packed_record, 4, 6,
				     sa_index * 12 + 0);
	if (unlikely(ret))
		return ret;
	counters->untagged_hit_pkts[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->untagged_hit_pkts[1] =
		packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6,
				     sa_index * 12 + 1);
	if (unlikely(ret))
		return ret;
	counters->ctrl_hit_drop_redir_pkts[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->ctrl_hit_drop_redir_pkts[1] =
		packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6,
				     sa_index * 12 + 2);
	if (unlikely(ret))
		return ret;
	counters->not_using_sa[0] = packed_record[0] | (packed_record[1] << 16);
	counters->not_using_sa[1] = packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6,
				     sa_index * 12 + 3);
	if (unlikely(ret))
		return ret;
	counters->unused_sa[0] = packed_record[0] | (packed_record[1] << 16);
	counters->unused_sa[1] = packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6,
				     sa_index * 12 + 4);
	if (unlikely(ret))
		return ret;
	counters->not_valid_pkts[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->not_valid_pkts[1] =
		packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6,
				     sa_index * 12 + 5);
	if (unlikely(ret))
		return ret;
	counters->invalid_pkts[0] = packed_record[0] | (packed_record[1] << 16);
	counters->invalid_pkts[1] = packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6,
				     sa_index * 12 + 6);
	if (unlikely(ret))
		return ret;
	counters->ok_pkts[0] = packed_record[0] | (packed_record[1] << 16);
	counters->ok_pkts[1] = packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6,
				     sa_index * 12 + 7);
	if (unlikely(ret))
		return ret;
	counters->late_pkts[0] = packed_record[0] | (packed_record[1] << 16);
	counters->late_pkts[1] = packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6,
				     sa_index * 12 + 8);
	if (unlikely(ret))
		return ret;
	counters->delayed_pkts[0] = packed_record[0] | (packed_record[1] << 16);
	counters->delayed_pkts[1] = packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6,
				     sa_index * 12 + 9);
	if (unlikely(ret))
		return ret;
	counters->unchecked_pkts[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->unchecked_pkts[1] =
		packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6,
				     sa_index * 12 + 10);
	if (unlikely(ret))
		return ret;
	counters->validated_octets[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->validated_octets[1] =
		packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6,
				     sa_index * 12 + 11);
	if (unlikely(ret))
		return ret;
	counters->decrypted_octets[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->decrypted_octets[1] =
		packed_record[2] | (packed_record[3] << 16);

	return 0;
}

int aq_mss_get_ingress_sa_counters(struct atl_hw *hw,
				   struct aq_mss_ingress_sa_counters *counters,
				   u16 sa_index)
{
	memset(counters, 0, sizeof(*counters));

	return AQ_API_CALL_SAFE(get_ingress_sa_counters, hw, counters,
				sa_index);
}

static int
get_ingress_common_counters(struct atl_hw *hw,
			    struct aq_mss_ingress_common_counters *counters)
{
	u16 packed_record[4];
	int ret;

	ret = get_raw_ingress_record(hw, packed_record, 4, 6, 385 + 0);
	if (unlikely(ret))
		return ret;
	counters->ctl_pkts[0] = packed_record[0] | (packed_record[1] << 16);
	counters->ctl_pkts[1] = packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6, 385 + 1);
	if (unlikely(ret))
		return ret;
	counters->tagged_miss_pkts[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->tagged_miss_pkts[1] =
		packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6, 385 + 2);
	if (unlikely(ret))
		return ret;
	counters->untagged_miss_pkts[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->untagged_miss_pkts[1] =
		packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6, 385 + 3);
	if (unlikely(ret))
		return ret;
	counters->notag_pkts[0] = packed_record[0] | (packed_record[1] << 16);
	counters->notag_pkts[1] = packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6, 385 + 4);
	if (unlikely(ret))
		return ret;
	counters->untagged_pkts[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->untagged_pkts[1] =
		packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6, 385 + 5);
	if (unlikely(ret))
		return ret;
	counters->bad_tag_pkts[0] = packed_record[0] | (packed_record[1] << 16);
	counters->bad_tag_pkts[1] = packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6, 385 + 6);
	if (unlikely(ret))
		return ret;
	counters->no_sci_pkts[0] = packed_record[0] | (packed_record[1] << 16);
	counters->no_sci_pkts[1] = packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6, 385 + 7);
	if (unlikely(ret))
		return ret;
	counters->unknown_sci_pkts[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->unknown_sci_pkts[1] =
		packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6, 385 + 8);
	if (unlikely(ret))
		return ret;
	counters->ctrl_prt_pass_pkts[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->ctrl_prt_pass_pkts[1] =
		packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6, 385 + 9);
	if (unlikely(ret))
		return ret;
	counters->unctrl_prt_pass_pkts[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->unctrl_prt_pass_pkts[1] =
		packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6, 385 + 10);
	if (unlikely(ret))
		return ret;
	counters->ctrl_prt_fail_pkts[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->ctrl_prt_fail_pkts[1] =
		packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6, 385 + 11);
	if (unlikely(ret))
		return ret;
	counters->unctrl_prt_fail_pkts[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->unctrl_prt_fail_pkts[1] =
		packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6, 385 + 12);
	if (unlikely(ret))
		return ret;
	counters->too_long_pkts[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->too_long_pkts[1] =
		packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6, 385 + 13);
	if (unlikely(ret))
		return ret;
	counters->igpoc_ctl_pkts[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->igpoc_ctl_pkts[1] =
		packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6, 385 + 14);
	if (unlikely(ret))
		return ret;
	counters->ecc_error_pkts[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->ecc_error_pkts[1] =
		packed_record[2] | (packed_record[3] << 16);

	ret = get_raw_ingress_record(hw, packed_record, 4, 6, 385 + 15);
	if (unlikely(ret))
		return ret;
	counters->unctrl_hit_drop_redir[0] =
		packed_record[0] | (packed_record[1] << 16);
	counters->unctrl_hit_drop_redir[1] =
		packed_record[2] | (packed_record[3] << 16);

	return 0;
}

int aq_mss_get_ingress_common_counters(
	struct atl_hw *hw, struct aq_mss_ingress_common_counters *counters)
{
	memset(counters, 0, sizeof(*counters));

	return AQ_API_CALL_SAFE(get_ingress_common_counters, hw, counters);
}

static int clear_ingress_counters(struct atl_hw *hw)
{
	struct mss_ingress_ctl_register ctl_reg;
	int ret;

	memset(&ctl_reg, 0, sizeof(ctl_reg));

	ret = __atl_mdio_read(hw, 0, MDIO_MMD_VEND1,
			      MSS_INGRESS_CTL_REGISTER_ADDR,
			      &ctl_reg.word_0);
	if (unlikely(ret))
		return ret;
	ret = __atl_mdio_read(hw, 0, MDIO_MMD_VEND1,
			      MSS_INGRESS_CTL_REGISTER_ADDR + 4,
			      &ctl_reg.word_1);
	if (unlikely(ret))
		return ret;

	/* Toggle the Ingress MIB clear bit 0->1->0 */
	ctl_reg.bits_0.clear_count = 0;
	ret = __atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
			       MSS_INGRESS_CTL_REGISTER_ADDR,
			       ctl_reg.word_0);
	if (unlikely(ret))
		return ret;
	ret = __atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
			       MSS_INGRESS_CTL_REGISTER_ADDR + 4,
			       ctl_reg.word_1);
	if (unlikely(ret))
		return ret;

	ctl_reg.bits_0.clear_count = 1;
	ret = __atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
			       MSS_INGRESS_CTL_REGISTER_ADDR,
			       ctl_reg.word_0);
	if (unlikely(ret))
		return ret;
	ret = __atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
			       MSS_INGRESS_CTL_REGISTER_ADDR + 4,
			       ctl_reg.word_1);
	if (unlikely(ret))
		return ret;

	ctl_reg.bits_0.clear_count = 0;
	ret = __atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
			       MSS_INGRESS_CTL_REGISTER_ADDR,
			       ctl_reg.word_0);
	if (unlikely(ret))
		return ret;
	ret = __atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
			       MSS_INGRESS_CTL_REGISTER_ADDR + 4,
			       ctl_reg.word_1);
	if (unlikely(ret))
		return ret;

	return 0;
}

int aq_mss_clear_ingress_counters(struct atl_hw *hw)
{
	return AQ_API_CALL_SAFE(clear_ingress_counters, hw);
}

static int get_egress_sa_expired(struct atl_hw *hw, u32 *expired)
{
	u16 val;
	int ret;

	ret = __atl_mdio_read(hw, 0, MDIO_MMD_VEND1,
			      MSS_EGRESS_SA_EXPIRED_STATUS_REGISTER_ADDR, &val);
	if (unlikely(ret))
		return ret;

	*expired = val;

	ret = __atl_mdio_read(hw, 0, MDIO_MMD_VEND1,
			      MSS_EGRESS_SA_EXPIRED_STATUS_REGISTER_ADDR + 1,
			      &val);
	if (unlikely(ret))
		return ret;

	*expired |= val << 16;

	return 0;
}

int aq_mss_get_egress_sa_expired(struct atl_hw *hw, u32 *expired)
{
	*expired = 0;

	return AQ_API_CALL_SAFE(get_egress_sa_expired, hw, expired);
}

static int get_egress_sa_threshold_expired(struct atl_hw *hw, u32 *expired)
{
	u16 val;
	int ret;

	ret = __atl_mdio_read(
		hw, 0, MDIO_MMD_VEND1,
		MSS_EGRESS_SA_THRESHOLD_EXPIRED_STATUS_REGISTER_ADDR, &val);
	if (unlikely(ret))
		return ret;

	*expired = val;

	ret = __atl_mdio_read(
		hw, 0, MDIO_MMD_VEND1,
		MSS_EGRESS_SA_THRESHOLD_EXPIRED_STATUS_REGISTER_ADDR + 1, &val);
	if (unlikely(ret))
		return ret;

	*expired |= val << 16;

	return 0;
}

int aq_mss_get_egress_sa_threshold_expired(struct atl_hw *hw, u32 *expired)
{
	*expired = 0;

	return AQ_API_CALL_SAFE(get_egress_sa_threshold_expired, hw, expired);
}

static int set_egress_sa_expired(struct atl_hw *hw, u32 expired)
{
	int ret;

	ret = __atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
			       MSS_EGRESS_SA_EXPIRED_STATUS_REGISTER_ADDR,
			       expired & 0xFFFF);
	if (unlikely(ret))
		return ret;

	ret = __atl_mdio_write(hw, 0, MDIO_MMD_VEND1,
			       MSS_EGRESS_SA_EXPIRED_STATUS_REGISTER_ADDR + 1,
			       expired >> 16);
	if (unlikely(ret))
		return ret;

	return 0;
}

int aq_mss_set_egress_sa_expired(struct atl_hw *hw, u32 expired)
{
	return AQ_API_CALL_SAFE(set_egress_sa_expired, hw, expired);
}

static int set_egress_sa_threshold_expired(struct atl_hw *hw, u32 expired)
{
	int ret;

	ret = __atl_mdio_write(
		hw, 0, MDIO_MMD_VEND1,
		MSS_EGRESS_SA_THRESHOLD_EXPIRED_STATUS_REGISTER_ADDR,
		expired & 0xFFFF);
	if (unlikely(ret))
		return ret;

	ret = __atl_mdio_write(
		hw, 0, MDIO_MMD_VEND1,
		MSS_EGRESS_SA_THRESHOLD_EXPIRED_STATUS_REGISTER_ADDR + 1,
		expired >> 16);
	if (unlikely(ret))
		return ret;

	return 0;
}

int aq_mss_set_egress_sa_threshold_expired(struct atl_hw *hw, u32 expired)
{
	return AQ_API_CALL_SAFE(set_egress_sa_threshold_expired, hw, expired);
}
