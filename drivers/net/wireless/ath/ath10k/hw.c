/*
 * Copyright (c) 2014-2015, 2017 Qualcomm Atheros, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/types.h>
#include "core.h"
#include "hw.h"

const struct ath10k_hw_regs qca988x_regs = {
	.rtc_soc_base_address		= 0x00004000,
	.rtc_wmac_base_address		= 0x00005000,
	.soc_core_base_address		= 0x00009000,
	.ce_wrapper_base_address	= 0x00057000,
	.ce0_base_address		= 0x00057400,
	.ce1_base_address		= 0x00057800,
	.ce2_base_address		= 0x00057c00,
	.ce3_base_address		= 0x00058000,
	.ce4_base_address		= 0x00058400,
	.ce5_base_address		= 0x00058800,
	.ce6_base_address		= 0x00058c00,
	.ce7_base_address		= 0x00059000,
	.soc_reset_control_si0_rst_mask	= 0x00000001,
	.soc_reset_control_ce_rst_mask	= 0x00040000,
	.soc_chip_id_address		= 0x000000ec,
	.scratch_3_address		= 0x00000030,
	.fw_indicator_address		= 0x00009030,
	.pcie_local_base_address	= 0x00080000,
	.ce_wrap_intr_sum_host_msi_lsb	= 0x00000008,
	.ce_wrap_intr_sum_host_msi_mask	= 0x0000ff00,
	.pcie_intr_fw_mask		= 0x00000400,
	.pcie_intr_ce_mask_all		= 0x0007f800,
	.pcie_intr_clr_address		= 0x00000014,
};

const struct ath10k_hw_regs qca6174_regs = {
	.rtc_soc_base_address			= 0x00000800,
	.rtc_wmac_base_address			= 0x00001000,
	.soc_core_base_address			= 0x0003a000,
	.ce_wrapper_base_address		= 0x00034000,
	.ce0_base_address			= 0x00034400,
	.ce1_base_address			= 0x00034800,
	.ce2_base_address			= 0x00034c00,
	.ce3_base_address			= 0x00035000,
	.ce4_base_address			= 0x00035400,
	.ce5_base_address			= 0x00035800,
	.ce6_base_address			= 0x00035c00,
	.ce7_base_address			= 0x00036000,
	.soc_reset_control_si0_rst_mask		= 0x00000000,
	.soc_reset_control_ce_rst_mask		= 0x00000001,
	.soc_chip_id_address			= 0x000000f0,
	.scratch_3_address			= 0x00000028,
	.fw_indicator_address			= 0x0003a028,
	.pcie_local_base_address		= 0x00080000,
	.ce_wrap_intr_sum_host_msi_lsb		= 0x00000008,
	.ce_wrap_intr_sum_host_msi_mask		= 0x0000ff00,
	.pcie_intr_fw_mask			= 0x00000400,
	.pcie_intr_ce_mask_all			= 0x0007f800,
	.pcie_intr_clr_address			= 0x00000014,
};

const struct ath10k_hw_regs qca99x0_regs = {
	.rtc_soc_base_address			= 0x00080000,
	.rtc_wmac_base_address			= 0x00000000,
	.soc_core_base_address			= 0x00082000,
	.ce_wrapper_base_address		= 0x0004d000,
	.ce0_base_address			= 0x0004a000,
	.ce1_base_address			= 0x0004a400,
	.ce2_base_address			= 0x0004a800,
	.ce3_base_address			= 0x0004ac00,
	.ce4_base_address			= 0x0004b000,
	.ce5_base_address			= 0x0004b400,
	.ce6_base_address			= 0x0004b800,
	.ce7_base_address			= 0x0004bc00,
	/* Note: qca99x0 supports upto 12 Copy Engines. Other than address of
	 * CE0 and CE1 no other copy engine is directly referred in the code.
	 * It is not really necessary to assign address for newly supported
	 * CEs in this address table.
	 *	Copy Engine		Address
	 *	CE8			0x0004c000
	 *	CE9			0x0004c400
	 *	CE10			0x0004c800
	 *	CE11			0x0004cc00
	 */
	.soc_reset_control_si0_rst_mask		= 0x00000001,
	.soc_reset_control_ce_rst_mask		= 0x00000100,
	.soc_chip_id_address			= 0x000000ec,
	.scratch_3_address			= 0x00040050,
	.fw_indicator_address			= 0x00040050,
	.pcie_local_base_address		= 0x00000000,
	.ce_wrap_intr_sum_host_msi_lsb		= 0x0000000c,
	.ce_wrap_intr_sum_host_msi_mask		= 0x00fff000,
	.pcie_intr_fw_mask			= 0x00100000,
	.pcie_intr_ce_mask_all			= 0x000fff00,
	.pcie_intr_clr_address			= 0x00000010,
};

const struct ath10k_hw_regs qca4019_regs = {
	.rtc_soc_base_address                   = 0x00080000,
	.soc_core_base_address                  = 0x00082000,
	.ce_wrapper_base_address                = 0x0004d000,
	.ce0_base_address                       = 0x0004a000,
	.ce1_base_address                       = 0x0004a400,
	.ce2_base_address                       = 0x0004a800,
	.ce3_base_address                       = 0x0004ac00,
	.ce4_base_address                       = 0x0004b000,
	.ce5_base_address                       = 0x0004b400,
	.ce6_base_address                       = 0x0004b800,
	.ce7_base_address                       = 0x0004bc00,
	/* qca4019 supports upto 12 copy engines. Since base address
	 * of ce8 to ce11 are not directly referred in the code,
	 * no need have them in separate members in this table.
	 *      Copy Engine             Address
	 *      CE8                     0x0004c000
	 *      CE9                     0x0004c400
	 *      CE10                    0x0004c800
	 *      CE11                    0x0004cc00
	 */
	.soc_reset_control_si0_rst_mask         = 0x00000001,
	.soc_reset_control_ce_rst_mask          = 0x00000100,
	.soc_chip_id_address                    = 0x000000ec,
	.fw_indicator_address                   = 0x0004f00c,
	.ce_wrap_intr_sum_host_msi_lsb          = 0x0000000c,
	.ce_wrap_intr_sum_host_msi_mask         = 0x00fff000,
	.pcie_intr_fw_mask                      = 0x00100000,
	.pcie_intr_ce_mask_all                  = 0x000fff00,
	.pcie_intr_clr_address                  = 0x00000010,
};

const struct ath10k_hw_regs wcn3990_regs = {
	.rtc_soc_base_address			= 0x00000000,
	.rtc_wmac_base_address			= 0x00000000,
	.soc_core_base_address			= 0x00000000,
	.ce_wrapper_base_address		= 0x0024C000,
	.soc_global_reset_address		= 0x00000008,
	.ce0_base_address			= 0x00240000,
	.ce1_base_address			= 0x00241000,
	.ce2_base_address			= 0x00242000,
	.ce3_base_address			= 0x00243000,
	.ce4_base_address			= 0x00244000,
	.ce5_base_address			= 0x00245000,
	.ce6_base_address			= 0x00246000,
	.ce7_base_address			= 0x00247000,
	.ce8_base_address			= 0x00248000,
	.ce9_base_address			= 0x00249000,
	.ce10_base_address			= 0x0024A000,
	.ce11_base_address			= 0x0024B000,
	.soc_chip_id_address			= 0x000000f0,
	.soc_reset_control_si0_rst_mask		= 0x00000001,
	.soc_reset_control_ce_rst_mask		= 0x00000100,
	.ce_wrap_intr_sum_host_msi_lsb		= 0x0000000c,
	.ce_wrap_intr_sum_host_msi_mask		= 0x00fff000,
	.pcie_intr_fw_mask			= 0x00100000,
};

static unsigned int
ath10k_set_ring_byte(unsigned int offset,
		     struct ath10k_hw_ce_regs_addr_map *addr_map)
{
	return (((0 | (offset)) << addr_map->lsb) & addr_map->mask);
}

static unsigned int
ath10k_get_ring_byte(unsigned int offset,
		     struct ath10k_hw_ce_regs_addr_map *addr_map)
{
	return (((offset) & addr_map->mask) >> (addr_map->lsb));
}

struct ath10k_hw_ce_regs_addr_map wcn3990_src_ring = {
	.msb	= 0x00000010,
	.lsb	= 0x00000010,
	.mask	= 0x00020000,
	.set	= &ath10k_set_ring_byte,
	.get	= &ath10k_get_ring_byte,
};

struct ath10k_hw_ce_regs_addr_map wcn3990_dst_ring = {
	.msb	= 0x00000012,
	.lsb	= 0x00000012,
	.mask	= 0x00040000,
	.set	= &ath10k_set_ring_byte,
	.get	= &ath10k_get_ring_byte,
};

struct ath10k_hw_ce_regs_addr_map wcn3990_dmax = {
	.msb	= 0x00000000,
	.lsb	= 0x00000000,
	.mask	= 0x0000ffff,
	.set	= &ath10k_set_ring_byte,
	.get	= &ath10k_get_ring_byte,
};

struct ath10k_hw_ce_ctrl1 wcn3990_ctrl1 = {
	.addr		= 0x00000018,
	.src_ring	= &wcn3990_src_ring,
	.dst_ring	= &wcn3990_dst_ring,
	.dmax		= &wcn3990_dmax,
};

struct ath10k_hw_ce_regs wcn3990_ce_regs = {
	.sr_base_addr		= 0x00000000,
	.sr_size_addr		= 0x00000008,
	.dr_base_addr		= 0x0000000c,
	.dr_size_addr		= 0x00000014,
	.misc_ie_addr		= 0x00000034,
	.sr_wr_index_addr	= 0x0000003c,
	.dst_wr_index_addr	= 0x00000040,
	.current_srri_addr	= 0x00000044,
	.current_drri_addr	= 0x00000048,
	.ddr_addr_for_rri_low	= 0x00000004,
	.ddr_addr_for_rri_high	= 0x00000008,
	.ce_rri_low		= 0x0024C004,
	.ce_rri_high		= 0x0024C008,
	.host_ie_addr		= 0x0000002c,
	.ctrl1_regs		= &wcn3990_ctrl1,
};

struct ath10k_hw_ce_regs_addr_map qcax_src_ring = {
	.msb	= 0x00000010,
	.lsb	= 0x00000010,
	.mask	= 0x00010000,
	.set	= &ath10k_set_ring_byte,
};

struct ath10k_hw_ce_regs_addr_map qcax_dst_ring = {
	.msb	= 0x00000011,
	.lsb	= 0x00000011,
	.mask	= 0x00020000,
	.set	= &ath10k_set_ring_byte,
	.get	= &ath10k_get_ring_byte,
};

struct ath10k_hw_ce_regs_addr_map qcax_dmax = {
	.msb	= 0x0000000f,
	.lsb	= 0x00000000,
	.mask	= 0x0000ffff,
	.set	= &ath10k_set_ring_byte,
	.get    = &ath10k_get_ring_byte,
};

struct ath10k_hw_ce_ctrl1 qcax_ctrl1 = {
	.addr		= 0x00000010,
	.hw_mask	= 0x0007ffff,
	.sw_mask	= 0x0007ffff,
	.hw_wr_mask	= 0x00000000,
	.sw_wr_mask	= 0x0007ffff,
	.reset_mask	= 0xffffffff,
	.reset		= 0x00000080,
	.src_ring	= &qcax_src_ring,
	.dst_ring	= &qcax_dst_ring,
	.dmax		= &qcax_dmax,
};

struct ath10k_hw_ce_regs qcax_ce_regs = {
	.sr_base_addr		= 0x00000000,
	.sr_size_addr		= 0x00000004,
	.dr_base_addr		= 0x00000008,
	.dr_size_addr		= 0x0000000c,
	.ce_cmd_addr		= 0x00000018,
	.misc_ie_addr		= 0x00000034,
	.sr_wr_index_addr	= 0x0000003c,
	.dst_wr_index_addr	= 0x00000040,
	.current_srri_addr	= 0x00000044,
	.current_drri_addr	= 0x00000048,
	.host_ie_addr		= 0x0000002c,
	.ctrl1_regs		= &qcax_ctrl1,
};

const struct ath10k_hw_values qca988x_values = {
	.rtc_state_val_on		= 3,
	.ce_count			= 8,
	.msi_assign_ce_max		= 7,
	.num_target_ce_config_wlan	= 7,
	.ce_desc_meta_data_mask		= 0xFFFC,
	.ce_desc_meta_data_lsb		= 2,
};

const struct ath10k_hw_values qca6174_values = {
	.rtc_state_val_on		= 3,
	.ce_count			= 8,
	.msi_assign_ce_max		= 7,
	.num_target_ce_config_wlan	= 7,
	.ce_desc_meta_data_mask		= 0xFFFC,
	.ce_desc_meta_data_lsb		= 2,
};

const struct ath10k_hw_values qca99x0_values = {
	.rtc_state_val_on		= 5,
	.ce_count			= 12,
	.msi_assign_ce_max		= 12,
	.num_target_ce_config_wlan	= 10,
	.ce_desc_meta_data_mask		= 0xFFF0,
	.ce_desc_meta_data_lsb		= 4,
};

const struct ath10k_hw_values qca9888_values = {
	.rtc_state_val_on		= 3,
	.ce_count			= 12,
	.msi_assign_ce_max		= 12,
	.num_target_ce_config_wlan	= 10,
	.ce_desc_meta_data_mask		= 0xFFF0,
	.ce_desc_meta_data_lsb		= 4,
};

const struct ath10k_hw_values qca4019_values = {
	.ce_count                       = 12,
	.num_target_ce_config_wlan      = 10,
	.ce_desc_meta_data_mask         = 0xFFF0,
	.ce_desc_meta_data_lsb          = 4,
};

const struct ath10k_hw_values wcn3990_values = {
	.rtc_state_val_on		= 5,
	.ce_count			= 12,
	.msi_assign_ce_max		= 12,
	.num_target_ce_config_wlan	= 12,
	.ce_desc_meta_data_mask		= 0xFFF0,
	.ce_desc_meta_data_lsb		= 4,
};

struct fw_flag wcn3990_fw_flags = {
	.flags = 0x82E,
};

struct ath10k_shadow_reg_value wcn3990_shadow_reg_value = {
	.shadow_reg_value_0  = 0x00032000,
	.shadow_reg_value_1  = 0x00032004,
	.shadow_reg_value_2  = 0x00032008,
	.shadow_reg_value_3  = 0x0003200C,
	.shadow_reg_value_4  = 0x00032010,
	.shadow_reg_value_5  = 0x00032014,
	.shadow_reg_value_6  = 0x00032018,
	.shadow_reg_value_7  = 0x0003201C,
	.shadow_reg_value_8  = 0x00032020,
	.shadow_reg_value_9  = 0x00032024,
	.shadow_reg_value_10 = 0x00032028,
	.shadow_reg_value_11 = 0x0003202C,
	.shadow_reg_value_12 = 0x00032030,
	.shadow_reg_value_13 = 0x00032034,
	.shadow_reg_value_14 = 0x00032038,
	.shadow_reg_value_15 = 0x0003203C,
	.shadow_reg_value_16 = 0x00032040,
	.shadow_reg_value_17 = 0x00032044,
	.shadow_reg_value_18 = 0x00032048,
	.shadow_reg_value_19 = 0x0003204C,
	.shadow_reg_value_20 = 0x00032050,
	.shadow_reg_value_21 = 0x00032054,
	.shadow_reg_value_22 = 0x00032058,
	.shadow_reg_value_23 = 0x0003205C
};

struct ath10k_shadow_reg_address wcn3990_shadow_reg_address = {
	.shadow_reg_address_0  = 0x00030020,
	.shadow_reg_address_1  = 0x00030024,
	.shadow_reg_address_2  = 0x00030028,
	.shadow_reg_address_3  = 0x0003002C,
	.shadow_reg_address_4  = 0x00030030,
	.shadow_reg_address_5  = 0x00030034,
	.shadow_reg_address_6  = 0x00030038,
	.shadow_reg_address_7  = 0x0003003C,
	.shadow_reg_address_8  = 0x00030040,
	.shadow_reg_address_9  = 0x00030044,
	.shadow_reg_address_10 = 0x00030048,
	.shadow_reg_address_11 = 0x0003004C,
	.shadow_reg_address_12 = 0x00030050,
	.shadow_reg_address_13 = 0x00030054,
	.shadow_reg_address_14 = 0x00030058,
	.shadow_reg_address_15 = 0x0003005C,
	.shadow_reg_address_16 = 0x00030060,
	.shadow_reg_address_17 = 0x00030064,
	.shadow_reg_address_18 = 0x00030068,
	.shadow_reg_address_19 = 0x0003006C,
	.shadow_reg_address_20 = 0x00030070,
	.shadow_reg_address_21 = 0x00030074,
	.shadow_reg_address_22 = 0x00030078,
	.shadow_reg_address_23 = 0x0003007C
};

void ath10k_hw_fill_survey_time(struct ath10k *ar, struct survey_info *survey,
				u32 cc, u32 rcc, u32 cc_prev, u32 rcc_prev)
{
	u32 cc_fix = 0;
	u32 rcc_fix = 0;
	enum ath10k_hw_cc_wraparound_type wraparound_type;

	survey->filled |= SURVEY_INFO_TIME |
			  SURVEY_INFO_TIME_BUSY;

	wraparound_type = ar->hw_params.cc_wraparound_type;

	if (cc < cc_prev || rcc < rcc_prev) {
		switch (wraparound_type) {
		case ATH10K_HW_CC_WRAP_SHIFTED_ALL:
			if (cc < cc_prev) {
				cc_fix = 0x7fffffff;
				survey->filled &= ~SURVEY_INFO_TIME_BUSY;
			}
			break;
		case ATH10K_HW_CC_WRAP_SHIFTED_EACH:
			if (cc < cc_prev)
				cc_fix = 0x7fffffff;

			if (rcc < rcc_prev)
				rcc_fix = 0x7fffffff;
			break;
		case ATH10K_HW_CC_WRAP_DISABLED:
			break;
		}
	}

	cc -= cc_prev - cc_fix;
	rcc -= rcc_prev - rcc_fix;

	survey->time = CCNT_TO_MSEC(ar, cc);
	survey->time_busy = CCNT_TO_MSEC(ar, rcc);
}

const struct ath10k_hw_ops qca988x_ops = {
};

static int ath10k_qca99x0_rx_desc_get_l3_pad_bytes(struct htt_rx_desc *rxd)
{
	return MS(__le32_to_cpu(rxd->msdu_end.qca99x0.info1),
		  RX_MSDU_END_INFO1_L3_HDR_PAD);
}

const struct ath10k_hw_ops qca99x0_ops = {
	.rx_desc_get_l3_pad_bytes = ath10k_qca99x0_rx_desc_get_l3_pad_bytes,
};

const struct ath10k_hw_ops wcn3990_ops = {0};
