/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef IPA_MHI_H_
#define IPA_MHI_H_

#include <linux/ipa.h>
#include <linux/types.h>

/**
 * enum ipa_mhi_event_type - event type for mhi callback
 *
 * @IPA_MHI_EVENT_READY: IPA MHI is ready and IPA uC is loaded. After getting
 *	this event MHI client is expected to call to ipa_mhi_start() API
 * @IPA_MHI_EVENT_DATA_AVAILABLE: downlink data available on MHI channel
 */
enum ipa_mhi_event_type {
	IPA_MHI_EVENT_READY,
	IPA_MHI_EVENT_DATA_AVAILABLE,
	IPA_MHI_EVENT_MAX,
};

typedef void (*mhi_client_cb)(void *priv, enum ipa_mhi_event_type event,
	unsigned long data);

/**
 * struct ipa_mhi_msi_info - parameters for MSI (Message Signaled Interrupts)
 * @addr_low: MSI lower base physical address
 * @addr_hi: MSI higher base physical address
 * @data: Data Pattern to use when generating the MSI
 * @mask: Mask indicating number of messages assigned by the host to device
 *
 * msi value is written according to this formula:
 *	((data & ~mask) | (mmio.msiVec & mask))
 */
struct ipa_mhi_msi_info {
	u32 addr_low;
	u32 addr_hi;
	u32 data;
	u32 mask;
};

/**
 * struct ipa_mhi_init_params - parameters for IPA MHI initialization API
 *
 * @msi: MSI (Message Signaled Interrupts) parameters
 * @mmio_addr: MHI MMIO physical address
 * @first_ch_idx: First channel ID for hardware accelerated channels.
 * @first_er_idx: First event ring ID for hardware accelerated channels.
 * @assert_bit40: should assert bit 40 in order to access hots space.
 *	if PCIe iATU is configured then not need to assert bit40
 * @notify: client callback
 * @priv: client private data to be provided in client callback
 * @test_mode: flag to indicate if IPA MHI is in unit test mode
 */
struct ipa_mhi_init_params {
	struct ipa_mhi_msi_info msi;
	u32 mmio_addr;
	u32 first_ch_idx;
	u32 first_er_idx;
	bool assert_bit40;
	mhi_client_cb notify;
	void *priv;
	bool test_mode;
};

/**
 * struct ipa_mhi_start_params - parameters for IPA MHI start API
 *
 * @host_ctrl_addr: Base address of MHI control data structures
 * @host_data_addr: Base address of MHI data buffers
 * @channel_context_addr: channel context array address in host address space
 * @event_context_addr: event context array address in host address space
 */
struct ipa_mhi_start_params {
	u32 host_ctrl_addr;
	u32 host_data_addr;
	u64 channel_context_array_addr;
	u64 event_context_array_addr;
};

/**
 * struct ipa_mhi_connect_params - parameters for IPA MHI channel connect API
 *
 * @sys: IPA EP configuration info
 * @channel_id: MHI channel id
 */
struct ipa_mhi_connect_params {
	struct ipa_sys_connect_params sys;
	u8 channel_id;
};

/* bit #40 in address should be asserted for MHI transfers over pcie */
#define IPA_MHI_HOST_ADDR(addr) ((addr) | BIT_ULL(40))

#if defined CONFIG_IPA || defined CONFIG_IPA3

int ipa_mhi_init(struct ipa_mhi_init_params *params);

int ipa_mhi_start(struct ipa_mhi_start_params *params);

int ipa_mhi_connect_pipe(struct ipa_mhi_connect_params *in, u32 *clnt_hdl);

int ipa_mhi_disconnect_pipe(u32 clnt_hdl);

int ipa_mhi_suspend(bool force);

int ipa_mhi_resume(void);

void ipa_mhi_destroy(void);

#else /* (CONFIG_IPA || CONFIG_IPA3) */

static inline int ipa_mhi_init(struct ipa_mhi_init_params *params)
{
	return -EPERM;
}

static inline int ipa_mhi_start(struct ipa_mhi_start_params *params)
{
	return -EPERM;
}

static inline int ipa_mhi_connect_pipe(struct ipa_mhi_connect_params *in,
	u32 *clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_mhi_disconnect_pipe(u32 clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_mhi_suspend(bool force)
{
	return -EPERM;
}

static inline int ipa_mhi_resume(void)
{
	return -EPERM;
}

static inline void ipa_mhi_destroy(void)
{

}

#endif /* (CONFIG_IPA || CONFIG_IPA3) */

#endif /* IPA_MHI_H_ */
