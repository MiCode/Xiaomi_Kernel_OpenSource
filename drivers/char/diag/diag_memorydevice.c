// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2014-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/ratelimit.h>
#include <linux/workqueue.h>
#include <linux/diagchar.h>
#include <linux/delay.h>
#include <linux/kmemleak.h>
#include <linux/uaccess.h>
#include "diagchar.h"
#include "diag_memorydevice.h"
#include "diagfwd_bridge.h"
#include "diag_mux.h"
#include "diagmem.h"
#include "diagfwd.h"
#include "diagfwd_peripheral.h"
#include "diag_ipc_logging.h"

struct diag_md_info diag_md[NUM_DIAG_MD_DEV] = {
	{
		.id = DIAG_MD_LOCAL,
		.ctx = 0,
		.mempool = POOL_TYPE_MUX_APPS,
		.num_tbl_entries = 0,
		.md_info_inited = 0,
		.tbl = NULL,
		.ops = NULL,
	},
#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
	{
		.id = DIAG_MD_MDM,
		.ctx = 0,
		.mempool = POOL_TYPE_MDM_MUX,
		.num_tbl_entries = 0,
		.md_info_inited = 0,
		.tbl = NULL,
		.ops = NULL,
	},
	{
		.id = DIAG_MD_MDM2,
		.ctx = 0,
		.mempool = POOL_TYPE_MDM2_MUX,
		.num_tbl_entries = 0,
		.md_info_inited = 0,
		.tbl = NULL,
		.ops = NULL,
	},
	{
		.id = DIAG_MD_SMUX,
		.ctx = 0,
		.mempool = POOL_TYPE_QSC_MUX,
		.num_tbl_entries = 0,
		.md_info_inited = 0,
		.tbl = NULL,
		.ops = NULL,
	}
#endif
};

int diag_md_register(int id, int ctx, struct diag_mux_ops *ops)
{
	if (id < 0 || id >= NUM_DIAG_MD_DEV || !ops)
		return -EINVAL;

	diag_md[id].ops = ops;
	diag_md[id].ctx = ctx;
	return 0;
}

void diag_md_open_all(void)
{
	int i;
	struct diag_md_info *ch = NULL;

	for (i = 0; i < NUM_DIAG_MD_DEV; i++) {
		ch = &diag_md[i];
		if (!ch->md_info_inited)
			continue;
		if (ch->ops && ch->ops->open)
			ch->ops->open(ch->ctx, DIAG_MEMORY_DEVICE_MODE);
	}
}
void diag_md_open_device(int id)
{

	struct diag_md_info *ch = NULL;

		ch = &diag_md[id];
		if (!ch->md_info_inited)
			return;
		if (ch->ops && ch->ops->open)
			ch->ops->open(ch->ctx, DIAG_MEMORY_DEVICE_MODE);

}
void diag_md_close_all(void)
{
	int i, j;
	unsigned long flags;
	struct diag_md_info *ch = NULL;
	struct diag_buf_tbl_t *entry = NULL;

	for (i = 0; i < NUM_DIAG_MD_DEV; i++) {
		ch = &diag_md[i];
		if (!ch->md_info_inited)
			continue;

		if (ch->ops && ch->ops->close)
			ch->ops->close(ch->ctx, DIAG_MEMORY_DEVICE_MODE);

		/*
		 * When we close the Memory device mode, make sure we flush the
		 * internal buffers in the table so that there are no stale
		 * entries.
		 */
		spin_lock_irqsave(&ch->lock, flags);
		for (j = 0; j < ch->num_tbl_entries; j++) {
			entry = &ch->tbl[j];
			if (entry->len <= 0)
				continue;
			if (ch->ops && ch->ops->write_done)
				ch->ops->write_done(entry->buf, entry->len,
						    entry->ctx,
						    DIAG_MEMORY_DEVICE_MODE);
			entry->buf = NULL;
			entry->len = 0;
			entry->ctx = 0;
		}
		spin_unlock_irqrestore(&ch->lock, flags);
	}

	diag_ws_reset(DIAG_WS_MUX);
}
void diag_md_close_device(int id)
{
	int  j;
	unsigned long flags;
	struct diag_md_info *ch = NULL;
	struct diag_buf_tbl_t *entry = NULL;

		ch = &diag_md[id];
		if (!ch->md_info_inited)
			return;

		if (ch->ops && ch->ops->close)
			ch->ops->close(ch->ctx, DIAG_MEMORY_DEVICE_MODE);

		/*
		 * When we close the Memory device mode, make sure we flush the
		 * internal buffers in the table so that there are no stale
		 * entries.
		 */
		spin_lock_irqsave(&ch->lock, flags);
		for (j = 0; j < ch->num_tbl_entries; j++) {
			entry = &ch->tbl[j];
			if (entry->len <= 0)
				continue;
			if (ch->ops && ch->ops->write_done)
				ch->ops->write_done(entry->buf, entry->len,
						    entry->ctx,
						    DIAG_MEMORY_DEVICE_MODE);
			entry->buf = NULL;
			entry->len = 0;
			entry->ctx = 0;
		}
		spin_unlock_irqrestore(&ch->lock, flags);

	diag_ws_reset(DIAG_WS_MUX);
}
int diag_md_write(int id, unsigned char *buf, int len, int ctx)
{
	int i, peripheral, pid = 0;
	uint8_t found = 0;
	unsigned long flags, flags_sec;
	struct diag_md_info *ch = NULL;
	struct diag_md_session_t *session_info = NULL;

	if (id < 0 || id >= NUM_DIAG_MD_DEV || id >= DIAG_NUM_PROC)
		return -EINVAL;

	if (!buf || len < 0)
		return -EINVAL;
	if (id == DIAG_LOCAL_PROC) {
		peripheral = diag_md_get_peripheral(ctx);
		if (peripheral < 0)
			return -EINVAL;
	} else {
		peripheral = 0;
	}
	mutex_lock(&driver->md_session_lock);
	session_info = diag_md_session_get_peripheral(id, peripheral);
	if (!session_info) {
		mutex_unlock(&driver->md_session_lock);
		return -EIO;
	}

	pid = session_info->pid;

	ch = &diag_md[id];
	if (!ch || !ch->md_info_inited) {
		mutex_unlock(&driver->md_session_lock);
		return -EINVAL;
	}

	spin_lock_irqsave(&ch->lock, flags);
	if (peripheral == APPS_DATA) {
		spin_lock_irqsave(&driver->diagmem_lock, flags_sec);
		if (!hdlc_data.allocated && !non_hdlc_data.allocated) {
			spin_unlock_irqrestore(&driver->diagmem_lock,
				flags_sec);
			spin_unlock_irqrestore(&ch->lock, flags);
			mutex_unlock(&driver->md_session_lock);
			return -EINVAL;
		}
	}
	for (i = 0; i < ch->num_tbl_entries && !found; i++) {
		if (ch->tbl[i].buf != buf)
			continue;
		found = 1;
		pr_err_ratelimited("diag: trying to write the same buffer buf: %pK, len: %d, back to the table for p: %d, t: %d, buf_num: %d, proc: %d, i: %d\n",
				   buf, ch->tbl[i].len, GET_BUF_PERIPHERAL(ctx),
				   GET_BUF_TYPE(ctx), GET_BUF_NUM(ctx), id, i);
		ch->tbl[i].buf = NULL;
		ch->tbl[i].len = 0;
		ch->tbl[i].ctx = 0;
	}

	if (found) {
		if (peripheral == APPS_DATA)
			spin_unlock_irqrestore(&driver->diagmem_lock,
				flags_sec);
		spin_unlock_irqrestore(&ch->lock, flags);
		mutex_unlock(&driver->md_session_lock);
		return -ENOMEM;
	}

	for (i = 0; i < ch->num_tbl_entries && !found; i++) {
		if (ch->tbl[i].len == 0) {
			ch->tbl[i].buf = buf;
			ch->tbl[i].len = len;
			ch->tbl[i].ctx = ctx;
			found = 1;
			diag_ws_on_read(DIAG_WS_MUX, len);
		}
	}
	if (peripheral == APPS_DATA)
		spin_unlock_irqrestore(&driver->diagmem_lock, flags_sec);
	spin_unlock_irqrestore(&ch->lock, flags);
	mutex_unlock(&driver->md_session_lock);

	if (!found) {
		pr_err_ratelimited("diag: Unable to find an empty space in table, please reduce logging rate, proc: %d\n",
				   id);
		return -ENOMEM;
	}

	found = 0;
	mutex_lock(&driver->diagchar_mutex);
	for (i = 0; i < driver->num_clients && !found; i++) {
		if ((driver->client_map[i].pid != pid) ||
		    (driver->client_map[i].pid == 0))
			continue;

		found = 1;
		if (!(driver->data_ready[i] & USER_SPACE_DATA_TYPE)) {
			driver->data_ready[i] |= USER_SPACE_DATA_TYPE;
			atomic_inc(&driver->data_ready_notif[i]);
		}
		pr_debug("diag: wake up logging process\n");
		wake_up_interruptible(&driver->wait_q);
	}
	mutex_unlock(&driver->diagchar_mutex);

	if (!found)
		return -EINVAL;

	return 0;
}

int diag_md_copy_to_user(char __user *buf, int *pret, size_t buf_size,
			struct diag_md_session_t *info)
{
	int i, j;
	int err = 0;
	int ret = *pret;
	int num_data = 0;
	int remote_token;
	unsigned long flags;
	struct diag_md_info *ch = NULL;
	struct diag_buf_tbl_t *entry = NULL;
	uint8_t drain_again = 0;
	int peripheral = 0;
	struct diag_md_session_t *session_info = NULL;
	struct pid *pid_struct = NULL;

	for (i = 0; i < NUM_DIAG_MD_DEV && !err; i++) {
		ch = &diag_md[i];
		if (!ch->md_info_inited)
			continue;
		for (j = 0; j < ch->num_tbl_entries && !err; j++) {
			entry = &ch->tbl[j];
			if (entry->len <= 0 || entry->buf == NULL)
				continue;

			peripheral = diag_md_get_peripheral(entry->ctx);
			if (peripheral < 0)
				goto drop_data;
			session_info =
			diag_md_session_get_peripheral(i, peripheral);
			if (!session_info)
				goto drop_data;

			if (session_info && info &&
				(session_info->pid != info->pid))
				continue;
			if ((info && (info->peripheral_mask[i] &
			    MD_PERIPHERAL_MASK(peripheral)) == 0))
				goto drop_data;
			pid_struct = find_get_pid(session_info->pid);
			if (!pid_struct) {
				err = -ESRCH;
				DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
					"diag: No such md_session_map[%d] with pid = %d err=%d exists..\n",
					peripheral, session_info->pid, err);
				goto drop_data;
			}
			/*
			 * If the data is from remote processor, copy the remote
			 * token first
			 */
			if (i > 0) {
				if ((ret + (3 * sizeof(int)) + entry->len) >=
							buf_size) {
					drain_again = 1;
					break;
				}
			} else {
				if ((ret + (2 * sizeof(int)) + entry->len) >=
						buf_size) {
					drain_again = 1;
					break;
				}
			}
			if (i > 0) {
				remote_token = diag_get_remote(i);
				if (get_pid_task(pid_struct, PIDTYPE_PID)) {
					err = copy_to_user(buf + ret,
							&remote_token,
							sizeof(int));
					if (err)
						goto drop_data;
					ret += sizeof(int);
				}
			}

			/* Copy the length of data being passed */
			if (get_pid_task(pid_struct, PIDTYPE_PID)) {
				err = copy_to_user(buf + ret,
						(void *)&(entry->len),
						sizeof(int));
				if (err)
					goto drop_data;
				ret += sizeof(int);
			}

			/* Copy the actual data being passed */
			if (get_pid_task(pid_struct, PIDTYPE_PID)) {
				err = copy_to_user(buf + ret,
						(void *)entry->buf,
						entry->len);
				if (err)
					goto drop_data;
				ret += entry->len;
			}
			/*
			 * The data is now copied to the user space client,
			 * Notify that the write is complete and delete its
			 * entry from the table
			 */
			num_data++;
drop_data:
			spin_lock_irqsave(&ch->lock, flags);
			if (ch->ops && ch->ops->write_done)
				ch->ops->write_done(entry->buf, entry->len,
						    entry->ctx,
						    DIAG_MEMORY_DEVICE_MODE);
			diag_ws_on_copy(DIAG_WS_MUX);
			entry->buf = NULL;
			entry->len = 0;
			entry->ctx = 0;
			spin_unlock_irqrestore(&ch->lock, flags);
		}
	}

	*pret = ret;
	if (pid_struct && get_pid_task(pid_struct, PIDTYPE_PID)) {
		err = copy_to_user(buf + sizeof(int),
				(void *)&num_data,
				sizeof(int));
	}
	diag_ws_on_copy_complete(DIAG_WS_MUX);
	if (drain_again)
		chk_logging_wakeup();

	return err;
}

int diag_md_close_peripheral(int id, uint8_t peripheral)
{
	int i;
	uint8_t found = 0;
	unsigned long flags;
	struct diag_md_info *ch = NULL;
	struct diag_buf_tbl_t *entry = NULL;

	if (id < 0 || id >= NUM_DIAG_MD_DEV || id >= DIAG_NUM_PROC)
		return -EINVAL;

	ch = &diag_md[id];
	if (!ch || !ch->md_info_inited)
		return -EINVAL;

	spin_lock_irqsave(&ch->lock, flags);
	for (i = 0; i < ch->num_tbl_entries && !found; i++) {
		entry = &ch->tbl[i];

		if (peripheral > NUM_PERIPHERALS) {
			if (GET_PD_CTXT(entry->ctx) != peripheral)
				continue;
		} else {
			if (GET_BUF_PERIPHERAL(entry->ctx) !=
					peripheral)
				continue;
		}
		found = 1;
		if (ch->ops && ch->ops->write_done) {
			ch->ops->write_done(entry->buf, entry->len,
					    entry->ctx,
					    DIAG_MEMORY_DEVICE_MODE);
			entry->buf = NULL;
			entry->len = 0;
			entry->ctx = 0;
		}
	}
	spin_unlock_irqrestore(&ch->lock, flags);
	return 0;
}

int diag_md_init(void)
{
	int i, j;
	struct diag_md_info *ch = NULL;

	for (i = 0; i < DIAG_MD_LOCAL_LAST; i++) {
		ch = &diag_md[i];
		ch->num_tbl_entries = diag_mempools[ch->mempool].poolsize;
		ch->tbl = kcalloc(ch->num_tbl_entries,
				  sizeof(struct diag_buf_tbl_t), GFP_KERNEL);
		if (!ch->tbl)
			goto fail;

		for (j = 0; j < ch->num_tbl_entries; j++) {
			ch->tbl[j].buf = NULL;
			ch->tbl[j].len = 0;
			ch->tbl[j].ctx = 0;
		}
		spin_lock_init(&(ch->lock));
		ch->md_info_inited = 1;
	}

	return 0;

fail:
	diag_md_exit();
	return -ENOMEM;
}

int diag_md_mdm_init(void)
{
	int i, j;
	struct diag_md_info *ch = NULL;

	for (i = DIAG_MD_BRIDGE_BASE; i < NUM_DIAG_MD_DEV; i++) {
		ch = &diag_md[i];
		ch->num_tbl_entries = diag_mempools[ch->mempool].poolsize;
		ch->tbl = kcalloc(ch->num_tbl_entries, sizeof(*ch->tbl),
				GFP_KERNEL);
		if (!ch->tbl)
			goto fail;

		for (j = 0; j < ch->num_tbl_entries; j++) {
			ch->tbl[j].buf = NULL;
			ch->tbl[j].len = 0;
			ch->tbl[j].ctx = 0;
		}
		spin_lock_init(&(ch->lock));
		ch->md_info_inited = 1;
	}

	return 0;

fail:
	diag_md_mdm_exit();
	return -ENOMEM;
}

void diag_md_exit(void)
{
	int i;
	struct diag_md_info *ch = NULL;

	for (i = 0; i < DIAG_MD_LOCAL_LAST; i++) {
		ch = &diag_md[i];
		kfree(ch->tbl);
		ch->num_tbl_entries = 0;
		ch->ops = NULL;
	}
}

void diag_md_mdm_exit(void)
{
	int i;
	struct diag_md_info *ch = NULL;

	for (i = DIAG_MD_BRIDGE_BASE; i < NUM_DIAG_MD_DEV; i++) {
		ch = &diag_md[i];
		kfree(ch->tbl);
		ch->num_tbl_entries = 0;
		ch->ops = NULL;
	}
}
