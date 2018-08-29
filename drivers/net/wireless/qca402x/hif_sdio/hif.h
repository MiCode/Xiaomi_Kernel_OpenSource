/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

/* This file was originally distributed by Qualcomm Atheros, Inc.
 * before Copyright ownership was assigned to the Linux Foundation.
 */

#ifndef _HIF_H_
#define _HIF_H_

#define DEBUG
#undef DEBUG

#define HIF_OK 0
#define HIF_PENDING 1
#define HIF_ERROR 2
#define HIF_EINVAL 3

/* direction - Direction of transfer (HIF_READ/HIF_WRITE). */
#define HIF_READ 0x00000001
#define HIF_WRITE 0x00000002
#define HIF_DIR_MASK (HIF_READ | HIF_WRITE)

/* type - An interface may support different kind of read/write commands.
 * For example: SDIO supports CMD52/CMD53s. In case of MSIO it
 * translates to using different kinds of TPCs. The command type
 * is thus divided into a basic and an extended command and can
 * be specified using HIF_BASIC_IO/HIF_EXTENDED_IO.
 */
#define HIF_BASIC_IO 0x00000004
#define HIF_EXTENDED_IO 0x00000008
#define HIF_TYPE_MASK (HIF_BASIC_IO | HIF_EXTENDED_IO)

/* emode - This indicates the whether the command is to be executed in a
 * blocking or non-blocking fashion (HIF_SYNCHRONOUS/
 * HIF_ASYNCHRONOUS). The read/write data paths in HTCA have been
 * implemented using the asynchronous mode allowing the the bus
 * driver to indicate the completion of operation through the
 * registered callback routine. The requirement primarily comes
 * from the contexts these operations get called from (a driver's
 * transmit context or the ISR context in case of receive).
 * Support for both of these modes is essential.
 */
#define HIF_SYNCHRONOUS 0x00000010
#define HIF_ASYNCHRONOUS 0x00000020
#define HIF_EMODE_MASK (HIF_SYNCHRONOUS | HIF_ASYNCHRONOUS)

/* dmode - An interface may support different kinds of commands based on
 * the tradeoff between the amount of data it can carry and the
 * setup time. Byte and Block modes are supported (HIF_BYTE_BASIS/
 * HIF_BLOCK_BASIS). In case of latter, the data is rounded off
 * to the nearest block size by padding. The size of the block is
 * configurable at compile time using the HIF_BLOCK_SIZE and is
 * negotiated with the target during initialization after the
 * AR6000 interrupts are enabled.
 */
#define HIF_BYTE_BASIS 0x00000040
#define HIF_BLOCK_BASIS 0x00000080
#define HIF_DMODE_MASK (HIF_BYTE_BASIS | HIF_BLOCK_BASIS)

/* amode - This indicates if the address has to be incremented on AR6000
 * after every read/write operation (HIF?FIXED_ADDRESS/
 * HIF_INCREMENTAL_ADDRESS).
 */
#define HIF_FIXED_ADDRESS 0x00000100
#define HIF_INCREMENTAL_ADDRESS 0x00000200
#define HIF_AMODE_MASK (HIF_FIXED_ADDRESS | HIF_INCREMENTAL_ADDRESS)

#define HIF_WR_ASYNC_BYTE_FIX \
	(HIF_WRITE | HIF_ASYNCHRONOUS | HIF_EXTENDED_IO | HIF_BYTE_BASIS | \
	 HIF_FIXED_ADDRESS)
#define HIF_WR_ASYNC_BYTE_INC \
	(HIF_WRITE | HIF_ASYNCHRONOUS | HIF_EXTENDED_IO | HIF_BYTE_BASIS | \
	 HIF_INCREMENTAL_ADDRESS)
#define HIF_WR_ASYNC_BLOCK_INC \
	(HIF_WRITE | HIF_ASYNCHRONOUS | HIF_EXTENDED_IO | HIF_BLOCK_BASIS | \
	 HIF_INCREMENTAL_ADDRESS)
#define HIF_WR_SYNC_BYTE_FIX \
	(HIF_WRITE | HIF_SYNCHRONOUS | HIF_EXTENDED_IO | HIF_BYTE_BASIS | \
	 HIF_FIXED_ADDRESS)
#define HIF_WR_SYNC_BYTE_INC \
	(HIF_WRITE | HIF_SYNCHRONOUS | HIF_EXTENDED_IO | HIF_BYTE_BASIS | \
	 HIF_INCREMENTAL_ADDRESS)
#define HIF_WR_SYNC_BLOCK_INC \
	(HIF_WRITE | HIF_SYNCHRONOUS | HIF_EXTENDED_IO | HIF_BLOCK_BASIS | \
	 HIF_INCREMENTAL_ADDRESS)
#define HIF_WR_ASYNC_BLOCK_FIX \
	(HIF_WRITE | HIF_ASYNCHRONOUS | HIF_EXTENDED_IO | HIF_BLOCK_BASIS | \
	 HIF_FIXED_ADDRESS)
#define HIF_WR_SYNC_BLOCK_FIX \
	(HIF_WRITE | HIF_SYNCHRONOUS | HIF_EXTENDED_IO | HIF_BLOCK_BASIS | \
	 HIF_FIXED_ADDRESS)
#define HIF_RD_SYNC_BYTE_INC \
	(HIF_READ | HIF_SYNCHRONOUS | HIF_EXTENDED_IO | HIF_BYTE_BASIS | \
	 HIF_INCREMENTAL_ADDRESS)
#define HIF_RD_SYNC_BYTE_FIX \
	(HIF_READ | HIF_SYNCHRONOUS | HIF_EXTENDED_IO | HIF_BYTE_BASIS | \
	 HIF_FIXED_ADDRESS)
#define HIF_RD_ASYNC_BYTE_FIX \
	(HIF_READ | HIF_ASYNCHRONOUS | HIF_EXTENDED_IO | HIF_BYTE_BASIS | \
	 HIF_FIXED_ADDRESS)
#define HIF_RD_ASYNC_BLOCK_FIX \
	(HIF_READ | HIF_ASYNCHRONOUS | HIF_EXTENDED_IO | HIF_BLOCK_BASIS | \
	 HIF_FIXED_ADDRESS)
#define HIF_RD_ASYNC_BYTE_INC \
	(HIF_READ | HIF_ASYNCHRONOUS | HIF_EXTENDED_IO | HIF_BYTE_BASIS | \
	 HIF_INCREMENTAL_ADDRESS)
#define HIF_RD_ASYNC_BLOCK_INC \
	(HIF_READ | HIF_ASYNCHRONOUS | HIF_EXTENDED_IO | HIF_BLOCK_BASIS | \
	 HIF_INCREMENTAL_ADDRESS)
#define HIF_RD_SYNC_BLOCK_INC \
	(HIF_READ | HIF_SYNCHRONOUS | HIF_EXTENDED_IO | HIF_BLOCK_BASIS | \
	 HIF_INCREMENTAL_ADDRESS)
#define HIF_RD_SYNC_BLOCK_FIX \
	(HIF_READ | HIF_SYNCHRONOUS | HIF_EXTENDED_IO | HIF_BLOCK_BASIS | \
	 HIF_FIXED_ADDRESS)

enum hif_device_config_opcode {
	HIF_DEVICE_GET_MBOX_BLOCK_SIZE,
	HIF_DEVICE_SET_CONTEXT,
	HIF_DEVICE_GET_CONTEXT,
};

/* HIF CONFIGURE definitions:
 *
 * HIF_DEVICE_GET_MBOX_BLOCK_SIZE
 * input : none
 * output : array of 4 u32s
 * notes: block size is returned for each mailbox (4)
 *
 * HIF_DEVICE_SET_CONTEXT
 * input : arbitrary pointer-sized value
 * output: none
 * notes: stores an arbitrary value which can be retrieved later
 *
 * HIF_DEVICE_GET_CONTEXT
 * input: none
 * output : arbitrary pointer-sized value
 * notes: retrieves an arbitrary value which was set earlier
 */
struct cbs_from_hif {
	void *context; /* context to pass to the dsrhandler
			* note : rw_completion_hdl is provided the context
			* passed to hif_read_write
			*/
	int (*rw_completion_hdl)(void *rw_context, int status);
	int (*dsr_hdl)(void *context);
};

struct cbs_from_os {
	void *context; /* context to pass for all callbacks except
			* dev_removed_hdl the dev_removed_hdl is only called if
			* the device is claimed
			*/
	int (*dev_inserted_hdl)(void *context, void *hif_handle);
	int (*dev_removed_hdl)(void *claimed_context, void *hif_handle);
	int (*dev_suspend_hdl)(void *context);
	int (*dev_resume_hdl)(void *context);
	int (*dev_wakeup_hdl)(void *context);
#if defined(DEVICE_POWER_CHANGE)
	int (*dev_pwr_change_hdl)(void *context,
				  HIF_DEVICE_POWER_CHANGE_TYPE config);
#endif /* DEVICE_POWER_CHANGE */
};

/* other interrupts (non-Recv) are pending, host
 * needs to read the register table to figure out what
 */
#define HIF_OTHER_EVENTS BIT(0)

#define HIF_RECV_MSG_AVAIL BIT(1) /* pending recv packet */

struct hif_pending_events_info {
	u32 events;
	u32 look_ahead;
	u32 available_recv_bytes;
};

/* function to get pending events , some HIF modules use special mechanisms
 * to detect packet available and other interrupts
 */
typedef int (*HIF_PENDING_EVENTS_FUNC)(void *device,
				       struct hif_pending_events_info *p_events,
				       void *async_context);

#define HIF_MASK_RECV TRUE
#define HIF_UNMASK_RECV FALSE
/* function to mask recv events */
typedef int (*HIF_MASK_UNMASK_RECV_EVENT)(void *device, bool mask,
					  void *async_context);

#ifdef HIF_MBOX_SLEEP_WAR
/* This API is used to update the target sleep state */
void hif_set_mbox_sleep(void *device, bool sleep, bool wait,
			bool cache);
#endif
/* This API is used to perform any global initialization of the HIF layer
 * and to set OS driver callbacks (i.e. insertion/removal) to the HIF layer
 */
int hif_init(struct cbs_from_os *callbacks);

/* This API claims the HIF device and provides a context for handling removal.
 * The device removal callback is only called when the OS claims
 * a device.  The claimed context must be non-NULL
 */
void hif_claim_device(void *device, void *claimed_context);

/* release the claimed device */
void hif_release_device(void *device);

/* This API allows the calling layer to attach callbacks from HIF */
int hif_attach(void *device, struct cbs_from_hif *callbacks);

/* This API allows the calling layer to detach callbacks from HIF */
void hif_detach(void *device);

void hif_set_handle(void *hif_handle, void *handle);

int hif_sync_read(void *device, u32 address, u8 *buffer,
		  u32 length, u32 request, void *context);

size_t hif_get_device_size(void);

/* This API is used to provide the read/write interface over the specific bus
 * interface.
 * address - Starting address in the AR6000's address space. For mailbox
 * writes, it refers to the start of the mbox boundary. It should
 * be ensured that the last byte falls on the mailbox's EOM. For
 * mailbox reads, it refers to the end of the mbox boundary.
 * buffer - Pointer to the buffer containg the data to be transmitted or
 * received.
 * length - Amount of data to be transmitted or received.
 * request - Characterizes the attributes of the command.
 */
int hif_read_write(void *device, u32 address, void *buffer,
		   u32 length, u32 request, void *context);

/* This can be initiated from the unload driver context when the OS has no more
 * use for
 * the device.
 */
void hif_shutdown_device(void *device);
void hif_surprise_removed(void *device);

void hif_mask_interrupt(void *device);

void hif_un_mask_interrupt(void *device);

int hif_configure_device(void *device,
			 enum hif_device_config_opcode opcode,
			 void *config, u32 config_len);

/* This API wait for the remaining MBOX messages to be drained
 * This should be moved to HTCA AR6K layer
 */
int hif_wait_for_pending_recv(void *device);

/* BMI and Diag window abstraction
 */

#define HIF_BMI_EXCHANGE_NO_TIMEOUT ((u32)(0))

#define DIAG_TRANSFER_LIMIT 2048U /* maximum number of bytes that can be handled
				   * atomically by DiagRead/DiagWrite
				   */

#ifdef FEATURE_RUNTIME_PM
/* Runtime power management API of HIF to control
 * runtime pm. During Runtime Suspend the get API
 * return -EAGAIN. The caller can queue the cmd or return.
 * The put API decrements the usage count.
 * The get API increments the usage count.
 * The API's are exposed to HTT and WMI Services only.
 */
int hif_pm_runtime_get(void *device);
int hif_pm_runtime_put(void *device);
void *hif_runtime_pm_prevent_suspend_init(const char *name);
void hif_runtime_pm_prevent_suspend_deinit(void *data);
int hif_pm_runtime_prevent_suspend(void *ol_sc, void *data);
int hif_pm_runtime_allow_suspend(void *ol_sc, void *data);
int hif_pm_runtime_prevent_suspend_timeout(void *ol_sc, void *data,
					   unsigned int delay);
void hif_request_runtime_pm_resume(void *ol_sc);
#else
static inline int hif_pm_runtime_get(void *device)
{
	return 0;
}

static inline int hif_pm_runtime_put(void *device)
{
	return 0;
}

static inline int hif_pm_runtime_prevent_suspend(void *ol_sc, void *context)
{
	return 0;
}

static inline int hif_pm_runtime_allow_suspend(void *ol_sc, void *context)
{
	return 0;
}

static inline int hif_pm_runtime_prevent_suspend_timeout(void *ol_sc,
							 void *context,
							 unsigned int msec)
{
	return 0;
}

static inline void *hif_runtime_pm_prevent_suspend_init(const char *name)
{
	return NULL;
}

static inline void hif_runtime_pm_prevent_suspend_deinit(void *context)
{
}

static inline void hif_request_runtime_pm_resume(void *ol_sc)
{
}
#endif

#endif /* _HIF_H_ */
