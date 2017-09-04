/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
#ifndef __HAB_H
#define __HAB_H

#define pr_fmt(fmt) "hab: " fmt

#include <linux/types.h>

#include <linux/habmm.h>

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/rbtree.h>
#include <linux/idr.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/dma-direction.h>
#include <linux/dma-mapping.h>

enum hab_payload_type {
	HAB_PAYLOAD_TYPE_MSG = 0x0,
	HAB_PAYLOAD_TYPE_INIT,
	HAB_PAYLOAD_TYPE_INIT_ACK,
	HAB_PAYLOAD_TYPE_ACK,
	HAB_PAYLOAD_TYPE_EXPORT,
	HAB_PAYLOAD_TYPE_EXPORT_ACK,
	HAB_PAYLOAD_TYPE_PROFILE,
	HAB_PAYLOAD_TYPE_CLOSE,
};
#define LOOPBACK_DOM 0xFF

/*
 * Tuning required. If there are multiple clients, the aging of previous
 * "request" might be discarded
 */
#define Q_AGE_THRESHOLD  1000000

/* match the name to dtsi if for real HYP framework */
#define DEVICE_AUD1_NAME "hab_aud1"
#define DEVICE_AUD2_NAME "hab_aud2"
#define DEVICE_AUD3_NAME "hab_aud3"
#define DEVICE_AUD4_NAME "hab_aud4"
#define DEVICE_CAM_NAME "hab_cam"
#define DEVICE_DISP1_NAME "hab_disp1"
#define DEVICE_DISP2_NAME "hab_disp2"
#define DEVICE_DISP3_NAME "hab_disp3"
#define DEVICE_DISP4_NAME "hab_disp4"
#define DEVICE_DISP5_NAME "hab_disp5"
#define DEVICE_GFX_NAME "hab_ogles"
#define DEVICE_VID_NAME "hab_vid"
#define DEVICE_MISC_NAME "hab_misc"
#define DEVICE_QCPE1_NAME "hab_qcpe_vm1"
#define DEVICE_QCPE2_NAME "hab_qcpe_vm2"
#define DEVICE_QCPE3_NAME "hab_qcpe_vm3"
#define DEVICE_QCPE4_NAME "hab_qcpe_vm4"

/* "Size" of the HAB_HEADER_ID and HAB_VCID_ID must match */
#define HAB_HEADER_SIZE_SHIFT 0
#define HAB_HEADER_TYPE_SHIFT 16
#define HAB_HEADER_ID_SHIFT 24
#define HAB_HEADER_SIZE_MASK 0x0000FFFF
#define HAB_HEADER_TYPE_MASK 0x00FF0000
#define HAB_HEADER_ID_MASK   0xFF000000
#define HAB_HEADER_INITIALIZER {0}

#define HAB_MMID_GET_MAJOR(mmid) (mmid & 0xFFFF)
#define HAB_MMID_GET_MINOR(mmid) ((mmid>>16) & 0xFF)

#define HAB_VCID_ID_SHIFT 0
#define HAB_VCID_DOMID_SHIFT 8
#define HAB_VCID_MMID_SHIFT 16
#define HAB_VCID_ID_MASK 0x000000FF
#define HAB_VCID_DOMID_MASK 0x0000FF00
#define HAB_VCID_MMID_MASK 0xFFFF0000
#define HAB_VCID_GET_ID(vcid) \
	(((vcid) & HAB_VCID_ID_MASK) >> HAB_VCID_ID_SHIFT)

#define HAB_HEADER_SET_SIZE(header, size) \
	((header).info = (((header).info) & (~HAB_HEADER_SIZE_MASK)) | \
		(((size) << HAB_HEADER_SIZE_SHIFT) & HAB_HEADER_SIZE_MASK))

#define HAB_HEADER_SET_TYPE(header, type) \
	((header).info = (((header).info) & (~HAB_HEADER_TYPE_MASK)) | \
		(((type) << HAB_HEADER_TYPE_SHIFT) & HAB_HEADER_TYPE_MASK))

#define HAB_HEADER_SET_ID(header, id) \
	((header).info = (((header).info) & (~HAB_HEADER_ID_MASK)) | \
		((HAB_VCID_GET_ID(id) << HAB_HEADER_ID_SHIFT) \
		& HAB_HEADER_ID_MASK))

#define HAB_HEADER_GET_SIZE(header) \
	((((header).info) & HAB_HEADER_SIZE_MASK) >> HAB_HEADER_SIZE_SHIFT)

#define HAB_HEADER_GET_TYPE(header) \
	((((header).info) & HAB_HEADER_TYPE_MASK) >> HAB_HEADER_TYPE_SHIFT)

#define HAB_HEADER_GET_ID(header) \
	(((((header).info) & HAB_HEADER_ID_MASK) >> \
	(HAB_HEADER_ID_SHIFT - HAB_VCID_ID_SHIFT)) & HAB_VCID_ID_MASK)

struct hab_header {
	uint32_t info;
};

struct physical_channel {
	struct kref refcount;
	struct hab_device *habdev;
	struct list_head node;
	struct idr vchan_idr;
	spinlock_t vid_lock;

	struct idr expid_idr;
	spinlock_t expid_lock;

	void *hyp_data;
	int dom_id;
	int closed;

	spinlock_t rxbuf_lock;
};

struct hab_open_send_data {
	int vchan_id;
	int sub_id;
	int open_id;
};

struct hab_open_request {
	int type;
	struct physical_channel *pchan;
	int vchan_id;
	int sub_id;
	int open_id;
};

struct hab_open_node {
	struct hab_open_request request;
	struct list_head node;
	int age;
};

struct hab_export_ack {
	uint32_t export_id;
	int32_t vcid_local;
	int32_t vcid_remote;
};

struct hab_export_ack_recvd {
	struct hab_export_ack ack;
	struct list_head node;
	int age;
};

struct hab_message {
	size_t sizebytes;
	struct list_head node;
	uint32_t data[];
};

struct hab_device {
	const char *name;
	unsigned int id;
	struct list_head pchannels;
	struct mutex pchan_lock;
	struct list_head openq_list;
	spinlock_t openlock;
	wait_queue_head_t openq;
};

struct uhab_context {
	struct kref refcount;
	struct list_head vchannels;

	struct list_head exp_whse;
	uint32_t export_total;

	wait_queue_head_t exp_wq;
	struct list_head exp_rxq;
	rwlock_t exp_lock;
	spinlock_t expq_lock;

	struct list_head imp_whse;
	spinlock_t imp_lock;
	uint32_t import_total;

	void *import_ctx;

	rwlock_t ctx_lock;
	int closing;
	int kernel;
};

struct hab_driver {
	struct device *dev;
	struct cdev cdev;
	dev_t major;
	struct class *class;
	int irq;

	int ndevices;
	struct hab_device *devp;
	struct uhab_context *kctx;
	int b_server_dom;
	int loopback_num;
	int b_loopback;
};

struct virtual_channel {
	struct work_struct work;
	/*
	 * refcount is used to track the references from hab core to the virtual
	 * channel such as references from physical channels,
	 * i.e. references from the "other" side
	 */
	struct kref refcount;
	/*
	 * usagecnt is used to track the clients who are using this virtual
	 * channel such as local clients, client sowftware etc,
	 * i.e. references from "this" side
	 */
	struct kref usagecnt;
	struct physical_channel *pchan;
	struct uhab_context *ctx;
	struct list_head node;
	struct list_head rx_list;
	wait_queue_head_t rx_queue;
	spinlock_t rx_lock;
	int id;
	int otherend_id;
	int otherend_closed;
};

/*
 * Struct shared between local and remote, contents are composed by exporter,
 * the importer only writes to pdata and local (exporter) domID
 */
struct export_desc {
	uint32_t  export_id;
	int       readonly;
	uint64_t  import_index;

	struct virtual_channel *vchan;

	int32_t             vcid_local;
	int32_t             vcid_remote;
	int                 domid_local;
	int                 domid_remote;

	struct list_head    node;
	void *kva;
	int                 payload_count;
	unsigned char       payload[1];
};

int hab_vchan_open(struct uhab_context *ctx,
		unsigned int mmid, int32_t *vcid, uint32_t flags);
void hab_vchan_close(struct uhab_context *ctx,
		int32_t vcid);
long hab_vchan_send(struct uhab_context *ctx,
		int vcid,
		size_t sizebytes,
		void *data,
		unsigned int flags);
struct hab_message *hab_vchan_recv(struct uhab_context *ctx,
				int vcid,
				unsigned int flags);
void hab_vchan_stop(struct virtual_channel *vchan);
void hab_vchan_stop_notify(struct virtual_channel *vchan);

int hab_mem_export(struct uhab_context *ctx,
		struct hab_export *param, int kernel);
int hab_mem_import(struct uhab_context *ctx,
		struct hab_import *param, int kernel);
int hab_mem_unexport(struct uhab_context *ctx,
		struct hab_unexport *param, int kernel);
int hab_mem_unimport(struct uhab_context *ctx,
		struct hab_unimport *param, int kernel);

void habmem_remove_export(struct export_desc *exp);

/* memory hypervisor framework plugin I/F */
void *habmm_hyp_allocate_grantable(int page_count,
		uint32_t *sizebytes);

int habmem_hyp_grant_user(unsigned long address,
		int page_count,
		int flags,
		int remotedom,
		void *ppdata);

int habmem_hyp_grant(unsigned long address,
		int page_count,
		int flags,
		int remotedom,
		void *ppdata);

int habmem_hyp_revoke(void *expdata, uint32_t count);

void *habmem_imp_hyp_open(void);
void habmem_imp_hyp_close(void *priv, int kernel);

long habmem_imp_hyp_map(void *priv, void *impdata, uint32_t count,
		uint32_t remotedom,
		uint64_t *index,
		void **pkva,
		int kernel,
		uint32_t userflags);

long habmm_imp_hyp_unmap(void *priv, uint64_t index,
		uint32_t count,
		int kernel);

int habmem_imp_hyp_mmap(struct file *flip, struct vm_area_struct *vma);



void hab_msg_free(struct hab_message *message);
struct hab_message *hab_msg_dequeue(struct virtual_channel *vchan,
		int wait_flag);

void hab_msg_recv(struct physical_channel *pchan,
		struct hab_header *header);

void hab_open_request_init(struct hab_open_request *request,
		int type,
		struct physical_channel *pchan,
		int vchan_id,
		int sub_id,
		int open_id);
int hab_open_request_send(struct hab_open_request *request);
int hab_open_request_add(struct physical_channel *pchan,
		struct hab_header *header);
void hab_open_request_free(struct hab_open_request *request);
int hab_open_listen(struct uhab_context *ctx,
		struct hab_device *dev,
		struct hab_open_request *listen,
		struct hab_open_request **recv_request,
		int ms_timeout);

struct virtual_channel *hab_vchan_alloc(struct uhab_context *ctx,
		struct physical_channel *pchan);
struct virtual_channel *hab_vchan_get(struct physical_channel *pchan,
		uint32_t vchan_id);
void hab_vchan_put(struct virtual_channel *vchan);

struct virtual_channel *hab_get_vchan_fromvcid(int32_t vcid,
		struct uhab_context *ctx);
struct physical_channel *hab_pchan_alloc(struct hab_device *habdev,
		int otherend_id);
struct physical_channel *hab_pchan_find_domid(struct hab_device *dev,
		int dom_id);
int hab_vchan_find_domid(struct virtual_channel *vchan);

void hab_pchan_get(struct physical_channel *pchan);
void hab_pchan_put(struct physical_channel *pchan);

struct uhab_context *hab_ctx_alloc(int kernel);

void hab_ctx_free(struct kref *ref);

static inline void hab_ctx_get(struct uhab_context *ctx)
{
	if (ctx)
		kref_get(&ctx->refcount);
}

static inline void hab_ctx_put(struct uhab_context *ctx)
{
	if (ctx)
		kref_put(&ctx->refcount, hab_ctx_free);
}

void hab_send_close_msg(struct virtual_channel *vchan);
int hab_hypervisor_register(void);
void hab_hypervisor_unregister(void);

int physical_channel_read(struct physical_channel *pchan,
		void *payload,
		size_t read_size);

int physical_channel_send(struct physical_channel *pchan,
		struct hab_header *header,
		void *payload);

void physical_channel_rx_dispatch(unsigned long physical_channel);

int loopback_pchan_create(char *dev_name);

bool hab_is_loopback(void);

/* Global singleton HAB instance */
extern struct hab_driver hab_driver;

#endif /* __HAB_H */
