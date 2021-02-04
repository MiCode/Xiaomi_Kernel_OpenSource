/*
 * Copyright (c) 2015-2018 TrustKernel Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/file.h>
#include <linux/atomic.h>
#include <linux/uaccess.h>
#include <linux/anon_inodes.h>
#include <linux/fs.h>

#include "tee_shm.h"
#include "tee_core_priv.h"
#include "tee_tui_hal.h"

static int _init_tee_cmd(struct tee_session *sess, struct tee_cmd_io *cmd_io,
			 struct tee_cmd *cmd);
static void _update_client_tee_cmd(struct tee_session *sess,
				   struct tee_cmd_io *cmd_io,
				   struct tee_cmd *cmd);
static void _release_tee_cmd(struct tee_session *sess, struct tee_cmd *cmd);

/******************************************************************************/

static inline bool flag_set(int val, int flags)
{
	return (val & flags) == flags;
}

static inline bool is_mapped_temp(int flags)
{
	return flag_set(flags, TEE_SHM_MAPPED | TEE_SHM_TEMP);
}

/******************************************************************************/
static int tee_copy_from_user(struct tee_context *ctx, void *to, void *from,
				size_t size)
{
	if ((!to) || (!from) || (!size))
		return 0;

	if (ctx->usr_client)
		return copy_from_user(to, from, size);

	memcpy(to, from, size);
	return 0;
}

static int tee_copy_to_user(struct tee_context *ctx, void *to, void *from,
				size_t size)
{
	if ((!to) || (!from) || (!size))
		return 0;

	if (ctx->usr_client)
		return copy_to_user(to, from, size);

	memcpy(to, from, size);
	return 0;
}

/* Defined as macro to let the put_user macro see the types */
#define tee_put_user(ctx, from, to)				\
	do {							\
		if ((ctx)->usr_client)				\
			put_user(from, to);			\
		else						\
			*to = from;				\
	} while (0)

static inline int tee_session_is_opened(struct tee_session *sess)
{
	if (sess && sess->sessid)
		return (sess->sessid != 0);
	return 0;
}

static int tee_session_open_be(struct tee_session *sess,
				struct tee_cmd_io *cmd_io)
{
	int ret = -EINVAL;
	struct tee *tee;
	struct tee_cmd cmd;

	WARN_ON(!sess || !sess->ctx || !sess->ctx->tee);

	tee = sess->ctx->tee;


	sess->sessid = 0;
	ret = _init_tee_cmd(sess, cmd_io, &cmd);
	if (ret) {
		pr_err(
			"init tee command failed with 0x%08x\n",
			ret);
		goto out;
	}

	ret = tee->ops->open(sess, &cmd);
	if (ret == 0)
		_update_client_tee_cmd(sess, cmd_io, &cmd);
	else {
		/* propagate the reason of the error */
		cmd_io->origin = cmd.origin;
		cmd_io->err = cmd.err;
	}

out:
	_release_tee_cmd(sess, &cmd);
	return ret;
}

int tee_session_invoke_be(struct tee_session *sess, struct tee_cmd_io *cmd_io)
{
	int ret = -EINVAL;
	struct tee *tee;
	struct tee_cmd cmd;

	WARN_ON(!sess || !sess->ctx || !sess->ctx->tee);

	tee = sess->ctx->tee;

	ret = _init_tee_cmd(sess, cmd_io, &cmd);
	if (ret)
		goto out;

	ret = tee->ops->invoke(sess, &cmd);
	if (!ret)
		_update_client_tee_cmd(sess, cmd_io, &cmd);
	else {
		/* propagate the reason of the error */
		cmd_io->origin = cmd.origin;
		cmd_io->err = cmd.err;
	}

out:
	_release_tee_cmd(sess, &cmd);
	return ret;
}

static int tee_session_close_be(struct tee_session *sess)
{
	int ret = -EINVAL;
	struct tee *tee;

	WARN_ON(!sess || !sess->ctx || !sess->ctx->tee);

	tee = sess->ctx->tee;


	ret = tee->ops->close(sess);
	sess->sessid = 0;

	return ret;
}

static int tee_session_cancel_be(struct tee_session *sess,
				 struct tee_cmd_io *cmd_io)
{
	int ret = -EINVAL;
	struct tee *tee;
	struct tee_cmd cmd;

	WARN_ON(!sess || !sess->ctx || !sess->ctx->tee);

	tee = sess->ctx->tee;


	ret = _init_tee_cmd(sess, cmd_io, &cmd);
	if (ret)
		goto out;

	ret = tee->ops->cancel(sess, &cmd);

out:
	_release_tee_cmd(sess, &cmd);
	return ret;
}

static int tee_do_invoke_command(struct tee_session *sess,
				 struct tee_cmd_io __user *u_cmd)
{
	int ret = -EINVAL;
	struct tee *tee;
	struct tee_cmd_io k_cmd;
	struct tee_context *ctx;

	WARN_ON(!sess->ctx);
	WARN_ON(!sess->ctx->tee);
	ctx = sess->ctx;
	tee = sess->ctx->tee;

	WARN_ON(!sess->sessid);

	if (tee_copy_from_user
		(ctx, &k_cmd, (void *)u_cmd, sizeof(struct tee_cmd_io))) {
		pr_err("tee_copy_from_user failed\n");
		goto exit;
	}

	if ((k_cmd.op == NULL) || (k_cmd.uuid != NULL) ||
		(k_cmd.data != NULL) || (k_cmd.data_size != 0)) {
		pr_err(
			"op or/and data parameters are not valid\n");
		goto exit;
	}

	ret = tee_session_invoke_be(sess, &k_cmd);
	if (ret)
		pr_err("tee_invoke_command failed with %d\n", ret);

	tee_put_user(ctx, k_cmd.err, &u_cmd->err);
	tee_put_user(ctx, k_cmd.origin, &u_cmd->origin);

exit:
	return ret;
}

static int tee_do_cancel_cmd(struct tee_session *sess,
				struct tee_cmd_io __user *u_cmd)
{
	int ret = -EINVAL;
	struct tee *tee;
	struct tee_cmd_io k_cmd;
	struct tee_context *ctx;

	WARN_ON(!sess->ctx);
	WARN_ON(!sess->ctx->tee);
	ctx = sess->ctx;
	tee = sess->ctx->tee;


	WARN_ON(!sess->sessid);

	if (tee_copy_from_user
		(ctx, &k_cmd, (void *)u_cmd, sizeof(struct tee_cmd_io))) {
		pr_err("cancel_cmd: tee_copy_from_user failed\n");
		goto exit;
	}

	if ((k_cmd.op == NULL) || (k_cmd.uuid != NULL) ||
		(k_cmd.data != NULL) || (k_cmd.data_size != 0)) {
		pr_err("op or/and data parameters are not valid\n");
		goto exit;
	}

	ret = tee_session_cancel_be(sess, &k_cmd);
	if (ret)
		pr_err("tee_invoke_command failed\n");

	tee_put_user(ctx, k_cmd.err, &u_cmd->err);
	tee_put_user(ctx, k_cmd.origin, &u_cmd->origin);

exit:
	return ret;
}

static int tee_do_kernel_cancel_cmd(struct tee_session *sess,
					struct tee_cmd_io *k_cmd)
{
	int ret = -EINVAL;
	struct tee *tee;
	struct tee_context *ctx;
	struct tee_cmd cmd;

	WARN_ON(!sess->ctx);
	WARN_ON(!sess->ctx->tee);
	ctx = sess->ctx;
	tee = sess->ctx->tee;


	WARN_ON(!sess->sessid);

	if ((k_cmd == NULL || k_cmd->op == NULL) || (k_cmd->uuid != NULL) ||
		(k_cmd->data != NULL) || (k_cmd->data_size != 0)) {
		pr_err(
			"op or/and data parameters are not valid\n");
		goto exit;
	}

	cmd.cmd = k_cmd->cmd;
	cmd.origin = TEEC_ORIGIN_TEE;
	cmd.err = TEEC_ERROR_BAD_PARAMETERS;

	cmd.param.type_original = 0;

	ret = tee->ops->cancel(sess, &cmd);

	if (ret)
		pr_err("tee_cancel failed\n");

exit:
	return ret;
}

DECLARE_COMPLETION(io_comp);
static int flag_tui_obj;

struct tui_obj {
	struct tee_cmd_io op;
	struct tee_session *sess;
	uint32_t cmd_id;
	uint32_t status;
	struct mutex lock;
} g_tui_obj;

static uint32_t send_cmd_to_user(uint32_t command_id)
{
	uint32_t ret = 0;

	g_tui_obj.cmd_id = command_id;
	complete(&io_comp);

	return ret;
}

bool teec_notify_event(uint32_t event_type)
{
	bool ret = false;

	/* Cancel the TUI session if exists */
	if (g_tui_obj.status)
		ret = tee_do_kernel_cancel_cmd(g_tui_obj.sess, &g_tui_obj.op);

	return ret;
}
EXPORT_SYMBOL(teec_notify_event);

int teec_wait_cmd(uint32_t *cmd_id)
{
	/* Wait for signal from DCI handler */
	wait_for_completion(&io_comp);
#ifdef INIT_COMPLETION
	INIT_COMPLETION(io_comp);
#else
	io_comp.done = 0;
#endif

	*cmd_id = g_tui_obj.cmd_id;
	return 0;
}
EXPORT_SYMBOL(teec_wait_cmd);

static long tee_session_internal_ioctl(struct tee_session *sess,
					unsigned int cmd, unsigned long arg)
{
	int ret;
	struct tee *tee;

	tee = sess->ctx->tee;


	switch (cmd) {
	case TEE_INVOKE_COMMAND_IOC:
		ret = tee_do_invoke_command(sess,
			(struct tee_cmd_io __user *) arg);
		break;
	case TEE_REQUEST_CANCELLATION_IOC:
		ret = tee_do_cancel_cmd(sess,
			(struct tee_cmd_io __user *) arg);
		break;
	case TEE_TUI_OPEN_SESSION_IOC:
		pr_debug("TEE_TUI_OPEN_SESSION_IOC.\n");
		if (flag_tui_obj == 0) {
			mutex_init(&g_tui_obj.lock);
			flag_tui_obj = 1;
		}
		mutex_lock(&g_tui_obj.lock);
		if (g_tui_obj.status != 0 ||
			(struct tee_cmd_io __user *)arg == NULL) {
			ret = -EBUSY;
			mutex_unlock(&g_tui_obj.lock);
			pr_warn(
				"TEE_TUI_OPEN_SESSION_IOC: tui busy or invalid argument\n");
			break;
		}
		if (tee_copy_from_user(sess->ctx, &g_tui_obj.op, (void *)arg,
					sizeof(struct tee_cmd_io))) {
			ret = -EINVAL;
			mutex_unlock(&g_tui_obj.lock);
			break;
		}
		/* reset part of op */
		g_tui_obj.op.uuid = NULL;
		g_tui_obj.op.data = NULL;
		g_tui_obj.op.data_size = 0;

		g_tui_obj.sess = sess;
		g_tui_obj.status = 1;
		mutex_unlock(&g_tui_obj.lock);
		/* Start android TUI activity */
		ret = send_cmd_to_user(TEEC_TUI_CMD_START_ACTIVITY);
		if (ret != 0) {
			mutex_lock(&g_tui_obj.lock);
			g_tui_obj.status = 0;
			mutex_unlock(&g_tui_obj.lock);
			break;
		}
		/* Deactivate linux UI drivers */
		ret = hal_tui_deactivate();
		if (ret != 0) {
			mutex_lock(&g_tui_obj.lock);
			g_tui_obj.status = 0;
			mutex_unlock(&g_tui_obj.lock);
			send_cmd_to_user(TEEC_TUI_CMD_STOP_ACTIVITY);
			break;
		}
		break;
	case TEE_TUI_CLOSE_SESSION_IOC:
		pr_debug(
			"TEE_TUI_CLOSE_SESSION_IOC.\n");
		if (flag_tui_obj == 0) {
			mutex_init(&g_tui_obj.lock);
			flag_tui_obj = 1;
		}
		mutex_lock(&g_tui_obj.lock);
		g_tui_obj.status = 0;
		mutex_unlock(&g_tui_obj.lock);
		/* Activate linux UI drivers */
		ret = hal_tui_activate();
		/* Stop android TUI activity */
		ret = send_cmd_to_user(TEEC_TUI_CMD_STOP_ACTIVITY);
		break;
	default:
		ret = -EINVAL;
		break;
	}


	return ret;
}

static long tee_session_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	struct tee_session *sess = filp->private_data;

	WARN_ON(!sess || !sess->ctx || !sess->ctx->tee);

	return tee_session_internal_ioctl(sess, cmd, arg);
}

#ifdef CONFIG_COMPAT

static long tee_session_compat_ioctl(struct file *filp, unsigned int cmd,
					unsigned long arg)
{
	struct tee_session *sess = filp->private_data;

	WARN_ON(!sess || !sess->ctx || !sess->ctx->tee);

	switch (cmd) {
	case TEE_INVOKE_COMMAND_IOC:
	case TEE_REQUEST_CANCELLATION_IOC:
	case TEE_TUI_OPEN_SESSION_IOC:
		if (convert_compat_tee_cmd((struct tee_cmd_io __user *) arg))
			return -EFAULT;
		break;
	default:
		break;
	}

	return tee_session_internal_ioctl(sess, cmd, arg);
}

#endif

static int tee_session_release(struct inode *inode, struct file *filp)
{
	struct tee_session *sess = filp->private_data;
	int ret = 0;
	struct tee *tee;

	WARN_ON(!sess || !sess->ctx || !sess->ctx->tee);
	tee = sess->ctx->tee;

	ret = tee_session_close_and_destroy(sess);
	return ret;
}

const struct file_operations tee_session_fops = {
	.owner = THIS_MODULE,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tee_session_compat_ioctl,
#endif
	.unlocked_ioctl = tee_session_ioctl,
	.release = tee_session_release,
};

int tee_session_close_and_destroy(struct tee_session *sess)
{
	int ret;
	struct tee *tee;
	struct tee_context *ctx;

	if (!sess || !sess->ctx || !sess->ctx->tee)
		return -EINVAL;

	ctx = sess->ctx;
	tee = ctx->tee;


	if (!tee_session_is_opened(sess))
		return -EINVAL;

	ret = tee_session_close_be(sess);

	mutex_lock(&tee->lock);
	tee_dec_stats(&tee->stats[TEE_STATS_SESSION_IDX]);
	list_del(&sess->entry);

	devm_kfree(_DEV(tee), sess);
	tee_context_put(ctx);
	tee_put(tee);
	mutex_unlock(&tee->lock);

	return ret;
}

struct tee_session *tee_session_create_and_open(struct tee_context *ctx,
		struct tee_cmd_io *cmd_io)
{
	int ret = 0;
	struct tee_session *sess;
	struct tee *tee;

	WARN_ON(!ctx->tee);

	tee = ctx->tee;

	ret = tee_get(tee);
	if (ret)
		return ERR_PTR(-EBUSY);

	sess = devm_kzalloc(_DEV(tee), sizeof(struct tee_session), GFP_KERNEL);
	if (!sess) {
		tee_put(tee);
		return ERR_PTR(-ENOMEM);
	}

	tee_context_get(ctx);
	sess->ctx = ctx;

	ret = tee_session_open_be(sess, cmd_io);
	mutex_lock(&tee->lock);
	if (ret || !sess->sessid || cmd_io->err) {
		pr_err(
			"ERROR ret=%d (err=0x%08x, org=%d,  sessid=0x%08x)\n",
			ret, cmd_io->err,
			cmd_io->origin, sess->sessid);
		tee_put(tee);
		tee_context_put(ctx);
		devm_kfree(_DEV(tee), sess);
		mutex_unlock(&tee->lock);
		if (ret)
			return ERR_PTR(ret);
		else
			return NULL;
	}

	tee_inc_stats(&tee->stats[TEE_STATS_SESSION_IDX]);
	list_add_tail(&sess->entry, &ctx->list_sess);
	mutex_unlock(&tee->lock);

	return sess;
}

int tee_session_create_fd(struct tee_context *ctx, struct tee_cmd_io *cmd_io)
{
	int ret;
	struct tee_session *sess;
	struct tee *tee = ctx->tee;

	(void) tee;

	WARN_ON(cmd_io->fd_sess > 0);


	sess = tee_session_create_and_open(ctx, cmd_io);
	if (IS_ERR_OR_NULL(sess)) {
		ret = PTR_ERR(sess);
		pr_warn(
			"ERROR can't create the session (ret=%d, err=0x%08x, org=%d)\n",
			ret, cmd_io->err, cmd_io->origin);
		cmd_io->fd_sess = -1;
		goto out;
	}

	/* Retrieve a fd */
	cmd_io->fd_sess = -1;
	ret = anon_inode_getfd("tee_session",
			&tee_session_fops, sess, O_CLOEXEC);
	if (ret < 0) {
		pr_err("ERROR can't get a fd (ret=%d)\n",
			ret);
		tee_session_close_and_destroy(sess);
		goto out;
	}
	cmd_io->fd_sess = ret;
	ret = 0;

out:
	return ret;
}

static bool tee_session_is_supported_type(struct tee_session *sess, int type)
{
	switch (type) {
	case TEEC_NONE:
	case TEEC_VALUE_INPUT:
	case TEEC_VALUE_OUTPUT:
	case TEEC_VALUE_INOUT:
	case TEEC_MEMREF_TEMP_INPUT:
	case TEEC_MEMREF_TEMP_OUTPUT:
	case TEEC_MEMREF_TEMP_INOUT:
	case TEEC_MEMREF_WHOLE:
	case TEEC_MEMREF_PARTIAL_INPUT:
	case TEEC_MEMREF_PARTIAL_OUTPUT:
	case TEEC_MEMREF_PARTIAL_INOUT:
		return true;
	default:
		pr_err("type is invalid (type %02x)\n", type);
		return false;
	}
}

static int to_memref_type(int flags)
{
	if (flag_set(flags, TEEC_MEM_INPUT | TEEC_MEM_OUTPUT))
		return TEEC_MEMREF_TEMP_INOUT;

	if (flag_set(flags, TEEC_MEM_INPUT))
		return TEEC_MEMREF_TEMP_INPUT;

	if (flag_set(flags, TEEC_MEM_OUTPUT))
		return TEEC_MEMREF_TEMP_OUTPUT;

	pr_warn("tkcoredrv: %s: bad flags=%x\n", __func__, flags);
	return 0;
}

static int _init_tee_cmd(struct tee_session *sess, struct tee_cmd_io *cmd_io,
			 struct tee_cmd *cmd)
{
	int ret = -EINVAL;
	int idx;
	struct TEEC_Operation op;
	struct tee_data *param = &cmd->param;
	struct tee *tee;
	struct tee_context *ctx;

	WARN_ON(!sess->ctx);
	WARN_ON(!sess->ctx->tee);
	ctx = sess->ctx;
	tee = sess->ctx->tee;


	memset(cmd, 0, sizeof(struct tee_cmd));

	cmd->cmd = cmd_io->cmd;
	cmd->origin = TEEC_ORIGIN_TEE;
	cmd->err = TEEC_ERROR_BAD_PARAMETERS;
	cmd_io->origin = cmd->origin;
	cmd_io->err = cmd->err;

	if (tee_context_copy_from_client(ctx, &op, cmd_io->op, sizeof(op)))
		goto out;

	cmd->param.type_original = op.paramTypes;

	for (idx = 0; idx < TEEC_CONFIG_PAYLOAD_REF_COUNT; ++idx) {
		uint32_t offset = 0;
		uint32_t size = 0;
		int type = TEEC_PARAM_TYPE_GET(op.paramTypes, idx);

		switch (type) {
		case TEEC_NONE:
			break;

		case TEEC_VALUE_INPUT:
		case TEEC_VALUE_OUTPUT:
		case TEEC_VALUE_INOUT:
			param->params[idx].value = op.params[idx].value;
			break;

		case TEEC_MEMREF_TEMP_INPUT:
		case TEEC_MEMREF_TEMP_OUTPUT:
		case TEEC_MEMREF_TEMP_INOUT:
			param->params[idx].shm =
				tee_context_create_tmpref_buffer(
				ctx, op.params[idx].tmpref.size,
				op.params[idx].tmpref.buffer,
				type);
			if (IS_ERR_OR_NULL(param->params[idx].shm))
				goto out;

			break;

		case TEEC_MEMREF_PARTIAL_INPUT:
		case TEEC_MEMREF_PARTIAL_OUTPUT:
		case TEEC_MEMREF_PARTIAL_INOUT:
		case TEEC_MEMREF_WHOLE:
			if (tee_copy_from_user(ctx, &param->c_shm[idx],
						op.params[idx].memref.parent,
						sizeof(param->c_shm[idx])))
				goto out;

			if (type == TEEC_MEMREF_WHOLE) {
				offset = 0;
				size = param->c_shm[idx].size;
			} else { /* for PARTIAL, check the size */
				offset = op.params[idx].memref.offset;
				size = op.params[idx].memref.size;
				if (param->c_shm[idx].size < size + offset) {
					pr_err(
						"A PARTIAL parameter is bigger than the parent %zd < %d + %d\n",
						param->c_shm[idx].size, size,
						offset);
					goto out;
				}
			}

			type = to_memref_type(param->c_shm[idx].flags);
			if (type == 0)
				goto out;

			param->params[idx].shm = tee_shm_get(ctx,
					&param->c_shm[idx], size, offset);

			if (IS_ERR_OR_NULL(param->params[idx].shm)) {
				param->params[idx].shm =
					tee_context_create_tmpref_buffer(
							ctx, size,
							param->c_shm[idx].buffer
								+ offset, type);

				if (IS_ERR_OR_NULL(param->params[idx].shm))
					goto out;
			}

			break;
		default:
			WARN_ON(1);
		}

		param->type |= (type << (idx * 4));
	}

	if (cmd_io->uuid != NULL) {
		cmd->uuid = tee_context_alloc_shm_tmp(sess->ctx,
			sizeof(*cmd_io->uuid), cmd_io->uuid, TEEC_MEM_INPUT);
		if (IS_ERR_OR_NULL(cmd->uuid)) {
			ret = -EINVAL;
			goto out;
		}
	}

	ret = 0;

out:
	if (ret)
		_release_tee_cmd(sess, cmd);

	return ret;
}

static void _update_client_tee_cmd(struct tee_session *sess,
				   struct tee_cmd_io *cmd_io,
				   struct tee_cmd *cmd)
{
	int idx;
	struct tee_context *ctx;
	struct TEEC_Operation tmp_op;

	WARN_ON(!cmd_io);
	WARN_ON(!cmd_io->op);
	WARN_ON(!cmd);
	WARN_ON(!sess->ctx);
	ctx = sess->ctx;


	cmd_io->origin = cmd->origin;
	cmd_io->err = cmd->err;

	if (tee_context_copy_from_client(ctx,
			&tmp_op, cmd_io->op, sizeof(tmp_op))) {
		pr_err("Failed to copy op from client\n");
		return;
	}

	if (cmd->param.type_original == TEEC_PARAM_TYPES(TEEC_NONE,
			TEEC_NONE, TEEC_NONE, TEEC_NONE))
		return;

	for (idx = 0; idx < TEEC_CONFIG_PAYLOAD_REF_COUNT; ++idx) {
		int type = TEEC_PARAM_TYPE_GET(cmd->param.type_original, idx);
		int offset = 0;
		size_t size;
		size_t size_new;
		struct TEEC_SharedMemory *parent;

		WARN_ON(!tee_session_is_supported_type(sess, type));
		switch (type) {
		case TEEC_NONE:
		case TEEC_VALUE_INPUT:
		case TEEC_MEMREF_TEMP_INPUT:
		case TEEC_MEMREF_PARTIAL_INPUT:
			break;
		case TEEC_VALUE_OUTPUT:
		case TEEC_VALUE_INOUT:
			if (tee_copy_to_user
				(ctx, &cmd_io->op->params[idx].value,
				&cmd->param.params[idx].value,
				sizeof(tmp_op.params[idx].value))) {

				pr_err(
					"can't update %d result to user\n",
					idx);
			}
			break;
		case TEEC_MEMREF_TEMP_OUTPUT:
		case TEEC_MEMREF_TEMP_INOUT:
			/* Returned updated size */
			size_new = cmd->param.params[idx].shm->size_req;
			if (size_new !=
				tmp_op.params[idx].tmpref.size) {
				tee_put_user(ctx, size_new,
					&cmd_io->op->params[idx].tmpref.size);
			}

			/* ensure we do not exceed the shared buffer length */
			if (size_new > tmp_op.params[idx].tmpref.size) {
				pr_err(
					"  *** Wrong returned size from %d:%zd > %zd\n",
					idx, size_new,
					tmp_op.params[idx].tmpref.size);
			} else if (tee_copy_to_user
				   (ctx,
					tmp_op.params[idx].tmpref.buffer,
					cmd->param.params[idx].shm->resv.kaddr,
					size_new)) {
				pr_err(
					"can't update %d result to user\n",
					idx);
			}
			break;

		case TEEC_MEMREF_PARTIAL_OUTPUT:
		case TEEC_MEMREF_PARTIAL_INOUT:
		case TEEC_MEMREF_WHOLE:
			parent = &cmd->param.c_shm[idx];
			if (type == TEEC_MEMREF_WHOLE) {
				offset = 0;
				size = parent->size;
			} else {
				offset = tmp_op.params[idx].memref.offset;
				size = tmp_op.params[idx].memref.size;
			}

			/* Returned updated size */
			size_new = cmd->param.params[idx].shm->size_req;
			tee_put_user(ctx, size_new,
					&cmd_io->op->params[idx].memref.size);

			/*
			 * If we allocated a tmpref buffer,
			 * copy back data to the user buffer
			 */
			if (is_mapped_temp(cmd->param.params[idx].shm->flags)) {
				if (parent->buffer &&
					offset + size_new <= parent->size) {
					if (tee_copy_to_user(ctx,
						parent->buffer + offset,
						cmd->param.params[idx].shm
							->resv.kaddr,
						size_new)) {
						pr_err(
							"can't update %d data to user\n",
							idx);
					}
				}
			}
			break;
		default:
			WARN_ON(1);
		}
	}

}

static void _release_tee_cmd(struct tee_session *sess, struct tee_cmd *cmd)
{
	int idx;
	struct tee_context *ctx;

	WARN_ON(!cmd);
	WARN_ON(!sess);
	WARN_ON(!sess->ctx);
	WARN_ON(!sess->ctx->tee);

	ctx = sess->ctx;


	tee_shm_free(cmd->uuid);

	if (cmd->param.type_original == TEEC_PARAM_TYPES(TEEC_NONE,
			TEEC_NONE, TEEC_NONE, TEEC_NONE))
		goto out;

	for (idx = 0; idx < TEEC_CONFIG_PAYLOAD_REF_COUNT; ++idx) {
		int type = TEEC_PARAM_TYPE_GET(cmd->param.type_original, idx);
		struct tee_shm *shm;

		switch (type) {
		case TEEC_NONE:
		case TEEC_VALUE_INPUT:
		case TEEC_VALUE_OUTPUT:
		case TEEC_VALUE_INOUT:
			break;
		case TEEC_MEMREF_TEMP_INPUT:
		case TEEC_MEMREF_TEMP_OUTPUT:
		case TEEC_MEMREF_TEMP_INOUT:
		case TEEC_MEMREF_WHOLE:
		case TEEC_MEMREF_PARTIAL_INPUT:
		case TEEC_MEMREF_PARTIAL_OUTPUT:
		case TEEC_MEMREF_PARTIAL_INOUT:
			if (IS_ERR_OR_NULL(cmd->param.params[idx].shm))
				break;

			shm = cmd->param.params[idx].shm;

			if (is_mapped_temp(shm->flags))
				tee_shm_free(shm);
			else
				tee_shm_put(ctx, shm);
			break;
		default:
			WARN_ON(1);
		}
	}

out:
	memset(cmd, 0, sizeof(struct tee_cmd));
}
