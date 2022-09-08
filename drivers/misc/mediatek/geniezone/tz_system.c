// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 * GenieZone (hypervisor-based seucrity platform) enables hardware protected
 * and isolated security execution environment, includes
 * 1. GZ hypervisor
 * 2. Hypervisor-TEE OS (built-in Trusty OS)
 * 3. Drivers (ex: debug, communication and interrupt) for GZ and
 *    hypervisor-TEE OS
 * 4. GZ and hypervisor-TEE and GZ framework (supporting multiple TEE
 *    ecosystem, ex: M-TEE, Trusty, GlobalPlatform, ...)
 */


#include <linux/module.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/fcntl.h>
#include <linux/mutex.h>
#include <asm/arch_timer.h>
#include <gz-trusty/trusty_ipc.h>
#include <gz-trusty/smcall.h>
#include <gz-trusty/trusty.h>
#include <kree/system.h>
#include <kree/mem.h>
#include <linux/atomic.h>
#include <linux/vmalloc.h>
#if IS_ENABLED(CONFIG_PM_SLEEP)
#include <linux/pm_wakeup.h>
#endif
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <tz_cross/trustzone.h>
#include <tz_cross/ta_system.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#if IS_ENABLED(CONFIG_TEE)
#include "tee_client_api.h"
#endif

#if IS_ENABLED(CONFIG_MTK_ENG_BUILD)
#define DBG_KREE_SYS
#endif

#ifdef DBG_KREE_SYS
#define KREE_DEBUG(fmt...) pr_info("[KREE]" fmt)
#define KREE_INFO(fmt...) pr_info("[KREE]" fmt)
#define KREE_ERR(fmt...) pr_info("[KREE][ERR]" fmt)
#else
#define KREE_DEBUG(fmt...)
#define KREE_INFO(fmt...) pr_info("[KREE]" fmt)
#define KREE_ERR(fmt...) pr_info("[KREE][ERR]" fmt)
#endif

#define DYNAMIC_TIPC_LEN
#define GZ_SYS_SERVICE_NAME_TRUSTY "com.mediatek.geniezone.srv.sys"
#define GZ_SYS_SERVICE_NAME_NEBULA "nebula.com.mediatek.geniezone.srv.sys"

#define GZ_MEM_SERVICE_NAME_TRUSTY "com.mediatek.geniezone.srv.mem"
#define GZ_MEM_SERVICE_NAME_NEBULA "nebula.com.mediatek.geniezone.srv.mem"

#define KREE_SESSION_HANDLE_MAX_SIZE (512)
#define KREE_SESSION_HANDLE_SIZE_MASK (KREE_SESSION_HANDLE_MAX_SIZE - 1)
#define KREE_SESSION_HANDLE_TRUSTY (KREE_SESSION_HANDLE_MAX_SIZE)
#define KREE_SESSION_HANDLE_NEBULA (KREE_SESSION_HANDLE_MAX_SIZE + 1)

#define TIPC_RETRY_MAX_COUNT (100)
#define TIPC_RETRY_WAIT_MS (10)
#define IS_RESTARTSYS_ERROR(err) (err == -ERESTARTSYS)
#define RETRY_REQUIRED(cnt) (cnt <= TIPC_RETRY_MAX_COUNT)

DEFINE_MUTEX(fd_mutex);
DEFINE_MUTEX(session_mutex);

/* For high performance required and do not use mutiha API user, it may have
 * some delay when run into multi HA situation. Use servicecall_lock to ensure
 * the performance for such users.
 */
DEFINE_MUTEX(servicecall_lock);

int perf_boost_cnt;
EXPORT_SYMBOL(perf_boost_cnt);

struct mutex perf_boost_lock;
EXPORT_SYMBOL(perf_boost_lock);

struct platform_device *tz_system_dev;
EXPORT_SYMBOL(tz_system_dev);

struct completion ree_dummy_event;
struct task_struct *ree_dummy_task;

#if IS_ENABLED(CONFIG_PM_SLEEP)
struct wakeup_source *TeeServiceCall_wake_lock; /*4.19*/
EXPORT_SYMBOL(TeeServiceCall_wake_lock);
#endif

 /* only need to open sys service once */
static int32_t _sys_service_Fd[TEE_ID_END] = { [0 ... TEE_ID_END - 1] = -1 };
static struct tipc_k_handle _sys_service_h[TEE_ID_END];
static char *gz_sys_service_name[] = {
	GZ_SYS_SERVICE_NAME_TRUSTY,
	GZ_SYS_SERVICE_NAME_NEBULA
};

static struct tipc_k_handle
	_kree_session_handle_pool[KREE_SESSION_HANDLE_MAX_SIZE];
static uint32_t _kree_session_handle_idx;

#define debugFg 0

static bool _tipc_retry_check_and_wait(int err, int retry_cnt, int tag)
{
	if (likely(!IS_RESTARTSYS_ERROR(err)))
		return false;

	if (IS_RESTARTSYS_ERROR(err) && RETRY_REQUIRED(retry_cnt)) {
		KREE_DEBUG("%s: wait %d ms (retry:%d times)(%d)\n", __func__,
			   TIPC_RETRY_WAIT_MS, retry_cnt, tag);
		msleep(TIPC_RETRY_WAIT_MS);
		return true;
	}

	/* Caution: reach maximum retry count! */
	KREE_ERR("%s: system error, please check(%d)!\n", __func__, tag);
	return false;
}

static ssize_t _tipc_k_read_retry(struct tipc_k_handle *h, void *buf,
	size_t buf_len, unsigned int flags)
{
	ssize_t rc;
	int retry = 0;

	do {
		rc = tipc_k_read(h, (void *)buf, buf_len, flags);
		retry++;
	} while (_tipc_retry_check_and_wait(rc, retry, 0));

	return rc;
}

static ssize_t _tipc_k_write_retry(struct tipc_k_handle *h, void *buf,
	size_t buf_len, unsigned int flags)
{
	ssize_t rc;
	int retry = 0;

	do {
		rc = tipc_k_write(h, (void *)buf, buf_len, flags);
		retry++;
	} while (_tipc_retry_check_and_wait(rc, retry, 1));

	return rc;
}

enum tipc_chan_state {
	TIPC_DISCONNECTED = 0,
	TIPC_CONNECTING,
	TIPC_CONNECTED,
	TIPC_STALE,
};

static bool _is_tipc_channel_connected(struct tipc_dn_chan *dn)
{
	bool is_chan_connected = false;

	mutex_lock(&dn->lock);
	if (dn->state == TIPC_CONNECTED)
		is_chan_connected = true;
	mutex_unlock(&dn->lock);

	return is_chan_connected;
}

static int _tipc_k_connect_retry(struct tipc_k_handle *h, const char *port_name)
{
	int rc = 0;
	int retry = 0;

	do {
		if (unlikely(IS_RESTARTSYS_ERROR(rc))) {
			struct tipc_dn_chan *dn = h->dn;

			if (dn && _is_tipc_channel_connected(dn)) {
				KREE_DEBUG(
					"%s: channel is connected already!\n",
					__func__);
				return 0;
			}
			KREE_DEBUG("%s: disconnect and retry!\n", __func__);
			tipc_k_disconnect(h);
		}
		rc = tipc_k_connect(h, port_name);
		retry++;
	} while (_tipc_retry_check_and_wait(rc, retry, 2));

	if (rc != 0 && h)
		tipc_k_disconnect(h);

	return rc;
}

int32_t _setSessionHandle(struct tipc_k_handle h)
{
	int32_t session;
	int32_t i;

	mutex_lock(&fd_mutex);
	for (i = 0; i < KREE_SESSION_HANDLE_MAX_SIZE; i++) {
		if (_kree_session_handle_pool[_kree_session_handle_idx].dn == 0)
			break;
		_kree_session_handle_idx = (_kree_session_handle_idx + 1)
					   % KREE_SESSION_HANDLE_MAX_SIZE;
	}
	if (i == KREE_SESSION_HANDLE_MAX_SIZE) {
		KREE_ERR(" %s: can not get empty slot for session!\n",
			 __func__);
		return -1;
	}
	_kree_session_handle_pool[_kree_session_handle_idx].dn = h.dn;
	session = _kree_session_handle_idx;
	_kree_session_handle_idx =
		(_kree_session_handle_idx + 1) % KREE_SESSION_HANDLE_MAX_SIZE;
	mutex_unlock(&fd_mutex);

	return session;
}

void _clearSessionHandle(uint32_t session)
{
	mutex_lock(&fd_mutex);
	_kree_session_handle_pool[session].dn = 0;
	mutex_unlock(&fd_mutex);
}


int _getSessionHandle(int32_t session, struct tipc_k_handle **h)
{
	if (session == KREE_SESSION_HANDLE_MAX_SIZE + TEE_ID_TRUSTY)
		*h = &_sys_service_h[TEE_ID_TRUSTY];
	else if (session == KREE_SESSION_HANDLE_MAX_SIZE + TEE_ID_NEBULA)
		*h = &_sys_service_h[TEE_ID_NEBULA];
	else if (session >= 0 && session < KREE_SESSION_HANDLE_MAX_SIZE)
		*h = &_kree_session_handle_pool[session];
	else
		return -1;
	return 0;
}


struct tipc_k_handle *_FdToHandle(int32_t session)
{
	struct tipc_k_handle *h = 0;
	int ret;

	ret = _getSessionHandle(session, &h);
	if (ret < 0)
		KREE_ERR(" %s: can not get seesion handle!\n", __func__);

	return h;
}

int32_t _HandleToFd(struct tipc_k_handle h)
{
	return _setSessionHandle(h);
}

static inline struct tipc_dn_chan *_HandleToChanInfo(struct tipc_k_handle *handle)
{
	return handle ? (struct tipc_dn_chan *)(handle->dn) : NULL;
}

TZ_RESULT KREE_SessionToTID(KREE_SESSION_HANDLE session, enum tee_id_t *o_tid)
{
	struct tipc_dn_chan *dn = _HandleToChanInfo(_FdToHandle(session));

	if (dn) {
		*o_tid = dn->tee_id;
		return TZ_RESULT_SUCCESS;
	}

	port_lookup_tid("com.default.tee_id", o_tid);
	pr_info("[%s] session %d to tee id failed, return default %d\n",
		__func__, session, *o_tid);
	return TZ_RESULT_ERROR_NO_DATA;
}
EXPORT_SYMBOL(KREE_SessionToTID);

void KREE_SESSION_LOCK(int32_t handle)
{
	struct tipc_dn_chan *chan_p = _HandleToChanInfo(_FdToHandle(handle));

	if (!chan_p)
		return;

	if (!is_tee_id(chan_p->tee_id))
		return;

	if (handle != _sys_service_Fd[chan_p->tee_id])
		mutex_lock(&chan_p->sess_lock);
}

void KREE_SESSION_UNLOCK(int32_t handle)
{
	struct tipc_dn_chan *chan_p = _HandleToChanInfo(_FdToHandle(handle));

	if (!chan_p)
		return;

	if (!is_tee_id(chan_p->tee_id))
		return;

	if (handle != _sys_service_Fd[chan_p->tee_id])
		mutex_unlock(&chan_p->sess_lock);
}

static TZ_RESULT KREE_OpenSysFd(uint32_t tee_id)
{
	TZ_RESULT ret = TZ_RESULT_SUCCESS;

	ret = _tipc_k_connect_retry(&_sys_service_h[tee_id],
				    gz_sys_service_name[tee_id]);
	if (ret < 0) {
		KREE_DEBUG("%s: Failed to connect to service, ret = %d\n",
			   __func__, ret);
		return TZ_RESULT_ERROR_COMMUNICATION;
	}

	_sys_service_Fd[tee_id] = KREE_SESSION_HANDLE_MAX_SIZE + tee_id;

	KREE_DEBUG("===> %s: chan_p = %p, tee_id %d\n", __func__,
		   _sys_service_h[tee_id].dn, tee_id);

	return ret;
}

static TZ_RESULT KREE_OpenFd(const char *port, int32_t *Fd)
{
	struct tipc_k_handle h = {.dn = NULL};
	TZ_RESULT ret = TZ_RESULT_SUCCESS;
	int32_t tmp;

	*Fd = -1; /* invalid fd */

	if (!port)
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

	KREE_DEBUG(" ===> %s: %s.\n", __func__, port);
	ret = _tipc_k_connect_retry(&h, port);
	if (ret < 0) {
		KREE_ERR("%s: Failed to connect to service, ret = %d\n",
			 __func__, ret);
		return TZ_RESULT_ERROR_COMMUNICATION;
	}
	tmp = _HandleToFd(h);
	if (tmp < 0) {
		KREE_ERR("%s: Failed to get session\n", __func__);
		return TZ_RESULT_ERROR_OUT_OF_MEMORY;
	}

	*Fd = tmp;

	KREE_DEBUG("%s: Fd = %d, chan_p = %p\n", __func__, *Fd, h.dn);

	return ret;
}

static TZ_RESULT KREE_CloseFd(int32_t Fd)
{

	int rc;
	struct tipc_k_handle *h;
	int ret = TZ_RESULT_SUCCESS;

	KREE_DEBUG(" ===> %s: Close FD %u\n", __func__, Fd);

	h = _FdToHandle(Fd); /* verify Fd inside */
	if (!h) {
		KREE_ERR("%s: get tipc handle failed\n", __func__);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	if (!h->dn) {
		KREE_ERR("%s: get tipc dn channel failed\n", __func__);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	rc = tipc_k_disconnect(h);
	if (rc) {
		KREE_ERR("%s: tipc_k_disconnect failed\n", __func__);
		ret = TZ_RESULT_ERROR_COMMUNICATION;
	}

	_clearSessionHandle(Fd);

	return ret;
}

/* GZ client issue command */
int _gz_client_cmd(int32_t Fd, int session, unsigned int cmd, void *param,
		   int param_size)
{
	ssize_t rc;
	struct tipc_k_handle *handle;

	handle = _FdToHandle(Fd);
	if (!handle) {
		KREE_ERR("%s: get tipc handle failed\n", __func__);
		return -1;
	}

	KREE_DEBUG(" ===> %s: command = %d.\n", __func__, cmd);
	KREE_DEBUG(" ===> %s: param_size = %d.\n", __func__, param_size);
	rc = _tipc_k_write_retry(handle, param, param_size, O_RDWR);
	KREE_DEBUG(" ===> %s: tipc_k_write rc = %d.\n", __func__, (int)rc);

	return rc;
}

/* GZ client wait command ack and return value */
int _gz_client_wait_ret(int32_t Fd, struct gz_syscall_cmd_param *data)
{
	ssize_t rc;
	struct tipc_k_handle *handle;
	int size;

	handle = _FdToHandle(Fd);
	if (!handle) {
		KREE_ERR("%s: get tipc handle failed\n", __func__);
		return -1;
	}

	KREE_DEBUG(" ===> %s: tipc_k_read\n", __func__);
	rc = _tipc_k_read_retry(handle, (void *)data,
				sizeof(struct gz_syscall_cmd_param), O_RDWR);
	size = data->payload_size;
	KREE_DEBUG(" ===> %s: tipc_k_read(1) rc = %d.\n", __func__, (int)rc);
	KREE_DEBUG(" ===> %s: data payload size = %d.\n", __func__, size);
#if debugFg
	if (size > GZ_MSG_DATA_MAX_LEN) {
		KREE_ERR("%s: ret payload size(%d) exceed max len(%d)\n",
			__func__, size, GZ_MSG_DATA_MAX_LEN);
		return -1;
	}
#endif

	/* FIXME!!!: need to check return... */

	return rc;
}

static void recover_64_params(union MTEEC_PARAM *dst, union MTEEC_PARAM *src,
			      uint32_t types)
{
	int i;
	uint32_t type;

	for (i = 0; i < 4; i++) {
		type = TZ_GetParamTypes(types, i);
		if (type == TZPT_MEM_OUTPUT || type == TZPT_MEM_INOUT
		    || type == TZPT_MEM_INPUT) {
			KREE_DEBUG("RP: recover %p to %p(%u)\n",
				   dst[i].mem.buffer, src[i].mem.buffer,
				   src[i].mem.size);
			dst[i].mem.buffer = src[i].mem.buffer;
			dst[i].mem.size = src[i].mem.size;
		}
	}
}

#ifdef GP_PARAM_DEBUG
static void report_param_byte(struct gz_syscall_cmd_param *param)
{
	int i, len;
	char *ptr = (char *)&(param->param);

	len = sizeof(union MTEEC_PARAM) * 4;
	KREE_DEBUG("==== report param byte ====\n");
	KREE_DEBUG("head of param: %p\n", param->param);
	KREE_DEBUG("len = %d\n", len);
	for (i = 0; i < len; i++) {
		KREE_DEBUG("byte[%d]: 0x%x\n", i, *ptr);
		ptr++;
	}
}

static void report_param(struct gz_syscall_cmd_param *param)
{
	int i;
	union MTEEC_PARAM *param_p = (union MTEEC_PARAM *) param->param;

	KREE_DEBUG("session: 0x%x, command: 0x%x\n", param->handle,
		   param->command);
	KREE_DEBUG("paramTypes: 0x%x\n", param->paramTypes);
	for (i = 0; i < 4; i++)
		KREE_DEBUG("param[%d]: 0x%x, 0x%x\n", i, param_p[i].value.a,
			   param_p[i].value.b);
}
#endif

static int GZ_CopyMemToBuffer(struct gz_syscall_cmd_param *param)
{
	uint32_t type, size, offset = 0;
	int i;
	void *addr;
	void *end_buf = &param->data[GZ_MSG_DATA_MAX_LEN];

	for (i = 0; i < 4; i++) {
		type = TZ_GetParamTypes(param->paramTypes, i);

		if (type == TZPT_MEM_INPUT || type == TZPT_MEM_INOUT) {
			addr = param->param[i].mem.buffer;
			size = param->param[i].mem.size;

			/* check mem addr & size */
			if (!addr) {
				KREE_DEBUG("CMTB: empty gp mem addr\n");
				continue;
			}
			if (!size) {
				KREE_DEBUG("CMTB: mem size=0\n");
				continue;
			}
/* NOTE: currently a single mem param can use up to GZ_MSG_DATA_MAX_LEN */
/* Users just need to make sure that total does not exceed the limit */
#if debugFg
			if (size > GZ_MEM_MAX_LEN) {
				KREE_DEBUG("CMTB: invalid gp mem size: %u\n",
					size);
				return -1;
			}
#endif

			/* if (offset >= GZ_MSG_DATA_MAX_LEN) { */
			if ((void *)&param->data[offset + size] > end_buf) {
				KREE_ERR("CMTB: ERR: buffer overflow\n");
				return -1;
			}

			KREE_DEBUG("CMTB: cp param %d from %p to %p, size %u\n",
				   i, addr, (void *)&param->data[offset], size);
			memcpy((void *)&param->data[offset], addr, size);
#ifdef FIX_MEM_DATA_BUFFER
			offset += GZ_MEM_MAX_LEN;
#else
			offset += size;
#endif
		}
	}
	KREE_DEBUG("CMTB: total %u bytes copied\n", offset);
	return offset;
}

static int GZ_CopyMemFromBuffer(struct gz_syscall_cmd_param *param)
{
	uint32_t type, size, offset = 0;
	int i;
	void *addr;
	void *end_buf = &param->data[GZ_MSG_DATA_MAX_LEN];

	KREE_DEBUG("CMTB: end of buf: %p\n", end_buf);

	for (i = 0; i < 4; i++) {
		type = TZ_GetParamTypes(param->paramTypes, i);

		if (type == TZPT_MEM_OUTPUT || type == TZPT_MEM_INOUT) {
			addr = param->param[i].mem.buffer;
			size = param->param[i].mem.size;

			/* check mem addr & size */
			if (!addr) {
				KREE_DEBUG("CMTB: empty gp mem addr\n");
				continue;
			}
			if (!size) {
				KREE_DEBUG("CMTB: mem size=0\n");
				continue;
			}
/* NOTE: currently a single mem param can use up to GZ_MSG_DATA_MAX_LEN */
/* Users just need to make sure that total does not exceed the limit */
#if debugFg
			if (size > GZ_MEM_MAX_LEN) {
				KREE_DEBUG("CMTB: invalid gp mem size: %u\n",
					size);
				return -1;
			}
#endif

			/* if (offset >= GZ_MSG_DATA_MAX_LEN) { */
			if ((void *)&param->data[offset + size] > end_buf) {
				KREE_ERR("CMFB: ERR: buffer overflow\n");
				break;
			}

			KREE_DEBUG(
				"CMFB: copy param %d from %p to %p, size %d\n",
				i, (void *)&param->data[offset], addr, size);
			memcpy(addr, (void *)&param->data[offset], size);
#ifdef FIX_MEM_DATA_BUFFER
			offset += GZ_MEM_MAX_LEN;
#else
			offset += size;
#endif
		}
	}
	return offset;
}


void GZ_RewriteParamMemAddr(struct gz_syscall_cmd_param *param)
{
	uint32_t type, size, offset = 0;
	int i;
	union MTEEC_PARAM *param_p = (union MTEEC_PARAM *) param->param;

	KREE_DEBUG("RPMA: head of param: %p\n", param_p);

	for (i = 0; i < 4; i++) {
		type = TZ_GetParamTypes(param->paramTypes, i);

		if (type == TZPT_MEM_OUTPUT || type == TZPT_MEM_INOUT) {
			size = param_p[i]
				       .mem
				       .size; /* GZ use mem rather than mem64 */
			KREE_DEBUG("RPMA: param %d mem size: %u\n", i, size);

			param_p[i].mem.buffer = &(param->data[offset]);
			param_p[i].mem.size = size;
			KREE_DEBUG("RPMA: head of param[%d]: %p\n", i,
				   &param_p[i]);
			KREE_DEBUG("RPMA: rewrite param %d mem addr: %p\n", i,
				   param_p[i].mem.buffer);
			KREE_DEBUG("RPMA: rewrite param %d mem size: %u\n", i,
				   param_p[i].mem.size);

			if (offset >= GZ_MSG_DATA_MAX_LEN) {
				KREE_ERR("CMTB: ERR: buffer overflow\n");
				break;
			}
#ifdef FIX_MEM_DATA_BUFFER
			offset += GZ_MEM_MAX_LEN;
#else
			offset += size;
#endif
		}
	}
#ifdef GP_PARAM_DEBUG
	KREE_DEBUG("======== in RPMA =========\n");
	report_param_byte(param);
	KREE_DEBUG("======== leave RPMA =========\n");
#endif
}


void make_64_params_local(union MTEEC_PARAM *dst, uint32_t types)
{
	int i;
	long addr;
	uint32_t type, size;
	uint64_t tmp;
	uint64_t high = 0, low = 0;

	for (i = 0; i < 4; i++) {
		type = TZ_GetParamTypes(types, i);
		if (type == TZPT_MEM_INPUT || type == TZPT_MEM_INOUT
		    || type == TZPT_MEM_OUTPUT) {
			KREE_DEBUG("make 64 params local, addr: %p/%u\n",
				   dst[i].mem.buffer, dst[i].mem.size);
			size = dst[i].mem.size;
			addr = (long)dst[i].mem.buffer;
			high = addr;
			high >>= 32;
			high <<= 32;
			low = (addr & 0xffffffff);
			KREE_DEBUG(
				"make 64 params local, high/low: 0x%llx/0x%llx\n",
				high, low);
			tmp = (high | low);

			dst[i].mem64.buffer = tmp;
			dst[i].mem64.size = size;
			KREE_DEBUG(
				"make 64 params local, new addr: 0x%llx/%u\n",
				dst[i].mem64.buffer, dst[i].mem64.size);
		}
	}
}


/* GZ Ree service
 */
enum GZ_ReeServiceCommand {

	REE_SERVICE_CMD_BASE = 0x0,
	REE_SERVICE_CMD_ADD,
	REE_SERVICE_CMD_MUL,
	REE_SERVICE_CMD_NEW_THREAD,
	REE_SERVICE_CMD_KICK_SEM,
	REE_SERVICE_CMD_TEE_INIT_CTX,
	REE_SERVICE_CMD_TEE_FINAL_CTX,
	REE_SERVICE_CMD_TEE_OPEN_SE,
	REE_SERVICE_CMD_TEE_CLOSE_SE,
	REE_SERVICE_CMD_TEE_INVOK_CMD,
	REE_SERVICE_CMD_END
};


struct nop_param {
	uint32_t type; /* 1: new thread, 2: kick semaphore */
	uint64_t value;
	uint32_t boost_enabled;
};

struct nop_param new_param;

int ree_dummy_thread(void *data)
{
	/* int curr_cpu = get_cpu(); */
	/* uint32_t boost_enabled = 0; */
	struct platform_device *pdev = (struct platform_device *)data;
	struct device *trusty_dev = NULL;
	struct sched_param param = { .sched_priority = 1 };
	struct trusty_nop nop;
	int ret;

	KREE_DEBUG("%s +++++\n", __func__);

	if (!data) {
		KREE_ERR("%s failed to get data %p\n", __func__, data);
		return 0;
	}

	trusty_dev = pdev->dev.parent;
	if (!pdev) {
		KREE_ERR("%s failed to get device %p\n", __func__, trusty_dev);
		return 0;
	}

	sched_setscheduler(current, SCHED_FIFO, &param);
	trusty_nop_init(&nop, 0, 0, 0);

	while (1) {
		ret = wait_for_completion_interruptible(&ree_dummy_event);
		if (ret != 0)
			KREE_DEBUG("%s: wait_for_completion failed, ret=%d %p",
				__func__, ret, get_current());

		/* REE_SERVICE_CMD_KICK_SEM */
		if (new_param.type == 2)
			usleep_range(100, 200);

		nop.args[0] = new_param.type;
		nop.args[1] = (uint32_t)new_param.value;
		nop.args[2] = (uint32_t)(new_param.value >> 32);

		/* get into GZ through NOP SMC call */
		trusty_enqueue_nop(trusty_dev, &nop, smp_processor_id());
	}
	KREE_ERR("%s leave(%d)\n", __func__, ret);

	return 0;
}

static int ree_service_threads(uint32_t type, uint32_t val_a, uint32_t val_b,
			       uint32_t param1_val_b)
{
	struct cpumask ree_cpumask;

	new_param.type = type;
	new_param.value = (((uint64_t)val_b) << 32) | val_a;
	new_param.boost_enabled = (param1_val_b & 0x00ff0000)>>16;

	cpumask_copy(&ree_cpumask, cpu_all_mask);
	cpumask_clear_cpu(smp_processor_id(), &ree_cpumask);

	if (!cpumask_empty(&ree_cpumask))
		set_cpus_allowed_ptr(ree_dummy_task, &ree_cpumask);

	complete(&ree_dummy_event);
	return 0;
}

TZ_RESULT _Gz_KreeServiceCall_body(KREE_SESSION_HANDLE handle, uint32_t command,
				   uint32_t paramTypes,
				   union MTEEC_PARAM param[4])
{
	int ret;

	/*
	 * NOTE: Only support VALUE type gp parameters
	 */
	KREE_DEBUG("=====> session %d, command %x\n", handle, command);
	KREE_DEBUG("=====> %s, param values %x %x %x %x\n", __func__,
		   param[0].value.a, param[1].value.a, param[2].value.a,
		   param[3].value.a);

	switch (command) {
	case REE_SERVICE_CMD_ADD:
		param[2].value.a = param[0].value.a + param[1].value.a;
		break;

	case REE_SERVICE_CMD_MUL:
		param[2].value.a = param[0].value.a * param[1].value.a;
		break;

	case REE_SERVICE_CMD_NEW_THREAD:
		KREE_DEBUG(
			"[REE service call] fork threads 0x%x%x, parm1ValB=0x%x\n",
			param[0].value.b, param[0].value.a, param[1].value.b);
		ret = ree_service_threads(1, (uint32_t)param[0].value.a,
					  (uint32_t)param[0].value.b,
					  (uint32_t)param[1].value.b);
		param[1].value.a = ret;
		break;

	case REE_SERVICE_CMD_KICK_SEM:
		KREE_DEBUG(
			"[REE service call] kick semaphore 0x%x%x, parm1ValB=0x%x\n",
			param[0].value.b, param[0].value.a, param[1].value.b);
		ret = ree_service_threads(2, (uint32_t)param[0].value.a,
					  (uint32_t)param[0].value.b,
					  (uint32_t)param[1].value.b);
		param[1].value.a = ret;
		break;

#if IS_ENABLED(CONFIG_TEE)
	case REE_SERVICE_CMD_TEE_INIT_CTX:
		ret = TEEC_InitializeContext(
			(char *)param[0].mem.buffer,
			(struct TEEC_Context *)param[1].mem.buffer);
		if (ret != TEEC_SUCCESS)
			KREE_ERR("[ERROR] TEEC_InitializeContext failed: %x\n",
				 ret);
		param[2].value.a = ret;
		break;

	case REE_SERVICE_CMD_TEE_FINAL_CTX:
		TEEC_FinalizeContext(
			(struct TEEC_Context *)param[0].mem.buffer);
		break;

	case REE_SERVICE_CMD_TEE_OPEN_SE:
		ret = TEEC_OpenSession(
			(struct TEEC_Context *)param[0].mem.buffer,
			(struct TEEC_Session *)param[1].mem.buffer,
			(struct TEEC_UUID *)param[2].mem.buffer,
			TEEC_LOGIN_PUBLIC, NULL, NULL, NULL);
		if (ret != TEEC_SUCCESS)
			KREE_ERR("[ERROR] TEEC_OpenSession failed: %x\n", ret);
		param[3].value.a = ret;
		break;

	case REE_SERVICE_CMD_TEE_CLOSE_SE:
		TEEC_CloseSession((struct TEEC_Session *)param[0].mem.buffer);
		break;

	case REE_SERVICE_CMD_TEE_INVOK_CMD:
		ret = TEEC_InvokeCommand(
			(struct TEEC_Session *)param[0].mem.buffer,
			param[1].value.a,
			(struct TEEC_Operation *)param[2].mem.buffer, NULL);
		if (ret != TEEC_SUCCESS)
			KREE_ERR("[ERROR] TEEC_InvokeCommand failed: %x\n",
				 ret);
		param[3].value.a = ret;
		break;
#endif

	default:
		KREE_DEBUG("[REE service call] %s: invalid command = 0x%x\n",
			   __func__, command);
		break;
	}

	return 0;
}


TZ_RESULT _GzServiceCall_body(int32_t Fd, unsigned int cmd,
			      struct gz_syscall_cmd_param *param,
			      union MTEEC_PARAM origin[4])
{
	int ret = TZ_RESULT_SUCCESS;
	int rc, copied = 0;
	KREE_SESSION_HANDLE session;
	struct tipc_dn_chan *chan_p;
	struct gz_syscall_cmd_param *ree_param = NULL;

	/* get session from Fd */
	chan_p = _HandleToChanInfo(_FdToHandle(Fd));
	if (!chan_p) {
		KREE_ERR("%s: NULL chan_p, invalid Fd\n", __func__);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}
	session = chan_p->session;
	KREE_DEBUG("%s: session = %d.\n", __func__, (int)session);

	/* make input */
	param->handle = session;
	copied = GZ_CopyMemToBuffer(param);
	if (copied < 0) {
		KREE_ERR(" invalid input gp params in service call\n");
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}
	param->payload_size = copied;

	make_64_params_local(param->param, param->paramTypes);

/* send command to establish system session in geniezone */
#ifdef DYNAMIC_TIPC_LEN
	rc = _gz_client_cmd(Fd, session, cmd, param,
			    GZ_MSG_HEADER_LEN + param->payload_size);
#else
	rc = _gz_client_cmd(Fd, session, cmd, param, sizeof(*param));
#endif
	if (rc < 0) {
		KREE_ERR("%s: gz client cmd failed\n", __func__);
		return TZ_RESULT_ERROR_COMMUNICATION;
	}

	/*  Allocate cmd param */
	ree_param = kzalloc(sizeof(struct gz_syscall_cmd_param), GFP_KERNEL);
	if (!ree_param) {
		KREE_ERR("%s: alloc ree_param failed\n", __func__);
		return TZ_RESULT_ERROR_OUT_OF_MEMORY;
	}

	/* keeps serving REE call until ends */
	while (1) {
		rc = _gz_client_wait_ret(Fd, ree_param);
		if (rc < 0) {
			KREE_ERR("%s: wait ret failed(%d)\n", __func__, rc);
			ret = TZ_RESULT_ERROR_COMMUNICATION;
			break;
		}
		KREE_DEBUG("=====> %s, ree service %d\n", __func__,
			   ree_param->ree_service);

		/* TODO: ret = ree_param->ret */

		/* check if REE service */
		if (ree_param->ree_service == 0) {
			KREE_DEBUG("=====> %s, general return!!!!\n", __func__);
			memcpy(param, ree_param, sizeof(*param));
			recover_64_params(param->param, origin,
					  param->paramTypes);

			copied = GZ_CopyMemFromBuffer(param);
			if (copied < 0) {
				KREE_ERR(" invalid output gp params\n");
				ret = TZ_RESULT_ERROR_BAD_PARAMETERS;
			}
			break;
		} else if (ree_param->ree_service != 1) {
			KREE_ERR("invalid ree_service value\n");
			break;
		}

		/* REE service main function */
		GZ_RewriteParamMemAddr(ree_param);
		_Gz_KreeServiceCall_body(ree_param->handle, ree_param->command,
					 ree_param->paramTypes,
					 ree_param->param);

		/* return param to GZ */
		copied = GZ_CopyMemToBuffer(ree_param);
		if (copied < 0) {
			KREE_ERR(" invalid gp params\n");
			break;
		}
		ree_param->payload_size = copied;

		make_64_params_local(ree_param->param, ree_param->paramTypes);
#ifdef DYNAMIC_TIPC_LEN
		rc = _gz_client_cmd(Fd, session, 0, ree_param,
				    GZ_MSG_HEADER_LEN
					    + ree_param->payload_size);
#else
		rc = _gz_client_cmd(Fd, session, 0, ree_param,
				    sizeof(struct gz_syscall_cmd_param));
#endif
		if (rc < 0) {
			KREE_ERR("%s: gz client cmd failed\n", __func__);
			ret = TZ_RESULT_ERROR_COMMUNICATION;
			break;
		}

		KREE_DEBUG("=========> %s _GzRounter done\n", __func__);
	}

	kfree(ree_param);

	return ret;
}

static void kree_perf_boost(int enable)
{
	/* struct ppm_limit_data freq_to_set[2]; */

	mutex_lock(&perf_boost_lock);
	/* KREE_ERR("%s %s\n", __func__, enable>0?"enable":"disable"); */

	if (enable) {
		if (perf_boost_cnt == 0) {
			KREE_DEBUG("%s wake_lock\n", __func__);
#if IS_ENABLED(CONFIG_PM_SLEEP)
			__pm_stay_awake(TeeServiceCall_wake_lock); /*4.19*/
#endif
		}
		perf_boost_cnt++;
	} else {
		if (perf_boost_cnt == 1) {
			KREE_DEBUG("%s wake_unlock\n", __func__);
#if IS_ENABLED(CONFIG_PM_SLEEP)
			__pm_relax(TeeServiceCall_wake_lock); /*4.19*/
#endif
		}
		if (perf_boost_cnt > 0)
			perf_boost_cnt--;
	}

	mutex_unlock(&perf_boost_lock);
}

TZ_RESULT KREE_CreateSession(const char *ta_uuid, KREE_SESSION_HANDLE *pHandle)
{
	TZ_RESULT ret = TZ_RESULT_SUCCESS;
	int32_t chan_fd;
	struct tipc_dn_chan *chan_p;
	union MTEEC_PARAM p[4];
	KREE_SESSION_HANDLE session;
	uint tee_id;

	KREE_DEBUG("%s: %s\n", __func__, ta_uuid);

	if (!pHandle || !ta_uuid)
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

	mutex_lock(&session_mutex);

	/* connect to target service */
	ret = KREE_OpenFd(ta_uuid, &chan_fd);
	if (ret) {
		KREE_ERR("%s: open fd fail\n", __func__);
		goto create_session_out;
	}

	chan_p = _HandleToChanInfo(_FdToHandle(chan_fd));
	if (!chan_p) {
		KREE_ERR("%s: NULL chan_p, invalid Fd\n", __func__);
		ret = TZ_RESULT_ERROR_BAD_PARAMETERS;
		goto create_session_out;
	}
	KREE_DEBUG("===> _GzCreateSession_body: chan_p = 0x%llx\n",
		   (uint64_t)chan_p);

	tee_id = chan_p->tee_id;
	KREE_DEBUG("%s: get tee_id %d\n", __func__, tee_id);

	/* connect to sys service */
	if (_sys_service_Fd[tee_id] < 0) {
		KREE_DEBUG("%s: Open sys service fd first.\n", __func__);
		ret = KREE_OpenSysFd(tee_id);
		if (ret) {
			KREE_ERR("%s: open sys service fd failed, ret = 0x%x\n",
				 __func__, ret);
			goto create_session_out;
		}
	}

	p[0].mem.buffer = (void *)ta_uuid;
	p[0].mem.size = (uint32_t)(strnlen(ta_uuid, MAX_UUID_LEN) + 1);
	ret = KREE_TeeServiceCall(
		_sys_service_Fd[tee_id], TZCMD_SYS_SESSION_CREATE,
		TZ_ParamTypes2(TZPT_MEM_INPUT, TZPT_VALUE_OUTPUT), p);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("%s: create session fail\n", __func__);
		goto create_session_out;
	}
	session = p[1].value.a;

	KREE_DEBUG(" ===> %s: Get session = %d.\n", __func__, (int)session);

	chan_p->session = session;
	mutex_init(&chan_p->sess_lock);

create_session_out:
	*pHandle = chan_fd;
	mutex_unlock(&session_mutex);

	return ret;
}
EXPORT_SYMBOL(KREE_CreateSession);

TZ_RESULT KREE_CloseSession(KREE_SESSION_HANDLE handle)
{
	TZ_RESULT ret = TZ_RESULT_SUCCESS;
	int32_t Fd;
	KREE_SESSION_HANDLE session;
	union MTEEC_PARAM p[4];
	struct tipc_dn_chan *chan_p;
	enum tee_id_t tee_id;

	KREE_DEBUG(" ===> %s: Close session ...\n", __func__);

	mutex_lock(&session_mutex);

	Fd = handle;

	/* get session from Fd */
	chan_p = _HandleToChanInfo(_FdToHandle(Fd));
	if (!chan_p) {
		KREE_ERR("%s: NULL chan_p, invalid Fd\n", __func__);
		ret = TZ_RESULT_ERROR_BAD_PARAMETERS;
		goto close_session_out;
	}

	tee_id = chan_p->tee_id;
	if (!is_tee_id(tee_id) || _sys_service_Fd[tee_id] < 0) {
		KREE_ERR("%s: sys service fd is not open.\n", __func__);
		ret = TZ_RESULT_ERROR_GENERIC;
		goto close_session_out;
	}

	session = chan_p->session;
	KREE_DEBUG(" ===> %s: session = %d.\n", __func__, (int)session);

	/* close session */
	p[0].value.a = session;
	ret = KREE_TeeServiceCall(_sys_service_Fd[tee_id],
				  TZCMD_SYS_SESSION_CLOSE,
				  TZ_ParamTypes1(TZPT_VALUE_INPUT), p);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("%s: close session fail\n", __func__);
		goto close_session_out;
	}

	mutex_destroy(&chan_p->sess_lock);

	/* close Fd */
	ret = KREE_CloseFd(Fd);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("%s: close FD fail\n", __func__);
		goto close_session_out;
	}

close_session_out:
	mutex_unlock(&session_mutex);

	return ret;
}
EXPORT_SYMBOL(KREE_CloseSession);

TZ_RESULT KREE_TeeServiceCallPlus(KREE_SESSION_HANDLE handle, uint32_t command,
				  uint32_t paramTypes, union MTEEC_PARAM param[4],
				  int32_t cpumask)
{
	int iret;
	struct gz_syscall_cmd_param *cparam = NULL;
	int32_t Fd;
	struct tipc_dn_chan *chan_p;

	Fd = handle;

	chan_p = _HandleToChanInfo(_FdToHandle(handle));
	if (!chan_p) {
		KREE_ERR("%s: get tipc dn channel failed\n", __func__);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	/* pass cpumask to channel */
	chan_p->cpumask = cpumask;

	cparam = kzalloc(sizeof(struct gz_syscall_cmd_param), GFP_KERNEL);
	if (!cparam) {
		KREE_ERR("%s: alloc cparam failed\n", __func__);
		return TZ_RESULT_ERROR_OUT_OF_MEMORY;
	}

	cparam->command = command;
	cparam->paramTypes = paramTypes;
	memcpy(cparam->param, param, sizeof(union MTEEC_PARAM) * 4);

	KREE_DEBUG(" ===> KREE Tee Service Call cmd = %d / %d\n", command,
		   cparam->command);

	if (cpumask == -1)
		mutex_lock(&servicecall_lock);

	KREE_SESSION_LOCK(Fd);
	kree_perf_boost(1);

	iret = _GzServiceCall_body(Fd, command, cparam, param);

	kree_perf_boost(0);
	KREE_SESSION_UNLOCK(Fd);

	if (cpumask == -1)
		mutex_unlock(&servicecall_lock);

	memcpy(param, cparam->param, sizeof(union MTEEC_PARAM) * 4);

	kfree(cparam);

	return iret;
}
EXPORT_SYMBOL(KREE_TeeServiceCallPlus);

TZ_RESULT KREE_TeeServiceCall(KREE_SESSION_HANDLE handle, uint32_t command,
			      uint32_t paramTypes, union MTEEC_PARAM param[4])
{
	return KREE_TeeServiceCallPlus(handle, command, paramTypes, param, -1);
}
EXPORT_SYMBOL(KREE_TeeServiceCall);

#if debugFg
#include "tz_cross/tz_error_strings.h"

const char *TZ_GetErrorString(TZ_RESULT res)
{
	return _TZ_GetErrorString(res);
}
EXPORT_SYMBOL(TZ_GetErrorString);
#endif

/***** System Hareware Counter *****/

u64 KREE_GetSystemCnt(void)
{
	u64 cnt = 0;

	/*cnt = arch_counter_get_cntvct();*/
	cnt = __arch_counter_get_cntvct(); /*5.4*/
	return cnt;
}
EXPORT_SYMBOL(KREE_GetSystemCnt);

u32 KREE_GetSystemCntFrq(void)
{
	u32 freq = 0;

	freq = arch_timer_get_cntfrq();
	return freq;
}
EXPORT_SYMBOL(KREE_GetSystemCntFrq);

static int tz_system_probe(struct platform_device *pdev)
{
	int ret = 0;

	KREE_DEBUG("%s\n", __func__);

	init_completion(&ree_dummy_event);

	ree_dummy_task = kthread_create(ree_dummy_thread, pdev, "ree_dummy_task");
	if (IS_ERR(ree_dummy_task)) {
		KREE_ERR("Unable to start kernel thread %s\n",
			__func__);
		ret = PTR_ERR(ree_dummy_task);
	} else {
		set_user_nice(ree_dummy_task, -20);
		wake_up_process(ree_dummy_task);
	}

	tz_system_dev = pdev;

	return ret;
}
static int tz_system_remove(struct platform_device *pdev)
{
	KREE_DEBUG("%s\n", __func__);

	return 0;
}

#define MODULE_NAME "trusty_gz"
static const struct of_device_id tz_system_of_match[] = {
	{ .compatible = "mediatek,trusty-gz", },
	{},
};
MODULE_DEVICE_TABLE(of, tz_system_of_match);

struct platform_driver tz_system_driver = {
	.probe = tz_system_probe,
	.remove = tz_system_remove,
	.driver	= {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tz_system_of_match,
	},
};
EXPORT_SYMBOL(tz_system_driver);

int tz_system_std_call32(u32 smcnr, u32 a0, u32 a1, u32 a2)
{
	return trusty_std_call32(tz_system_dev->dev.parent,
				smcnr, a0, a1, a2);
}

MODULE_LICENSE("GPL v2");
