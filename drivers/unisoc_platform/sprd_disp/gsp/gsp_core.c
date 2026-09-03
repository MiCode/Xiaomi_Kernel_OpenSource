// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/sprd_iommu.h>
#include "gsp_core.h"
#include "gsp_dev.h"
#include "gsp_debug.h"
#include "gsp_interface.h"
#include "gsp_kcfg.h"
#include "gsp_sysfs.h"
#include "gsp_workqueue.h"
#include <uapi/linux/sched/types.h>

int gsp_core_verify(struct gsp_core *core)
{
	int ret = -1;
	struct gsp_dev *gsp = NULL;

	if (IS_ERR_OR_NULL(core)) {
		GSP_ERR("core verify params error\n");
		return ret;
	}

	gsp = core->parent;

	ret = (core->id > gsp->core_cnt) || (core->id < 0) ? 1 : 0;

	return ret;
}

void gsp_core_state_set(struct gsp_core *core, enum gsp_core_state st)
{
	atomic_set(&core->state, st);
}

enum gsp_core_state gsp_core_state_get(struct gsp_core *core)
{
	return (enum gsp_core_state)atomic_read(&core->state);
}

void gsp_core_suspend_state_set(struct gsp_core *core,
			enum gsp_core_suspend_state state)
{
	atomic_set(&core->suspend_state, state);
}

enum gsp_core_suspend_state gsp_core_suspend_state_get(struct gsp_core *core)
{
	return (enum gsp_core_suspend_state)atomic_read(&core->suspend_state);
}


int gsp_core_is_idle(struct gsp_core *core)
{
	int ret = 0;

	if (gsp_core_state_get(core) == CORE_STATE_IDLE)
		ret = 1;

	return ret;
}

int gsp_core_is_err(struct gsp_core *core)
{
	int ret = 0;

	if (gsp_core_state_get(core) < CORE_STATE_ERR)
		ret = 1;

	return ret;
}

int gsp_core_is_irq_handled(struct gsp_core *core)
{
	int ret = 0;

	if (gsp_core_state_get(core) == CORE_STATE_IRQ_HANDLED)
		ret = 1;

	return ret;
}

int gsp_core_is_irq_error(struct gsp_core *core)
{
	int ret = 0;

	if (gsp_core_state_get(core) == CORE_STATE_IRQ_ERR)
		ret = 1;

	return ret;
}

int gsp_core_is_trigger(struct gsp_core *core)
{
	int ret = 0;

	if (gsp_core_state_get(core) == CORE_STATE_TRIGGER)
		ret = 1;

	return ret;
}

int gsp_core_is_suspend(struct gsp_core *core)
{
	int ret = 0;

	if (gsp_core_state_get(core) == CORE_STATE_SUSPEND)
		ret = 1;

	return ret;
}

struct gsp_workqueue *gsp_core_to_workqueue(struct gsp_core *core)
{
	if (gsp_core_verify(core)) {
		GSP_ERR("core to workqueue params error\n");
		return NULL;
	}

	return core->wq;
}

struct device *gsp_core_to_device(struct gsp_core *core)
{
	return core->dev;
}

int gsp_core_to_id(struct gsp_core *core)
{
	return core->id;
}

struct gsp_dev *gsp_core_to_parent(struct gsp_core *core)
{
	return core->parent;
}

void gsp_core_reg_write(void __iomem *addr, u32 value)
{
	writel(value, addr);
}

u32 gsp_core_reg_read(void __iomem *addr)
{
	return readl(addr);
}

void gsp_core_reg_update(void __iomem *addr, u32 value, u32 mask)
{
	u32 orig = 0;
	u32 tmp = 0;

	orig = gsp_core_reg_read(addr);
	tmp = orig & (~mask);
	tmp |= value & mask;
	gsp_core_reg_write(addr, tmp);
}

int gsp_core_enable(struct gsp_core *core)
{
	if (gsp_core_verify(core)) {
		GSP_ERR("core[%d] enable params failed\n",
			gsp_core_to_id(core));
		return -1;
	}

	return core->ops->enable(core);
}

void gsp_core_disable(struct gsp_core *core)
{
	if (gsp_core_verify(core)) {
		GSP_ERR("core[%d] disable params failed\n",
			gsp_core_to_id(core));
		return;
	}

	core->ops->disable(core);
}

int gsp_core_parse_dt(struct gsp_core *core)
{
	int ret = -1;
	struct resource res;

	/*get gsp module register*/
	ret = of_address_to_resource(core->node, 0, &res);
	if (ret) {
		GSP_ERR("core address to resource failed\n");
		return ret;
	}

	core->base = devm_ioremap_resource(core->dev, &res);

	ret = core->ops->parse_dt(core);
	if (ret) {
		GSP_ERR("core[%d] parse specific dt failed\n", core->id);
		return ret;
	}

	return ret;
}

int gsp_core_suspend_wait(struct gsp_core *core)
{
	int ret = 0;
	int err = 0;
	long time = 0;

	if (gsp_core_is_idle(core))
		return ret;

	time = wait_for_completion_interruptible_timeout(&core->suspend_done,
						  GSP_CORE_SUSPEND_WAIT);
	if (time == -ERESTARTSYS) {
		GSP_ERR("core[%d] interrupt when suspend wait\n",
			gsp_core_to_id(core));
		err++;
	} else if (time == 0) {
		GSP_ERR("core[%d] suspend wait timeout\n",
			gsp_core_to_id(core));
		err++;
	} else if (time > 0) {
		GSP_DEBUG("core[%d] suspend wait success\n",
			gsp_core_to_id(core));
	}

	if (err)
		ret = -1;

	return ret;
}

int gsp_core_release_wait(struct gsp_core *core)
{
	int ret = -1;
	int err = 0;
	long time = 0;

	time = wait_for_completion_interruptible_timeout(&core->release_done,
							 GSP_CORE_RELEASE_WAIT);
	if (time == -ERESTARTSYS) {
		GSP_ERR("core[%d] interrupt when release wait\n",
			gsp_core_to_id(core));
		err++;
	} else if (time == 0) {
		GSP_ERR("core[%d] release wait timeout\n",
			gsp_core_to_id(core));
		err++;
	} else if (time > 0) {
		GSP_DEBUG("core[%d] release wait success\n",
			gsp_core_to_id(core));
	}

	if (err)
		ret = -1;

	reinit_completion(&core->release_done);
	return ret;
}

void gsp_core_work(struct gsp_core *core)
{
	bool ret = false;
	struct gsp_workqueue *wq = NULL;

	if (gsp_core_verify(core)) {
		GSP_ERR("core work params error\n");
		return;
	}

	wq = gsp_core_to_workqueue(core);
	if (!gsp_workqueue_is_filled(wq))
		return;

	if (gsp_core_state_get(core) != CORE_STATE_IDLE)
		return;

	gsp_core_state_set(core, CORE_STATE_TRIGGER);

	ret = kthread_queue_work(&core->kworker, &core->trigger);
	if (ret == false)
		GSP_WARN("queue trigger work failed\n");
}

void gsp_core_start_timer(struct gsp_core *core)
{
	if (gsp_core_verify(core)) {
		GSP_ERR("core start timer params error\n");
		return;
	}

	mod_timer(&core->timer, jiffies + msecs_to_jiffies(GSP_CORE_TIMER_OUT));
}

void gsp_core_stop_timer(struct gsp_core *core)
{
	if (gsp_core_verify(core)) {
		GSP_ERR("core stop timer params error\n");
		return;
	}

	del_timer(&core->timer);
}

void gsp_core_dump(struct gsp_core *core)
{
	struct gsp_dev *gsp = NULL;
	struct gsp_interface *interface = NULL;

	gsp = gsp_core_to_parent(core);
	interface = gsp_dev_to_interface(gsp);
	if (interface->ops && interface->ops->dump)
		interface->ops->dump(interface);
	core->ops->dump(core);
}

void gsp_core_trigger(struct kthread_work *work)
{
	int ret = 0;
	struct gsp_core *core = NULL;
	struct gsp_kcfg *kcfg = NULL;
	struct gsp_interface *interface = NULL;
	struct gsp_dev *gsp = NULL;

	core = container_of(work, struct gsp_core, trigger);
	if (gsp_core_verify(core)) {
		GSP_ERR("core[%d] trigger params error\n", core->id);
		goto done;
	}
	gsp = gsp_core_to_parent(core);
	interface = gsp_dev_to_interface(gsp);

	if (gsp_core_is_suspend(core)) {
		GSP_WARN("can't trigger when core is suspended\n");
		goto done;
	}

	pm_runtime_mark_last_busy(core->parent->dev);

	if (!core->ops->trigger) {
		GSP_ERR("core[%d] has no trigger func\n", core->id);
		goto done;
	}

	GSP_DEBUG("gsp core[%d] start trigger\n", gsp_core_to_id(core));
	if (gsp_core_is_trigger(core)) {
		gsp_core_state_set(core, CORE_STATE_BUSY);
		kcfg = gsp_workqueue_pull(core->wq);
		if (IS_ERR_OR_NULL(kcfg)) {
			gsp_core_state_set(core, CORE_STATE_IDLE);
			GSP_ERR("pull null kcfg\n");
			goto done;
		} else {
			core->current_kcfg = kcfg;
			gsp_kcfg_set_pulled(kcfg);
		}
	} else {
		GSP_WARN("core can't trigger because error state\n");
		goto done;
	}

	/* enable core must be invoked before iommu map */
	GSP_DEBUG("gsp core enable\n");
	ret = gsp_interface_prepare(interface);
	/*ret |= sprd_iommu_resume(core->dev);*/
	ret |= gsp_core_enable(core);
	if (ret) {
		gsp_core_state_set(core, CORE_STATE_ENABLE_ERR);
		GSP_ERR("core enable failed\n");
		goto done;
	}

	GSP_DEBUG("kcfg[%d] iommu map start\n", gsp_kcfg_to_tag(kcfg));
	ret = gsp_kcfg_iommu_map(kcfg);
	if (ret) {
		GSP_ERR("kcfg[%d] iommu map failed\n", gsp_kcfg_to_tag(kcfg));
		gsp_core_state_set(core, CORE_STATE_MAP_ERR);
		goto done;
	}

	if (gsp_kcfg_is_async(kcfg)) {
		GSP_DEBUG("gsp kcfg start fence wait\n");
		ret = gsp_kcfg_fence_wait(kcfg);
		if (ret) {
			GSP_ERR("acuqire fence wait failed\n");
			gsp_core_state_set(core, CORE_STATE_WAIT_ERR);
			goto done;
		}
	}

	ret = core->ops->trigger(core);
	switch (ret) {
	case GSP_K_HW_BUSY_ERR:
		gsp_core_state_set(core, CORE_STATE_HW_HANG_ERR);
		break;
	case GSP_K_CLK_CHK_ERR:
	case GSP_K_CTL_CODE_ERR:
		gsp_core_state_set(core, CORE_STATE_TRIGGER_ERR);
		break;
	case GSP_NO_ERR:
	default:
		gsp_core_start_timer(core);
		break;
	}

done:
	if (ret)
		kthread_queue_work(&core->kworker, &core->recover);
}

void gsp_core_release(struct kthread_work *work)
{
	int ret = -1;
	bool success = false;
	struct gsp_core *core = NULL;
	struct gsp_kcfg *kcfg = NULL;
	struct gsp_interface *interface = NULL;
	struct gsp_dev *gsp = NULL;

	core = container_of(work, struct gsp_core, release);
	if (gsp_core_verify(core)) {
		GSP_ERR("core[%d] release params error\n", core->id);
		return;
	}
	gsp = gsp_core_to_parent(core);
	interface = gsp_dev_to_interface(gsp);

	if (gsp_core_is_irq_error(core))
		gsp_core_dump(core);

	GSP_DEBUG("gsp core[%d] start release\n", gsp_core_to_id(core));
	if (gsp_core_is_irq_handled(core) || gsp_core_is_suspend(core)
	    || gsp_core_is_irq_error(core)) {
		kcfg = core->current_kcfg;
	} else {
		GSP_WARN("core can't release before irq handled\n");
		return;
	}

	if (gsp_kcfg_verify(kcfg)) {
		GSP_ERR("attempt to release invalid kcfg\n");
		return;
	}

	gsp_core_stop_timer(core);

	gsp_kcfg_iommu_unmap(kcfg);

	gsp_kcfg_put_dmabuf(kcfg);

	if (gsp_kcfg_is_async(kcfg))
		gsp_kcfg_fence_signal(kcfg);
	else
		gsp_kcfg_complete(kcfg);

	if (core->ops->release)
		ret = core->ops->release(core);

	/* disable core must be invoked after iommu map */
	gsp_core_disable(core);
	/*sprd_iommu_suspend(core->dev);*/
	gsp_interface_unprepare(interface);

	if (ret) {
		gsp_core_state_set(core, CORE_STATE_RELEASE_ERR);
		kthread_queue_work(&core->kworker, &core->recover);
		return;
	}

	if (gsp_workqueue_is_exhausted(core->wq))
		complete(&core->release_done);

	gsp_kcfg_put(kcfg);
	core->current_kcfg = NULL;
	GSP_DEBUG("core release resource success\n");

	if (gsp_core_is_suspend(core)) {
		complete(&core->suspend_done);
		return;
	}

	pm_runtime_mark_last_busy(core->parent->dev);
	pm_runtime_put_autosuspend(core->parent->dev);
	if (gsp_core_suspend_state_get(core) ==
			 CORE_STATE_SUSPEND_WAIT) {
		complete(&core->suspend_done);
	}

	if (!gsp_workqueue_is_filled(core->wq)) {
		gsp_core_state_set(core, CORE_STATE_IDLE);
		return;
	}

	gsp_core_state_set(core, CORE_STATE_TRIGGER);
	success = kthread_queue_work(&core->kworker, &core->trigger);
	if (success == false)
		GSP_WARN("queue trigger work failed\n");

	pm_runtime_mark_last_busy(core->parent->dev);
}

void gsp_core_recover(struct kthread_work *work)
{
	bool need_reset = false;
	bool need_dump = false;
	struct gsp_core *core = NULL;
	struct gsp_kcfg *kcfg = NULL;
	struct gsp_interface *interface = NULL;
	struct gsp_dev *gsp = NULL;

	core = container_of(work, struct gsp_core, recover);
	if (gsp_core_verify(core)) {
		GSP_ERR("core recover params error\n");
		return;
	}

	GSP_INFO("enter gsp core recover, with state: %d\n",
		gsp_core_state_get(core));

	if (!gsp_core_is_err(core)) {
		GSP_ERR("there is nothing wrong with core\n");
		return;
	}

	if (!core->current_kcfg) {
		GSP_INFO("nothing need to do\n");
		return;
	}

	gsp_core_stop_timer(core);
	kcfg = core->current_kcfg;
	gsp = gsp_core_to_parent(core);
	interface = gsp_dev_to_interface(gsp);

	switch (gsp_core_state_get(core)) {
	case CORE_STATE_ENABLE_ERR:
		gsp_kcfg_put_dmabuf(kcfg);
		if (gsp_kcfg_is_async(kcfg))
			gsp_kcfg_fence_free(kcfg);
		else
			gsp_kcfg_complete(kcfg);
		break;

	case CORE_STATE_MAP_ERR:
		gsp_kcfg_put_dmabuf(kcfg);
		if (gsp_kcfg_is_async(kcfg))
			gsp_kcfg_fence_free(kcfg);
		else
			gsp_kcfg_complete(kcfg);
		break;

	case CORE_STATE_WAIT_ERR:
		need_dump = true;
		gsp_kcfg_iommu_unmap(kcfg);
		gsp_kcfg_put_dmabuf(kcfg);
		if (gsp_kcfg_is_async(kcfg))
			gsp_kcfg_fence_free(kcfg);
		else
			gsp_kcfg_complete(kcfg);
		break;

	case CORE_STATE_TRIGGER_ERR:
		need_dump = true;
		gsp_kcfg_iommu_unmap(kcfg);
		gsp_kcfg_put_dmabuf(kcfg);
		if (gsp_kcfg_is_async(kcfg))
			gsp_kcfg_fence_free(kcfg);
		else
			gsp_kcfg_complete(kcfg);
		break;

	case CORE_STATE_HW_HANG_ERR:
		need_reset = true;
		need_dump = true;
		gsp_kcfg_iommu_unmap(kcfg);
		gsp_kcfg_put_dmabuf(kcfg);
		if (gsp_kcfg_is_async(kcfg))
			gsp_kcfg_fence_free(kcfg);
		else
			gsp_kcfg_complete(kcfg);
		break;

	case CORE_STATE_RELEASE_ERR:
		/* to do */
		break;

	default:
		GSP_WARN("unknown error state\n");
		break;
	}

	gsp_kcfg_put(kcfg);

	if (need_dump)
		gsp_core_dump(core);

	if (need_reset)
		gsp_core_reset(core);

	gsp_core_disable(core);

	/*sprd_iommu_suspend(core->dev);*/
	gsp_interface_unprepare(interface);

	core->current_kcfg = NULL;

	pm_runtime_mark_last_busy(core->parent->dev);
	pm_runtime_put_autosuspend(core->parent->dev);

	if (gsp_core_suspend_state_get(core))
		complete(&core->suspend_done);
	else {
		gsp_core_state_set(core, CORE_STATE_IDLE);
		gsp_core_suspend_state_set(core, CORE_STATE_SUSPEND_EXIT);
	}

	if (gsp_workqueue_is_filled(core->wq))
		gsp_workqueue_filled_invalidate(core->wq);
}

void gsp_core_free(struct gsp_core *core)
{
	struct gsp_kcfg *tmp = NULL;
	struct gsp_kcfg *kcfg = NULL;

	/* free cfgs memory*/
	list_for_each_entry(kcfg, &core->kcfgs, sibling) {
		kfree(kcfg->cfg);
		kcfg->cfg = NULL;
	}

	/* free kcfgs memory*/
	list_for_each_entry_safe(kcfg, tmp, &core->kcfgs, sibling) {
		kfree(kcfg);
		kcfg = NULL;
	}

	/* free workquue memory */
	kfree(core->wq);
	core->wq = NULL;

	/* free capability memory */
	kfree(core->capa);
	core->capa = NULL;
}

int gsp_core_alloc(struct gsp_core **core,
		   struct gsp_core_ops *ops,
		   struct device_node *node)
{
	int ret = -1;
	u32 tmp = 0;
	int i;
	struct gsp_kcfg *kcfg = NULL;
	struct gsp_workqueue *wq = NULL;

	if (IS_ERR_OR_NULL(core)
	|| IS_ERR_OR_NULL(ops)
	|| IS_ERR_OR_NULL(node)) {
		GSP_ERR("core alloc params error\n");
		return ret;
	}

	/* do specific core allocate operations */
	if (ops->alloc)
		ret = ops->alloc(core, node);
	if (ret) {
		GSP_ERR("specific core allocate failed\n");
		return ret;
	}
	(*core)->ops = ops;

	/* read core id from dts as an identifier to distinguish multi-cores */
	ret = of_property_read_u32(node, "core-id", &tmp);
	if (ret) {
		GSP_ERR("read core id failed\n");
		return ret;
	}
	(*core)->id = tmp;
	GSP_INFO("read core[%u] id\n", tmp);

	/* assign core name with core-id here */
	snprintf((*core)->name, sizeof((*core)->name), "gsp-core[%d]",
		(*core)->id);

	/* read max kcfg number possessed by core*/
	ret = of_property_read_u32(node, "kcfg-num", &tmp);
	if (ret || tmp < 1) {
		GSP_ERR("kcfg number read failed\n");
		return ret;
	}
	(*core)->kcfg_num = tmp;
	GSP_INFO("read core[%u] kfg number: %d\n", (*core)->id, tmp);

	/* read this property to set whether core work thread is real-time*/
	if (of_find_property(node, "real-time", NULL))
		(*core)->rt = true;
	else
		(*core)->rt = false;
	GSP_INFO("core[%u] realtime priority: %s\n", (*core)->id,
		 (*core)->rt ? "enable" : "disable");

	if (of_find_property(node, "map-64-bit", NULL))
		(*core)->need_iommu = true;
	else
		(*core)->need_iommu = false;
	GSP_INFO("core[%u] need iommu: %s\n", (*core)->id,
		 (*core)->need_iommu ? "need" : "no need");

	INIT_LIST_HEAD(&(*core)->kcfgs);
	/* allocate memory for kcfgs and set specific cfg size*/
	for (i = 0; i < (*core)->kcfg_num; i++) {
		kcfg = kzalloc(sizeof(struct gsp_kcfg), GFP_KERNEL);
		if (IS_ERR_OR_NULL(kcfg))
			break;
		INIT_LIST_HEAD(&kcfg->sibling);
		list_add_tail(&kcfg->sibling, &(*core)->kcfgs);
		kcfg->cfg_size = (*core)->cfg_size;
		kcfg->need_iommu = (*core)->need_iommu == 1 ? true : false;

		kcfg->cfg = kzalloc(kcfg->cfg_size, GFP_KERNEL);
		if (IS_ERR_OR_NULL(kcfg->cfg))
			break;
	}

	if (i < (*core)->kcfg_num) {
		GSP_ERR("core kcfg allocate failed\n");
		return ret;
	}

	/* allocate memory for work queue attached to core */
	if (!ret) {
		wq = kzalloc(sizeof(struct gsp_workqueue),
			     GFP_KERNEL);
		if (wq)
			(*core)->wq = wq;
		else
			ret = -1;
	}

	return ret;
}

struct gsp_core *gsp_core_select(struct gsp_dev *gsp)
{
	int weight = 0;
	struct gsp_core *core = NULL;
	struct gsp_core *result = NULL;

	if (gsp_dev_verify(gsp)) {
		GSP_ERR("core select params error\n");
		return NULL;
	}

	result = gsp_dev_to_core(gsp, 0);
	if (gsp_core_verify(result)) {
		GSP_ERR("gsp dev has no core\n");
		return NULL;
	}

	weight = result->weight;
	for_each_gsp_core(core, gsp) {
		if (weight > core->weight) {
			weight = core->weight;
			result = core;
		}
	}

	if (gsp_core_verify(result)) {
		GSP_ERR("choose error core\n");
		return NULL;
	}

	if (gsp_workqueue_is_exhausted(result->wq)) {
		GSP_DEBUG("wait because workqueue is exhausted\n");
		gsp_core_release_wait(result);
	}

	result->weight++;
	return result;
}

void gsp_core_hang_handler(struct timer_list *time)
{
	struct gsp_core *core = from_timer(core, time, timer);

	gsp_core_state_set(core, CORE_STATE_HW_HANG_ERR);
	kthread_queue_work(&core->kworker, &core->recover);
}

void gsp_core_reset(struct gsp_core *core)
{
	struct gsp_dev *gsp = NULL;
	struct gsp_interface *interface = NULL;

	GSP_ERR("core[%d] need reset because of hung\n",
		gsp_core_state_get(core));

	gsp = gsp_core_to_parent(core);
	interface = gsp_dev_to_interface(gsp);
	if (core->ops->reset && interface->ops->reset) {
		core->ops->reset(core);
		interface->ops->reset(interface);
	}
}

int gsp_core_create_timeline(struct gsp_core *core)
{
	struct gsp_sync_timeline *timeline = NULL;

	if (gsp_core_verify(core)) {
		GSP_ERR("create timeline with invalidate gsp\n");
		return -1;
	}

	timeline = gsp_sync_timeline_create(core->name);
	if (IS_ERR_OR_NULL(timeline)) {
		GSP_ERR("sync timeline create failed\n");
		return -1;
	}

	core->timeline = timeline;

	return 0;
}

int gsp_core_init(struct gsp_core *core)
{
	int ret = -1;
	struct task_struct *task = NULL;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	if (IS_ERR_OR_NULL(core)) {
		GSP_ERR("core init params error\n");
		return ret;
	}

	ret = core->ops->init(core);
	if (ret) {
		GSP_ERR("core[%d] init failed\n", gsp_core_to_id(core));
		return ret;
	}

	ret = gsp_core_parse_dt(core);
	if (ret) {
		GSP_ERR("core[%d] parse dt failed\n", gsp_core_to_id(core));
		return ret;
	}

	ret = gsp_workqueue_init(core->wq, core);
	if (ret) {
		GSP_ERR("workqueue init failed\n");
		core->wq = NULL;
		return ret;
	}

	ret = gsp_core_create_timeline(core);
	if (ret) {
		GSP_ERR("gsp core[%d] create timeline failed\n",
			gsp_core_to_id(core));
		return ret;
	}

	ret = gsp_core_sysfs_init(core);
	if (ret) {
		GSP_ERR("gsp core sysfs initialize failed\n");
		return ret;
	}

	timer_setup(&core->timer, gsp_core_hang_handler, 0);

	gsp_core_state_set(core, CORE_STATE_IDLE);
	gsp_core_suspend_state_set(core, CORE_STATE_SUSPEND_EXIT);
	init_completion(&core->suspend_done);
	init_completion(&core->resume_done);
	init_completion(&core->release_done);

	/* initialize kworker & kwork and create kthread */
	kthread_init_worker(&core->kworker);

	kthread_init_work(&core->trigger, gsp_core_trigger);
	kthread_init_work(&core->release, gsp_core_release);
	kthread_init_work(&core->recover, gsp_core_recover);

	task = kthread_run(kthread_worker_fn, &core->kworker,
			  "gsp-core[%d]", core->id);
	if (task)
		core->work_thread = task;
	else
		return ret;

	/* when device probe, there is no task. so initialize weight to zero */
	core->weight = 0;

	/*
	 * core dts will indicate if this core should run the core kthread
	 * with high (realtime) priority to reduce the cpu schedule time.
	 * Without this setting core kthread will remain at default priority.
	 */
	if (core->rt) {
		GSP_INFO("run core kthread with realtime priority\n");
		sched_setscheduler(core->work_thread, SCHED_FIFO, &param);
	}

	return ret;
}

void gsp_core_timeline_destroy(struct gsp_core *core)
{
	if (core->timeline)
		gsp_sync_timeline_destroy(core->timeline);
}

void gsp_core_deinit(struct gsp_core *core)
{
	if (IS_ERR_OR_NULL(core)) {
		GSP_DEBUG("nothing to do for uninit core\n");
		return;
	}

	if (IS_ERR_OR_NULL(core->ops)) {
		GSP_DEBUG("nothing to do for uninitialized core ops\n");
		return;
	}

	if (core->work_thread)
		kthread_stop(core->work_thread);

	gsp_core_sysfs_destroy(core);

	gsp_core_timeline_destroy(core);
}

void gsp_core_suspend(struct gsp_core *core)
{
	/*
	 * modify state machine of core to "suspend"
	 * to ensure there is no new kcfg to process
	 */
	gsp_core_state_set(core, CORE_STATE_SUSPEND);

	reinit_completion(&core->suspend_done);
}

void gsp_core_resume(struct gsp_core *core)
{

	/*
	 * have to reset coef params because older ones
	 * are pointless after suspend and resume
	 */
	core->force_calc = 1;
	gsp_core_state_set(core, CORE_STATE_IDLE);
}

int gsp_core_stop(struct gsp_core *core)
{
	struct gsp_workqueue *wq = NULL;

	if (!gsp_core_is_suspend(core)) {
		GSP_ERR("core state not suspend before core stop, %d\n",
			 (int)gsp_core_state_get(core));
		return -1;
	}

	/* to ensure complete current kcfg before this function */
	wq = gsp_core_to_workqueue(core);
	gsp_workqueue_filled_invalidate(wq);

	return 0;
}

struct gsp_capability *gsp_core_get_capability(struct gsp_core *core)
{
	return core->capa;
}

int gsp_core_get_kcfg_num(struct gsp_core *core)
{
	return core->kcfg_num;
}
