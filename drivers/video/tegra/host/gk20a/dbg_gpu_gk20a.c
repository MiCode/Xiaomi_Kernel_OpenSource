/*
 * Tegra GK20A GPU Debugger/Profiler Driver
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/fs.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/nvhost.h>
#include <linux/nvhost_dbg_gpu_ioctl.h>

#include "dev.h"
#include "nvhost_hwctx.h"
#include "nvhost_acm.h"
#include "gk20a.h"
#include "gr_gk20a.h"
#include "gk20a_gating_reglist.h"
#include "dbg_gpu_gk20a.h"
#include "regops_gk20a.h"
#include "hw_therm_gk20a.h"

struct dbg_gpu_session_ops dbg_gpu_session_ops_gk20a = {
	.exec_reg_ops = exec_regops_gk20a,
};

/* silly allocator - just increment session id */
static atomic_t session_id = ATOMIC_INIT(0);
static int generate_session_id(void)
{
	return atomic_add_return(1, &session_id);
}

static int alloc_session(struct dbg_session_gk20a **_dbg_s)
{
	struct dbg_session_gk20a *dbg_s;
	*_dbg_s = NULL;

	nvhost_dbg(dbg_fn | dbg_gpu_dbg, "");

	dbg_s = kzalloc(sizeof(*dbg_s), GFP_KERNEL);
	if (!dbg_s)
		return -ENOMEM;

	dbg_s->id = generate_session_id();
	dbg_s->ops = &dbg_gpu_session_ops_gk20a;
	*_dbg_s = dbg_s;
	return 0;
}

int gk20a_dbg_gpu_do_dev_open(struct inode *inode, struct file *filp, bool is_profiler)
{
	struct dbg_session_gk20a *dbg_session;
	struct nvhost_device_data *pdata;

	struct platform_device *pdev;
	struct device *dev;

	int err;

	if (!is_profiler)
		pdata = container_of(inode->i_cdev,
			     struct nvhost_device_data, dbg_cdev);
	else
		pdata = container_of(inode->i_cdev,
				     struct nvhost_device_data, prof_cdev);
	pdev = pdata->pdev;
	dev  = &pdev->dev;

	nvhost_dbg(dbg_fn | dbg_gpu_dbg, "dbg session: %s", dev_name(dev));

	err  = alloc_session(&dbg_session);
	if (err)
		return err;

	filp->private_data = dbg_session;
	dbg_session->pdata = pdata;
	dbg_session->pdev  = pdev;
	dbg_session->dev   = dev;
	dbg_session->g     = get_gk20a(pdev);
	dbg_session->is_profiler = is_profiler;
	dbg_session->is_pg_disabled = false;

	INIT_LIST_HEAD(&dbg_session->dbg_s_list_node);
	init_waitqueue_head(&dbg_session->dbg_events.wait_queue);
	dbg_session->dbg_events.events_enabled = false;
	dbg_session->dbg_events.num_pending_events = 0;

	return 0;
}

/* used in scenarios where the debugger session can take just the inter-session
 * lock for performance, but the profiler session must take the per-gpu lock
 * since it might not have an associated channel. */
static void gk20a_dbg_session_mutex_lock(struct dbg_session_gk20a *dbg_s)
{
	if (dbg_s->is_profiler)
		mutex_lock(&dbg_s->g->dbg_sessions_lock);
	else
		mutex_lock(&dbg_s->ch->dbg_s_lock);
}

static void gk20a_dbg_session_mutex_unlock(struct dbg_session_gk20a *dbg_s)
{
	if (dbg_s->is_profiler)
		mutex_unlock(&dbg_s->g->dbg_sessions_lock);
	else
		mutex_unlock(&dbg_s->ch->dbg_s_lock);
}

static void gk20a_dbg_gpu_events_enable(struct dbg_session_gk20a *dbg_s)
{
	nvhost_dbg(dbg_fn | dbg_gpu_dbg, "");

	gk20a_dbg_session_mutex_lock(dbg_s);

	dbg_s->dbg_events.events_enabled = true;
	dbg_s->dbg_events.num_pending_events = 0;

	gk20a_dbg_session_mutex_unlock(dbg_s);
}

static void gk20a_dbg_gpu_events_disable(struct dbg_session_gk20a *dbg_s)
{
	nvhost_dbg(dbg_fn | dbg_gpu_dbg, "");

	gk20a_dbg_session_mutex_lock(dbg_s);

	dbg_s->dbg_events.events_enabled = false;
	dbg_s->dbg_events.num_pending_events = 0;

	gk20a_dbg_session_mutex_unlock(dbg_s);
}

static void gk20a_dbg_gpu_events_clear(struct dbg_session_gk20a *dbg_s)
{
	nvhost_dbg(dbg_fn | dbg_gpu_dbg, "");

	gk20a_dbg_session_mutex_lock(dbg_s);

	if (dbg_s->dbg_events.events_enabled &&
			dbg_s->dbg_events.num_pending_events > 0)
		dbg_s->dbg_events.num_pending_events--;

	gk20a_dbg_session_mutex_unlock(dbg_s);
}

static int gk20a_dbg_gpu_events_ctrl(struct dbg_session_gk20a *dbg_s,
			  struct nvhost_dbg_gpu_events_ctrl_args *args)
{
	int ret = 0;

	nvhost_dbg(dbg_fn | dbg_gpu_dbg, "dbg events ctrl cmd %d", args->cmd);

	if (!dbg_s->ch) {
		nvhost_err(dev_from_gk20a(dbg_s->g),
			   "no channel bound to dbg session\n");
		return -EINVAL;
	}

	switch (args->cmd) {
	case NVHOST_DBG_GPU_EVENTS_CTRL_CMD_ENABLE:
		gk20a_dbg_gpu_events_enable(dbg_s);
		break;

	case NVHOST_DBG_GPU_EVENTS_CTRL_CMD_DISABLE:
		gk20a_dbg_gpu_events_disable(dbg_s);
		break;

	case NVHOST_DBG_GPU_EVENTS_CTRL_CMD_CLEAR:
		gk20a_dbg_gpu_events_clear(dbg_s);
		break;

	default:
		nvhost_err(dev_from_gk20a(dbg_s->g),
			   "unrecognized dbg gpu events ctrl cmd: 0x%x",
			   args->cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

unsigned int gk20a_dbg_gpu_dev_poll(struct file *filep, poll_table *wait)
{
	unsigned int mask = 0;
	struct dbg_session_gk20a *dbg_s = filep->private_data;

	nvhost_dbg(dbg_fn | dbg_gpu_dbg, "");

	poll_wait(filep, &dbg_s->dbg_events.wait_queue, wait);

	gk20a_dbg_session_mutex_lock(dbg_s);

	if (dbg_s->dbg_events.events_enabled &&
			dbg_s->dbg_events.num_pending_events > 0) {
		nvhost_dbg(dbg_gpu_dbg, "found pending event on session id %d",
				dbg_s->id);
		nvhost_dbg(dbg_gpu_dbg, "%d events pending",
				dbg_s->dbg_events.num_pending_events);
		mask = (POLLPRI | POLLIN);
	}

	gk20a_dbg_session_mutex_unlock(dbg_s);

	return mask;
}

int gk20a_dbg_gpu_dev_open(struct inode *inode, struct file *filp)
{
	nvhost_dbg(dbg_fn | dbg_gpu_dbg, "");
	return gk20a_dbg_gpu_do_dev_open(inode, filp, false /* not profiler */);
}

int gk20a_prof_gpu_dev_open(struct inode *inode, struct file *filp)
{
	nvhost_dbg(dbg_fn | dbg_gpu_dbg, "");
	return gk20a_dbg_gpu_do_dev_open(inode, filp, true /* is profiler */);
}

void gk20a_dbg_gpu_post_events(struct channel_gk20a *ch)
{
	struct dbg_session_gk20a *dbg_s;

	nvhost_dbg(dbg_fn | dbg_gpu_dbg, "");

	/* guard against the session list being modified */
	mutex_lock(&ch->dbg_s_lock);

	list_for_each_entry(dbg_s, &ch->dbg_s_list, dbg_s_list_node) {
		if (dbg_s->dbg_events.events_enabled) {
			nvhost_dbg(dbg_gpu_dbg, "posting event on session id %d",
					dbg_s->id);
			nvhost_dbg(dbg_gpu_dbg, "%d events pending",
					dbg_s->dbg_events.num_pending_events);

			dbg_s->dbg_events.num_pending_events++;

			wake_up_interruptible_all(&dbg_s->dbg_events.wait_queue);
		}
	}

	mutex_unlock(&ch->dbg_s_lock);
}


static int dbg_set_powergate(struct dbg_session_gk20a *dbg_s,
				__u32  powermode);

static int dbg_unbind_channel_gk20a(struct dbg_session_gk20a *dbg_s)
{
	struct channel_gk20a *ch_gk20a = dbg_s->ch;
	struct gk20a *g = dbg_s->g;

	nvhost_dbg(dbg_fn | dbg_gpu_dbg, "");

	/* wasn't bound to start with ? */
	if (!ch_gk20a) {
		nvhost_dbg(dbg_gpu_dbg | dbg_fn, "not bound already?");
		return -ENODEV;
	}

	mutex_lock(&g->dbg_sessions_lock);
	mutex_lock(&ch_gk20a->dbg_s_lock);

	--g->dbg_sessions;

	/* Powergate enable is called here as possibility of dbg_session
	 * which called powergate disable ioctl, to be killed without calling
	 * powergate enable ioctl
	 */
	dbg_set_powergate(dbg_s, NVHOST_DBG_GPU_POWERGATE_MODE_ENABLE);

	dbg_s->ch       = NULL;
	fput(dbg_s->hwctx_f);
	dbg_s->hwctx_f   = NULL;

	list_del_init(&dbg_s->dbg_s_list_node);

	mutex_unlock(&ch_gk20a->dbg_s_lock);
	mutex_unlock(&g->dbg_sessions_lock);

	return 0;
}

int gk20a_dbg_gpu_dev_release(struct inode *inode, struct file *filp)
{
	struct dbg_session_gk20a *dbg_s = filp->private_data;

	nvhost_dbg(dbg_gpu_dbg | dbg_fn, "%s", dev_name(dbg_s->dev));

	/* unbind if it was bound */
	if (!dbg_s->ch)
		return 0;
	dbg_unbind_channel_gk20a(dbg_s);

	kfree(dbg_s);
	return 0;
}

static int dbg_bind_channel_gk20a(struct dbg_session_gk20a *dbg_s,
			  struct nvhost_dbg_gpu_bind_channel_args *args)
{
	struct file *f;
	struct nvhost_hwctx *hwctx;
	struct gk20a *g;
	struct channel_gk20a *ch_gk20a;

	nvhost_dbg(dbg_fn|dbg_gpu_dbg, "%s fd=%d",
		   dev_name(dbg_s->dev), args->channel_fd);

	if (args->channel_fd == ~0)
		return dbg_unbind_channel_gk20a(dbg_s);

	/* even though get_file_hwctx is doing this it releases it as well */
	/* by holding it here we'll keep it from disappearing while the
	 * debugger is in session */
	f = fget(args->channel_fd);
	if (!f)
		return -ENODEV;

	hwctx = nvhost_channel_get_file_hwctx(args->channel_fd);
	if (!hwctx) {
		nvhost_dbg_fn("no hwctx found for fd");
		fput(f);
		return -EINVAL;
	}
	/* be sure this is actually the right type of hwctx */
	if (hwctx->channel->dev != dbg_s->pdev) {
		nvhost_dbg_fn("hwctx module type mismatch");
		fput(f);
		return -EINVAL;
	}
	if (!hwctx->priv) {
		nvhost_dbg_fn("no priv");
		fput(f);
		return -ENODEV;
	}

	ch_gk20a = (struct channel_gk20a *)hwctx->priv;
	g = dbg_s->g;
	nvhost_dbg_fn("%s hwchid=%d", dev_name(dbg_s->dev), ch_gk20a->hw_chid);

	mutex_lock(&g->dbg_sessions_lock);
	mutex_lock(&ch_gk20a->dbg_s_lock);

	dbg_s->hwctx_f  = f;
	dbg_s->ch       = ch_gk20a;
	list_add(&dbg_s->dbg_s_list_node, &dbg_s->ch->dbg_s_list);

	g->dbg_sessions++;

	mutex_unlock(&ch_gk20a->dbg_s_lock);
	mutex_unlock(&g->dbg_sessions_lock);
	return 0;
}

static int nvhost_ioctl_channel_reg_ops(struct dbg_session_gk20a *dbg_s,
				struct nvhost_dbg_gpu_exec_reg_ops_args *args);

static int nvhost_ioctl_powergate_gk20a(struct dbg_session_gk20a *dbg_s,
				struct nvhost_dbg_gpu_powergate_args *args);

static int nvhost_dbg_gpu_ioctl_smpc_ctxsw_mode(struct dbg_session_gk20a *dbg_s,
			      struct nvhost_dbg_gpu_smpc_ctxsw_mode_args *args);

long gk20a_dbg_gpu_dev_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg)
{
	struct dbg_session_gk20a *dbg_s = filp->private_data;
	struct gk20a *g = get_gk20a(dbg_s->pdev);
	u8 buf[NVHOST_DBG_GPU_IOCTL_MAX_ARG_SIZE];
	int err = 0;

	nvhost_dbg(dbg_fn | dbg_gpu_dbg, "");

	if ((_IOC_TYPE(cmd) != NVHOST_DBG_GPU_IOCTL_MAGIC) ||
	    (_IOC_NR(cmd) == 0) ||
	    (_IOC_NR(cmd) > NVHOST_DBG_GPU_IOCTL_LAST))
		return -EFAULT;

	BUG_ON(_IOC_SIZE(cmd) > NVHOST_DBG_GPU_IOCTL_MAX_ARG_SIZE);

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (copy_from_user(buf, (void __user *)arg, _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	switch (cmd) {
	case NVHOST_DBG_GPU_IOCTL_BIND_CHANNEL:
		err = dbg_bind_channel_gk20a(dbg_s,
			     (struct nvhost_dbg_gpu_bind_channel_args *)buf);
		nvhost_dbg(dbg_gpu_dbg, "ret=%d", err);
		break;

	case NVHOST_DBG_GPU_IOCTL_REG_OPS:
		err = nvhost_ioctl_channel_reg_ops(dbg_s,
			   (struct nvhost_dbg_gpu_exec_reg_ops_args *)buf);
		nvhost_dbg(dbg_gpu_dbg, "ret=%d", err);
		break;

	case NVHOST_DBG_GPU_IOCTL_POWERGATE:
		err = nvhost_ioctl_powergate_gk20a(dbg_s,
			   (struct nvhost_dbg_gpu_powergate_args *)buf);
		nvhost_dbg(dbg_gpu_dbg, "ret=%d", err);
		break;

	case NVHOST_DBG_GPU_IOCTL_EVENTS_CTRL:
		err = gk20a_dbg_gpu_events_ctrl(dbg_s,
			   (struct nvhost_dbg_gpu_events_ctrl_args *)buf);
		break;

	case NVHOST_DBG_GPU_IOCTL_SMPC_CTXSW_MODE:
		err = nvhost_dbg_gpu_ioctl_smpc_ctxsw_mode(dbg_s,
			   (struct nvhost_dbg_gpu_smpc_ctxsw_mode_args *)buf);
		break;

	default:
		nvhost_err(dev_from_gk20a(g),
			   "unrecognized dbg gpu ioctl cmd: 0x%x",
			   cmd);
		err = -ENOTTY;
		break;
	}

	if ((err == 0) && (_IOC_DIR(cmd) & _IOC_READ))
		err = copy_to_user((void __user *)arg,
				   buf, _IOC_SIZE(cmd));

	return err;
}

/* In order to perform a context relative op the context has
 * to be created already... which would imply that the
 * context switch mechanism has already been put in place.
 * So by the time we perform such an opertation it should always
 * be possible to query for the appropriate context offsets, etc.
 *
 * But note: while the dbg_gpu bind requires the a channel fd with
 * a bound hwctx it doesn't require an allocated gr/compute obj
 * at that point... so just having the bound hwctx doesn't work
 * to guarantee this.
 */
static bool gr_context_info_available(struct dbg_session_gk20a *dbg_s,
				      struct gr_gk20a *gr)
{
	int err;

	mutex_lock(&gr->ctx_mutex);
	err = !gr->ctx_vars.golden_image_initialized;
	mutex_unlock(&gr->ctx_mutex);
	if (err)
		return false;
	return true;

}

static int nvhost_ioctl_channel_reg_ops(struct dbg_session_gk20a *dbg_s,
				struct nvhost_dbg_gpu_exec_reg_ops_args *args)
{
	int err;
	struct device *dev = dbg_s->dev;
	struct gk20a *g = get_gk20a(dbg_s->pdev);
	struct nvhost_dbg_gpu_reg_op *ops;
	u64 ops_size = sizeof(ops[0]) * args->num_ops;

	nvhost_dbg_fn("%d ops, total size %llu", args->num_ops, ops_size);

	if (!dbg_s->ops) {
		nvhost_err(dev, "can't call reg_ops on an unbound debugger session");
		return -EINVAL;
	}

	/* be sure that ctx info is in place */
	if (!gr_context_info_available(dbg_s, &g->gr)) {
		nvhost_err(dev, "gr context data not available\n");
		return -ENODEV;
	}

	ops = kzalloc(ops_size, GFP_KERNEL);
	if (!ops) {
		nvhost_err(dev, "Allocating memory failed!");
		return -ENOMEM;
	}

	nvhost_dbg_fn("Copying regops from userspace");

	if (copy_from_user(ops, (void *)(uintptr_t)args->ops, ops_size)) {
		dev_err(dev, "copy_from_user failed!");
		err = -EFAULT;
		goto clean_up;
	}

	/* since exec_reg_ops sends methods to the ucode, it must take the
	 * global gpu lock to protect against mixing methods from debug sessions
	 * on other channels */
	mutex_lock(&g->dbg_sessions_lock);

	err = dbg_s->ops->exec_reg_ops(dbg_s, ops, args->num_ops);

	mutex_unlock(&g->dbg_sessions_lock);

	if (err) {
		nvhost_err(dev, "dbg regops failed");
		goto clean_up;
	}

	nvhost_dbg_fn("Copying result to userspace");

	if (copy_to_user((void *)(uintptr_t)args->ops, ops, ops_size)) {
		dev_err(dev, "copy_to_user failed!");
		err = -EFAULT;
		goto clean_up;
	}
	return 0;
 clean_up:
	kfree(ops);
	return err;
}

static int dbg_set_powergate(struct dbg_session_gk20a *dbg_s,
				__u32  powermode)
{
	int err = 0;
	struct gk20a *g = get_gk20a(dbg_s->pdev);

	 /* This function must be called with g->dbg_sessions_lock held */

	nvhost_dbg(dbg_fn|dbg_gpu_dbg, "%s powergate mode = %d",
		   dev_name(dbg_s->dev), powermode);

	switch (powermode) {
	case NVHOST_DBG_GPU_POWERGATE_MODE_DISABLE:
		/* save off current powergate, clk state.
		 * set gpu module's can_powergate = 0.
		 * set gpu module's clk to max.
		 * while *a* debug session is active there will be no power or
		 * clocking state changes allowed from mainline code (but they
		 * should be saved).
		 */
		/* Allow powergate disable if the current dbg_session doesn't
		 * call a powergate disable ioctl and the global
		 * powergating_disabled_refcount is zero
		 */

		if ((dbg_s->is_pg_disabled == false) &&
		    (g->dbg_powergating_disabled_refcount++ == 0)) {

			nvhost_dbg(dbg_gpu_dbg | dbg_fn, "module busy");
			nvhost_module_busy(dbg_s->pdev);

			gr_gk20a_slcg_gr_load_gating_prod(g, false);
			gr_gk20a_slcg_perf_load_gating_prod(g, false);
			gr_gk20a_init_blcg_mode(g, BLCG_RUN, ENGINE_GR_GK20A);

			g->elcg_enabled = false;
			gr_gk20a_init_elcg_mode(g, ELCG_RUN, ENGINE_GR_GK20A);
			gr_gk20a_init_elcg_mode(g, ELCG_RUN, ENGINE_CE2_GK20A);

			gk20a_pmu_disable_elpg(g);
		}

		dbg_s->is_pg_disabled = true;
		break;

	case NVHOST_DBG_GPU_POWERGATE_MODE_ENABLE:
		/* restore (can) powergate, clk state */
		/* release pending exceptions to fault/be handled as usual */
		/*TBD: ordering of these? */

		/* Re-enabling powergate as no other sessions want
		 * powergate disabled and the current dbg-sessions had
		 * requested the powergate disable through ioctl
		*/
		if (dbg_s->is_pg_disabled &&
		    --g->dbg_powergating_disabled_refcount == 0) {

			g->elcg_enabled = true;
			gr_gk20a_init_elcg_mode(g, ELCG_AUTO, ENGINE_GR_GK20A);
			gr_gk20a_init_elcg_mode(g, ELCG_AUTO, ENGINE_CE2_GK20A);
			gr_gk20a_init_blcg_mode(g, BLCG_AUTO, ENGINE_GR_GK20A);

			gr_gk20a_slcg_gr_load_gating_prod(g, g->slcg_enabled);
			gr_gk20a_slcg_perf_load_gating_prod(g, g->slcg_enabled);

			gk20a_pmu_enable_elpg(g);

			nvhost_dbg(dbg_gpu_dbg | dbg_fn, "module idle");
			nvhost_module_idle(dbg_s->pdev);
		}

		dbg_s->is_pg_disabled = false;
		break;

	default:
		nvhost_err(dev_from_gk20a(g),
			   "unrecognized dbg gpu powergate mode: 0x%x",
			   powermode);
		err = -ENOTTY;
		break;
	}

	return err;
}

static int nvhost_ioctl_powergate_gk20a(struct dbg_session_gk20a *dbg_s,
				struct nvhost_dbg_gpu_powergate_args *args)
{
	int err;
	struct gk20a *g = get_gk20a(dbg_s->pdev);
	nvhost_dbg_fn("%s  powergate mode = %d",
		      dev_name(dbg_s->dev), args->mode);

	mutex_lock(&g->dbg_sessions_lock);
	err = dbg_set_powergate(dbg_s, args->mode);
	mutex_unlock(&g->dbg_sessions_lock);
	return  err;
}

static int nvhost_dbg_gpu_ioctl_smpc_ctxsw_mode(struct dbg_session_gk20a *dbg_s,
			       struct nvhost_dbg_gpu_smpc_ctxsw_mode_args *args)
{
	int err;
	struct gk20a *g = get_gk20a(dbg_s->pdev);
	struct channel_gk20a *ch_gk20a;

	nvhost_dbg_fn("%s smpc ctxsw mode = %d",
		      dev_name(dbg_s->dev), args->mode);

	/* Take the global lock, since we'll be doing global regops */
	mutex_lock(&g->dbg_sessions_lock);

	ch_gk20a = dbg_s->ch;

	if (!ch_gk20a) {
		nvhost_err(dev_from_gk20a(dbg_s->g),
		   "no bound channel for smpc ctxsw mode update\n");
		err = -EINVAL;
		goto clean_up;
	}

	err = gr_gk20a_update_smpc_ctxsw_mode(g, ch_gk20a,
		      args->mode == NVHOST_DBG_GPU_SMPC_CTXSW_MODE_CTXSW);
	if (err) {
		nvhost_err(dev_from_gk20a(dbg_s->g),
			   "error (%d) during smpc ctxsw mode update\n", err);
		goto clean_up;
	}
	/* The following regops are a hack/war to make up for the fact that we
	 * just scribbled into the ctxsw image w/o really knowing whether
	 * it was already swapped out in/out once or not, etc.
	 */
	{
		struct nvhost_dbg_gpu_reg_op ops[4];
		int i;
		for (i = 0; i < ARRAY_SIZE(ops); i++) {
			ops[i].op     = NVHOST_DBG_GPU_REG_OP_WRITE_32;
			ops[i].type   = NVHOST_DBG_GPU_REG_OP_TYPE_GR_CTX;
			ops[i].status = NVHOST_DBG_GPU_REG_OP_STATUS_SUCCESS;
			ops[i].value_hi      = 0;
			ops[i].and_n_mask_lo = 0;
			ops[i].and_n_mask_hi = 0;
		}
		/* gr_pri_gpcs_tpcs_sm_dsm_perf_counter_control_sel1_r();*/
		ops[0].offset   = 0x00419e08;
		ops[0].value_lo = 0x1d;

		/* gr_pri_gpcs_tpcs_sm_dsm_perf_counter_control5_r(); */
		ops[1].offset   = 0x00419e58;
		ops[1].value_lo = 0x1;

		/* gr_pri_gpcs_tpcs_sm_dsm_perf_counter_control3_r(); */
		ops[2].offset   = 0x00419e68;
		ops[2].value_lo = 0xaaaa;

		/* gr_pri_gpcs_tpcs_sm_dsm_perf_counter4_control_r(); */
		ops[3].offset   = 0x00419f40;
		ops[3].value_lo = 0x18;

		err = dbg_s->ops->exec_reg_ops(dbg_s, ops, ARRAY_SIZE(ops));
	}

 clean_up:
	mutex_unlock(&g->dbg_sessions_lock);
	return  err;
}
