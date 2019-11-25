/*
 * SMC Invoke driver
 *
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/anon_inodes.h>
#include <linux/hashtable.h>
#include <linux/smcinvoke.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include <linux/kref.h>

#include <soc/qcom/scm.h>
#include <asm/cacheflush.h>
#include <soc/qcom/qseecomi.h>

#include "smcinvoke_object.h"
#include "../../misc/qseecom_kernel.h"

#define SMCINVOKE_DEV                   "smcinvoke"
#define SMCINVOKE_TZ_ROOT_OBJ           1
#define SMCINVOKE_TZ_OBJ_NULL           0
#define SMCINVOKE_TZ_MIN_BUF_SIZE       4096
#define SMCINVOKE_ARGS_ALIGN_SIZE       (sizeof(uint64_t))
#define SMCINVOKE_NEXT_AVAILABLE_TXN    0
#define SMCINVOKE_REQ_PLACED            1
#define SMCINVOKE_REQ_PROCESSING        2
#define SMCINVOKE_REQ_PROCESSED         3
#define SMCINVOKE_INCREMENT             1
#define SMCINVOKE_DECREMENT             0
#define SMCINVOKE_OBJ_TYPE_TZ_OBJ       0
#define SMCINVOKE_OBJ_TYPE_SERVER       1
#define SMCINVOKE_MEM_MAP_OBJ           0
#define SMCINVOKE_MEM_RGN_OBJ           1
#define SMCINVOKE_MEM_PERM_RW           6

/* TZ defined values - Start */
#define SMCINVOKE_INVOKE_PARAM_ID       0x224
#define SMCINVOKE_CB_RSP_PARAM_ID       0x22
#define SMCINVOKE_INVOKE_CMD            0x32000600
#define SMCINVOKE_CB_RSP_CMD            0x32000601
#define SMCINVOKE_RESULT_INBOUND_REQ_NEEDED 3
/* TZ defined values - End */

/*
 * This is the state when server FD has been closed but
 * TZ still has refs of CBOBjs served by this server
 */
#define SMCINVOKE_SERVER_STATE_DEFUNCT  1

#define FOR_ARGS(ndxvar, counts, section) \
	for (ndxvar = OBJECT_COUNTS_INDEX_##section(counts); \
		ndxvar < (OBJECT_COUNTS_INDEX_##section(counts) \
		+ OBJECT_COUNTS_NUM_##section(counts)); \
		++ndxvar)

#define TZCB_BUF_OFFSET(tzcb_req) (sizeof(tzcb_req->result) + \
			sizeof(struct smcinvoke_msg_hdr) + \
			sizeof(union smcinvoke_tz_args) * \
				OBJECT_COUNTS_TOTAL(tzcb_req->hdr.counts))

/*
 * +ve uhandle : either remote obj or mem obj, decided by f_ops
 * -ve uhandle : either Obj NULL or CBObj
 *	- -1: OBJ NULL
 *	- < -1: CBObj
 */
#define UHANDLE_IS_FD(h) ((h) >= 0)
#define UHANDLE_IS_NULL(h) ((h) == SMCINVOKE_USERSPACE_OBJ_NULL)
#define UHANDLE_IS_CB_OBJ(h) (h < SMCINVOKE_USERSPACE_OBJ_NULL)
#define UHANDLE_NULL (SMCINVOKE_USERSPACE_OBJ_NULL)
/*
 * MAKE => create handle for other domain i.e. TZ or userspace
 * GET => retrieve obj from incoming handle
 */
#define UHANDLE_GET_CB_OBJ(h) (-2-(h))
#define UHANDLE_MAKE_CB_OBJ(o) (-2-(o))
#define UHANDLE_GET_FD(h) (h)

/*
 * +ve tzhandle : remote object i.e. owned by TZ
 * -ve tzhandle : local object i.e. owned by linux
 * --------------------------------------------------
 *| 1 (1 bit) | Obj Id (15 bits) | srvr id (16 bits) |
 * ---------------------------------------------------
 * Server ids are defined below for various local objects
 * server id 0 : Kernel Obj
 * server id 1 : Memory region Obj
 * server id 2 : Memory map Obj
 * server id 3-15: Reserverd
 * server id 16 & up: Callback Objs
 */
#define KRNL_SRVR_ID 0
#define MEM_RGN_SRVR_ID 1
#define MEM_MAP_SRVR_ID 2
#define CBOBJ_SERVER_ID_START 0x10
#define CBOBJ_SERVER_ID_END ((1<<16) - 1)
/* local obj id is represented by 15 bits */
#define MAX_LOCAL_OBJ_ID ((1<<15) - 1)
/* CBOBJs will be served by server id 0x10 onwards */
#define TZHANDLE_GET_SERVER(h) ((uint16_t)((h) & 0xFFFF))
#define TZHANDLE_GET_OBJID(h) (((h) >> 16) & 0x7FFF)
#define TZHANDLE_MAKE_LOCAL(s, o) (((0x8000 | (o)) << 16) | s)

#define TZHANDLE_IS_NULL(h) ((h) == SMCINVOKE_TZ_OBJ_NULL)
#define TZHANDLE_IS_LOCAL(h) ((h) & 0x80000000)
#define TZHANDLE_IS_REMOTE(h) (!TZHANDLE_IS_NULL(h) && !TZHANDLE_IS_LOCAL(h))

#define TZHANDLE_IS_KERNEL_OBJ(h) (TZHANDLE_IS_LOCAL(h) && \
				TZHANDLE_GET_SERVER(h) == KRNL_SRVR_ID)
#define TZHANDLE_IS_MEM_RGN_OBJ(h) (TZHANDLE_IS_LOCAL(h) && \
				TZHANDLE_GET_SERVER(h) == MEM_RGN_SRVR_ID)
#define TZHANDLE_IS_MEM_MAP_OBJ(h) (TZHANDLE_IS_LOCAL(h) && \
				TZHANDLE_GET_SERVER(h) == MEM_MAP_SRVR_ID)
#define TZHANDLE_IS_MEM_OBJ(h) (TZHANDLE_IS_MEM_RGN_OBJ(h) || \
				TZHANDLE_IS_MEM_MAP_OBJ(h))
#define TZHANDLE_IS_CB_OBJ(h) (TZHANDLE_IS_LOCAL(h) && \
				TZHANDLE_GET_SERVER(h) >= CBOBJ_SERVER_ID_START)

#define FILE_IS_REMOTE_OBJ(f) ((f)->f_op && (f)->f_op == &g_smcinvoke_fops)

static DEFINE_MUTEX(g_smcinvoke_lock);
#define NO_LOCK 0
#define TAKE_LOCK 1
#define MUTEX_LOCK(x) { if (x) mutex_lock(&g_smcinvoke_lock); }
#define MUTEX_UNLOCK(x) { if (x) mutex_unlock(&g_smcinvoke_lock); }
static DEFINE_HASHTABLE(g_cb_servers, 8);
static LIST_HEAD(g_mem_objs);
static uint16_t g_last_cb_server_id = CBOBJ_SERVER_ID_START;
static uint16_t g_last_mem_rgn_id, g_last_mem_map_obj_id;
static size_t g_max_cb_buf_size = SMCINVOKE_TZ_MIN_BUF_SIZE;

static long smcinvoke_ioctl(struct file *, unsigned int, unsigned long);
static int smcinvoke_open(struct inode *, struct file *);
static int smcinvoke_release(struct inode *, struct file *);
static int destroy_cb_server(uint16_t);

static const struct file_operations g_smcinvoke_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= smcinvoke_ioctl,
	.compat_ioctl	= smcinvoke_ioctl,
	.open		= smcinvoke_open,
	.release	= smcinvoke_release,
};

static dev_t smcinvoke_device_no;
static struct cdev smcinvoke_cdev;
static struct class *driver_class;
static struct device *class_dev;
static struct platform_device *smcinvoke_pdev;

struct smcinvoke_buf_hdr {
	uint32_t offset;
	uint32_t size;
};

union smcinvoke_tz_args {
	struct smcinvoke_buf_hdr b;
	int32_t handle;
};

struct smcinvoke_msg_hdr {
	uint32_t tzhandle;
	uint32_t op;
	uint32_t counts;
};

/* Inbound reqs from TZ */
struct smcinvoke_tzcb_req {
	int32_t result;
	struct smcinvoke_msg_hdr hdr;
	union smcinvoke_tz_args args[0];
};

struct smcinvoke_file_data {
	uint32_t context_type;
	union {
		uint32_t tzhandle;
		uint16_t server_id;
	};
};

struct smcinvoke_piggyback_msg {
	uint32_t version;
	uint32_t op;
	uint32_t counts;
	int32_t objs[0];
};

/* Data structure to hold request coming from TZ */
struct smcinvoke_cb_txn {
	uint32_t txn_id;
	int32_t state;
	struct smcinvoke_tzcb_req *cb_req;
	size_t cb_req_bytes;
	struct file **filp_to_release;
	struct hlist_node hash;
	struct kref ref_cnt;
};

struct smcinvoke_server_info {
	uint16_t server_id;
	uint16_t state;
	uint32_t txn_id;
	wait_queue_head_t req_wait_q;
	wait_queue_head_t rsp_wait_q;
	size_t cb_buf_size;
	DECLARE_HASHTABLE(reqs_table, 4);
	DECLARE_HASHTABLE(responses_table, 4);
	struct hlist_node hash;
	struct list_head pending_cbobjs;
};

struct smcinvoke_cbobj {
	uint16_t cbobj_id;
	struct kref ref_cnt;
	struct smcinvoke_server_info *server;
	struct list_head list;
};

/*
 * We require couple of objects, one for mem region & another
 * for mapped mem_obj once mem region has been mapped. It is
 * possible that TZ can release either independent of other.
 */
struct smcinvoke_mem_obj {
	/* these ids are objid part of tzhandle */
	uint16_t mem_region_id;
	uint16_t mem_map_obj_id;
	struct dma_buf *dma_buf;
	struct dma_buf_attachment *buf_attach;
	struct sg_table *sgt;
	struct kref mem_regn_ref_cnt;
	struct kref mem_map_obj_ref_cnt;
	uint64_t p_addr;
	size_t p_addr_len;
	struct list_head list;
};

static struct smcinvoke_server_info *find_cb_server_locked(uint16_t server_id)
{
	struct smcinvoke_server_info *data = NULL;

	hash_for_each_possible(g_cb_servers, data, hash, server_id) {
		if (data->server_id == server_id)
			return data;
	}
	return  NULL;
}

static uint16_t next_cb_server_id_locked(void)
{
	if (g_last_cb_server_id == CBOBJ_SERVER_ID_END)
		g_last_cb_server_id = CBOBJ_SERVER_ID_START;

	while (find_cb_server_locked(++g_last_cb_server_id));

	return g_last_cb_server_id;
}

static inline void release_filp(struct file **filp_to_release, size_t arr_len)
{
	size_t i = 0;

	for (i = 0; i < arr_len; i++) {
		if (filp_to_release[i]) {
			fput(filp_to_release[i]);
			filp_to_release[i] = NULL;
		}
	}
}

static  struct smcinvoke_mem_obj *find_mem_obj_locked(uint16_t mem_obj_id,
							bool is_mem_rgn_obj)
{
	struct smcinvoke_mem_obj *mem_obj = NULL;

	if (list_empty(&g_mem_objs))
		return NULL;

	list_for_each_entry(mem_obj, &g_mem_objs, list) {
		if ((is_mem_rgn_obj &&
			(mem_obj->mem_region_id == mem_obj_id)) ||
		    (!is_mem_rgn_obj &&
			(mem_obj->mem_map_obj_id == mem_obj_id)))
			return mem_obj;
	}
	return NULL;
}

static uint32_t next_mem_region_obj_id_locked(void)
{
	if (g_last_mem_rgn_id == MAX_LOCAL_OBJ_ID)
		g_last_mem_rgn_id = 0;

	while (find_mem_obj_locked(++g_last_mem_rgn_id, SMCINVOKE_MEM_RGN_OBJ));

	return g_last_mem_rgn_id;
}

static uint32_t next_mem_map_obj_id_locked(void)
{
	if (g_last_mem_map_obj_id == MAX_LOCAL_OBJ_ID)
		g_last_mem_map_obj_id = 0;

	while (find_mem_obj_locked(++g_last_mem_map_obj_id,
					SMCINVOKE_MEM_MAP_OBJ));

	return g_last_mem_map_obj_id;
}

static inline void free_mem_obj_locked(struct smcinvoke_mem_obj *mem_obj)
{
	list_del(&mem_obj->list);
	dma_buf_put(mem_obj->dma_buf);
	kfree(mem_obj);
}

static void del_mem_regn_obj_locked(struct kref *kref)
{
	struct smcinvoke_mem_obj *mem_obj = container_of(kref,
			struct smcinvoke_mem_obj, mem_regn_ref_cnt);

	/*
	 * mem_regn obj and mem_map obj are held into mem_obj structure which
	 * can't be released until both kinds of objs have been released.
	 * So check whether mem_map iobj has ref 0 and only then release mem_obj
	 */
	if (kref_read(&mem_obj->mem_map_obj_ref_cnt) == 0)
		free_mem_obj_locked(mem_obj);
}

static void del_mem_map_obj_locked(struct kref *kref)
{
	struct smcinvoke_mem_obj *mem_obj = container_of(kref,
			struct smcinvoke_mem_obj, mem_map_obj_ref_cnt);

	mem_obj->p_addr_len = 0;
	mem_obj->p_addr = 0;
	if (mem_obj->sgt)
		dma_buf_unmap_attachment(mem_obj->buf_attach,
					mem_obj->sgt, DMA_BIDIRECTIONAL);
	if (mem_obj->buf_attach)
		dma_buf_detach(mem_obj->dma_buf, mem_obj->buf_attach);

	/*
	 * mem_regn obj and mem_map obj are held into mem_obj structure which
	 * can't be released until both kinds of objs have been released.
	 * So check if mem_regn obj has ref 0 and only then release mem_obj
	 */
	if (kref_read(&mem_obj->mem_regn_ref_cnt) == 0)
		free_mem_obj_locked(mem_obj);
}

static int release_mem_obj_locked(int32_t tzhandle)
{
	int is_mem_regn_obj = TZHANDLE_IS_MEM_RGN_OBJ(tzhandle);
	struct smcinvoke_mem_obj *mem_obj = find_mem_obj_locked(
			TZHANDLE_GET_OBJID(tzhandle), is_mem_regn_obj);

	if (!mem_obj)
		return OBJECT_ERROR_BADOBJ;

	if (is_mem_regn_obj)
		kref_put(&mem_obj->mem_regn_ref_cnt, del_mem_regn_obj_locked);
	else
		kref_put(&mem_obj->mem_map_obj_ref_cnt, del_mem_map_obj_locked);

	return OBJECT_OK;
}

static void free_pending_cbobj_locked(struct kref *kref)
{
	struct smcinvoke_server_info *server = NULL;
	struct smcinvoke_cbobj *obj = container_of(kref,
					struct smcinvoke_cbobj, ref_cnt);
	list_del(&obj->list);
	server = obj->server;
	kfree(obj);
	if ((server->state == SMCINVOKE_SERVER_STATE_DEFUNCT) &&
				list_empty(&server->pending_cbobjs)) {
		hash_del(&server->hash);
		kfree(server);
	}
}

static int get_pending_cbobj_locked(uint16_t srvr_id, int16_t obj_id)
{
	struct list_head *head = NULL;
	struct smcinvoke_cbobj *cbobj = NULL;
	struct smcinvoke_cbobj *obj = NULL;
	struct smcinvoke_server_info *server = find_cb_server_locked(srvr_id);

	if (!server)
		return OBJECT_ERROR_BADOBJ;

	head = &server->pending_cbobjs;
	list_for_each_entry(cbobj, head, list)
		if (cbobj->cbobj_id == obj_id)  {
			kref_get(&cbobj->ref_cnt);
			return 0;
		}

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj)
		return OBJECT_ERROR_KMEM;

	obj->cbobj_id = obj_id;
	kref_init(&obj->ref_cnt);
	obj->server = server;
	list_add_tail(&obj->list, head);

	return 0;
}

static int put_pending_cbobj_locked(uint16_t srvr_id, int16_t obj_id)
{
	struct smcinvoke_server_info *srvr_info =
					find_cb_server_locked(srvr_id);
	struct list_head *head = NULL;
	struct smcinvoke_cbobj *cbobj = NULL;

	if (!srvr_info)
		return -EINVAL;

	head = &srvr_info->pending_cbobjs;
	list_for_each_entry(cbobj, head, list)
		if (cbobj->cbobj_id == obj_id)  {
			kref_put(&cbobj->ref_cnt, free_pending_cbobj_locked);
			return 0;
		}
	return -EINVAL;
}

static int release_tzhandle_locked(int32_t tzhandle)
{
	if (TZHANDLE_IS_MEM_OBJ(tzhandle))
		return release_mem_obj_locked(tzhandle);
	else if (TZHANDLE_IS_CB_OBJ(tzhandle))
		return put_pending_cbobj_locked(TZHANDLE_GET_SERVER(tzhandle),
						TZHANDLE_GET_OBJID(tzhandle));
	return OBJECT_ERROR;
}

static void release_tzhandles(const int32_t *tzhandles, size_t len)
{
	size_t i;

	mutex_lock(&g_smcinvoke_lock);
	for (i = 0; i < len; i++)
		release_tzhandle_locked(tzhandles[i]);
	mutex_unlock(&g_smcinvoke_lock);
}

static void delete_cb_txn(struct kref *kref)
{
	struct smcinvoke_cb_txn *cb_txn = container_of(kref,
					struct smcinvoke_cb_txn, ref_cnt);

	if (OBJECT_OP_METHODID(cb_txn->cb_req->hdr.op) == OBJECT_OP_RELEASE)
		release_tzhandle_locked(cb_txn->cb_req->hdr.tzhandle);

	kfree(cb_txn->cb_req);
	hash_del(&cb_txn->hash);
	kfree(cb_txn);
}

static struct smcinvoke_cb_txn *find_cbtxn_locked(
				struct smcinvoke_server_info *server,
				uint32_t txn_id, int32_t state)
{
	int i = 0;
	struct smcinvoke_cb_txn *cb_txn = NULL;

	/*
	 * Since HASH_BITS() does not work on pointers, we can't select hash
	 * table using state and loop over it.
	 */
	if (state == SMCINVOKE_REQ_PLACED) {
		/* pick up 1st req */
		hash_for_each(server->reqs_table, i, cb_txn, hash) {
			kref_get(&cb_txn->ref_cnt);
			hash_del(&cb_txn->hash);
			return cb_txn;
		}
	} else if (state == SMCINVOKE_REQ_PROCESSING) {
		hash_for_each_possible(
				server->responses_table, cb_txn, hash, txn_id) {
			if (cb_txn->txn_id == txn_id) {
				kref_get(&cb_txn->ref_cnt);
				hash_del(&cb_txn->hash);
				return cb_txn;
			}
		}
	}
	return NULL;
}

/*
 * size_add saturates at SIZE_MAX. If integer overflow is detected,
 * this function would return SIZE_MAX otherwise normal a+b is returned.
 */
static inline size_t size_add(size_t a, size_t b)
{
	return (b > (SIZE_MAX - a)) ? SIZE_MAX : a + b;
}

/*
 * pad_size is used along with size_align to define a buffer overflow
 * protected version of ALIGN
 */
static inline size_t pad_size(size_t a, size_t b)
{
	return (~a + 1) % b;
}

/*
 * size_align saturates at SIZE_MAX. If integer overflow is detected, this
 * function would return SIZE_MAX otherwise next aligned size is returned.
 */
static inline size_t size_align(size_t a, size_t b)
{
	return size_add(a, pad_size(a, b));
}

static uint16_t get_server_id(int cb_server_fd)
{
	uint16_t server_id = 0;
	struct smcinvoke_file_data *svr_cxt = NULL;
	struct file *tmp_filp = fget(cb_server_fd);

	if (!tmp_filp)
		return server_id;

	svr_cxt = tmp_filp->private_data;
	if (svr_cxt && svr_cxt->context_type ==  SMCINVOKE_OBJ_TYPE_SERVER)
		server_id = svr_cxt->server_id;

	if (tmp_filp)
		fput(tmp_filp);

	return server_id;
}

static bool is_dma_fd(int32_t uhandle, struct dma_buf **dma_buf)
{
	*dma_buf = dma_buf_get(uhandle);
	return IS_ERR_OR_NULL(*dma_buf) ? false : true;
}

static bool is_remote_obj(int32_t uhandle, struct smcinvoke_file_data **tzobj,
							struct file **filp)
{
	bool ret = false;
	struct file *tmp_filp = fget(uhandle);

	if (!tmp_filp)
		return ret;

	if (FILE_IS_REMOTE_OBJ(tmp_filp)) {
		*tzobj = tmp_filp->private_data;
		if ((*tzobj)->context_type == SMCINVOKE_OBJ_TYPE_TZ_OBJ) {
			*filp = tmp_filp;
			tmp_filp = NULL;
			ret = true;
		}
	}

	if (tmp_filp)
		fput(tmp_filp);
	return ret;
}

static int create_mem_obj(struct dma_buf *dma_buf, int32_t *mem_obj)
{
	struct smcinvoke_mem_obj *t_mem_obj =
				kzalloc(sizeof(*t_mem_obj), GFP_KERNEL);

	if (!t_mem_obj) {
		dma_buf_put(dma_buf);
		return -ENOMEM;
	}

	kref_init(&t_mem_obj->mem_regn_ref_cnt);
	t_mem_obj->dma_buf = dma_buf;
	mutex_lock(&g_smcinvoke_lock);
	t_mem_obj->mem_region_id = next_mem_region_obj_id_locked();
	list_add_tail(&t_mem_obj->list, &g_mem_objs);
	mutex_unlock(&g_smcinvoke_lock);
	*mem_obj = TZHANDLE_MAKE_LOCAL(MEM_RGN_SRVR_ID,
					t_mem_obj->mem_region_id);
	return 0;
}

/*
 * This function retrieves file pointer corresponding to FD provided. It stores
 * retrived file pointer until IOCTL call is concluded. Once call is completed,
 * all stored file pointers are released. file pointers are stored to prevent
 * other threads from releasing that FD while IOCTL is in progress.
 */
static int get_tzhandle_from_uhandle(int32_t uhandle, int32_t server_fd,
				struct file **filp, uint32_t *tzhandle)
{
	int ret = -EBADF;
	uint16_t server_id = 0;

	if (UHANDLE_IS_NULL(uhandle)) {
		*tzhandle = SMCINVOKE_TZ_OBJ_NULL;
		ret = 0;
	} else if (UHANDLE_IS_CB_OBJ(uhandle)) {
		server_id = get_server_id(server_fd);
		if (server_id < CBOBJ_SERVER_ID_START)
			goto out;

		mutex_lock(&g_smcinvoke_lock);
		ret = get_pending_cbobj_locked(server_id,
					UHANDLE_GET_CB_OBJ(uhandle));
		mutex_unlock(&g_smcinvoke_lock);
		if (ret)
			goto out;
		*tzhandle = TZHANDLE_MAKE_LOCAL(server_id,
						UHANDLE_GET_CB_OBJ(uhandle));
		ret = 0;
	} else if (UHANDLE_IS_FD(uhandle)) {
		struct dma_buf *dma_buf = NULL;
		struct smcinvoke_file_data *tzobj = NULL;

		if (is_dma_fd(UHANDLE_GET_FD(uhandle), &dma_buf)) {
			ret = create_mem_obj(dma_buf, tzhandle);
		} else if (is_remote_obj(UHANDLE_GET_FD(uhandle),
						&tzobj, filp)) {
			*tzhandle = tzobj->tzhandle;
			ret = 0;
		}
	}
out:
	return ret;
}

static int get_fd_for_obj(uint32_t obj_type, uint32_t obj, int64_t *fd)
{
	int unused_fd = -1, ret = -EINVAL;
	struct file *f = NULL;
	struct smcinvoke_file_data *cxt = NULL;

	cxt = kzalloc(sizeof(*cxt), GFP_KERNEL);
	if (!cxt) {
		ret = -ENOMEM;
		goto out;
	}
	if (obj_type == SMCINVOKE_OBJ_TYPE_TZ_OBJ) {
		cxt->context_type = SMCINVOKE_OBJ_TYPE_TZ_OBJ;
		cxt->tzhandle = obj;
	} else if (obj_type == SMCINVOKE_OBJ_TYPE_SERVER) {
		cxt->context_type = SMCINVOKE_OBJ_TYPE_SERVER;
		cxt->server_id = obj;
	} else {
		goto out;
	}

	unused_fd = get_unused_fd_flags(O_RDWR);
	if (unused_fd < 0)
		goto out;

	if (fd == NULL)
		goto out;

	f = anon_inode_getfile(SMCINVOKE_DEV, &g_smcinvoke_fops, cxt, O_RDWR);
	if (IS_ERR(f))
		goto out;

	*fd = unused_fd;
	fd_install(*fd, f);
	return 0;
out:
	if (unused_fd >= 0)
		put_unused_fd(unused_fd);
	kfree(cxt);

	return ret;
}

static int get_uhandle_from_tzhandle(int32_t tzhandle, int32_t srvr_id,
				int64_t *uhandle, bool lock)
{
	int ret = -1;

	if (TZHANDLE_IS_NULL(tzhandle)) {
		*uhandle = UHANDLE_NULL;
		ret = 0;
	} else if (TZHANDLE_IS_CB_OBJ(tzhandle)) {
		if (srvr_id != TZHANDLE_GET_SERVER(tzhandle))
			goto out;
		*uhandle = UHANDLE_MAKE_CB_OBJ(TZHANDLE_GET_OBJID(tzhandle));
		MUTEX_LOCK(lock)
		ret = get_pending_cbobj_locked(TZHANDLE_GET_SERVER(tzhandle),
						TZHANDLE_GET_OBJID(tzhandle));
		MUTEX_UNLOCK(lock)
	} else if (TZHANDLE_IS_MEM_RGN_OBJ(tzhandle)) {
		struct smcinvoke_mem_obj *mem_obj =  NULL;

		MUTEX_LOCK(lock)
		mem_obj = find_mem_obj_locked(TZHANDLE_GET_OBJID(tzhandle),
						SMCINVOKE_MEM_RGN_OBJ);

		if (mem_obj != NULL) {
			unsigned long flags = 0;
			int fd;

			if (dma_buf_get_flags(mem_obj->dma_buf, &flags))
				goto exit_lock;
			fd = dma_buf_fd(mem_obj->dma_buf, flags);

			if (fd < 0)
				goto exit_lock;
			*uhandle = fd;
			ret = 0;
		}
exit_lock:
		MUTEX_UNLOCK(lock)
	} else if (TZHANDLE_IS_REMOTE(tzhandle)) {
		/* if execution comes here => tzhandle is an unsigned int */
		ret = get_fd_for_obj(SMCINVOKE_OBJ_TYPE_TZ_OBJ,
						(uint32_t)tzhandle, uhandle);
	}
out:
	return ret;
}

static int32_t smcinvoke_release_mem_obj_locked(void *buf, size_t buf_len)
{
	struct smcinvoke_tzcb_req *msg = buf;

	if (msg->hdr.counts != OBJECT_COUNTS_PACK(0, 0, 0, 0))
		return OBJECT_ERROR_INVALID;

	return release_tzhandle_locked(msg->hdr.tzhandle);
}

static int32_t smcinvoke_map_mem_region(void *buf, size_t buf_len)
{
	int ret = OBJECT_OK;
	struct smcinvoke_tzcb_req *msg = buf;
	struct {
		uint64_t p_addr;
		uint64_t len;
		uint32_t perms;
	} *ob = NULL;
	int32_t *oo = NULL;
	struct smcinvoke_mem_obj *mem_obj = NULL;
	struct dma_buf_attachment *buf_attach = NULL;
	struct sg_table *sgt = NULL;

	if (msg->hdr.counts != OBJECT_COUNTS_PACK(0, 1, 1, 1) ||
		(buf_len - msg->args[0].b.offset <  msg->args[0].b.size))
		return OBJECT_ERROR_INVALID;

	/* args[0] = BO, args[1] = OI, args[2] = OO */
	ob = buf + msg->args[0].b.offset;
	oo =  &msg->args[2].handle;

	mutex_lock(&g_smcinvoke_lock);
	mem_obj = find_mem_obj_locked(TZHANDLE_GET_OBJID(msg->args[1].handle),
						SMCINVOKE_MEM_RGN_OBJ);
	if (!mem_obj) {
		mutex_unlock(&g_smcinvoke_lock);
		return OBJECT_ERROR_BADOBJ;
	}

	if (!mem_obj->p_addr) {
		kref_init(&mem_obj->mem_map_obj_ref_cnt);
		buf_attach = dma_buf_attach(mem_obj->dma_buf,
					&smcinvoke_pdev->dev);
		if (IS_ERR(buf_attach)) {
			ret = OBJECT_ERROR_KMEM;
			goto out;
		}
		mem_obj->buf_attach = buf_attach;

		sgt = dma_buf_map_attachment(buf_attach, DMA_BIDIRECTIONAL);
		if (IS_ERR(sgt)) {
			ret = OBJECT_ERROR_KMEM;
			goto out;
		}
		mem_obj->sgt = sgt;

		/* contiguous only => nents=1 */
		if (sgt->nents != 1) {
			ret = OBJECT_ERROR_INVALID;
			goto out;
		}
		mem_obj->p_addr = sg_dma_address(sgt->sgl);
		mem_obj->p_addr_len = sgt->sgl->length;
		if (!mem_obj->p_addr) {
			ret = OBJECT_ERROR_INVALID;
			goto out;
		}
		mem_obj->mem_map_obj_id = next_mem_map_obj_id_locked();
	} else {
		kref_get(&mem_obj->mem_map_obj_ref_cnt);
	}
	ob->p_addr = mem_obj->p_addr;
	ob->len = mem_obj->p_addr_len;
	ob->perms = SMCINVOKE_MEM_PERM_RW;
	*oo = TZHANDLE_MAKE_LOCAL(MEM_MAP_SRVR_ID, mem_obj->mem_map_obj_id);
out:
	if (ret != OBJECT_OK)
		kref_put(&mem_obj->mem_map_obj_ref_cnt, del_mem_map_obj_locked);
	mutex_unlock(&g_smcinvoke_lock);
	return ret;
}

static void process_kernel_obj(void *buf, size_t buf_len)
{
	struct smcinvoke_tzcb_req *cb_req = buf;

	cb_req->result = (cb_req->hdr.op == OBJECT_OP_MAP_REGION) ?
			smcinvoke_map_mem_region(buf, buf_len) :
			OBJECT_ERROR_INVALID;
}

static void process_mem_obj(void *buf, size_t buf_len)
{
	struct smcinvoke_tzcb_req *cb_req = buf;

	mutex_lock(&g_smcinvoke_lock);
	cb_req->result = (cb_req->hdr.op == OBJECT_OP_RELEASE) ?
			smcinvoke_release_mem_obj_locked(buf, buf_len) :
			OBJECT_ERROR_INVALID;
	mutex_unlock(&g_smcinvoke_lock);
}

/*
 * Buf should be aligned to struct smcinvoke_tzcb_req
 */
static void process_tzcb_req(void *buf, size_t buf_len, struct file **arr_filp)
{
	/* ret is going to TZ. Provide values from OBJECT_ERROR_<> */
	int ret = OBJECT_ERROR_DEFUNCT;
	struct smcinvoke_cb_txn *cb_txn = NULL;
	struct smcinvoke_tzcb_req *cb_req = NULL, *tmp_cb_req = NULL;
	struct smcinvoke_server_info *srvr_info = NULL;

	if (buf_len < sizeof(struct smcinvoke_tzcb_req))
		return;

	cb_req = buf;

	/* check whether it is to be served by kernel or userspace */
	if (TZHANDLE_IS_KERNEL_OBJ(cb_req->hdr.tzhandle)) {
		return process_kernel_obj(buf, buf_len);
	} else if (TZHANDLE_IS_MEM_OBJ(cb_req->hdr.tzhandle)) {
		return process_mem_obj(buf, buf_len);
	} else if (!TZHANDLE_IS_CB_OBJ(cb_req->hdr.tzhandle)) {
		cb_req->result = OBJECT_ERROR_INVALID;
		return;
	}

	/*
	 * We need a copy of req that could be sent to server. Otherwise, if
	 * someone kills invoke caller, buf would go away and server would be
	 * working on already freed buffer, causing a device crash.
	 */
	tmp_cb_req = kmemdup(buf, buf_len, GFP_KERNEL);
	if (!tmp_cb_req) {
		/* we need to return error to caller so fill up result */
		cb_req->result = OBJECT_ERROR_KMEM;
		return;
	}

	cb_txn = kzalloc(sizeof(*cb_txn), GFP_KERNEL);
	if (!cb_txn) {
		cb_req->result = OBJECT_ERROR_KMEM;
		kfree(tmp_cb_req);
		return;
	}
	/* no need for memcpy as we did kmemdup() above */
	cb_req  = tmp_cb_req;

	cb_txn->state = SMCINVOKE_REQ_PLACED;
	cb_txn->cb_req = cb_req;
	cb_txn->cb_req_bytes = buf_len;
	cb_txn->filp_to_release = arr_filp;
	kref_init(&cb_txn->ref_cnt);

	mutex_lock(&g_smcinvoke_lock);
	srvr_info = find_cb_server_locked(
				TZHANDLE_GET_SERVER(cb_req->hdr.tzhandle));
	if (!srvr_info || srvr_info->state == SMCINVOKE_SERVER_STATE_DEFUNCT) {
		/* ret equals Object_ERROR_DEFUNCT, at this point go to out */
		mutex_unlock(&g_smcinvoke_lock);
		goto out;
	}

	cb_txn->txn_id = ++srvr_info->txn_id;
	hash_add(srvr_info->reqs_table, &cb_txn->hash, cb_txn->txn_id);
	mutex_unlock(&g_smcinvoke_lock);
	/*
	 * we need not worry that server_info will be deleted because as long
	 * as this CBObj is served by this server, srvr_info will be valid.
	 */
	wake_up_interruptible(&srvr_info->req_wait_q);
	ret = wait_event_interruptible(srvr_info->rsp_wait_q,
			(cb_txn->state == SMCINVOKE_REQ_PROCESSED) ||
			(srvr_info->state == SMCINVOKE_SERVER_STATE_DEFUNCT));
out:
	/*
	 * we could be here because of either: a. Req is PROCESSED
	 * b. Server was killed                c. Invoke thread is killed
	 * sometime invoke thread and server are part of same process.
	 */
	mutex_lock(&g_smcinvoke_lock);
	hash_del(&cb_txn->hash);
	if (cb_txn->state == SMCINVOKE_REQ_PROCESSED) {
		/*
		 * it is possible that server was killed immediately
		 * after CB Req was processed but who cares now!
		 */
	} else if (!srvr_info ||
		srvr_info->state == SMCINVOKE_SERVER_STATE_DEFUNCT) {
		cb_req->result = OBJECT_ERROR_DEFUNCT;
	} else {
		pr_debug("%s wait_event interrupted ret = %d\n", __func__, ret);
		cb_req->result = OBJECT_ERROR_ABORT;
	}
	memcpy(buf, cb_req, buf_len);
	kref_put(&cb_txn->ref_cnt, delete_cb_txn);
	mutex_unlock(&g_smcinvoke_lock);
}

static int marshal_out_invoke_req(const uint8_t *buf, uint32_t buf_size,
				struct smcinvoke_cmd_req *req,
				union smcinvoke_arg *args_buf)
{
	int ret = -EINVAL, i = 0;
	union smcinvoke_tz_args *tz_args = NULL;
	size_t offset = sizeof(struct smcinvoke_msg_hdr) +
				OBJECT_COUNTS_TOTAL(req->counts) *
					sizeof(union smcinvoke_tz_args);

	if (offset > buf_size)
		goto out;

	tz_args = (union smcinvoke_tz_args *)
				(buf + sizeof(struct smcinvoke_msg_hdr));

	tz_args += OBJECT_COUNTS_NUM_BI(req->counts);

	if (args_buf == NULL)
		return 0;

	FOR_ARGS(i, req->counts, BO) {
		args_buf[i].b.size = tz_args->b.size;
		if ((buf_size - tz_args->b.offset < tz_args->b.size) ||
			tz_args->b.offset > buf_size) {
			pr_err("%s: buffer overflow detected\n", __func__);
			goto out;
		}
		if (copy_to_user((void __user *)(uintptr_t)(args_buf[i].b.addr),
			(uint8_t *)(buf) + tz_args->b.offset,
						tz_args->b.size)) {
			pr_err("Error %d copying ctxt to user\n", ret);
			goto out;
		}
		tz_args++;
	}
	tz_args += OBJECT_COUNTS_NUM_OI(req->counts);

	FOR_ARGS(i, req->counts, OO) {
		/*
		 * create a new FD and assign to output object's context.
		 * We are passing cb_server_fd from output param in case OO
		 * is a CBObj. For CBObj, we have to ensure that it is sent
		 * to server who serves it and that info comes from USpace.
		 */
		ret = get_uhandle_from_tzhandle(tz_args->handle,
					TZHANDLE_GET_SERVER(tz_args->handle),
					&(args_buf[i].o.fd), NO_LOCK);
		if (ret)
			goto out;
		tz_args++;
	}
	ret = 0;
out:
	return ret;
}

static bool is_inbound_req(int val)
{
	return (val == SMCINVOKE_RESULT_INBOUND_REQ_NEEDED ||
		val == QSEOS_RESULT_INCOMPLETE ||
		val == QSEOS_RESULT_BLOCKED_ON_LISTENER);
}

static int prepare_send_scm_msg(const uint8_t *in_buf, size_t in_buf_len,
				uint8_t *out_buf, size_t out_buf_len,
				struct smcinvoke_cmd_req *req,
				union smcinvoke_arg *args_buf,
				bool *tz_acked)
{
	int ret = 0, cmd;
	struct scm_desc desc = {0};
	struct file *arr_filp[OBJECT_COUNTS_MAX_OO] = {NULL};

	*tz_acked = false;
	/* buf size should be page aligned */
	if ((in_buf_len % PAGE_SIZE) != 0 || (out_buf_len % PAGE_SIZE) != 0)
		return -EINVAL;

	desc.arginfo = SMCINVOKE_INVOKE_PARAM_ID;
	desc.args[0] = (uint64_t)virt_to_phys(in_buf);
	desc.args[1] = in_buf_len;
	desc.args[2] = (uint64_t)virt_to_phys(out_buf);
	desc.args[3] = out_buf_len;
	cmd = SMCINVOKE_INVOKE_CMD;
	dmac_flush_range(in_buf, in_buf + in_buf_len);
	dmac_flush_range(out_buf, out_buf + out_buf_len);
	/*
	 * purpose of lock here is to ensure that any CB obj that may be going
	 * to user as OO is not released by piggyback message on another invoke
	 * request. We should not move this lock to process_invoke_req() because
	 * that will either cause deadlock or prevent any other invoke request
	 * to come in. We release this lock when either
	 *     a) TZ requires HLOS action to complete ongoing invoke operation
	 *     b) Final response to invoke has been marshalled out
	 */
	while (1) {
		mutex_lock(&g_smcinvoke_lock);
		ret = scm_call2(cmd, &desc);
		req->result = (int32_t)desc.ret[1];
		if (!ret && !is_inbound_req(desc.ret[0])) {

			/* dont marshal if Obj returns an error */
			if (!req->result) {
				dmac_inv_range(in_buf, in_buf + in_buf_len);
				if (args_buf != NULL)
					ret = marshal_out_invoke_req(in_buf,
						in_buf_len, req, args_buf);
			}
			*tz_acked = true;
		}
		mutex_unlock(&g_smcinvoke_lock);

		if (cmd == SMCINVOKE_CB_RSP_CMD)
			release_filp(arr_filp, OBJECT_COUNTS_MAX_OO);

		if (ret || !is_inbound_req(desc.ret[0]))
			break;

		/* process listener request */
		if (desc.ret[0] == QSEOS_RESULT_INCOMPLETE ||
		    desc.ret[0] == QSEOS_RESULT_BLOCKED_ON_LISTENER) {
			ret = qseecom_process_listener_from_smcinvoke(&desc);
			req->result = (int32_t)desc.ret[1];
			if (!req->result) {
				dmac_inv_range(in_buf, in_buf + in_buf_len);
				ret = marshal_out_invoke_req(in_buf,
						in_buf_len, req, args_buf);
			}
			*tz_acked = true;
		}

		/*
		 * qseecom does not understand smcinvoke's callback object &&
		 * erringly sets ret value as -EINVAL :( We need to handle it.
		 */
		if (desc.ret[0] != SMCINVOKE_RESULT_INBOUND_REQ_NEEDED)
			break;

		/*
		 * At this point we are convinced it is an inbnd req but it is
		 * possible that it is a resp to inbnd req that has failed and
		 * returned an err. Ideally scm_call should have returned err
		 * but err comes in  ret[1]. So check that out otherwise it
		 * could cause infinite loop.
		 */
		if (req->result &&
			desc.ret[0] == SMCINVOKE_RESULT_INBOUND_REQ_NEEDED) {
			ret = req->result;
			break;
		}

		dmac_inv_range(out_buf, out_buf + out_buf_len);

		if (desc.ret[0] == SMCINVOKE_RESULT_INBOUND_REQ_NEEDED) {
			process_tzcb_req(out_buf, out_buf_len, arr_filp);
			desc.arginfo = SMCINVOKE_CB_RSP_PARAM_ID;
			desc.args[0] = (uint64_t)virt_to_phys(out_buf);
			desc.args[1] = out_buf_len;
			cmd = SMCINVOKE_CB_RSP_CMD;
			dmac_flush_range(out_buf, out_buf + out_buf_len);
		}
	}
	return ret;
}
/*
 * SMC expects arguments in following format
 * ---------------------------------------------------------------------------
 * | cxt | op | counts | ptr|size |ptr|size...|ORef|ORef|...| rest of payload |
 * ---------------------------------------------------------------------------
 * cxt: target, op: operation, counts: total arguments
 * offset: offset is from beginning of buffer i.e. cxt
 * size: size is 8 bytes aligned value
 */
static size_t compute_in_msg_size(const struct smcinvoke_cmd_req *req,
					const union smcinvoke_arg *args_buf)
{
	uint32_t i = 0;

	size_t total_size = sizeof(struct smcinvoke_msg_hdr) +
				OBJECT_COUNTS_TOTAL(req->counts) *
					sizeof(union smcinvoke_tz_args);

	/* Computed total_size should be 8 bytes aligned from start of buf */
	total_size = ALIGN(total_size, SMCINVOKE_ARGS_ALIGN_SIZE);

	/* each buffer has to be 8 bytes aligned */
	while (i < OBJECT_COUNTS_NUM_buffers(req->counts))
		total_size = size_add(total_size,
		size_align(args_buf[i++].b.size, SMCINVOKE_ARGS_ALIGN_SIZE));

	return PAGE_ALIGN(total_size);
}

static int marshal_in_invoke_req(const struct smcinvoke_cmd_req *req,
			const union smcinvoke_arg *args_buf, uint32_t tzhandle,
			uint8_t *buf, size_t buf_size, struct file **arr_filp,
			int32_t *tzhandles_to_release)
{
	int ret = -EINVAL, i = 0, j = 0, k = 0;
	const struct smcinvoke_msg_hdr msg_hdr = {
					tzhandle, req->op, req->counts};
	uint32_t offset = sizeof(struct smcinvoke_msg_hdr) +
				sizeof(union smcinvoke_tz_args) *
				OBJECT_COUNTS_TOTAL(req->counts);
	union smcinvoke_tz_args *tz_args = NULL;

	if (buf_size < offset)
		goto out;

	*(struct smcinvoke_msg_hdr *)buf = msg_hdr;
	tz_args = (union smcinvoke_tz_args *)(buf +
				sizeof(struct smcinvoke_msg_hdr));

	if (args_buf == NULL)
		return 0;

	FOR_ARGS(i, req->counts, BI) {
		offset = size_align(offset, SMCINVOKE_ARGS_ALIGN_SIZE);
		if ((offset > buf_size) ||
			(args_buf[i].b.size > (buf_size - offset)))
			goto out;

		tz_args[i].b.offset = offset;
		tz_args[i].b.size = args_buf[i].b.size;

		if (copy_from_user(buf + offset,
			(void __user *)(uintptr_t)(args_buf[i].b.addr),
						args_buf[i].b.size))
			goto out;

		offset += args_buf[i].b.size;
	}
	FOR_ARGS(i, req->counts, BO) {
		offset = size_align(offset, SMCINVOKE_ARGS_ALIGN_SIZE);
		if ((offset > buf_size) ||
			(args_buf[i].b.size > (buf_size - offset)))
			goto out;

		tz_args[i].b.offset = offset;
		tz_args[i].b.size = args_buf[i].b.size;
		offset += args_buf[i].b.size;
	}
	FOR_ARGS(i, req->counts, OI) {
		ret = get_tzhandle_from_uhandle(args_buf[i].o.fd,
				 args_buf[i].o.cb_server_fd, &arr_filp[j++],
							&(tz_args[i].handle));
		if (ret)
			goto out;
		tzhandles_to_release[k++] = tz_args[i].handle;
	}
	ret = 0;
out:
	return ret;
}

static int marshal_in_tzcb_req(const struct smcinvoke_cb_txn *cb_txn,
				struct smcinvoke_accept *user_req, int srvr_id)
{
	int ret = 0, i = 0;
	union smcinvoke_arg tmp_arg;
	struct smcinvoke_tzcb_req *tzcb_req = cb_txn->cb_req;
	union smcinvoke_tz_args *tz_args = tzcb_req->args;
	size_t tzcb_req_len = cb_txn->cb_req_bytes;
	size_t tz_buf_offset = TZCB_BUF_OFFSET(tzcb_req);
	size_t user_req_buf_offset = sizeof(union smcinvoke_arg) *
				OBJECT_COUNTS_TOTAL(tzcb_req->hdr.counts);

	if (tz_buf_offset > tzcb_req_len) {
		ret = -EINVAL;
		goto out;
	}

	user_req->txn_id = cb_txn->txn_id;
	if (get_uhandle_from_tzhandle(tzcb_req->hdr.tzhandle, srvr_id,
				(int64_t *)&user_req->cbobj_id, TAKE_LOCK)) {
		ret = -EINVAL;
		goto out;
	}
	user_req->op = tzcb_req->hdr.op;
	user_req->counts = tzcb_req->hdr.counts;
	user_req->argsize = sizeof(union smcinvoke_arg);

	FOR_ARGS(i, tzcb_req->hdr.counts, BI) {
		user_req_buf_offset = size_align(user_req_buf_offset,
					 SMCINVOKE_ARGS_ALIGN_SIZE);
		tmp_arg.b.size = tz_args[i].b.size;
		if ((tz_args[i].b.offset > tzcb_req_len) ||
		    (tz_args[i].b.size > tzcb_req_len - tz_args[i].b.offset) ||
		    (user_req_buf_offset > user_req->buf_len) ||
			(tmp_arg.b.size >
			user_req->buf_len - user_req_buf_offset)) {
			ret = -EINVAL;
			pr_err("%s: buffer overflow detected\n", __func__);
			goto out;
		}
		tmp_arg.b.addr = user_req->buf_addr + user_req_buf_offset;

		if (copy_to_user(u64_to_user_ptr
				(user_req->buf_addr + i * sizeof(tmp_arg)),
				&tmp_arg, sizeof(tmp_arg)) ||
		   copy_to_user(u64_to_user_ptr(tmp_arg.b.addr),
			(uint8_t *)(tzcb_req) + tz_args[i].b.offset,
				tz_args[i].b.size)) {
			ret = -EFAULT;
			goto out;
		}
		user_req_buf_offset += tmp_arg.b.size;
	}
	FOR_ARGS(i, tzcb_req->hdr.counts, BO) {
		user_req_buf_offset = size_align(user_req_buf_offset,
					SMCINVOKE_ARGS_ALIGN_SIZE);

		tmp_arg.b.size = tz_args[i].b.size;
		if ((user_req_buf_offset > user_req->buf_len) ||
		    (tmp_arg.b.size >
				user_req->buf_len - user_req_buf_offset)) {
			ret = -EINVAL;
			pr_err("%s: buffer overflow detected\n", __func__);
			goto out;
		}
		tmp_arg.b.addr = user_req->buf_addr + user_req_buf_offset;

		if (copy_to_user(u64_to_user_ptr
				(user_req->buf_addr + i * sizeof(tmp_arg)),
				&tmp_arg, sizeof(tmp_arg))) {
			ret = -EFAULT;
			goto out;
		}
		user_req_buf_offset += tmp_arg.b.size;
	}
	FOR_ARGS(i, tzcb_req->hdr.counts, OI) {
		/*
		 * create a new FD and assign to output object's
		 * context
		 */
		ret = get_uhandle_from_tzhandle(tz_args[i].handle, srvr_id,
						&(tmp_arg.o.fd), TAKE_LOCK);
		if (ret) {
			ret = -EINVAL;
			goto out;
		}
		if (copy_to_user(u64_to_user_ptr
				(user_req->buf_addr + i * sizeof(tmp_arg)),
				&tmp_arg, sizeof(tmp_arg))) {
			ret = -EFAULT;
			goto out;
		}
	}
out:
	return ret;
}

static int marshal_out_tzcb_req(const struct smcinvoke_accept *user_req,
				struct smcinvoke_cb_txn *cb_txn,
				struct file **arr_filp)
{
	int ret = -EINVAL, i = 0;
	int32_t tzhandles_to_release[OBJECT_COUNTS_MAX_OO] = {0};
	struct smcinvoke_tzcb_req *tzcb_req = cb_txn->cb_req;
	union smcinvoke_tz_args *tz_args = tzcb_req->args;

	release_tzhandles(&cb_txn->cb_req->hdr.tzhandle, 1);
	tzcb_req->result = user_req->result;
	FOR_ARGS(i, tzcb_req->hdr.counts, BO) {
		union smcinvoke_arg tmp_arg;

		if (copy_from_user((uint8_t *)&tmp_arg, u64_to_user_ptr(
			user_req->buf_addr + i * sizeof(union smcinvoke_arg)),
			sizeof(union smcinvoke_arg))) {
			ret = -EFAULT;
			goto out;
		}
		if (tmp_arg.b.size > tz_args[i].b.size)
			goto out;
		if (copy_from_user((uint8_t *)(tzcb_req) + tz_args[i].b.offset,
					u64_to_user_ptr(tmp_arg.b.addr),
						tmp_arg.b.size)) {
			ret = -EFAULT;
			goto out;
		}
	}

	FOR_ARGS(i, tzcb_req->hdr.counts, OO) {
		union smcinvoke_arg tmp_arg;

		if (copy_from_user((uint8_t *)&tmp_arg, u64_to_user_ptr(
			user_req->buf_addr + i * sizeof(union smcinvoke_arg)),
			sizeof(union smcinvoke_arg))) {
			ret = -EFAULT;
			goto out;
		}
		ret = get_tzhandle_from_uhandle(tmp_arg.o.fd,
				tmp_arg.o.cb_server_fd, &arr_filp[i],
						&(tz_args[i].handle));
		if (ret)
			goto out;
		tzhandles_to_release[i] = tz_args[i].handle;
	}
	FOR_ARGS(i, tzcb_req->hdr.counts, OI) {
		if (TZHANDLE_IS_CB_OBJ(tz_args[i].handle))
			release_tzhandles(&tz_args[i].handle, 1);
	}
	ret = 0;
out:
	if (ret)
		release_tzhandles(tzhandles_to_release, OBJECT_COUNTS_MAX_OO);
	return ret;
}

static void process_piggyback_data(void *buf, size_t buf_size)
{
	int i;
	struct smcinvoke_tzcb_req req = {0};
	struct smcinvoke_piggyback_msg *msg = buf;
	int32_t *objs = msg->objs;

	dmac_flush_range(buf, buf + buf_size);
	if (msg->counts)
		dmac_inv_range(buf, buf + buf_size);

	for (i = 0; i < msg->counts; i++) {
		req.hdr.op = msg->op;
		req.hdr.counts = 0; /* release op does not require any args */
		req.hdr.tzhandle = objs[i];
		process_tzcb_req(&req, sizeof(struct smcinvoke_tzcb_req), NULL);
		/* cbobjs_in_flight will be adjusted during CB processing */
	}
}


static long process_ack_local_obj(struct file *filp, unsigned int cmd,
						unsigned long arg)
{
	int ret = -1;
	int32_t local_obj = SMCINVOKE_USERSPACE_OBJ_NULL;
	struct smcinvoke_file_data *filp_data = filp->private_data;

	if (_IOC_SIZE(cmd) != sizeof(int32_t))
		return -EINVAL;

	ret = copy_from_user(&local_obj, (void __user *)(uintptr_t)arg,
					sizeof(int32_t));
	if (ret)
		return -EFAULT;

	mutex_lock(&g_smcinvoke_lock);
	if (UHANDLE_IS_CB_OBJ(local_obj))
		ret = put_pending_cbobj_locked(filp_data->server_id,
					UHANDLE_GET_CB_OBJ(local_obj));
	mutex_unlock(&g_smcinvoke_lock);

	return ret;
}

static long process_server_req(struct file *filp, unsigned int cmd,
						 unsigned long arg)
{
	int ret = -1;
	int64_t server_fd = -1;
	struct smcinvoke_server server_req = {0};
	struct smcinvoke_server_info *server_info = NULL;

	if (_IOC_SIZE(cmd) != sizeof(server_req))
		return -EINVAL;

	ret = copy_from_user(&server_req, (void __user *)(uintptr_t)arg,
					sizeof(server_req));
	if (ret)
		return -EFAULT;

	server_info = kzalloc(sizeof(*server_info), GFP_KERNEL);
	if (!server_info)
		return -ENOMEM;

	init_waitqueue_head(&server_info->req_wait_q);
	init_waitqueue_head(&server_info->rsp_wait_q);
	server_info->cb_buf_size = server_req.cb_buf_size;
	hash_init(server_info->reqs_table);
	hash_init(server_info->responses_table);
	INIT_LIST_HEAD(&server_info->pending_cbobjs);

	mutex_lock(&g_smcinvoke_lock);

	server_info->server_id = next_cb_server_id_locked();
	hash_add(g_cb_servers, &server_info->hash,
					server_info->server_id);
	if (g_max_cb_buf_size < server_req.cb_buf_size)
		g_max_cb_buf_size = server_req.cb_buf_size;

	mutex_unlock(&g_smcinvoke_lock);
	ret = get_fd_for_obj(SMCINVOKE_OBJ_TYPE_SERVER,
				server_info->server_id, &server_fd);

	if (ret)
		destroy_cb_server(server_info->server_id);

	return server_fd;
}

static long process_accept_req(struct file *filp, unsigned int cmd,
						unsigned long arg)
{
	int ret = -1;
	struct smcinvoke_file_data *server_obj = filp->private_data;
	struct smcinvoke_accept user_args = {0};
	struct smcinvoke_cb_txn *cb_txn = NULL;
	struct smcinvoke_server_info *server_info = NULL;

	if (_IOC_SIZE(cmd) != sizeof(struct smcinvoke_accept))
		return -EINVAL;

	if (copy_from_user(&user_args, (void __user *)arg,
					sizeof(struct smcinvoke_accept)))
		return -EFAULT;

	if (user_args.argsize != sizeof(union smcinvoke_arg))
		return -EINVAL;

	/* ACCEPT is available only on server obj */
	if (server_obj->context_type != SMCINVOKE_OBJ_TYPE_SERVER)
		return -EPERM;

	mutex_lock(&g_smcinvoke_lock);
	server_info = find_cb_server_locked(server_obj->server_id);
	mutex_unlock(&g_smcinvoke_lock);
	if (!server_info)
		return -EINVAL;

	/* First check if it has response otherwise wait for req */
	if (user_args.has_resp) {
		mutex_lock(&g_smcinvoke_lock);
		cb_txn = find_cbtxn_locked(server_info, user_args.txn_id,
					SMCINVOKE_REQ_PROCESSING);
		mutex_unlock(&g_smcinvoke_lock);
		/*
		 * cb_txn can be null if userspace provides wrong txn id OR
		 * invoke thread died while server was processing cb req.
		 * if invoke thread dies, it would remove req from Q. So
		 * no matching cb_txn would be on Q and hence NULL cb_txn.
		 */
		if (!cb_txn) {
			pr_err("%s txn %d either invalid or removed from Q\n",
					__func__, user_args.txn_id);
			goto out;
		}
		ret = marshal_out_tzcb_req(&user_args, cb_txn,
						cb_txn->filp_to_release);
		/*
		 * if client did not set error and we get error locally,
		 * we return local error to TA
		 */
		if (ret && cb_txn->cb_req->result == 0)
			cb_txn->cb_req->result = OBJECT_ERROR_UNAVAIL;

		cb_txn->state = SMCINVOKE_REQ_PROCESSED;
		kref_put(&cb_txn->ref_cnt, delete_cb_txn);
		wake_up(&server_info->rsp_wait_q);
		/*
		 * if marshal_out fails, we should let userspace release
		 * any ref/obj it created for CB processing
		 */
		if (ret && OBJECT_COUNTS_NUM_OO(user_args.counts))
			goto out;
	}
	/*
	 * Once response has been delivered, thread will wait for another
	 * callback req to process.
	 */
	do {
		ret = wait_event_interruptible(server_info->req_wait_q,
				!hash_empty(server_info->reqs_table));
		if (ret) {
			pr_debug("%s wait_event interrupted: ret = %d\n",
							__func__, ret);
			goto out;
		}

		mutex_lock(&g_smcinvoke_lock);
		cb_txn = find_cbtxn_locked(server_info,
						SMCINVOKE_NEXT_AVAILABLE_TXN,
						SMCINVOKE_REQ_PLACED);
		mutex_unlock(&g_smcinvoke_lock);
		if (cb_txn) {
			cb_txn->state = SMCINVOKE_REQ_PROCESSING;
			ret = marshal_in_tzcb_req(cb_txn, &user_args,
							server_obj->server_id);
			if (ret) {
				cb_txn->cb_req->result = OBJECT_ERROR_UNAVAIL;
				cb_txn->state = SMCINVOKE_REQ_PROCESSED;
				kref_put(&cb_txn->ref_cnt, delete_cb_txn);
				wake_up_interruptible(&server_info->rsp_wait_q);
				continue;
			}
			mutex_lock(&g_smcinvoke_lock);
			hash_add(server_info->responses_table, &cb_txn->hash,
							cb_txn->txn_id);
			kref_put(&cb_txn->ref_cnt, delete_cb_txn);
			mutex_unlock(&g_smcinvoke_lock);
			ret =  copy_to_user((void __user *)arg, &user_args,
					sizeof(struct smcinvoke_accept));
		}
	} while (!cb_txn);
out:
	return ret;
}

static long process_invoke_req(struct file *filp, unsigned int cmd,
						unsigned long arg)
{
	int    ret = -1, nr_args = 0;
	struct smcinvoke_cmd_req req = {0};
	void   *in_msg = NULL, *out_msg = NULL;
	size_t inmsg_size = 0, outmsg_size = SMCINVOKE_TZ_MIN_BUF_SIZE;
	union  smcinvoke_arg *args_buf = NULL;
	struct smcinvoke_file_data *tzobj = filp->private_data;
	/*
	 * Hold reference to remote object until invoke op is not
	 * completed. Release once invoke is done.
	 */
	struct file *filp_to_release[OBJECT_COUNTS_MAX_OO] = {NULL};
	/*
	 * If anything goes wrong, release alloted tzhandles for
	 * local objs which could be either CBObj or MemObj.
	 */
	int32_t tzhandles_to_release[OBJECT_COUNTS_MAX_OO] = {0};
	bool tz_acked = false;

	if (_IOC_SIZE(cmd) != sizeof(req))
		return -EINVAL;

	if (tzobj->context_type != SMCINVOKE_OBJ_TYPE_TZ_OBJ)
		return -EPERM;

	ret = copy_from_user(&req, (void __user *)arg, sizeof(req));
	if (ret)
		return -EFAULT;

	if (req.argsize != sizeof(union smcinvoke_arg))
		return -EINVAL;

	nr_args = OBJECT_COUNTS_NUM_buffers(req.counts) +
			OBJECT_COUNTS_NUM_objects(req.counts);

	if (nr_args) {
		args_buf = kcalloc(nr_args, req.argsize, GFP_KERNEL);
		if (!args_buf)
			return -ENOMEM;

		ret = copy_from_user(args_buf, u64_to_user_ptr(req.args),
					nr_args * req.argsize);

		if (ret) {
			ret = -EFAULT;
			goto out;
		}
	}

	inmsg_size = compute_in_msg_size(&req, args_buf);
	in_msg = (void *)__get_free_pages(GFP_KERNEL|__GFP_COMP,
						get_order(inmsg_size));
	if (!in_msg) {
		ret = -ENOMEM;
		goto out;
	}
	memset(in_msg, 0, inmsg_size);

	mutex_lock(&g_smcinvoke_lock);
	outmsg_size = PAGE_ALIGN(g_max_cb_buf_size);
	mutex_unlock(&g_smcinvoke_lock);
	out_msg = (void *)__get_free_pages(GFP_KERNEL | __GFP_COMP,
						get_order(outmsg_size));
	if (!out_msg) {
		ret = -ENOMEM;
		goto out;
	}
	memset(out_msg, 0, outmsg_size);

	ret = marshal_in_invoke_req(&req, args_buf, tzobj->tzhandle, in_msg,
			inmsg_size, filp_to_release, tzhandles_to_release);
	if (ret)
		goto out;

	ret = prepare_send_scm_msg(in_msg, inmsg_size, out_msg, outmsg_size,
					&req, args_buf, &tz_acked);

	/*
	 * If scm_call is success, TZ owns responsibility to release
	 * refs for local objs.
	 */
	if (tz_acked == false)
		goto out;
	memset(tzhandles_to_release, 0, sizeof(tzhandles_to_release));

	/*
	 * if invoke op results in an err, no need to marshal_out and
	 * copy args buf to user space
	 */
	if (!req.result) {
		/*
		 * Dont check ret of marshal_out because there might be a
		 * FD for OO which userspace must release even if an error
		 * occurs. Releasing FD from user space is much simpler than
		 * doing here. ORing of ret is reqd not to miss past error
		 */
		ret |=  copy_to_user(u64_to_user_ptr(req.args), args_buf,
					nr_args * req.argsize);
	}
	/* copy result of invoke op */
	ret |=  copy_to_user((void __user *)arg, &req, sizeof(req));
	if (ret)
		goto out;

	/* Outbuf could be carrying local objs to be released. */
	process_piggyback_data(out_msg, outmsg_size);
out:
	release_filp(filp_to_release, OBJECT_COUNTS_MAX_OO);
	if (ret)
		release_tzhandles(tzhandles_to_release, OBJECT_COUNTS_MAX_OO);
	free_pages((long)out_msg, get_order(outmsg_size));
	free_pages((long)in_msg, get_order(inmsg_size));
	kfree(args_buf);
	return ret;
}

static long smcinvoke_ioctl(struct file *filp, unsigned int cmd,
						unsigned long arg)
{
	long ret = 0;

	switch (cmd) {
	case SMCINVOKE_IOCTL_INVOKE_REQ:
		ret = process_invoke_req(filp, cmd, arg);
		break;
	case SMCINVOKE_IOCTL_ACCEPT_REQ:
		ret = process_accept_req(filp, cmd, arg);
		break;
	case SMCINVOKE_IOCTL_SERVER_REQ:
		ret = process_server_req(filp, cmd, arg);
		break;
	case SMCINVOKE_IOCTL_ACK_LOCAL_OBJ:
		ret = process_ack_local_obj(filp, cmd, arg);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}

static int smcinvoke_open(struct inode *nodp, struct file *filp)
{
	struct smcinvoke_file_data *tzcxt = NULL;

	tzcxt = kzalloc(sizeof(*tzcxt), GFP_KERNEL);
	if (!tzcxt)
		return -ENOMEM;

	tzcxt->tzhandle = SMCINVOKE_TZ_ROOT_OBJ;
	tzcxt->context_type = SMCINVOKE_OBJ_TYPE_TZ_OBJ;
	filp->private_data = tzcxt;

	return 0;
}

static int destroy_cb_server(uint16_t server_id)
{
	struct smcinvoke_server_info *server = NULL;

	mutex_lock(&g_smcinvoke_lock);
	server = find_cb_server_locked(server_id);
	if (server) {
		if (!list_empty(&server->pending_cbobjs)) {
			server->state = SMCINVOKE_SERVER_STATE_DEFUNCT;
			wake_up_interruptible(&server->rsp_wait_q);
			/*
			 * we dont worry about threads waiting on req_wait_q
			 * because server can't be closed as long as there is
			 * atleast one accept thread active
			 */
		} else {
			hash_del(&server->hash);
			kfree(server);
		}
	}
	mutex_unlock(&g_smcinvoke_lock);
	return 0;
}

static int smcinvoke_release(struct inode *nodp, struct file *filp)
{
	int ret = 0;
	bool release_handles;
	uint8_t *in_buf = NULL;
	uint8_t *out_buf = NULL;
	struct smcinvoke_msg_hdr hdr = {0};
	struct smcinvoke_file_data *file_data = filp->private_data;
	struct smcinvoke_cmd_req req = {0};
	uint32_t tzhandle = 0;

	if (file_data->context_type == SMCINVOKE_OBJ_TYPE_SERVER) {
		ret = destroy_cb_server(file_data->server_id);
		goto out;
	}

	tzhandle = file_data->tzhandle;
	/* Root object is special in sense it is indestructible */
	if (!tzhandle || tzhandle == SMCINVOKE_TZ_ROOT_OBJ)
		goto out;

	in_buf = (uint8_t *)__get_free_page(GFP_KERNEL | __GFP_COMP);
	out_buf = (uint8_t *)__get_free_page(GFP_KERNEL | __GFP_COMP);
	if (!in_buf || !out_buf) {
		ret = -ENOMEM;
		goto out;
	}

	memset(in_buf, 0, PAGE_SIZE);
	memset(out_buf, 0, PAGE_SIZE);
	hdr.tzhandle = tzhandle;
	hdr.op = OBJECT_OP_RELEASE;
	hdr.counts = 0;
	*(struct smcinvoke_msg_hdr *)in_buf = hdr;

	ret = prepare_send_scm_msg(in_buf, SMCINVOKE_TZ_MIN_BUF_SIZE, out_buf,
		SMCINVOKE_TZ_MIN_BUF_SIZE, &req, NULL, &release_handles);

	process_piggyback_data(out_buf, SMCINVOKE_TZ_MIN_BUF_SIZE);
out:
	kfree(filp->private_data);
	free_page((long)in_buf);
	free_page((long)out_buf);

	return ret;
}

static int smcinvoke_probe(struct platform_device *pdev)
{
	unsigned int baseminor = 0;
	unsigned int count = 1;
	int rc = 0;

	rc = alloc_chrdev_region(&smcinvoke_device_no, baseminor, count,
							SMCINVOKE_DEV);
	if (rc < 0) {
		pr_err("chrdev_region failed %d for %s\n", rc, SMCINVOKE_DEV);
		return rc;
	}
	driver_class = class_create(THIS_MODULE, SMCINVOKE_DEV);
	if (IS_ERR(driver_class)) {
		rc = -ENOMEM;
		pr_err("class_create failed %d\n", rc);
		goto exit_unreg_chrdev_region;
	}
	class_dev = device_create(driver_class, NULL, smcinvoke_device_no,
						NULL, SMCINVOKE_DEV);
	if (!class_dev) {
		pr_err("class_device_create failed %d\n", rc);
		rc = -ENOMEM;
		goto exit_destroy_class;
	}

	cdev_init(&smcinvoke_cdev, &g_smcinvoke_fops);
	smcinvoke_cdev.owner = THIS_MODULE;

	rc = cdev_add(&smcinvoke_cdev, MKDEV(MAJOR(smcinvoke_device_no), 0),
								count);
	if (rc < 0) {
		pr_err("cdev_add failed %d for %s\n", rc, SMCINVOKE_DEV);
		goto exit_destroy_device;
	}
	smcinvoke_pdev = pdev;

	return  0;

exit_destroy_device:
	device_destroy(driver_class, smcinvoke_device_no);
exit_destroy_class:
	class_destroy(driver_class);
exit_unreg_chrdev_region:
	unregister_chrdev_region(smcinvoke_device_no, count);
	return rc;
}

static int smcinvoke_remove(struct platform_device *pdev)
{
	int count = 1;

	cdev_del(&smcinvoke_cdev);
	device_destroy(driver_class, smcinvoke_device_no);
	class_destroy(driver_class);
	unregister_chrdev_region(smcinvoke_device_no, count);
	return 0;
}

static const struct of_device_id smcinvoke_match[] = {
	{
		.compatible = "qcom,smcinvoke",
	},
	{},
};

static struct platform_driver smcinvoke_plat_driver = {
	.probe = smcinvoke_probe,
	.remove = smcinvoke_remove,
	.driver = {
		.name = "smcinvoke",
		.owner = THIS_MODULE,
		.of_match_table = smcinvoke_match,
	},
};

static int smcinvoke_init(void)
{
	return platform_driver_register(&smcinvoke_plat_driver);
}

static void smcinvoke_exit(void)
{
	platform_driver_unregister(&smcinvoke_plat_driver);
}

module_init(smcinvoke_init);
module_exit(smcinvoke_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SMC Invoke driver");
