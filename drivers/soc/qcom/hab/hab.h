/* Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "hab:%s:%d " fmt, __func__, __LINE__

#include <linux/types.h>

#include <linux/habmm.h>
#include <linux/hab_ioctl.h>

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
#include <linux/jiffies.h>
#include <linux/reboot.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <soc/qcom/boot_stats.h>

enum hab_payload_type {
	HAB_PAYLOAD_TYPE_MSG = 0x0,
	HAB_PAYLOAD_TYPE_INIT,
	HAB_PAYLOAD_TYPE_INIT_ACK,
	HAB_PAYLOAD_TYPE_INIT_DONE,
	HAB_PAYLOAD_TYPE_EXPORT,
	HAB_PAYLOAD_TYPE_EXPORT_ACK,
	HAB_PAYLOAD_TYPE_PROFILE,
	HAB_PAYLOAD_TYPE_CLOSE,
	HAB_PAYLOAD_TYPE_INIT_CANCEL,
	HAB_PAYLOAD_TYPE_SCHE_MSG,
	HAB_PAYLOAD_TYPE_SCHE_MSG_ACK,
	HAB_PAYLOAD_TYPE_SCHE_RESULT_REQ,
	HAB_PAYLOAD_TYPE_SCHE_RESULT_RSP,
	HAB_PAYLOAD_TYPE_MAX,
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
#define DEVICE_CAM1_NAME "hab_cam1"
#define DEVICE_CAM2_NAME "hab_cam2"
#define DEVICE_DISP1_NAME "hab_disp1"
#define DEVICE_DISP2_NAME "hab_disp2"
#define DEVICE_DISP3_NAME "hab_disp3"
#define DEVICE_DISP4_NAME "hab_disp4"
#define DEVICE_DISP5_NAME "hab_disp5"
#define DEVICE_GFX_NAME "hab_ogles"
#define DEVICE_VID_NAME "hab_vid"
#define DEVICE_VID2_NAME "hab_vid2"
#define DEVICE_MISC_NAME "hab_misc"
#define DEVICE_QCPE1_NAME "hab_qcpe_vm1"
#define DEVICE_CLK1_NAME "hab_clock_vm1"
#define DEVICE_CLK2_NAME "hab_clock_vm2"
#define DEVICE_FDE1_NAME "hab_fde1"
#define DEVICE_BUFFERQ1_NAME "hab_bufferq1"

/* make sure concascaded name is less than this value */
#define MAX_VMID_NAME_SIZE 30

#define HABCFG_FILE_SIZE_MAX   256
#define HABCFG_MMID_AREA_MAX   (MM_ID_MAX/100)

#define HABCFG_VMID_MAX        16
#define HABCFG_VMID_INVALID    (-1)
#define HABCFG_VMID_DONT_CARE  (-2)

#define HABCFG_ID_LINE_LIMIT   ","
#define HABCFG_ID_VMID         "VMID="
#define HABCFG_ID_BE           "BE="
#define HABCFG_ID_FE           "FE="
#define HABCFG_ID_MMID         "MMID="
#define HABCFG_ID_RANGE        "-"
#define HABCFG_ID_DONTCARE     "X"

#define HABCFG_FOUND_VMID      1
#define HABCFG_FOUND_FE_MMIDS  2
#define HABCFG_FOUND_BE_MMIDS  3
#define HABCFG_FOUND_NOTHING   (-1)

#define HABCFG_BE_FALSE        0
#define HABCFG_BE_TRUE         1

#define HABCFG_GET_VMID(_local_cfg_, _vmid_) \
	((settings)->vmid_mmid_list[_vmid_].vmid)
#define HABCFG_GET_MMID(_local_cfg_, _vmid_, _mmid_) \
	((settings)->vmid_mmid_list[_vmid_].mmid[_mmid_])
#define HABCFG_GET_BE(_local_cfg_, _vmid_, _mmid_) \
	((settings)->vmid_mmid_list[_vmid_].is_listener[_mmid_])

struct hab_header {
	uint32_t id_type_size;
	uint32_t session_id;
	uint32_t signature;
	uint32_t sequence;
} __packed;

/* "Size" of the HAB_HEADER_ID and HAB_VCID_ID must match */
#define HAB_HEADER_SIZE_SHIFT 0
#define HAB_HEADER_TYPE_SHIFT 16
#define HAB_HEADER_ID_SHIFT 20
#define HAB_HEADER_SIZE_MASK 0x0000FFFF
#define HAB_HEADER_TYPE_MASK 0x000F0000
#define HAB_HEADER_ID_MASK   0xFFF00000
#define HAB_HEADER_INITIALIZER {0}

#define HAB_MMID_GET_MAJOR(mmid) (mmid & 0xFFFF)
#define HAB_MMID_GET_MINOR(mmid) ((mmid>>16) & 0xFF)

#define HAB_VCID_ID_SHIFT 0
#define HAB_VCID_DOMID_SHIFT 12
#define HAB_VCID_MMID_SHIFT 20
#define HAB_VCID_ID_MASK 0x00000FFF
#define HAB_VCID_DOMID_MASK 0x000FF000
#define HAB_VCID_MMID_MASK 0xFFF00000
#define HAB_VCID_GET_ID(vcid) \
	(((vcid) & HAB_VCID_ID_MASK) >> HAB_VCID_ID_SHIFT)


#define HAB_HEADER_SET_SESSION_ID(header, sid) \
	((header).session_id = (sid))

#define HAB_HEADER_SET_SIZE(header, size) \
	((header).id_type_size = ((header).id_type_size & \
			(uint32_t)(~HAB_HEADER_SIZE_MASK)) |	\
			(((size) << HAB_HEADER_SIZE_SHIFT) & \
			HAB_HEADER_SIZE_MASK))

#define HAB_HEADER_SET_TYPE(header, type) \
	((header).id_type_size = ((header).id_type_size & \
			(uint32_t)(~HAB_HEADER_TYPE_MASK)) | \
			(((type) << HAB_HEADER_TYPE_SHIFT) & \
			HAB_HEADER_TYPE_MASK))

#define HAB_HEADER_SET_ID(header, id) \
	((header).id_type_size = ((header).id_type_size & \
			(~HAB_HEADER_ID_MASK)) | \
			((HAB_VCID_GET_ID(id) << HAB_HEADER_ID_SHIFT) & \
			HAB_HEADER_ID_MASK))

#define HAB_HEADER_GET_SIZE(header) \
	(((header).id_type_size & \
		HAB_HEADER_SIZE_MASK) >> HAB_HEADER_SIZE_SHIFT)

#define HAB_HEADER_GET_TYPE(header) \
	(((header).id_type_size & \
		HAB_HEADER_TYPE_MASK) >> HAB_HEADER_TYPE_SHIFT)

#define HAB_HEADER_GET_ID(header) \
	((((header).id_type_size & HAB_HEADER_ID_MASK) >> \
	(HAB_HEADER_ID_SHIFT - HAB_VCID_ID_SHIFT)) & HAB_VCID_ID_MASK)

#define HAB_HEADER_GET_SESSION_ID(header) ((header).session_id)

#define HAB_HS_TIMEOUT (10*1000*1000)
#define HAB_HS_INIT_DONE_TIMEOUT (3*1000)

struct physical_channel {
	struct list_head node;
	char name[MAX_VMID_NAME_SIZE];
	int is_be;
	struct kref refcount;
	struct hab_device *habdev;
	struct idr vchan_idr;
	spinlock_t vid_lock;

	struct idr expid_idr;
	spinlock_t expid_lock;

	void *hyp_data;
	int dom_id; /* BE role: remote vmid; FE role: don't care */
	int vmid_local; /* from DT or hab_config */
	int vmid_remote;
	char vmname_local[12]; /* from DT */
	char vmname_remote[12];
	int closed;

	spinlock_t rxbuf_lock;

	/* debug only */
	uint32_t sequence_tx;
	uint32_t sequence_rx;
	uint32_t status;

	/* vchans on this pchan */
	struct list_head vchannels;
	int vcnt;
	rwlock_t vchans_lock;
};
/* this payload has to be used together with type */
struct hab_open_send_data {
	int vchan_id;
	int sub_id;
	int open_id;
	int ver_fe;
	int ver_be;
	int reserved;
};

struct hab_open_request {
	int type;
	struct physical_channel *pchan;
	struct hab_open_send_data xdata;
};

struct hab_open_node {
	struct hab_open_request request;
	struct list_head node;
	int64_t age; /* sec */
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
	struct list_head node;
	size_t sizebytes;
	uint32_t data[];
};

struct hab_forbidden_node {
	struct list_head node;
	uint32_t mmid;
};

/* for all the pchans of same kind */
struct hab_device {
	char name[MAX_VMID_NAME_SIZE];
	uint32_t id;
	struct list_head pchannels;
	int pchan_cnt;
	spinlock_t pchan_lock;
	struct list_head openq_list; /* received */
	spinlock_t openlock;
	wait_queue_head_t openq;
	int openq_cnt;
};

struct uhab_context {
	struct list_head node; /* managed by the driver */
	struct kref refcount;

	struct list_head vchannels;
	int vcnt;

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

	struct list_head pending_open; /* sent to remote */
	int pending_cnt;

	struct list_head forbidden_chans;
	spinlock_t forbidden_lock;

	rwlock_t ctx_lock;
	int closing;
	int kernel;
	int owner;

	int lb_be; /* loopback only */
};

/*
 * array to describe the VM and its MMID configuration as
 * what is connected to so this is describing a pchan's remote side
 */
struct vmid_mmid_desc {
	int vmid; /* remote vmid  */
	int mmid[HABCFG_MMID_AREA_MAX+1]; /* selected or not */
	int is_listener[HABCFG_MMID_AREA_MAX+1]; /* yes or no */
};

struct local_vmid {
	int32_t self; /* only this field is for local */
	struct vmid_mmid_desc vmid_mmid_list[HABCFG_VMID_MAX];
};

struct hab_driver {
	struct device *dev; /* mmid dev list */
	struct cdev cdev;
	dev_t major;
	struct class *class;
	int ndevices;
	struct hab_device *devp;
	struct uhab_context *kctx;

	struct list_head uctx_list;
	int ctx_cnt;
	spinlock_t drvlock;

	struct local_vmid settings; /* parser results */

	int b_server_dom;
	int b_loopback_be; /* only allow 2 apps simultaneously 1 fe 1 be */
	int b_loopback;

	void *hyp_priv; /* hypervisor plug-in storage */

	void *hab_vmm_handle;
};

struct virtual_channel {
	struct list_head node; /* for ctx */
	struct list_head pnode; /* for pchan */
	/*
	 * refcount is used to track the references from hab core to the virtual
	 * channel such as references from physical channels,
	 * i.e. references from the "other" side
	 */
	struct kref refcount;
	struct physical_channel *pchan;
	struct uhab_context *ctx;
	struct list_head rx_list;
	wait_queue_head_t rx_queue;
	spinlock_t rx_lock;
	int id;
	int otherend_id;
	int otherend_closed;
	uint32_t session_id;

	/*
	 * set when local close() is called explicitly. vchan could be
	 * used in hab-recv-msg() path (2) then close() is called (1).
	 * this is same case as close is not called and no msg path
	 */
	int closed;
	int forked; /* if fork is detected and assume only once */
};

/*
 * Struct shared between local and remote, contents
 * are composed by exporter, the importer only writes
 * to pdata and local (exporter) domID
 */
struct export_desc {
	uint32_t  export_id;
	int       readonly;
	uint64_t  import_index;

	struct virtual_channel *vchan; /* vchan could be freed earlier */
	struct uhab_context *ctx;
	struct physical_channel *pchan;

	int32_t             vcid_local;
	int32_t             vcid_remote;
	int                 domid_local;
	int                 domid_remote;
	int                 flags;

	struct list_head    node;
	void *kva;
	int                 payload_count;
	unsigned char       payload[1];
} __packed;

int hab_is_forbidden(struct uhab_context *ctx,
		struct hab_device *dev,
		uint32_t sub_id);
int hab_vchan_open(struct uhab_context *ctx,
		unsigned int mmid, int32_t *vcid,
		int32_t timeout, uint32_t flags);
void hab_vchan_close(struct uhab_context *ctx,
		int32_t vcid);
long hab_vchan_send(struct uhab_context *ctx,
		int vcid,
		size_t sizebytes,
		void *data,
		unsigned int flags);
int hab_vchan_recv(struct uhab_context *ctx,
		struct hab_message **msg,
		int vcid,
		int *rsize,
		unsigned int flags);
void hab_vchan_stop(struct virtual_channel *vchan);
void hab_vchans_stop(struct physical_channel *pchan);
void hab_vchan_stop_notify(struct virtual_channel *vchan);
void hab_vchans_empty_wait(int vmid);

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
		void *ppdata,
		int *compressed,
		int *compressed_size);

int habmem_hyp_grant(unsigned long address,
		int page_count,
		int flags,
		int remotedom,
		void *ppdata,
		int *compressed,
		int *compressed_size);

int habmem_hyp_revoke(void *expdata, uint32_t count);

void *habmem_imp_hyp_open(void);
void habmem_imp_hyp_close(void *priv, int kernel);

int habmem_imp_hyp_map(void *imp_ctx, struct hab_import *param,
		struct export_desc *exp, int kernel);

int habmm_imp_hyp_unmap(void *imp_ctx, struct export_desc *exp, int kernel);

int habmem_imp_hyp_mmap(struct file *flip, struct vm_area_struct *vma);

int habmm_imp_hyp_map_check(void *imp_ctx, struct export_desc *exp);

void hab_msg_free(struct hab_message *message);
int hab_msg_dequeue(struct virtual_channel *vchan,
		struct hab_message **msg, int *rsize, unsigned int flags);

int hab_msg_recv(struct physical_channel *pchan,
		struct hab_header *header);

void hab_open_request_init(struct hab_open_request *request,
		int type,
		struct physical_channel *pchan,
		int vchan_id,
		int sub_id,
		int open_id);
int hab_open_request_send(struct hab_open_request *request);
int hab_open_request_add(struct physical_channel *pchan,
		size_t sizebytes, int request_type);
void hab_open_request_free(struct hab_open_request *request);
int hab_open_listen(struct uhab_context *ctx,
		struct hab_device *dev,
		struct hab_open_request *listen,
		struct hab_open_request **recv_request,
		int ms_timeout);

struct virtual_channel *hab_vchan_alloc(struct uhab_context *ctx,
		struct physical_channel *pchan, int openid);
struct virtual_channel *hab_vchan_get(struct physical_channel *pchan,
						  struct hab_header *header);
void hab_vchan_put(struct virtual_channel *vchan);

struct virtual_channel *hab_get_vchan_fromvcid(int32_t vcid,
		struct uhab_context *ctx, int ignore_remote);
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
		kref_put(&ctx->refcount, &hab_ctx_free);
}

void hab_send_close_msg(struct virtual_channel *vchan);
int hab_hypervisor_register(void);
void hab_hypervisor_unregister(void);
void hab_hypervisor_unregister_common(void);
int habhyp_commdev_alloc(void **commdev, int is_be, char *name,
		int vmid_remote, struct hab_device *mmid_device);
int habhyp_commdev_dealloc(void *commdev);

int physical_channel_read(struct physical_channel *pchan,
		void *payload,
		size_t read_size);

int physical_channel_send(struct physical_channel *pchan,
		struct hab_header *header,
		void *payload);

void physical_channel_rx_dispatch(unsigned long physical_channel);

int loopback_pchan_create(struct hab_device *dev, char *pchan_name);

int hab_parse(struct local_vmid *settings);

int do_hab_parse(void);

int fill_default_gvm_settings(struct local_vmid *settings,
		int vmid_local, int mmid_start, int mmid_end);

bool hab_is_loopback(void);

int hab_vchan_query(struct uhab_context *ctx, int32_t vcid, uint64_t *ids,
		char *names, size_t name_size, uint32_t flags);

struct hab_device *find_hab_device(unsigned int mm_id);

int get_refcnt(struct kref ref);

int hab_open_pending_enter(struct uhab_context *ctx,
		struct physical_channel *pchan,
		struct hab_open_node *pending);

int hab_open_pending_exit(struct uhab_context *ctx,
		struct physical_channel *pchan,
		struct hab_open_node *pending);

int hab_open_cancel_notify(struct hab_open_request *request);

int hab_open_receive_cancel(struct physical_channel *pchan,
		size_t sizebytes);

int hab_stat_init(struct hab_driver *drv);
int hab_stat_deinit(struct hab_driver *drv);
int hab_stat_show_vchan(struct hab_driver *drv, char *buf, int sz);
int hab_stat_show_ctx(struct hab_driver *drv, char *buf, int sz);
int hab_stat_show_expimp(struct hab_driver *drv, int pid, char *buf, int sz);

int hab_stat_init_sub(struct hab_driver *drv);
int hab_stat_deinit_sub(struct hab_driver *drv);

/* Global singleton HAB instance */
extern struct hab_driver hab_driver;

#endif /* __HAB_H */
