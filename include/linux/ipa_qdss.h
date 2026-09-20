/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _IPA_QDSS_H_
#define _IPA_QDSS_H_

/**
 * enum ipa_qdss_notify - these are the only return items
 * @IPA_QDSS_SUCCESS: will be returned as it is for both conn
 *						and disconn
 * @IPA_QDSS_PIPE_CONN_FAILURE: will be returned as negative value
 * @IPA_QDSS_PIPE_DISCONN_FAILURE: will be returned as negative value
 */
enum ipa_qdss_notify {
	IPA_QDSS_SUCCESS,
	IPA_QDSS_PIPE_CONN_FAILURE,
	IPA_QDSS_PIPE_DISCONN_FAILURE,
};

/**
 * struct  ipa_qdss_conn_in_params - QDSS -> IPA TX configuration
 * @data_fifo_base_addr: Base address of the data FIFO used by BAM
 * @data_fifo_size: Size of the data FIFO
 * @desc_fifo_base_addr: Base address of the descriptor FIFO by BAM
 * @desc_fifo_size: Should be configured to 1 by QDSS
 * @bam_p_evt_dest_addr: equivalent to event_ring_doorbell_pa
 *			physical address of the doorbell that IPA uC
 *			will update the headpointer of the event ring.
 *			QDSS should send BAM_P_EVNT_REG address in this var
 *			Configured with the GSI Doorbell Address.
 *			GSI sends Update RP by doing a write to this address
 * @bam_p_evt_threshold: Threshold level of how many bytes consumed
 * @override_eot: if override EOT==1, it doesn't check the EOT bit in
 *			the descriptor
 */
struct ipa_qdss_conn_in_params {
	phys_addr_t  data_fifo_base_addr;
	u32  data_fifo_size;
	phys_addr_t desc_fifo_base_addr;
	u32 desc_fifo_size;
	phys_addr_t  bam_p_evt_dest_addr;
	u32 bam_p_evt_threshold;
	u32 override_eot;
};

/**
 * struct  ipa_qdss_conn_out_params - information provided
 *				to QDSS driver
 * @rx_db_pa: physical address of IPA doorbell for RX (QDSS->IPA transactions)
 *		QDSS to take this address and assign it to BAM_P_EVENT_DEST_ADDR
 */
struct ipa_qdss_conn_out_params {
	phys_addr_t ipa_rx_db_pa;
};

struct ipa_qdss_ops {
	int (*ipa_qdss_conn_pipes)(struct ipa_qdss_conn_in_params *in,
	struct ipa_qdss_conn_out_params *out);
	int (*ipa_qdss_disconn_pipes)(void);
};

#if IS_ENABLED(CONFIG_CORESIGHT_TMC_PCIE)
void ipa_qdss_ready_callback(void *ops);
void ipa_qdss_exit_callback(void);
#else
static inline void ipa_qdss_ready_callback(void *ops)
{ }
static inline void ipa_qdss_exit_callback(void)
{ }
#endif

#endif /* _IPA_QDSS_H_ */
