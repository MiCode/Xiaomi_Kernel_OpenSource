/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
#include <linux/trusty/trusty_ipc.h>
#include <kree/system.h>
#include <kree/mem.h>
#include <linux/atomic.h>
#include <linux/vmalloc.h>
#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif
#include <linux/delay.h>

#include <tz_cross/trustzone.h>
#include <tz_cross/ta_system.h>

#include "trusty-nop.h"

/* FIXME: MTK_PPM_SUPPORT is disabled temporarily */
#ifdef MTK_PPM_SUPPORT
#ifdef CONFIG_MACH_MT6758
#include "legacy_controller.h"
#else
#include "mtk_ppm_platform.h"
#endif
#endif

#ifdef CONFIG_MTK_TEE_GP_SUPPORT
#include "tee_client_api.h"
#endif

#ifdef CONFIG_ARM64
#define ARM_SMC_CALLING_CONVENTION
#endif

/* #define DBG_KREE_SYS */
#ifdef DBG_KREE_SYS
#define KREE_DEBUG(fmt...) pr_debug("[KREE]" fmt)
#define KREE_INFO(fmt...) pr_info("[KREE]" fmt)
#define KREE_ERR(fmt...) pr_info("[KREE][ERR]" fmt)
#else
#define KREE_DEBUG(fmt...)
#define KREE_INFO(fmt...) pr_info("[KREE]" fmt)
#define KREE_ERR(fmt...) pr_info("[KREE][ERR]" fmt)
#endif

#define DYNAMIC_TIPC_LEN

DEFINE_MUTEX(fd_mutex);
DEFINE_MUTEX(session_mutex);

#define GZ_SYS_SERVICE_NAME "com.mediatek.geniezone.srv.sys"

#define KREE_SESSION_HANDLE_MAX_SIZE 512
#define KREE_SESSION_HANDLE_SIZE_MASK (KREE_SESSION_HANDLE_MAX_SIZE - 1)

static struct cpumask trusty_all_cmask;
static struct cpumask trusty_big_cmask;
#ifdef CONFIG_PM_WAKELOCKS
struct wakeup_source TeeServiceCall_wake_lock;
#else
struct wake_lock TeeServiceCall_wake_lock;
#endif
static struct mutex perf_boost_lock;
static int perf_boost_cnt;

static int32_t _sys_service_Fd = -1; /* only need to open sys service once */
static struct tipc_k_handle _sys_service_h;

static struct tipc_k_handle
	_kree_session_handle_pool[KREE_SESSION_HANDLE_MAX_SIZE];
static int32_t _kree_session_handle_idx;

#define TIPC_RETRY_MAX_COUNT (100)
#define TIPC_RETRY_WAIT_MS (10)
#define IS_RESTARTSYS_ERROR(err) (err == -ERESTARTSYS)
#define RETRY_REQUIRED(cnt) (cnt <= TIPC_RETRY_MAX_COUNT)
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

static int _tipc_k_connect_retry(struct tipc_k_handle *h, const char *port)
{
	int rc = 0;
	int retry = 0;

	do {
		if (unlikely(IS_RESTARTSYS_ERROR(rc))) {
			struct tipc_dn_chan *dn = h->dn;

			if (_is_tipc_channel_connected(dn)) {
				KREE_DEBUG(
					"%s: channel is connected already!\n",
					__func__);
				return 0;
			}
			KREE_DEBUG("%s: disconnect and retry!\n", __func__);
			tipc_k_disconnect(h);
		}
		rc = tipc_k_connect(h, port);
		retry++;
	} while (_tipc_retry_check_and_wait(rc, retry, 2));

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

void _clearSessionHandle(int32_t session)
{
	mutex_lock(&fd_mutex);
	_kree_session_handle_pool[session].dn = 0;
	mutex_unlock(&fd_mutex);
}


int _getSessionHandle(int32_t session, struct tipc_k_handle **h)
{
	if (session < 0 || session > KREE_SESSION_HANDLE_MAX_SIZE)
		return -1;
	if (session == KREE_SESSION_HANDLE_MAX_SIZE)
		*h = &_sys_service_h;
	else
		*h = &_kree_session_handle_pool[session];
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

#define _HandleToChanInfo(x) (x?(struct tipc_dn_chan *)(x->dn):0)

void KREE_SESSION_LOCK(int32_t handle)
{
	struct tipc_dn_chan *chan_p = _HandleToChanInfo(_FdToHandle(handle));

	if (chan_p != NULL)
		mutex_lock(&chan_p->sess_lock);
}

void KREE_SESSION_UNLOCK(int32_t handle)
{
	struct tipc_dn_chan *chan_p = _HandleToChanInfo(_FdToHandle(handle));

	if (chan_p != NULL)
		mutex_unlock(&chan_p->sess_lock);
}

static TZ_RESULT KREE_OpenSysFd(void)
{
	TZ_RESULT ret = TZ_RESULT_SUCCESS;

	ret = _tipc_k_connect_retry(&_sys_service_h, GZ_SYS_SERVICE_NAME);
	if (ret < 0) {
		KREE_DEBUG("%s: Failed to connect to service, ret = %d\n",
			   __func__, ret);
		return TZ_RESULT_ERROR_COMMUNICATION;
	}
	_sys_service_Fd = KREE_SESSION_HANDLE_MAX_SIZE;

	KREE_DEBUG("===> %s: chan_p = 0x%llx\n", __func__,
		   (uint64_t)_sys_service_h->dn);

	return ret;
}

static TZ_RESULT KREE_OpenFd(const char *port, int32_t *Fd)
{

	struct tipc_k_handle h;
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

	KREE_DEBUG("KREE_OpenFd: Fd = %d, chan_p = 0x%llx\n", *Fd, (uint64_t)h);

	return ret;
}

static TZ_RESULT KREE_CloseFd(int32_t Fd)
{

	int rc;
	struct tipc_k_handle *h;
	int ret = TZ_RESULT_SUCCESS;

	KREE_DEBUG(" ===> %s: Close FD %u\n", __func__, Fd);

	h = _FdToHandle(Fd); /* verify Fd inside */
	if (!h)
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

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

	rc = _tipc_k_read_retry(handle, (void *)data,
				sizeof(struct gz_syscall_cmd_param), O_RDWR);
	size = data->payload_size;
	KREE_DEBUG(" ===> %s: tipc_k_read(1) rc = %d.\n", __func__, (int)rc);
	KREE_DEBUG(" ===> %s: data payload size = %d.\n", __func__, size);
#if 0
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
	union MTEEC_PARAM *param_p = param->param;

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
#if 0
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
#if 0
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
	union MTEEC_PARAM *param_p = param->param;

	KREE_DEBUG("RPMA: head of param: %p\n", param_p);

	for (i = 0; i < 4; i++) {
		type = TZ_GetParamTypes(param->paramTypes, i);

		if (type == TZPT_MEM_OUTPUT || type == TZPT_MEM_INOUT) {
			size = param_p[i]
				       .mem32
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
			KREE_DEBUG(" make_64_params_local, addr: %p/%u\n",
				   dst[i].mem.buffer, dst[i].mem.size);
			size = dst[i].mem.size;
			addr = (long)dst[i].mem.buffer;
			high = addr;
			high >>= 32;
			high <<= 32;
			low = (addr & 0xffffffff);
			KREE_DEBUG(
				" make_64_params_local, high/low: 0x%llx/0x%llx\n",
				high, low);
			tmp = (high | low);

			dst[i].mem64.buffer = tmp;
			dst[i].mem64.size = size;
			KREE_DEBUG(
				" make_64_params_local, new addr: 0x%llx/%u\n",
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

#ifdef MTK_PPM_SUPPORT
struct _cpus_cluster_freq cpus_cluster_freq[NR_PPM_CLUSTERS];
/* static atomic_t boost_cpu[NR_PPM_CLUSTERS]; */
#endif

struct nop_param {
	uint32_t type; /* 1: new thread, 2: kick semaphore */
	uint64_t value;
	uint32_t boost_enabled;
};

static int ree_dummy_thread(void *args)
{
	int ret;
	/* int curr_cpu = get_cpu(); */
	/* uint32_t boost_enabled = 0; */
	uint32_t param_type = 0;
	uint64_t param_value = 0;

	if (args != NULL) {
		/* boost_enabled = ((struct nop_param *)args)->boost_enabled; */
		param_type = ((struct nop_param *)args)->type;
		param_value = ((struct nop_param *)args)->value;
		vfree(args);
	} else {
		KREE_INFO("[ERROR] param is null\n");
	}

	set_user_nice(current, -20);

	usleep_range(100, 500);

	/* get into GZ through NOP SMC call */
	ret = trusty_call_nop_std32(param_type, param_value);
	if (ret != 0)
		KREE_DEBUG("%s: SMC_SC_NOP failed, ret=%d", __func__, ret);


	return 0;
}

static int ree_service_threads(uint32_t type, uint32_t val_a, uint32_t val_b,
			       uint32_t param1_val_b)
{
	int ret = 0;
	struct task_struct *task;
	/* uint32_t cpu = param1_val_b & 0x0000ffff; */
	struct nop_param *param;

	param = vmalloc(sizeof(struct nop_param));
	param->type = type;
	param->value = (((uint64_t)val_b) << 32) | val_a;
	param->boost_enabled = (param1_val_b & 0xffff0000) >> 16;

#if 0
	KREE_INFO("### cpu=%u, boost_enable=%u, raw=0x%x\n", cpu,
		boost_enabled, param1_val_b);
#endif

	task = kthread_create(ree_dummy_thread, param, "ree_dummy_thread");
	ret = (!task) ? 1 : 0;
	set_cpus_allowed_ptr(task, &trusty_big_cmask);
	wake_up_process(task);

	return ret;
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

#ifdef CONFIG_MTK_TEE_GP_SUPPORT
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
	struct gz_syscall_cmd_param *ree_param;

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

	ree_param = kmalloc(sizeof(*ree_param), GFP_KERNEL);
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

		/* TODO: ret = ree_param.ret */

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
				    sizeof(*ree_param));
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
		set_cpus_allowed_ptr(get_current(), &trusty_big_cmask);
		if (perf_boost_cnt == 0) {
			/*
			 * freq_to_set[0].min = cpus_cluster_freq[0].max_freq;
			 * freq_to_set[0].max = cpus_cluster_freq[0].max_freq;
			 * freq_to_set[1].min = cpus_cluster_freq[1].max_freq;
			 * freq_to_set[1].max = cpus_cluster_freq[1].max_freq;
			 * KREE_ERR("%s enable\n", __func__);
			 * update_userlimit_cpu_freq(PPM_KIR_MTEE, 2,
			 * freq_to_set);
			 */
			KREE_DEBUG("%s wake_lock\n", __func__);
#ifdef CONFIG_PM_WAKELOCKS
			__pm_stay_awake(&TeeServiceCall_wake_lock);
#else
			wake_lock(&TeeServiceCall_wake_lock);
#endif
		}
		perf_boost_cnt++;
	} else {
		perf_boost_cnt--;
		if (perf_boost_cnt == 0) {
			/*
			 * freq_to_set[0].min = -1;
			 * freq_to_set[0].max = -1;
			 * freq_to_set[1].min = -1;
			 * freq_to_set[1].max = -1;
			 * update_userlimit_cpu_freq(PPM_KIR_MTEE, 2,
			 * freq_to_set);
			 * KREE_ERR("%s disable\n", __func__);
			 */
			KREE_DEBUG("%s wake_unlock\n", __func__);
#ifdef CONFIG_PM_WAKELOCKS
			__pm_relax(&TeeServiceCall_wake_lock);
#else
			wake_unlock(&TeeServiceCall_wake_lock);
#endif
		}
		set_cpus_allowed_ptr(get_current(), &trusty_all_cmask);
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

	/* connect to sys service */
	if (_sys_service_Fd < 0) {
		KREE_DEBUG("%s: Open sys service fd first.\n", __func__);
		ret = KREE_OpenSysFd();
		if (ret) {
			KREE_ERR("%s: open sys service fd failed, ret = 0x%x\n",
				 __func__, ret);
			goto create_session_out;
		}
	}

	p[0].mem.buffer = (void *)ta_uuid;
	p[0].mem.size = (uint32_t)(strlen(ta_uuid) + 1);
	ret = KREE_TeeServiceCall(
		_sys_service_Fd, TZCMD_SYS_SESSION_CREATE,
		TZ_ParamTypes2(TZPT_MEM_INPUT, TZPT_VALUE_OUTPUT), p);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("%s: create session fail\n", __func__);
		goto create_session_out;
	}
	session = p[1].value.a;

	KREE_DEBUG(" ===> %s: Get session = %d.\n", __func__, (int)session);

	chan_p->session = session;
	mutex_init(&chan_p->sess_lock);

	*pHandle = chan_fd;

create_session_out:
	mutex_unlock(&session_mutex);

	return ret;
}
EXPORT_SYMBOL(KREE_CreateSession);

/*fix mtee sync*/
TZ_RESULT KREE_CreateSessionWithTag(const char *ta_uuid,
				    KREE_SESSION_HANDLE *pHandle,
				    const char *tag)
{
#if 0
	uint32_t paramTypes;
	union MTEEC_PARAM param[4];
	TZ_RESULT ret;

	if (!ta_uuid || !pHandle)
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

	param[0].mem.buffer = (char *)ta_uuid;
	param[0].mem.size = strlen(ta_uuid) + 1;
	param[1].mem.buffer = (char *)tag;
	if (tag != NULL && strlen(tag) != 0)
		param[1].mem.size = strlen(tag) + 1;
	else
		param[1].mem.size = 0;
	paramTypes = TZ_ParamTypes3(TZPT_MEM_INPUT,
					TZPT_MEM_INPUT,
					TZPT_VALUE_OUTPUT);

	ret = KREE_TeeServiceCall(
			(KREE_SESSION_HANDLE) MTEE_SESSION_HANDLE_SYSTEM,
			TZCMD_SYS_SESSION_CREATE_WITH_TAG, paramTypes, param);

	if (ret == TZ_RESULT_SUCCESS)
		*pHandle = (KREE_SESSION_HANDLE)param[2].value.a;

	return ret;
#endif
	KREE_DEBUG(" ===> %s: not support!\n", __func__);
	return -1;
}
EXPORT_SYMBOL(KREE_CreateSessionWithTag);

TZ_RESULT KREE_CloseSession(KREE_SESSION_HANDLE handle)
{
	TZ_RESULT ret = TZ_RESULT_SUCCESS;
	int32_t Fd;
	KREE_SESSION_HANDLE session;
	union MTEEC_PARAM p[4];
	struct tipc_dn_chan *chan_p;

	KREE_DEBUG(" ===> %s: Close session ...\n", __func__);

	if (_sys_service_Fd < 0) {
		KREE_ERR("%s: sys service fd is not open.\n", __func__);
		return TZ_RESULT_ERROR_GENERIC;
	}

	mutex_lock(&session_mutex);

	Fd = handle;

	/* get session from Fd */
	chan_p = _HandleToChanInfo(_FdToHandle(Fd));
	if (!chan_p) {
		KREE_ERR("%s: NULL chan_p, invalid Fd\n", __func__);
		ret = TZ_RESULT_ERROR_BAD_PARAMETERS;
		goto close_session_out;
	}

	session = chan_p->session;
	KREE_DEBUG(" ===> %s: session = %d.\n", __func__, (int)session);

	/* close session */
	p[0].value.a = session;
	ret = KREE_TeeServiceCall(_sys_service_Fd, TZCMD_SYS_SESSION_CLOSE,
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

TZ_RESULT KREE_TeeServiceCall(KREE_SESSION_HANDLE handle, uint32_t command,
			      uint32_t paramTypes, union MTEEC_PARAM param[4])
{
	int iret;
	struct gz_syscall_cmd_param *cparam;
	int32_t Fd;

	Fd = handle;

	kree_perf_boost(1);
	cparam = kmalloc(sizeof(*cparam), GFP_KERNEL);
	cparam->command = command;
	cparam->paramTypes = paramTypes;
	memcpy(&(cparam->param[0]), param, sizeof(union MTEEC_PARAM) * 4);

	KREE_DEBUG(" ===> KREE_TeeServiceCall cmd = %d / %d\n", command,
		   cparam->command);
	iret = _GzServiceCall_body(Fd, command, cparam, param);
	memcpy(param, &(cparam->param[0]), sizeof(union MTEEC_PARAM) * 4);
	kfree(cparam);
	kree_perf_boost(0);

	return iret;
}
EXPORT_SYMBOL(KREE_TeeServiceCall);

#if 0
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

	cnt = arch_counter_get_cntvct();
	return cnt;
}

u32 KREE_GetSystemCntFrq(void)
{
	u32 freq = 0;

	freq = arch_timer_get_cntfrq();
	return freq;
}

int gz_get_cpuinfo_thread(void *data)
{
#ifdef MTK_PPM_SUPPORT
	struct cpufreq_policy curr_policy;
#endif
#ifdef CONFIG_MACH_MT6758
	msleep(3000);
#else
	msleep(1000);
#endif

#ifdef MTK_PPM_SUPPORT
	cpufreq_get_policy(&curr_policy, 0);
	cpus_cluster_freq[0].max_freq = curr_policy.cpuinfo.max_freq;
	cpus_cluster_freq[0].min_freq = curr_policy.cpuinfo.min_freq;
	cpufreq_get_policy(&curr_policy, 4);
	cpus_cluster_freq[1].max_freq = curr_policy.cpuinfo.max_freq;
	cpus_cluster_freq[1].min_freq = curr_policy.cpuinfo.min_freq;
	KREE_INFO("%s, cluster [0]=%u-%u, [1]=%u-%u\n", __func__,
		  cpus_cluster_freq[0].max_freq, cpus_cluster_freq[0].min_freq,
		  cpus_cluster_freq[1].max_freq, cpus_cluster_freq[1].min_freq);
#endif

	cpumask_clear(&trusty_all_cmask);
	cpumask_setall(&trusty_all_cmask);
	cpumask_clear(&trusty_big_cmask);
	cpumask_set_cpu(4, &trusty_big_cmask);
	cpumask_set_cpu(5, &trusty_big_cmask);
	cpumask_set_cpu(6, &trusty_big_cmask);
	cpumask_set_cpu(7, &trusty_big_cmask);

	perf_boost_cnt = 0;
	mutex_init(&perf_boost_lock);
#ifdef CONFIG_PM_WAKELOCKS
	wakeup_source_init(&TeeServiceCall_wake_lock, "KREE_TeeServiceCall");
#else
	wake_lock_init(&TeeServiceCall_wake_lock, WAKE_LOCK_SUSPEND,
		       "KREE_TeeServiceCall");
#endif

	return 0;
}
