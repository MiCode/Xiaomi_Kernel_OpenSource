/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM smcinvoke

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE trace_smcinvoke

#if !defined(_TRACE_SMCINVOKE) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SMCINVOKE_H
#include <linux/types.h>
#include <linux/tracepoint.h>

TRACE_EVENT(put_pending_cbobj_locked,
	TP_PROTO(uint16_t srvr_id, uint16_t obj_id),
	TP_ARGS(srvr_id, obj_id),
	TP_STRUCT__entry(
		__field(uint16_t,	srvr_id)
		__field(uint16_t,	obj_id)
	),
	TP_fast_assign(
		__entry->srvr_id	= srvr_id;
		__entry->obj_id		= obj_id;
	),
	TP_printk("srvr_id=0x%x obj_id=0x%x",
			__entry->srvr_id, __entry->obj_id)
);

TRACE_EVENT(release_mem_obj_locked,
	TP_PROTO(uint32_t tzhandle, size_t buf_len),
	TP_ARGS(tzhandle, buf_len),
	TP_STRUCT__entry(
		__field(uint32_t,	tzhandle)
		__field(size_t,		buf_len)
	),
	TP_fast_assign(
		__entry->tzhandle	= tzhandle;
		__entry->buf_len	= buf_len;
	),
	TP_printk("tzhandle=0x%08x, buf_len=%zu",
			__entry->tzhandle, __entry->buf_len)
);

TRACE_EVENT(invoke_cmd_handler,
	TP_PROTO(int cmd, uint64_t response_type, int32_t result, int ret),
	TP_ARGS(cmd, response_type, result, ret),
	TP_STRUCT__entry(
		__field(int,		cmd)
		__field(uint64_t,	response_type)
		__field(int32_t,	result)
		__field(int,		ret)
	),
	TP_fast_assign(
		__entry->response_type	= response_type;
		__entry->result		= result;
		__entry->ret		= ret;
		__entry->cmd		= cmd;
	),
	TP_printk("cmd=0x%x (%d), response_type=%ld, result=0x%x (%d), ret=%d",
			__entry->cmd, __entry->cmd, __entry->response_type,
			__entry->result, __entry->result, __entry->ret)
);

TRACE_EVENT(process_tzcb_req_handle,
	TP_PROTO(uint32_t tzhandle, uint32_t op, uint32_t counts),
	TP_ARGS(tzhandle, op, counts),
	TP_STRUCT__entry(
		__field(uint32_t,	tzhandle)
		__field(uint32_t,	op)
		__field(uint32_t,	counts)
	),
	TP_fast_assign(
		__entry->tzhandle	= tzhandle;
		__entry->op		= op;
		__entry->counts		= counts;
	),
	TP_printk("tzhandle=0x%08x op=0x%02x counts=0x%04x",
			__entry->tzhandle, __entry->op, __entry->counts)
);

TRACE_EVENT(process_tzcb_req_wait,
	TP_PROTO(uint32_t tzhandle, int cbobj_retries, uint32_t txn_id, pid_t pid, pid_t tgid,
			uint16_t server_state, uint16_t server_id, unsigned int cb_reqs_inflight),
	TP_ARGS(tzhandle, cbobj_retries, txn_id, pid, tgid, server_state, server_id,
			cb_reqs_inflight),
	TP_STRUCT__entry(
		__field(uint32_t,	tzhandle)
		__field(int,		cbobj_retries)
		__field(uint32_t,	txn_id)
		__field(pid_t,		pid)
		__field(pid_t,		tgid)
		__field(uint16_t,	server_state)
		__field(uint16_t,	server_id)
		__field(unsigned int,	cb_reqs_inflight)
	),
	TP_fast_assign(
		__entry->tzhandle		= tzhandle;
		__entry->cbobj_retries		= cbobj_retries;
		__entry->txn_id			= txn_id;
		__entry->pid			= pid;
		__entry->tgid			= tgid;
		__entry->server_state		= server_state;
		__entry->server_id		= server_id;
		__entry->cb_reqs_inflight	= cb_reqs_inflight;
	),
	TP_printk("tzhandle=0x%08x, retries=%d, txn_id=%d, pid %x,tid %x, srvr state=%d, server_id=0x%x, cb_reqs_inflight=%d",
			__entry->tzhandle, __entry->cbobj_retries, __entry->txn_id,
			__entry->pid, __entry->tgid, __entry->server_state,
			__entry->server_id, __entry->cb_reqs_inflight)
);

TRACE_EVENT(process_tzcb_req_result,
	TP_PROTO(int32_t result, uint32_t tzhandle, uint32_t op, uint32_t counts,
			unsigned int cb_reqs_inflight),
	TP_ARGS(result, tzhandle, op, counts, cb_reqs_inflight),
	TP_STRUCT__entry(
		__field(int32_t,	result)
		__field(uint32_t,	tzhandle)
		__field(uint32_t,	op)
		__field(uint32_t,	counts)
		__field(unsigned int,	cb_reqs_inflight)
	),
	TP_fast_assign(
		__entry->result			= result;
		__entry->tzhandle		= tzhandle;
		__entry->op			= op;
		__entry->counts			= counts;
		__entry->cb_reqs_inflight	= cb_reqs_inflight;
	),
	TP_printk("result=%d tzhandle=0x%08x op=0x%02x counts=0x%04x, cb_reqs_inflight=%d",
			__entry->result, __entry->tzhandle, __entry->op, __entry->counts,
			__entry->cb_reqs_inflight)
);

TRACE_EVENT(marshal_out_invoke_req,
	TP_PROTO(int i, uint32_t tzhandle, uint16_t server, uint32_t fd),
	TP_ARGS(i, tzhandle, server, fd),
	TP_STRUCT__entry(
		__field(int,		i)
		__field(uint32_t,	tzhandle)
		__field(uint16_t,	server)
		__field(uint32_t,	fd)
	),
	TP_fast_assign(
		__entry->i		= i;
		__entry->tzhandle	= tzhandle;
		__entry->server		= server;
		__entry->fd		= fd;
	),
	TP_printk("OO[%d]: tzhandle=0x%x server=0x%x fd=0x%x",
			__entry->i, __entry->tzhandle, __entry->server, __entry->fd)
);

TRACE_EVENT(prepare_send_scm_msg,
	TP_PROTO(uint64_t response_type, int32_t result),
	TP_ARGS(response_type, result),
	TP_STRUCT__entry(
		__field(uint64_t,	response_type)
		__field(int32_t,	result)
	),
	TP_fast_assign(
		__entry->response_type	= response_type;
		__entry->result		= result;
	),
	TP_printk("response_type=0x%lx (%ld), result=0x%x (%d)",
			__entry->response_type, __entry->response_type,
			__entry->result, __entry->result)
);

TRACE_EVENT(marshal_in_invoke_req,
	TP_PROTO(int i, int64_t fd, int32_t cb_server_fd, uint32_t tzhandle),
	TP_ARGS(i, fd, cb_server_fd, tzhandle),
	TP_STRUCT__entry(
		__field(int,		i)
		__field(int64_t,	fd)
		__field(int32_t,	cb_server_fd)
		__field(uint32_t,	tzhandle)
	),
	TP_fast_assign(
		__entry->i		= i;
		__entry->fd		= fd;
		__entry->cb_server_fd	= cb_server_fd;
		__entry->tzhandle	= tzhandle;
	),
	TP_printk("OI[%d]: fd=0x%x cb_server_fd=0x%x tzhandle=0x%x",
			__entry->i, __entry->fd, __entry->cb_server_fd, __entry->tzhandle)
);

TRACE_EVENT(marshal_in_tzcb_req_handle,
	TP_PROTO(uint32_t tzhandle, int srvr_id, int32_t cbobj_id, uint32_t op, uint32_t counts),
	TP_ARGS(tzhandle, srvr_id, cbobj_id, op, counts),
	TP_STRUCT__entry(
		__field(uint32_t,	tzhandle)
		__field(int,		srvr_id)
		__field(int32_t,	cbobj_id)
		__field(uint32_t,	op)
		__field(uint32_t,	counts)
	),
	TP_fast_assign(
		__entry->tzhandle	= tzhandle;
		__entry->srvr_id	= srvr_id;
		__entry->cbobj_id	= cbobj_id;
		__entry->op		= op;
		__entry->counts		= counts;
	),
	TP_printk("tzhandle=0x%x srvr_id=0x%x cbobj_id=0x%08x op=0x%02x counts=0x%04x",
			__entry->tzhandle, __entry->srvr_id, __entry->cbobj_id,
			__entry->op, __entry->counts)
);

TRACE_EVENT(marshal_in_tzcb_req_fd,
	TP_PROTO(int i, uint32_t tzhandle, int srvr_id, int32_t fd),
	TP_ARGS(i, tzhandle, srvr_id, fd),
	TP_STRUCT__entry(
		__field(int,		i)
		__field(uint32_t,	tzhandle)
		__field(int,		srvr_id)
		__field(int32_t,	fd)
	),
	TP_fast_assign(
		__entry->i		= i;
		__entry->tzhandle	= tzhandle;
		__entry->srvr_id	= srvr_id;
		__entry->fd		= fd;
	),
	TP_printk("OI[%d]: tzhandle=0x%x srvr_id=0x%x fd=0x%x",
			__entry->i, __entry->tzhandle, __entry->srvr_id, __entry->fd)
);

TRACE_EVENT(marshal_out_tzcb_req,
	TP_PROTO(uint32_t i, int32_t fd, int32_t cb_server_fd, uint32_t tzhandle),
	TP_ARGS(i, fd, cb_server_fd, tzhandle),
	TP_STRUCT__entry(
		__field(int,		i)
		__field(int32_t,	fd)
		__field(int32_t,	cb_server_fd)
		__field(uint32_t,	tzhandle)
	),
	TP_fast_assign(
		__entry->i		= i;
		__entry->fd		= fd;
		__entry->cb_server_fd	= cb_server_fd;
		__entry->tzhandle	= tzhandle;
	),
	TP_printk("OO[%d]: fd=0x%x cb_server_fd=0x%x tzhandle=0x%x",
			__entry->i, __entry->fd, __entry->cb_server_fd, __entry->tzhandle)
);

TRACE_EVENT(process_invoke_req_tzhandle,
	TP_PROTO(uint32_t tzhandle, uint32_t op, uint32_t counts),
	TP_ARGS(tzhandle, op, counts),
	TP_STRUCT__entry(
		__field(uint32_t, tzhandle)
		__field(uint32_t, op)
		__field(uint32_t, counts)
	),
	TP_fast_assign(
		__entry->tzhandle	= tzhandle;
		__entry->op		= op;
		__entry->counts		= counts;
	),
	TP_printk("tzhandle=0x%08x op=0x%02x counts=0x%04x",
			__entry->tzhandle, __entry->op, __entry->counts)
);

TRACE_EVENT(process_invoke_req_result,
	TP_PROTO(int ret, int32_t result, uint32_t tzhandle, uint32_t op, uint32_t counts),
	TP_ARGS(ret, result, tzhandle, op, counts),
	TP_STRUCT__entry(
		__field(int,		ret)
		__field(int32_t,	result)
		__field(uint32_t,	tzhandle)
		__field(uint32_t,	op)
		__field(uint32_t,	counts)
	),
	TP_fast_assign(
		__entry->ret		= ret;
		__entry->result		= result;
		__entry->tzhandle	= tzhandle;
		__entry->op		= op;
		__entry->counts		= counts;
	),
	TP_printk("ret=%d result=%d tzhandle=0x%08x op=0x%02x counts=0x%04x",
			__entry->ret, __entry->result, __entry->tzhandle,
			__entry->op, __entry->counts)
);

TRACE_EVENT(process_log_info,
	TP_PROTO(char *buf, uint32_t context_type, uint32_t tzhandle),
	TP_ARGS(buf, context_type, tzhandle),
	TP_STRUCT__entry(
		__string(str,		buf)
		__field(uint32_t,	context_type)
		__field(uint32_t,	tzhandle)
	),
	TP_fast_assign(
		__assign_str(str, buf);
		__entry->context_type	= context_type;
		__entry->tzhandle	= tzhandle;
	),
	TP_printk("%s context_type=%d tzhandle=0x%08x",
			__get_str(str),
			__entry->context_type, __entry->tzhandle)
);

TRACE_EVENT_CONDITION(smcinvoke_ioctl,
	TP_PROTO(unsigned int cmd, long ret),
	TP_ARGS(cmd, ret),
	TP_CONDITION(ret),
	TP_STRUCT__entry(
		__field(unsigned int,	cmd)
		__field(long,		ret)
	),
	TP_fast_assign(
		__entry->cmd = cmd;
		__entry->ret = ret;
	),
	TP_printk("cmd=%s ret=%ld",
			__print_symbolic(__entry->cmd,
				{SMCINVOKE_IOCTL_INVOKE_REQ,	"SMCINVOKE_IOCTL_INVOKE_REQ"},
				{SMCINVOKE_IOCTL_ACCEPT_REQ,	"SMCINVOKE_IOCTL_ACCEPT_REQ"},
				{SMCINVOKE_IOCTL_SERVER_REQ,	"SMCINVOKE_IOCTL_SERVER_REQ"},
				{SMCINVOKE_IOCTL_ACK_LOCAL_OBJ,	"SMCINVOKE_IOCTL_ACK_LOCAL_OBJ"},
				{SMCINVOKE_IOCTL_LOG,		"SMCINVOKE_IOCTL_LOG"}
			), __entry->ret)
);

TRACE_EVENT(smcinvoke_create_bridge,
	TP_PROTO(uint64_t shmbridge_handle, uint16_t mem_region_id),
	TP_ARGS(shmbridge_handle, mem_region_id),
	TP_STRUCT__entry(
		__field(uint64_t,	shmbridge_handle)
		__field(uint16_t,	mem_region_id)
	),
	TP_fast_assign(
		__entry->shmbridge_handle	= shmbridge_handle;
		__entry->mem_region_id		= mem_region_id;
	),
	TP_printk("created shm bridge handle %llu for mem_region_id %u",
			__entry->shmbridge_handle, __entry->mem_region_id)
);

TRACE_EVENT(status,
	TP_PROTO(const char *func, const char *status),
	TP_ARGS(func, status),
	TP_STRUCT__entry(
		__string(str,	func)
		__string(str2,	status)
	),
	TP_fast_assign(
		__assign_str(str,	func);
		__assign_str(str2,	status);
	),
	TP_printk("%s status=%s", __get_str(str), __get_str(str2))
);

TRACE_EVENT(process_accept_req_has_response,
	TP_PROTO(pid_t pid, pid_t tgid),
	TP_ARGS(pid, tgid),
	TP_STRUCT__entry(
		__field(pid_t,	pid)
		__field(pid_t,	tgid)
	),
	TP_fast_assign(
		__entry->pid	= pid;
		__entry->tgid	= tgid;
	),
	TP_printk("pid=0x%x, tgid=0x%x", __entry->pid, __entry->tgid)
);

TRACE_EVENT(process_accept_req_ret,
	TP_PROTO(pid_t pid, pid_t tgid, int ret),
	TP_ARGS(pid, tgid, ret),
	TP_STRUCT__entry(
		__field(pid_t,	pid)
		__field(pid_t,	tgid)
		__field(int,	ret)
	),
	TP_fast_assign(
		__entry->pid	= pid;
		__entry->tgid	= tgid;
		__entry->ret	= ret;
	),
	TP_printk("pid=0x%x tgid=0x%x ret=%d", __entry->pid, __entry->tgid, __entry->ret)
);

TRACE_EVENT(process_accept_req_placed,
	TP_PROTO(pid_t pid, pid_t tgid),
	TP_ARGS(pid, tgid),
	TP_STRUCT__entry(
		__field(pid_t,	pid)
		__field(pid_t,	tgid)
	),
	TP_fast_assign(
		__entry->pid	= pid;
		__entry->tgid	= tgid;
	),
	TP_printk("pid=0x%x, tgid=0x%x", __entry->pid, __entry->tgid)
);

TRACE_EVENT(process_invoke_request_from_kernel_client,
	TP_PROTO(int fd, struct file *filp, int f_count),
	TP_ARGS(fd, filp, f_count),
	TP_STRUCT__entry(
		__field(int,		fd)
		__field(struct file*,	filp)
		__field(int,		f_count)
	),
	TP_fast_assign(
		__entry->fd		= fd;
		__entry->filp		= filp;
		__entry->f_count	= f_count;
	),
	TP_printk("fd=%d, filp=%p, f_count=%d",
			__entry->fd,
			__entry->filp,
			__entry->f_count)
);

TRACE_EVENT(smcinvoke_release_filp,
	TP_PROTO(struct files_struct *files, struct file *filp,
			int f_count, uint32_t context_type),
	TP_ARGS(files, filp, f_count, context_type),
	TP_STRUCT__entry(
		__field(struct files_struct*,	files)
		__field(struct file*,		filp)
		__field(int,			f_count)
		__field(uint32_t,		context_type)
	),
	TP_fast_assign(
		__entry->files		= files;
		__entry->filp		= filp;
		__entry->f_count	= f_count;
		__entry->context_type	= context_type;
	),
	TP_printk("files=%p, filp=%p, f_count=%u, cxt_type=%d",
			__entry->files,
			__entry->filp,
			__entry->f_count,
			__entry->context_type)
);

TRACE_EVENT(smcinvoke_release_from_kernel_client,
	TP_PROTO(struct files_struct *files, struct file *filp, int f_count),
	TP_ARGS(files, filp, f_count),
	TP_STRUCT__entry(
		__field(struct files_struct*,	files)
		__field(struct file*,		filp)
		__field(int,			f_count)
	),
	TP_fast_assign(
		__entry->files		= files;
		__entry->filp		= filp;
		__entry->f_count	= f_count;
	),
	TP_printk("files=%p, filp=%p, f_count=%u",
			__entry->files,
			__entry->filp,
			__entry->f_count)
);

TRACE_EVENT(smcinvoke_release,
	TP_PROTO(struct files_struct *files, struct file *filp,
			int f_count, void *private_data),
	TP_ARGS(files, filp, f_count, private_data),
	TP_STRUCT__entry(
		__field(struct files_struct*,	files)
		__field(struct file*,		filp)
		__field(int,			f_count)
		__field(void*,			private_data)
	),
	TP_fast_assign(
		__entry->files		= files;
		__entry->filp		= filp;
		__entry->f_count	= f_count;
		__entry->private_data	= private_data;
	),
	TP_printk("files=%p, filp=%p, f_count=%d, private_data=%p",
			__entry->files,
			__entry->filp,
			__entry->f_count,
			__entry->private_data)
);

#endif /* _TRACE_SMCINVOKE_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/soc/qcom/

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE trace_smcinvoke

/* This part must be outside protection */
#include <trace/define_trace.h>
