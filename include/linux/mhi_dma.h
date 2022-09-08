/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _MHI_DMA_H_
#define _MHI_DMA_H_

#include <linux/types.h>
#include <linux/dma-mapping.h>

/* Defines & Enums */

/* Bit #40 in address should be asserted for MHI transfers over pcie */
#define MHI_DMA_HOST_ADDR(addr) ((addr) | BIT_ULL(40))

/*
 * enum mhi_dma_function_type - function type for MHI PF\VF
 *
 * @MHI_DMA_FUNCTION_TYPE_PHYSICAL: Physical function
 * @MHI_DMA_FUNCTION_TYPE_VIRTUAL: Virtual function
 */
enum mhi_dma_function_type {
	MHI_DMA_FUNCTION_TYPE_PHYSICAL,
	MHI_DMA_FUNCTION_TYPE_VIRTUAL,
};

/*
 * enum mhi_dma_event_type - event type for mhi callback
 *
 * @MHI_DMA_EVENT_READY: DMA MHI is ready. After getting
 *	this event MHI Driver is expected to call to mhi_dma_start() API
 * @MHI_DMA_EVENT_DATA_AVAILABLE: Data available on MHI HOST channel
 */
enum mhi_dma_event_type {
	MHI_DMA_EVENT_READY,
	MHI_DMA_EVENT_DATA_AVAILABLE,
	MHI_DMA_EVENT_MAX,
};

enum mhi_dma_mstate {
	MHI_DMA_STATE_M0,
	MHI_DMA_STATE_M1,
	MHI_DMA_STATE_M2,
	MHI_DMA_STATE_M3,
	MHI_DMA_STATE_M_MAX
};

typedef void (*mhi_dma_cb)(void *priv, enum mhi_dma_event_type event,
			   unsigned long data);

/* Structures */

/*
 * struct mhi_dma_function_params - parameters for DMA APIs
 *
 * @function_type: either physical or virtual
 * @vf_id: ID of current virtual function, valid only if type is virtual
 */
struct mhi_dma_function_params {
	enum mhi_dma_function_type function_type;
	u8 vf_id;
};

/*
 * struct mhi_dma_msi_info -  parameters for MSI (Message Signaled
 *                                  Interrupts)
 * @addr_low: MSI lower base physical address
 * @addr_hi: MSI higher base physical address
 * @data: Data Pattern to use when generating the MSI
 * @mask: Mask indicating number of messages assigned by the host to device
 *
 * msi value is written according to this formula:
 *	((data & ~mask) | (mmio.msiVec & mask))
 */
struct mhi_dma_msi_info {
	u32 addr_low;
	u32 addr_hi;
	u32 data;
	u32 mask;
};

/*
 * struct mhi_dma_init_params - parameters for DMA MHI initialization
 *                                        API
 *
 * @msi: MSI (Message Signaled Interrupts) parameters
 * @mmio_addr: MHI MMIO physical address
 * @first_ch_idx: First channel ID for hardware accelerated channels.
 * @first_er_idx: First event ring ID for hardware accelerated channels.
 * @assert_bit40: should assert bit 40 in order to access host space.
 *	if PCIe iATU is configured then not need to assert bit40
 * @notify: client callback
 * @priv: client private data to be provided in client callback
 * @test_mode: flag to indicate if DMA MHI is in unit test mode
 */
struct mhi_dma_init_params {
	struct mhi_dma_msi_info msi;
	u32 mmio_addr;
	u32 first_ch_idx;
	u32 first_er_idx;
	bool assert_bit40;
	mhi_dma_cb notify;
	void *priv;
	bool test_mode;
};

/*
 * struct mhi_dma_init_out - parameters receivied from HW driver upon init
 *
 * @ch_db_fwd_base: Channels doorbell address on device HW side
 * @ev_db_fwd_base: Events doorbell address on device HW side
 * @ch_db_fwd_msk: Bit mask for enabled channels
 * @ev_db_fwd_msk: Bit mask for enabled events
 */
struct mhi_dma_init_out {
	u64 ch_db_fwd_base;
	u64 ev_db_fwd_base;
	u32 ch_db_fwd_msk;
	u32 ev_db_fwd_msk;
};

/*
 * struct mhi_dma_start_params - parameters for DMA MHI start API
 *
 * @host_ctrl_addr: Base address of MHI control data structures
 * @host_data_addr: Base address of MHI data buffers
 * @channel_context_addr: channel context array address in host address space
 * @event_context_addr: event context array address in host address space
 */
struct mhi_dma_start_params {
	u32 host_ctrl_addr;
	u32 host_data_addr;
	u64 channel_context_array_addr;
	u64 event_context_array_addr;
};

/*
 * struct mhi_dma_connect_params -    parameters for DMA MHI channel
 *                                          connect API
 *
 * @channel_id: MHI channel id
 * @desc_fifo_sz: size of desc FIFO. GSI ring is 2 * desc_fifo_sz.
 * @priv:	callback cookie
 * @notify:	callback
 *		priv - callback cookie
 *		evt - type of event
 *		data - data relevant to event.  May not be valid. See event_type
 *		enum for valid cases.
 * @int_modt: GSI event ring interrupt moderation time
 *		cycles base interrupt moderation (32KHz clock)
 * @int_modc: GSI event ring interrupt moderation packet counter
 * @buff_size: Actual buff size of rx_pkt
 */
struct mhi_dma_connect_params {
	u8 channel_id;
	u32 desc_fifo_sz;
	void *priv;
	mhi_dma_cb notify;
	u32 int_modt;
	u32 int_modc;
	u32 buff_size;
};

/*
 * struct mhi_dma_disconnect_params - parameters for DMA MHI channel
 *                                          disconnect API
 *
 * @clnt_hdl: Client handle for this endp
 */
struct mhi_dma_disconnect_params {
	u32 clnt_hdl;
};

/*
 * struct mhi_dma_ops - Structure to contain DMA driver API functions
 */
struct mhi_dma_ops {
	int (*mhi_dma_register_ready_cb)(void (*mhi_ready_cb)(void *user_data),
				void *user_data);
	int (*mhi_dma_init)(struct mhi_dma_function_params function,
			    struct mhi_dma_init_params *params,
			    struct mhi_dma_init_out *out);
	int (*mhi_dma_start)(struct mhi_dma_function_params function,
			     struct mhi_dma_start_params *params);
	int (*mhi_dma_connect_endp)(struct mhi_dma_function_params function,
				    struct mhi_dma_connect_params *in,
				    u32 *clnt_hdl);
	int (*mhi_dma_disconnect_endp)(struct mhi_dma_function_params function,
				       struct mhi_dma_disconnect_params *in);
	void (*mhi_dma_destroy)(struct mhi_dma_function_params function);
	int (*mhi_dma_memcpy_init)(struct mhi_dma_function_params function);
	void (*mhi_dma_memcpy_destroy)(struct mhi_dma_function_params function);
	int (*mhi_dma_memcpy_enable)(struct mhi_dma_function_params function);
	int (*mhi_dma_memcpy_disable)(struct mhi_dma_function_params function);
	int (*mhi_dma_sync_memcpy)(u64 dest, u64 src, int len,
				   struct mhi_dma_function_params function);
	int (*mhi_dma_async_memcpy)(u64 dest, u64 src, int len,
				    struct mhi_dma_function_params function,
				    void (*user_cb)(void *user1),
				    void *user_param);
	dma_addr_t (*mhi_dma_map_buffer)(void *virt, size_t size,
					 enum dma_data_direction dir);
	void (*mhi_dma_unmap_buffer)(dma_addr_t phys, size_t size,
		enum dma_data_direction dir);
	void* (*mhi_dma_alloc_buffer)(size_t size, dma_addr_t *phys, gfp_t gfp);
	void (*mhi_dma_free_buffer)(size_t size, void *virt, dma_addr_t phys);
	int (*mhi_dma_update_mstate)(struct mhi_dma_function_params function,
				  enum mhi_dma_mstate mstate_info);
	int (*mhi_dma_resume)(struct mhi_dma_function_params function);
	int (*mhi_dma_suspend)(struct mhi_dma_function_params function, bool force);
};

#if IS_ENABLED(CONFIG_MSM_MHI_DEV)
/*
 * mhi_dma_provide_ops() - DMA interface to provide OPs to MHI device driver
 *
 * @ops: Pointer to OPS struct with function pointers to DMA OPs
 * Expected to be copied to MHI driver memory
 *
 * The function will return: 0 - on success,
 *                             - Negative on error
 */
int mhi_dma_provide_ops(const struct mhi_dma_ops *ops);

#else

static inline int mhi_dma_provide_ops(const struct mhi_dma_ops *ops)
{
	return -EPERM;
}
#endif

/* Architecture API functions */
/*
 * mhi_dma_register_ready_cb() - register a callback to be invoked
 * when DMA driver initialization is complete.
 *
 * @mhi_ready_cb: CB to be invoked.
 * @user_data: Data to be sent to the originator of the CB.
 *
 * The function will return: 0       - on success,
 *                           -ENOMEM - on memory allocation fail
 *                           -EEXIST - if DMA driver initialization
 *                                     was already complete.
 */
int mhi_dma_register_ready_cb(void (*mhi_ready_cb)(void *user_data),
		void *user_data);

/*
 * mhi_dma_init() - Register to DMA MHI client
 * @function: function parameters
 * @params: Registration params
 * @params: Data returned from HW driver
 *
 * This function is called by MHI client driver on boot to register DMA MHI
 * Client. When this function returns device can move to READY state.
 * This function is doing the following:
 *	- Initialize MHI DMA internal data structures
 *	- Initialize debugfs
 *	- Initialize SYNC and ASYNC DMA ENDPs
 *
 * Return codes:    0 : success
 *		            negative : error
 */
int mhi_dma_init(struct mhi_dma_function_params function,
		 struct mhi_dma_init_params *params,
		 struct mhi_dma_init_out *out);

/*
 * mhi_dma_start() - Start DMA MHI engine
 * @function: function parameters
 * @params: pcie addresses for MHI
 *
 * This function is called by MHI client driver on MHI engine start for
 * handling MHI accelerated channels. This function is called after
 * mhi_dma_init() was called and can be called after MHI reset to restart
 * MHI engine. When this function returns device can move to M0 state.
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int mhi_dma_start(struct mhi_dma_function_params function,
		  struct mhi_dma_start_params *params);

/*
 * mhi_dma_connect_endp() - Connect endp to DMA and start
 * corresponding MHI channel
 * @function: function parameters
 * @in: connect parameters
 * @clnt_hdl: [out] client handle for this endp
 *
 * This function is called by MHI client driver on MHI channel start.
 * This function is called after MHI engine was started.
 *
 * Return codes:    0 : success
 *		            negative : error
 */
int mhi_dma_connect_endp(struct mhi_dma_function_params function,
			 struct mhi_dma_connect_params *in, u32 *clnt_hdl);

/*
 * mhi_dma_disconnect_endp() - Disconnect endp from DMA and reset
 * corresponding MHI channel
 * @function: function parameters
 * @in: disconnect parameters
 *
 * This function is called by MHI client driver on MHI channel reset.
 * This function is called after MHI channel was started.
 * This function is doing the following:
 *	- Send command to GSI to reset corresponding MHI channel
 *	- Configure DMA EP control
 *
 * Return codes:    0 : success
 *		            negative : error
 */
int mhi_dma_disconnect_endp(struct mhi_dma_function_params function,
			    struct mhi_dma_disconnect_params *in);

/*
 * mhi_dma_suspend() - Suspend MHI accelerated channels
 *
 * @function: function parameters
 * @force:
 *	false: In case of data pending in HW, MHI channels will not be
 *		suspended and function will fail.
 *	true:  In case of data pending in HW, make sure no further access from
 *		IPA to PCIe is possible. In this case suspend cannot fail.
 *
 * This function is called by MHI client driver on MHI suspend.
 * This function is called after MHI channel was started.
 * When this function returns device can move to M1/M2/M3/D3cold state.
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int mhi_dma_suspend(struct mhi_dma_function_params function, bool force);

/*
 * mhi_dma_resume() - Resume MHI accelerated channels
 *
 * @function: function parameters
 *
 * This function is called by MHI client driver on MHI resume.
 * This function is called after MHI channel was suspended.
 * When this function returns device can move to M0 state.
 * This function is doing the following:
 *	- Send command to GSI to resume corresponding MHI channel
 *	- Activate PM clients
 *	- Resume data to HW
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int mhi_dma_resume(struct mhi_dma_function_params function);

/*
 * mhi_dma_update_mstate() - Provides M state info
 * @function: function parameters
 * @mstate_info:
 *	state_m0:  in case of resume happening because of mhi going
 *		into M0 state.
 *	state_m2:  in case of suspend/resume happening because of mhi going
 *		into M2 state.
 *	state_m3:  in case of suspend/resume happening because of mhi going
 *		into M3 state.
 *
 * This function is called by MHI client driver before MHI suspend/ resume.
 * This function is called before MHI suspend or after MHI resume.
 * When this function returns device can move to M1/M2/M3/D3cold state.
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int mhi_dma_update_mstate(struct mhi_dma_function_params function,
			  enum mhi_dma_mstate mstate_info);

/*
 * mhi_dma_destroy() -teardown HW DMA pipes and release hw dma.
 *
 * @function: function parameters
 * @params: identification params
 *
 * this is a blocking function, returns just after destroying HW DMA.
 */
void mhi_dma_destroy(struct mhi_dma_function_params function);

/*
 * mhi_dma_memcpy_init() - Initialize memory copy DMA.
 * @function: function parameters
 *
 * This function initialize all memory copy DMA internal data and connect dma:
 *	MEMCPY_DMA_SYNC_PROD ->MEMCPY_DMA_SYNC_CONS
 *	MEMCPY_DMA_ASYNC_PROD->MEMCPY_DMA_SYNC_CONS
 *
 * Can be executed several times (re-entrant)
 *
 * Return codes: 0: success
 *		-EFAULT: Mismatch between context existence and init ref_cnt
 *		-EINVAL: HW driver is not initialized
 *		-ENOMEM: allocating memory error
 *		-EPERM: ENDP connection failed
 */
int mhi_dma_memcpy_init(struct mhi_dma_function_params function);


/*
 * mhi_dma_memcpy_destroy() - release all allocated resources.
 *
 * @function: function parameters
 *
 * this is a blocking function, returns just after destroying HW DMA.
 */
void mhi_dma_memcpy_destroy(struct mhi_dma_function_params function);

/*
 * mhi_dma_memcpy_enable() -Vote for HW clocks.
 *
 * @function: function parameters
 *
 *Return codes: 0: success
 *		-EINVAL: HW DMA is not initialized
 *		-EPERM: Operation not permitted as mhi_dma is already enabled
 */
int mhi_dma_memcpy_enable(struct mhi_dma_function_params function);

/*
 * mhi_dma_memcpy_disable()- Unvote for HW clocks.
 *
 * @function: function parameters
 *
 * enter to power save mode.
 *
 * Return codes: 0: success
 *		-EINVAL: HW DMA is not initialized
 *		-EPERM: Operation not permitted as mhi_dma is already disabled
 *		-EFAULT: can not disable mhi_dma as there are pending memcopy works
 */
int mhi_dma_memcpy_disable(struct mhi_dma_function_params function);

/*
 * mhi_dma_sync_memcpy()- Perform synchronous memcpy using DMA.
 *
 * @dest: physical address to store the copied data.
 * @src: physical address of the source data to copy.
 * @len: number of bytes to copy.
 * @function: function parameters
 *
 * Return codes:    0: success
 *		            -EINVAL: invalid params
 *		            -EPERM: operation not permitted as dma isn't
 *		                    enable or initialized
 *		            -gsi_status : on GSI failures
 *		            -EFAULT: other
 */
int mhi_dma_sync_memcpy(u64 dest, u64 src, int len,
			struct mhi_dma_function_params function);

/*
 * mhi_dma_async_memcpy()- Perform asynchronous memcpy using DMA.
 *
 * @dest: physical address to store the copied data.
 * @src: physical address of the source data to copy.
 * @len: number of bytes to copy.
 * @user_cb: callback function to notify the client when the copy was done.
 * @user_param: cookie for user_cb.
 *
 * Return codes:    0: success
 *		            -EINVAL: invalid params
 *		            -EPERM: operation not permitted as dma isn't
 *		                    enable or initialized
 *		            -gsi_status : on GSI failures
 *		            -EFAULT: descr fifo is full.
 */
int mhi_dma_async_memcpy(u64 dest, u64 src, int len,
			 struct mhi_dma_function_params function,
			 void (*user_cb)(void *user1), void *user_param);

/*
 * mhi_dma_map_buffer()- Mapping of provided buffer using DMA device
 *
 * @virt: virtual address of buffer to map
 * @size: size of buffer to map
 * @dir: direction of buffer to map
 *
 * Return: physical address mapped
 */
dma_addr_t mhi_dma_map_buffer(void *virt, size_t size,
			      enum dma_data_direction dir);

/*
 * mhi_dma_unmap_buffer()- Unmap buffer
 *
 * @phys: physical address of buffer to unmap
 * @size: size of buffer to map
 * @dir: direction of buffer to unmap
 *
 * In case an unexpected buffer address would be received an error is returned.
 */
void mhi_dma_unmap_buffer(dma_addr_t phys, size_t size,
				enum dma_data_direction dir);

/*
 * mhi_dma_alloc_buffer()- Allocating using DMA device
 *
 * @size: size of buffer to allocate
 * @phys: out param returning the physical address of buffer allocated
 * @gfp: GFP flag
 *
 * Return:    Virtual address of buffer
 */
void *mhi_dma_alloc_buffer(size_t size, dma_addr_t *phys, gfp_t gfp);

/*
 * mhi_dma_free_buffer()- Free buffer
 *
 * @function: function parameters
 * @size: size of buffer to free
 * @virt: virtual address of buffer to free
 * @phys: physical address of buffer to free
 */
void mhi_dma_free_buffer(size_t size, void *virt, dma_addr_t phys);

#endif //_MHI_DMA_H_
