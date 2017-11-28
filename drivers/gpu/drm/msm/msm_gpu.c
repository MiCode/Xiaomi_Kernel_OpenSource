/*
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "msm_gpu.h"
#include "msm_gem.h"
#include "msm_mmu.h"
#include "msm_trace.h"

/*
 * Power Management:
 */

#ifdef DOWNSTREAM_CONFIG_MSM_BUS_SCALING
#include <mach/board.h>
static void bs_init(struct msm_gpu *gpu)
{
	if (gpu->bus_scale_table) {
		gpu->bsc = msm_bus_scale_register_client(gpu->bus_scale_table);
		DBG("bus scale client: %08x", gpu->bsc);
	}
}

static void bs_fini(struct msm_gpu *gpu)
{
	if (gpu->bsc) {
		msm_bus_scale_unregister_client(gpu->bsc);
		gpu->bsc = 0;
	}
}

static void bs_set(struct msm_gpu *gpu, int idx)
{
	if (gpu->bsc) {
		DBG("set bus scaling: %d", idx);
		msm_bus_scale_client_update_request(gpu->bsc, idx);
	}
}
#else
static void bs_init(struct msm_gpu *gpu) {}
static void bs_fini(struct msm_gpu *gpu) {}
static void bs_set(struct msm_gpu *gpu, int idx) {}
#endif

static int enable_pwrrail(struct msm_gpu *gpu)
{
	struct drm_device *dev = gpu->dev;
	int ret = 0;

	if (gpu->gpu_reg) {
		ret = regulator_enable(gpu->gpu_reg);
		if (ret) {
			dev_err(dev->dev, "failed to enable 'gpu_reg': %d\n", ret);
			return ret;
		}
	}

	if (gpu->gpu_cx) {
		ret = regulator_enable(gpu->gpu_cx);
		if (ret) {
			dev_err(dev->dev, "failed to enable 'gpu_cx': %d\n", ret);
			return ret;
		}
	}

	return 0;
}

static int disable_pwrrail(struct msm_gpu *gpu)
{
	if (gpu->gpu_cx)
		regulator_disable(gpu->gpu_cx);
	if (gpu->gpu_reg)
		regulator_disable(gpu->gpu_reg);
	return 0;
}

static int enable_clk(struct msm_gpu *gpu)
{
	uint32_t rate = gpu->gpufreq[gpu->active_level];
	int i;

	if (gpu->core_clk)
		clk_set_rate(gpu->core_clk, rate);

	if (gpu->rbbmtimer_clk)
		clk_set_rate(gpu->rbbmtimer_clk, 19200000);

	for (i = gpu->nr_clocks - 1; i >= 0; i--)
		if (gpu->grp_clks[i])
			clk_prepare(gpu->grp_clks[i]);

	for (i = gpu->nr_clocks - 1; i >= 0; i--)
		if (gpu->grp_clks[i])
			clk_enable(gpu->grp_clks[i]);

	return 0;
}

static int disable_clk(struct msm_gpu *gpu)
{
	uint32_t rate = gpu->gpufreq[gpu->nr_pwrlevels - 1];
	int i;

	for (i = gpu->nr_clocks - 1; i >= 0; i--)
		if (gpu->grp_clks[i])
			clk_disable(gpu->grp_clks[i]);

	for (i = gpu->nr_clocks - 1; i >= 0; i--)
		if (gpu->grp_clks[i])
			clk_unprepare(gpu->grp_clks[i]);

	if (gpu->core_clk)
		clk_set_rate(gpu->core_clk, rate);

	if (gpu->rbbmtimer_clk)
		clk_set_rate(gpu->rbbmtimer_clk, 0);

	return 0;
}

static int enable_axi(struct msm_gpu *gpu)
{
	if (gpu->ebi1_clk)
		clk_prepare_enable(gpu->ebi1_clk);

	if (gpu->busfreq[gpu->active_level])
		bs_set(gpu, gpu->busfreq[gpu->active_level]);
	return 0;
}

static int disable_axi(struct msm_gpu *gpu)
{
	if (gpu->ebi1_clk)
		clk_disable_unprepare(gpu->ebi1_clk);

	if (gpu->busfreq[gpu->active_level])
		bs_set(gpu, 0);
	return 0;
}

int msm_gpu_pm_resume(struct msm_gpu *gpu)
{
	int ret;

	DBG("%s", gpu->name);

	ret = enable_pwrrail(gpu);
	if (ret)
		return ret;

	ret = enable_clk(gpu);
	if (ret)
		return ret;

	ret = enable_axi(gpu);
	if (ret)
		return ret;

	if (gpu->aspace && gpu->aspace->mmu)
		msm_mmu_enable(gpu->aspace->mmu);

	gpu->needs_hw_init = true;

	return 0;
}

int msm_gpu_pm_suspend(struct msm_gpu *gpu)
{
	int ret;

	DBG("%s", gpu->name);

	if (gpu->aspace && gpu->aspace->mmu)
		msm_mmu_disable(gpu->aspace->mmu);

	ret = disable_axi(gpu);
	if (ret)
		return ret;

	ret = disable_clk(gpu);
	if (ret)
		return ret;

	ret = disable_pwrrail(gpu);
	if (ret)
		return ret;

	return 0;
}

int msm_gpu_hw_init(struct msm_gpu *gpu)
{
	int ret;

	if (!gpu->needs_hw_init)
		return 0;

	disable_irq(gpu->irq);
	ret = gpu->funcs->hw_init(gpu);
	if (!ret)
		gpu->needs_hw_init = false;
	enable_irq(gpu->irq);

	return ret;
}

static void retire_guilty_submit(struct msm_gpu *gpu,
		struct msm_ringbuffer *ring)
{
	struct msm_gem_submit *submit = list_first_entry_or_null(&ring->submits,
		struct msm_gem_submit, node);

	if (!submit)
		return;

	submit->queue->faults++;

	msm_gem_submit_free(submit);
}

/*
 * Hangcheck detection for locked gpu:
 */

static void retire_submits(struct msm_gpu *gpu, struct msm_ringbuffer *ring,
		uint32_t fence);

static void recover_worker(struct work_struct *work)
{
	struct msm_gpu *gpu = container_of(work, struct msm_gpu, recover_work);
	struct drm_device *dev = gpu->dev;

	mutex_lock(&dev->struct_mutex);
	if (msm_gpu_active(gpu)) {
		struct msm_gem_submit *submit;
		struct msm_ringbuffer *ring;
		int i;

		/* Retire all events that have already passed */
		FOR_EACH_RING(gpu, ring, i)
			retire_submits(gpu, ring, ring->memptrs->fence);

		retire_guilty_submit(gpu, gpu->funcs->active_ring(gpu));

		/* Recover the GPU */
		gpu->funcs->recover(gpu);
		/* Decrement the device usage count for the guilty submit */
		pm_runtime_put_sync_autosuspend(&gpu->pdev->dev);

		/* Replay the remaining on all rings, highest priority first */
		for (i = 0;  i < gpu->nr_rings; i++) {
			struct msm_ringbuffer *ring = gpu->rb[i];

			list_for_each_entry(submit, &ring->submits, node)
				gpu->funcs->submit(gpu, submit);
		}
	}
	mutex_unlock(&dev->struct_mutex);

	msm_gpu_retire(gpu);
}

static void hangcheck_timer_reset(struct msm_gpu *gpu)
{
	DBG("%s", gpu->name);
	mod_timer(&gpu->hangcheck_timer,
			round_jiffies_up(jiffies + DRM_MSM_HANGCHECK_JIFFIES));
}

static void hangcheck_handler(unsigned long data)
{
	struct msm_gpu *gpu = (struct msm_gpu *)data;
	struct drm_device *dev = gpu->dev;
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_ringbuffer *ring = gpu->funcs->active_ring(gpu);
	uint32_t fence = ring->memptrs->fence;
	uint32_t submitted = gpu->funcs->submitted_fence(gpu, ring);

	if (fence != ring->hangcheck_fence) {
		/* some progress has been made.. ya! */
		ring->hangcheck_fence = fence;
	} else if (fence < submitted) {
		struct msm_gem_submit *submit;

		ring->hangcheck_fence = fence;

		/*
		 * No progress done, but see if the current submit is
		 * intentionally skipping the hangcheck
		 */
		submit = list_first_entry_or_null(&ring->submits,
			struct msm_gem_submit, node);

		if (!submit || (submit->queue->flags &
			MSM_SUBMITQUEUE_BYPASS_QOS_TIMEOUT))
			goto out;

		/* no progress and not done and not special .. hung! */
		dev_err(dev->dev, "%s: hangcheck detected gpu lockup rb %d!\n",
				gpu->name, ring->id);
		dev_err(dev->dev, "%s:     completed fence: %u\n",
				gpu->name, fence);
		dev_err(dev->dev, "%s:     submitted fence: %u\n",
				gpu->name, submitted);

		queue_work(priv->wq, &gpu->recover_work);
	}

out:
	/* if still more pending work, reset the hangcheck timer: */
	if (submitted > ring->hangcheck_fence)
		hangcheck_timer_reset(gpu);

	/* workaround for missing irq: */
	queue_work(priv->wq, &gpu->retire_work);
}

/*
 * Performance Counters:
 */

/* called under perf_lock */
static int update_hw_cntrs(struct msm_gpu *gpu, uint32_t ncntrs, uint32_t *cntrs)
{
	uint32_t current_cntrs[ARRAY_SIZE(gpu->last_cntrs)];
	int i, n = min(ncntrs, gpu->num_perfcntrs);

	/* read current values: */
	for (i = 0; i < gpu->num_perfcntrs; i++)
		current_cntrs[i] = gpu_read(gpu, gpu->perfcntrs[i].sample_reg);

	/* update cntrs: */
	for (i = 0; i < n; i++)
		cntrs[i] = current_cntrs[i] - gpu->last_cntrs[i];

	/* save current values: */
	for (i = 0; i < gpu->num_perfcntrs; i++)
		gpu->last_cntrs[i] = current_cntrs[i];

	return n;
}

static void update_sw_cntrs(struct msm_gpu *gpu)
{
	ktime_t time;
	uint32_t elapsed;
	unsigned long flags;

	spin_lock_irqsave(&gpu->perf_lock, flags);
	if (!gpu->perfcntr_active)
		goto out;

	time = ktime_get();
	elapsed = ktime_to_us(ktime_sub(time, gpu->last_sample.time));

	gpu->totaltime += elapsed;
	if (gpu->last_sample.active)
		gpu->activetime += elapsed;

	gpu->last_sample.active = msm_gpu_active(gpu);
	gpu->last_sample.time = time;

out:
	spin_unlock_irqrestore(&gpu->perf_lock, flags);
}

void msm_gpu_perfcntr_start(struct msm_gpu *gpu)
{
	unsigned long flags;

	pm_runtime_get_sync(&gpu->pdev->dev);

	spin_lock_irqsave(&gpu->perf_lock, flags);
	/* we could dynamically enable/disable perfcntr registers too.. */
	gpu->last_sample.active = msm_gpu_active(gpu);
	gpu->last_sample.time = ktime_get();
	gpu->activetime = gpu->totaltime = 0;
	gpu->perfcntr_active = true;
	update_hw_cntrs(gpu, 0, NULL);
	spin_unlock_irqrestore(&gpu->perf_lock, flags);
}

void msm_gpu_perfcntr_stop(struct msm_gpu *gpu)
{
	gpu->perfcntr_active = false;
	pm_runtime_put_sync(&gpu->pdev->dev);
}

/* returns -errno or # of cntrs sampled */
int msm_gpu_perfcntr_sample(struct msm_gpu *gpu, uint32_t *activetime,
		uint32_t *totaltime, uint32_t ncntrs, uint32_t *cntrs)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&gpu->perf_lock, flags);

	if (!gpu->perfcntr_active) {
		ret = -EINVAL;
		goto out;
	}

	*activetime = gpu->activetime;
	*totaltime = gpu->totaltime;

	gpu->activetime = gpu->totaltime = 0;

	ret = update_hw_cntrs(gpu, ncntrs, cntrs);

out:
	spin_unlock_irqrestore(&gpu->perf_lock, flags);

	return ret;
}

/*
 * Cmdstream submission/retirement:
 */

static void retire_submits(struct msm_gpu *gpu, struct msm_ringbuffer *ring,
		uint32_t fence)
{
	struct drm_device *dev = gpu->dev;
	struct msm_gem_submit *submit, *tmp;

	WARN_ON(!mutex_is_locked(&dev->struct_mutex));

	list_for_each_entry_safe(submit, tmp, &ring->submits, node) {
		struct msm_memptr_ticks *ticks;

		if (submit->fence > fence)
			break;

		ticks = &(ring->memptrs->ticks[submit->tick_index]);

		/* Add memory barrier to ensure the timer ticks are posted */
		rmb();

		trace_msm_retired(submit, ticks->started, ticks->retired);

		pm_runtime_mark_last_busy(&gpu->pdev->dev);
		pm_runtime_put_autosuspend(&gpu->pdev->dev);
		msm_gem_submit_free(submit);
	}
}

static bool _fence_signaled(struct msm_gem_object *obj, uint32_t fence)
{
	if (obj->write_fence & 0x3FFFFFFF)
		return COMPARE_FENCE_LTE(obj->write_fence, fence);

	return COMPARE_FENCE_LTE(obj->read_fence, fence);
}

static void _retire_ring(struct msm_gpu *gpu, struct msm_ringbuffer *ring,
		uint32_t fence)
{
	struct msm_gem_object *obj, *tmp;

	retire_submits(gpu, ring, fence);

	list_for_each_entry_safe(obj, tmp, &gpu->active_list, mm_list) {
		if (_fence_signaled(obj, fence)) {
			msm_gem_move_to_inactive(&obj->base);
			msm_gem_put_iova(&obj->base, gpu->aspace);
			drm_gem_object_unreference(&obj->base);
		}
	}
}

static void retire_worker(struct work_struct *work)
{
	struct msm_gpu *gpu = container_of(work, struct msm_gpu, retire_work);
	struct drm_device *dev = gpu->dev;
	struct msm_ringbuffer *ring;
	int i;

	FOR_EACH_RING(gpu, ring, i) {
		if (!ring)
			continue;

		msm_update_fence(gpu->dev, ring->memptrs->fence);

		mutex_lock(&dev->struct_mutex);
		_retire_ring(gpu, ring, ring->memptrs->fence);
		mutex_unlock(&dev->struct_mutex);
	}
}

/* call from irq handler to schedule work to retire bo's */
void msm_gpu_retire(struct msm_gpu *gpu)
{
	struct msm_drm_private *priv = gpu->dev->dev_private;
	queue_work(priv->wq, &gpu->retire_work);
	update_sw_cntrs(gpu);
}

/* add bo's to gpu's ring, and kick gpu: */
int msm_gpu_submit(struct msm_gpu *gpu, struct msm_gem_submit *submit)
{
	struct drm_device *dev = gpu->dev;
	struct msm_ringbuffer *ring = gpu->rb[submit->ring];
	int i;

	WARN_ON(!mutex_is_locked(&dev->struct_mutex));

	submit->fence = FENCE(submit->ring, ++ring->seqno);

	pm_runtime_get_sync(&gpu->pdev->dev);

	msm_gpu_hw_init(gpu);

	list_add_tail(&submit->node, &ring->submits);

	ring->submitted_fence = submit->fence;

	submit->tick_index = ring->tick_index;
	ring->tick_index = (ring->tick_index + 1) %
		ARRAY_SIZE(ring->memptrs->ticks);

	trace_msm_queued(submit);

	update_sw_cntrs(gpu);

	for (i = 0; i < submit->nr_bos; i++) {
		struct msm_gem_object *msm_obj = submit->bos[i].obj;

		/* can't happen yet.. but when we add 2d support we'll have
		 * to deal w/ cross-ring synchronization:
		 */
		WARN_ON(is_active(msm_obj) && (msm_obj->gpu != gpu));

		if (!is_active(msm_obj)) {
			struct msm_gem_address_space *aspace;
			uint64_t iova;

			aspace = (msm_obj->flags & MSM_BO_SECURE) ?
				gpu->secure_aspace : submit->aspace;

			/* ring takes a reference to the bo and iova: */
			drm_gem_object_reference(&msm_obj->base);
			msm_gem_get_iova(&msm_obj->base, aspace, &iova);

			submit->bos[i].iova = iova;
		}

		if (submit->bos[i].flags & MSM_SUBMIT_BO_READ)
			msm_gem_move_to_active(&msm_obj->base, gpu, false, submit->fence);
		else if (submit->bos[i].flags & MSM_SUBMIT_BO_WRITE)
			msm_gem_move_to_active(&msm_obj->base, gpu, true, submit->fence);
	}

	msm_rd_dump_submit(submit);

	gpu->funcs->submit(gpu, submit);

	hangcheck_timer_reset(gpu);

	return 0;
}

struct msm_context_counter {
	u32 groupid;
	int counterid;
	struct list_head node;
};

int msm_gpu_counter_get(struct msm_gpu *gpu, struct drm_msm_counter *data,
	struct msm_file_private *ctx)
{
	struct msm_context_counter *entry;
	int counterid;
	u32 lo = 0, hi = 0;

	if (!ctx || !gpu->funcs->get_counter)
		return -ENODEV;

	counterid = gpu->funcs->get_counter(gpu, data->groupid, data->countable,
		&lo, &hi);

	if (counterid < 0)
		return counterid;

	/*
	 * Check to see if the counter in question is already held by this
	 * process. If it does, put it back and return an error.
	 */
	list_for_each_entry(entry, &ctx->counters, node) {
		if (entry->groupid == data->groupid &&
			entry->counterid == counterid) {
			gpu->funcs->put_counter(gpu, data->groupid, counterid);
			return -EBUSY;
		}
	}

	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry) {
		gpu->funcs->put_counter(gpu, data->groupid, counterid);
		return -ENOMEM;
	}

	entry->groupid = data->groupid;
	entry->counterid = counterid;
	list_add_tail(&entry->node, &ctx->counters);

	data->counterid = counterid;
	data->counter_lo = lo;
	data->counter_hi = hi;

	return 0;
}

int msm_gpu_counter_put(struct msm_gpu *gpu, struct drm_msm_counter *data,
	struct msm_file_private *ctx)
{
	struct msm_context_counter *entry;

	if (!gpu || !ctx)
		return -ENODEV;

	list_for_each_entry(entry, &ctx->counters, node) {
		if (entry->groupid == data->groupid &&
			entry->counterid == data->counterid) {
			gpu->funcs->put_counter(gpu, data->groupid,
				data->counterid);

			list_del(&entry->node);
			kfree(entry);

			return 0;
		}
	}

	return -EINVAL;
}

void msm_gpu_cleanup_counters(struct msm_gpu *gpu,
	struct msm_file_private *ctx)
{
	struct msm_context_counter *entry, *tmp;

	if (!ctx)
		return;

	list_for_each_entry_safe(entry, tmp, &ctx->counters, node) {
		gpu->funcs->put_counter(gpu, entry->groupid, entry->counterid);
		list_del(&entry->node);
		kfree(entry);
	}
}

u64 msm_gpu_counter_read(struct msm_gpu *gpu, struct drm_msm_counter_read *data)
{
	int i;

	if (!gpu->funcs->read_counter)
		return 0;

	for (i = 0; i < data->nr_ops; i++) {
		struct drm_msm_counter_read_op op;
		void __user *ptr = (void __user *)(uintptr_t)
			(data->ops + (i * sizeof(op)));

		if (copy_from_user(&op, ptr, sizeof(op)))
			return -EFAULT;

		op.value = gpu->funcs->read_counter(gpu, op.groupid,
			op.counterid);

		if (copy_to_user(ptr, &op, sizeof(op)))
			return -EFAULT;
	}

	return 0;
}

/*
 * Init/Cleanup:
 */

static irqreturn_t irq_handler(int irq, void *data)
{
	struct msm_gpu *gpu = data;
	return gpu->funcs->irq(gpu);
}

static struct clk *get_clock(struct device *dev, const char *name)
{
	struct clk *clk = devm_clk_get(dev, name);

	DBG("clks[%s]: %p", name, clk);

	return IS_ERR(clk) ? NULL : clk;
}

static int get_clocks(struct platform_device *pdev, struct msm_gpu *gpu)
{
	struct device *dev = &pdev->dev;
	struct property *prop;
	const char *name;
	int i = 0;

	gpu->nr_clocks = of_property_count_strings(dev->of_node, "clock-names");
	if (gpu->nr_clocks < 1) {
		gpu->nr_clocks = 0;
		return 0;
	}

	gpu->grp_clks = devm_kcalloc(dev, sizeof(struct clk *), gpu->nr_clocks,
		GFP_KERNEL);
	if (!gpu->grp_clks)
		return -ENOMEM;

	of_property_for_each_string(dev->of_node, "clock-names", prop, name) {
		gpu->grp_clks[i] = get_clock(dev, name);

		/* Remember the key clocks that we need to control later */
		if (!strcmp(name, "core_clk"))
			gpu->core_clk = gpu->grp_clks[i];
		else if (!strcmp(name, "rbbmtimer_clk"))
			gpu->rbbmtimer_clk = gpu->grp_clks[i];

		++i;
	}

	return 0;
}

static struct msm_gem_address_space *
msm_gpu_create_address_space(struct msm_gpu *gpu, struct device *dev,
		int type, u64 start, u64 end, const char *name)
{
	struct msm_gem_address_space *aspace;
	struct iommu_domain *iommu;

	/*
	 * If start == end then assume we don't want an address space; this is
	 * mainly for targets to opt out of secure
	 */
	if (start == end)
		return NULL;

	iommu = iommu_domain_alloc(&platform_bus_type);
	if (!iommu) {
		dev_info(gpu->dev->dev,
			"%s: no IOMMU, fallback to VRAM carveout!\n",
			gpu->name);
		return NULL;
	}

	iommu->geometry.aperture_start = start;
	iommu->geometry.aperture_end = end;

	dev_info(gpu->dev->dev, "%s: using IOMMU '%s'\n", gpu->name, name);

	aspace = msm_gem_address_space_create(dev, iommu, type, name);
	if (IS_ERR(aspace)) {
		dev_err(gpu->dev->dev, "%s: failed to init IOMMU '%s': %ld\n",
			gpu->name, name, PTR_ERR(aspace));

		iommu_domain_free(iommu);
		return NULL;
	}

	if (aspace->mmu) {
		int ret = aspace->mmu->funcs->attach(aspace->mmu, NULL, 0);

		if (ret) {
			dev_err(gpu->dev->dev,
				"%s: failed to atach IOMMU '%s': %d\n",
				gpu->name, name, ret);
			msm_gem_address_space_put(aspace);
			aspace = ERR_PTR(ret);
		}
	}

	return aspace;
}

static void msm_gpu_destroy_address_space(struct msm_gem_address_space *aspace)
{
	if (!IS_ERR_OR_NULL(aspace) && aspace->mmu)
		aspace->mmu->funcs->detach(aspace->mmu);

	msm_gem_address_space_put(aspace);
}

int msm_gpu_init(struct drm_device *drm, struct platform_device *pdev,
		struct msm_gpu *gpu, const struct msm_gpu_funcs *funcs,
		const char *name, struct msm_gpu_config *config)
{
	int i, ret, nr_rings;
	void *memptrs;
	uint64_t memptrs_iova;

	if (WARN_ON(gpu->num_perfcntrs > ARRAY_SIZE(gpu->last_cntrs)))
		gpu->num_perfcntrs = ARRAY_SIZE(gpu->last_cntrs);

	gpu->dev = drm;
	gpu->funcs = funcs;
	gpu->name = name;

	INIT_LIST_HEAD(&gpu->active_list);
	INIT_WORK(&gpu->retire_work, retire_worker);
	INIT_WORK(&gpu->recover_work, recover_worker);


	setup_timer(&gpu->hangcheck_timer, hangcheck_handler,
			(unsigned long)gpu);

	spin_lock_init(&gpu->perf_lock);


	/* Map registers: */
	gpu->mmio = msm_ioremap(pdev, config->ioname, name);
	if (IS_ERR(gpu->mmio)) {
		ret = PTR_ERR(gpu->mmio);
		goto fail;
	}

	/* Get Interrupt: */
	gpu->irq = platform_get_irq_byname(pdev, config->irqname);
	if (gpu->irq < 0) {
		ret = gpu->irq;
		dev_err(drm->dev, "failed to get irq: %d\n", ret);
		goto fail;
	}

	ret = devm_request_irq(&pdev->dev, gpu->irq, irq_handler,
			IRQF_TRIGGER_HIGH, gpu->name, gpu);
	if (ret) {
		gpu->irq = ret;
		dev_err(drm->dev, "failed to request IRQ%u: %d\n", gpu->irq, ret);
		goto fail;
	}

	ret = get_clocks(pdev, gpu);
	if (ret)
		goto fail;

	gpu->ebi1_clk = devm_clk_get(&pdev->dev, "bus_clk");
	DBG("ebi1_clk: %p", gpu->ebi1_clk);
	if (IS_ERR(gpu->ebi1_clk))
		gpu->ebi1_clk = NULL;

	/* Acquire regulators: */
	gpu->gpu_reg = devm_regulator_get(&pdev->dev, "vdd");
	DBG("gpu_reg: %p", gpu->gpu_reg);
	if (IS_ERR(gpu->gpu_reg))
		gpu->gpu_reg = NULL;

	gpu->gpu_cx = devm_regulator_get(&pdev->dev, "vddcx");
	DBG("gpu_cx: %p", gpu->gpu_cx);
	if (IS_ERR(gpu->gpu_cx))
		gpu->gpu_cx = NULL;

	gpu->aspace = msm_gpu_create_address_space(gpu, &pdev->dev,
		MSM_IOMMU_DOMAIN_USER, config->va_start, config->va_end,
		"gpu");

	gpu->secure_aspace = msm_gpu_create_address_space(gpu, &pdev->dev,
		MSM_IOMMU_DOMAIN_SECURE, config->secure_va_start,
		config->secure_va_end, "gpu_secure");

	nr_rings = config->nr_rings;

	if (nr_rings > ARRAY_SIZE(gpu->rb)) {
		WARN(1, "Only creating %lu ringbuffers\n", ARRAY_SIZE(gpu->rb));
		nr_rings = ARRAY_SIZE(gpu->rb);
	}

	/* Allocate one buffer to hold all the memptr records for the rings */
	memptrs = msm_gem_kernel_new(drm, sizeof(struct msm_memptrs) * nr_rings,
		MSM_BO_UNCACHED, gpu->aspace, &gpu->memptrs_bo, &memptrs_iova);

	if (IS_ERR(memptrs)) {
		ret = PTR_ERR(memptrs);
		goto fail;
	}

	/* Create ringbuffer(s): */
	for (i = 0; i < nr_rings; i++) {
		gpu->rb[i] = msm_ringbuffer_new(gpu, i, memptrs, memptrs_iova);
		if (IS_ERR(gpu->rb[i])) {
			ret = PTR_ERR(gpu->rb[i]);
			gpu->rb[i] = NULL;
			dev_err(drm->dev,
				"could not create ringbuffer %d: %d\n", i, ret);
			goto fail;
		}

		memptrs += sizeof(struct msm_memptrs);
		memptrs_iova += sizeof(struct msm_memptrs);
	}

	gpu->nr_rings = nr_rings;

#ifdef CONFIG_SMP
	gpu->pm_qos_req_dma.type = PM_QOS_REQ_AFFINE_IRQ;
	gpu->pm_qos_req_dma.irq = gpu->irq;
#endif

	pm_qos_add_request(&gpu->pm_qos_req_dma, PM_QOS_CPU_DMA_LATENCY,
			PM_QOS_DEFAULT_VALUE);
	gpu->pdev = pdev;
	platform_set_drvdata(pdev, gpu);

	bs_init(gpu);

	gpu->snapshot = msm_snapshot_new(gpu);
	if (IS_ERR(gpu->snapshot))
		gpu->snapshot = NULL;

	return 0;

fail:
	for (i = 0; i < ARRAY_SIZE(gpu->rb); i++)
		msm_ringbuffer_destroy(gpu->rb[i]);

	if (gpu->memptrs_bo) {
		msm_gem_put_iova(gpu->memptrs_bo, gpu->aspace);
		drm_gem_object_unreference_unlocked(gpu->memptrs_bo);
	}

	msm_gpu_destroy_address_space(gpu->aspace);
	msm_gpu_destroy_address_space(gpu->secure_aspace);

	return ret;
}

void msm_gpu_cleanup(struct msm_gpu *gpu)
{
	struct drm_device *dev = gpu->dev;
	struct msm_drm_private *priv = dev->dev_private;
	struct platform_device *pdev = priv->gpu_pdev;
	int i;

	DBG("%s", gpu->name);

	WARN_ON(!list_empty(&gpu->active_list));

	if (gpu->irq >= 0) {
		disable_irq(gpu->irq);
		devm_free_irq(&pdev->dev, gpu->irq, gpu);
	}

	bs_fini(gpu);

	for (i = 0; i < ARRAY_SIZE(gpu->rb); i++)
		msm_ringbuffer_destroy(gpu->rb[i]);

	if (gpu->memptrs_bo) {
		msm_gem_put_iova(gpu->memptrs_bo, gpu->aspace);
		drm_gem_object_unreference_unlocked(gpu->memptrs_bo);
	}

	msm_snapshot_destroy(gpu, gpu->snapshot);

	msm_gpu_destroy_address_space(gpu->aspace);
	msm_gpu_destroy_address_space(gpu->secure_aspace);

	if (gpu->gpu_reg)
		devm_regulator_put(gpu->gpu_reg);

	if (gpu->gpu_cx)
		devm_regulator_put(gpu->gpu_cx);

	if (gpu->ebi1_clk)
		devm_clk_put(&pdev->dev, gpu->ebi1_clk);

	for (i = gpu->nr_clocks - 1; i >= 0; i--)
		if (gpu->grp_clks[i])
			devm_clk_put(&pdev->dev, gpu->grp_clks[i]);

	devm_kfree(&pdev->dev, gpu->grp_clks);

	if (gpu->mmio)
		devm_iounmap(&pdev->dev, gpu->mmio);
}
