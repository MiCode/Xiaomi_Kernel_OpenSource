/* Copyright (c) 2012, 2014-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/platform_device.h>

#ifndef __ARCH_ARM_MACH_MSM_RPM_SMD_H
#define __ARCH_ARM_MACH_MSM_RPM_SMD_H

#define SMD_EVENT_DATA 1
#define SMD_EVENT_OPEN 2
#define SMD_EVENT_CLOSE 3
#define SMD_EVENT_STATUS 4
#define SMD_EVENT_REOPEN_READY 5

enum {
	GLINK_CONNECTED,
	GLINK_LOCAL_DISCONNECTED,
	GLINK_REMOTE_DISCONNECTED,
};

enum tx_flags {
	GLINK_TX_REQ_INTENT = 0x1,
	GLINK_TX_SINGLE_THREADED = 0x2,
	GLINK_TX_TRACER_PKT = 0x4,
	GLINK_TX_ATOMIC = 0x8,
};

enum glink_link_state {
	GLINK_LINK_STATE_UP,
	GLINK_LINK_STATE_DOWN,
};

struct glink_link_state_cb_info {
	const char *transport;
	const char *edge;
	enum glink_link_state link_state;
};

struct glink_link_info {
	const char *transport;
	const char *edge;
	void (*glink_link_state_notif_cb)(
			struct glink_link_state_cb_info *cb_info,
			void *priv);
};

struct smd_channel {
	void __iomem *send; /* some variant of smd_half_channel */
	void __iomem *recv; /* some variant of smd_half_channel */
	unsigned char *send_data;
	unsigned char *recv_data;
	unsigned int fifo_size;
	struct list_head ch_list;
	unsigned int current_packet;
	unsigned int n;
	void *priv;
	void (*notify)(void *priv, unsigned int flags);
	int (*read)(struct smd_channel *ch, void *data, int len);
	int (*write)(struct smd_channel *ch, const void *data, int len,
						bool int_ntfy);
	int (*read_avail)(struct smd_channel *ch);
	int (*write_avail)(struct smd_channel *ch);
	int (*read_from_cb)(struct smd_channel *ch, void *data, int len);
	void (*update_state)(struct smd_channel *ch);
	unsigned int last_state;
	void (*notify_other_cpu)(struct smd_channel *ch);
	void * (*read_from_fifo)(void *dest, const void *src, size_t num_bytes);
	void * (*write_to_fifo)(void *dest, const void *src, size_t num_bytes);
	char name[20];
	struct platform_device pdev;
	unsigned int type;
	int pending_pkt_sz;
	char is_pkt_ch;
	/*
	 * private internal functions to access *send and *recv.
	 * never to be exported outside of smd
	 */
	struct smd_half_channel_access *half_ch;
};

struct glink_open_config {
	void *priv;
	uint32_t options;
	const char *transport;
	const char *edge;
	const char *name;
	unsigned int rx_intent_req_timeout_ms;
	void (*notify_rx)(void *handle, const void *priv, const void *pkt_priv,
			const void *ptr, size_t size);
	void (*notify_tx_done)(void *handle, const void *priv,
			const void *pkt_priv, const void *ptr);
	void (*notify_state)(void *handle, const void *priv,
						unsigned int event);
	bool (*notify_rx_intent_req)(void *handle, const void *priv,
			size_t req_size);
	void (*notify_rxv)(void *handle, const void *priv, const void *pkt_priv,
			   void *iovec, size_t size,
			   void * (*vbuf_provider)(void *iovec, size_t offset,
						 size_t *size),
			   void * (*pbuf_provider)(void *iovec, size_t offset,
						 size_t *size));
	void (*notify_rx_sigs)(void *handle, const void *priv,
			uint32_t old_sigs, uint32_t new_sigs);
	void (*notify_rx_abort)(void *handle, const void *priv,
			const void *pkt_priv);
	void (*notify_tx_abort)(void *handle, const void *priv,
			const void *pkt_priv);
	void (*notify_rx_tracer_pkt)(void *handle, const void *priv,
			const void *pkt_priv, const void *ptr, size_t size);
	void (*notify_remote_rx_intent)(void *handle, const void *priv,
					size_t size);
};

/**
 * enum msm_rpm_set - RPM enumerations for sleep/active set
 * %MSM_RPM_CTX_SET_0: Set resource parameters for active mode.
 * %MSM_RPM_CTX_SET_SLEEP: Set resource parameters for sleep.
 */
enum msm_rpm_set {
	MSM_RPM_CTX_ACTIVE_SET,
	MSM_RPM_CTX_SLEEP_SET,
};

struct msm_rpm_request;

struct msm_rpm_kvp {
	uint32_t key;
	uint32_t length;
	uint8_t *data;
};
#ifdef CONFIG_MSM_RPM_SMD
/**
 * msm_rpm_request() - Creates a parent element to identify the
 * resource on the RPM, that stores the KVPs for different fields modified
 * for a hardware resource
 *
 * @set: if the device is setting the active/sleep set parameter
 * for the resource
 * @rsc_type: unsigned 32 bit integer that identifies the type of the resource
 * @rsc_id: unsigned 32 bit that uniquely identifies a resource within a type
 * @num_elements: number of KVPs pairs associated with the resource
 *
 * returns pointer to a msm_rpm_request on success, NULL on error
 */
struct msm_rpm_request *msm_rpm_create_request(
		enum msm_rpm_set set, uint32_t rsc_type,
		uint32_t rsc_id, int num_elements);

/**
 * msm_rpm_request_noirq() - Creates a parent element to identify the
 * resource on the RPM, that stores the KVPs for different fields modified
 * for a hardware resource. This function is similar to msm_rpm_create_request
 * except that it has to be called with interrupts masked.
 *
 * @set: if the device is setting the active/sleep set parameter
 * for the resource
 * @rsc_type: unsigned 32 bit integer that identifies the type of the resource
 * @rsc_id: unsigned 32 bit that uniquely identifies a resource within a type
 * @num_elements: number of KVPs pairs associated with the resource
 *
 * returns pointer to a msm_rpm_request on success, NULL on error
 */
struct msm_rpm_request *msm_rpm_create_request_noirq(
		enum msm_rpm_set set, uint32_t rsc_type,
		uint32_t rsc_id, int num_elements);

/**
 * msm_rpm_add_kvp_data() - Adds a Key value pair to a existing RPM resource.
 *
 * @handle: RPM resource handle to which the data should be appended
 * @key:  unsigned integer identify the parameter modified
 * @data: byte array that contains the value corresponding to key.
 * @size:   size of data in bytes.
 *
 * returns 0 on success or errno
 */
int msm_rpm_add_kvp_data(struct msm_rpm_request *handle,
		uint32_t key, const uint8_t *data, int size);

/**
 * msm_rpm_add_kvp_data_noirq() - Adds a Key value pair to a existing RPM
 * resource. This function is similar to msm_rpm_add_kvp_data except that it
 * has to be called with interrupts masked.
 *
 * @handle: RPM resource handle to which the data should be appended
 * @key:  unsigned integer identify the parameter modified
 * @data: byte array that contains the value corresponding to key.
 * @size:   size of data in bytes.
 *
 * returns 0 on success or errno
 */
int msm_rpm_add_kvp_data_noirq(struct msm_rpm_request *handle,
		uint32_t key, const uint8_t *data, int size);

/** msm_rpm_free_request() - clean up the RPM request handle created with
 * msm_rpm_create_request
 *
 * @handle: RPM resource handle to be cleared.
 */

void msm_rpm_free_request(struct msm_rpm_request *handle);

/**
 * msm_rpm_send_request() - Send the RPM messages using SMD. The function
 * assigns a message id before sending the data out to the RPM. RPM hardware
 * uses the message id to acknowledge the messages.
 *
 * @handle: pointer to the msm_rpm_request for the resource being modified.
 *
 * returns non-zero message id on success and zero on a failed transaction.
 * The drivers use message id to wait for ACK from RPM.
 */
int msm_rpm_send_request(struct msm_rpm_request *handle);

/**
 * msm_rpm_send_request_noack() - Send the RPM messages using SMD. The function
 * assigns a message id before sending the data out to the RPM. RPM hardware
 * uses the message id to acknowledge the messages, but this API does not wait
 * on the ACK for this message id and it does not add the message id to the wait
 * list.
 *
 * @handle: pointer to the msm_rpm_request for the resource being modified.
 *
 * returns NULL on success and PTR_ERR on a failed transaction.
 */
void *msm_rpm_send_request_noack(struct msm_rpm_request *handle);

/**
 * msm_rpm_send_request_noirq() - Send the RPM messages using SMD. The
 * function assigns a message id before sending the data out to the RPM.
 * RPM hardware uses the message id to acknowledge the messages. This function
 * is similar to msm_rpm_send_request except that it has to be called with
 * interrupts masked.
 *
 * @handle: pointer to the msm_rpm_request for the resource being modified.
 *
 * returns non-zero message id on success and zero on a failed transaction.
 * The drivers use message id to wait for ACK from RPM.
 */
int msm_rpm_send_request_noirq(struct msm_rpm_request *handle);

/**
 * msm_rpm_wait_for_ack() - A blocking call that waits for acknowledgment of
 * a message from RPM.
 *
 * @msg_id: the return from msm_rpm_send_requests
 *
 * returns 0 on success or errno
 */
int msm_rpm_wait_for_ack(uint32_t msg_id);

/**
 * msm_rpm_wait_for_ack_noirq() - A blocking call that waits for acknowledgment
 * of a message from RPM. This function is similar to msm_rpm_wait_for_ack
 * except that it has to be called with interrupts masked.
 *
 * @msg_id: the return from msm_rpm_send_request
 *
 * returns 0 on success or errno
 */
int msm_rpm_wait_for_ack_noirq(uint32_t msg_id);

/**
 * msm_rpm_send_message() -Wrapper function for clients to send data given an
 * array of key value pairs.
 *
 * @set: if the device is setting the active/sleep set parameter
 * for the resource
 * @rsc_type: unsigned 32 bit integer that identifies the type of the resource
 * @rsc_id: unsigned 32 bit that uniquely identifies a resource within a type
 * @kvp: array of KVP data.
 * @nelem: number of KVPs pairs associated with the message.
 *
 * returns  0 on success and errno on failure.
 */
int msm_rpm_send_message(enum msm_rpm_set set, uint32_t rsc_type,
		uint32_t rsc_id, struct msm_rpm_kvp *kvp, int nelems);

/**
 * msm_rpm_send_message_noack() -Wrapper function for clients to send data
 * given an array of key value pairs without waiting for ack.
 *
 * @set: if the device is setting the active/sleep set parameter
 * for the resource
 * @rsc_type: unsigned 32 bit integer that identifies the type of the resource
 * @rsc_id: unsigned 32 bit that uniquely identifies a resource within a type
 * @kvp: array of KVP data.
 * @nelem: number of KVPs pairs associated with the message.
 *
 * returns  NULL on success and PTR_ERR(errno) on failure.
 */
void *msm_rpm_send_message_noack(enum msm_rpm_set set, uint32_t rsc_type,
		uint32_t rsc_id, struct msm_rpm_kvp *kvp, int nelems);

/**
 * msm_rpm_send_message_noirq() -Wrapper function for clients to send data
 * given an array of key value pairs. This function is similar to the
 * msm_rpm_send_message() except that it has to be called with interrupts
 * disabled. Clients should choose the irq version when possible for system
 * performance.
 *
 * @set: if the device is setting the active/sleep set parameter
 * for the resource
 * @rsc_type: unsigned 32 bit integer that identifies the type of the resource
 * @rsc_id: unsigned 32 bit that uniquely identifies a resource within a type
 * @kvp: array of KVP data.
 * @nelem: number of KVPs pairs associated with the message.
 *
 * returns  0 on success and errno on failure.
 */
int msm_rpm_send_message_noirq(enum msm_rpm_set set, uint32_t rsc_type,
		uint32_t rsc_id, struct msm_rpm_kvp *kvp, int nelems);

/**
 * msm_rpm_driver_init() - Initialization function that registers for a
 * rpm platform driver.
 *
 * returns 0 on success.
 */
int __init msm_rpm_driver_init(void);

#else

static inline struct msm_rpm_request *msm_rpm_create_request(
		enum msm_rpm_set set, uint32_t rsc_type,
		uint32_t rsc_id, int num_elements)
{
	return NULL;
}

static inline struct msm_rpm_request *msm_rpm_create_request_noirq(
		enum msm_rpm_set set, uint32_t rsc_type,
		uint32_t rsc_id, int num_elements)
{
	return NULL;

}
static inline uint32_t msm_rpm_add_kvp_data(struct msm_rpm_request *handle,
		uint32_t key, const uint8_t *data, int count)
{
	return 0;
}
static inline uint32_t msm_rpm_add_kvp_data_noirq(
		struct msm_rpm_request *handle, uint32_t key,
		const uint8_t *data, int count)
{
	return 0;
}

static inline void msm_rpm_free_request(struct msm_rpm_request *handle)
{
}

static inline int msm_rpm_send_request(struct msm_rpm_request *handle)
{
	return 0;
}

static inline int msm_rpm_send_request_noirq(struct msm_rpm_request *handle)
{
	return 0;

}

static inline void *msm_rpm_send_request_noack(struct msm_rpm_request *handle)
{
	return NULL;
}

static inline int msm_rpm_send_message(enum msm_rpm_set set, uint32_t rsc_type,
		uint32_t rsc_id, struct msm_rpm_kvp *kvp, int nelems)
{
	return 0;
}

static inline int msm_rpm_send_message_noirq(enum msm_rpm_set set,
		uint32_t rsc_type, uint32_t rsc_id, struct msm_rpm_kvp *kvp,
		int nelems)
{
	return 0;
}

static inline void *msm_rpm_send_message_noack(enum msm_rpm_set set,
		uint32_t rsc_type, uint32_t rsc_id, struct msm_rpm_kvp *kvp,
		int nelems)
{
	return NULL;
}

static inline int msm_rpm_wait_for_ack(uint32_t msg_id)
{
	return 0;

}
static inline int msm_rpm_wait_for_ack_noirq(uint32_t msg_id)
{
	return 0;
}

static inline int __init msm_rpm_driver_init(void)
{
	return 0;
}
#endif

static inline int glink_rpm_rx_poll(void *handle)
{
	return -ENODEV;
}

static inline int smd_is_pkt_avail(struct smd_channel *ch)
{
	return -ENODEV;
}

static inline int smd_cur_packet_size(struct smd_channel *ch)
{
	return -ENODEV;
}

static inline int smd_read_avail(struct smd_channel *ch)
{
	return -ENODEV;
}

static inline int smd_read(struct smd_channel *ch, void *data, int len)
{
	return -ENODEV;
}

static inline int smd_write_avail(struct smd_channel *ch)
{
	return -ENODEV;
}
static inline int smd_write(struct smd_channel *ch, const void *data, int len)
{
	return -ENODEV;
}

static inline int glink_tx(void *handle, void *pkt_priv, void *data,
					size_t size, uint32_t tx_flags)
{
	return -ENODEV;
}

static inline int smd_mask_receive_interrupt(struct smd_channel *ch, bool mask,
		const struct cpumask *cpumask)
{
	return -ENODEV;
}

static inline int glink_rpm_mask_rx_interrupt(void *handle, bool mask,
		void *pstruct)
{
	return -ENODEV;
}

static inline int glink_rx_done(void *handle, const void *ptr, bool reuse)
{
	return -ENODEV;
}
static inline void *glink_open(const struct glink_open_config *cfg_ptr)
{
	return NULL;
}

static inline void *glink_register_link_state_cb(
				struct glink_link_info *link_info, void *priv)
{
	return NULL;
}

static inline int smd_named_open_on_edge(const char *name, uint32_t edge,
				struct smd_channel **_ch, void *priv,
				void (*notify)(void *, unsigned int))
{
	return -ENODEV;
}

static inline void smd_disable_read_intr(struct smd_channel *ch)
{
}

#endif /*__ARCH_ARM_MACH_MSM_RPM_SMD_H*/
