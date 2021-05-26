// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/interconnect.h>
#include <linux/soc/qcom/llcc-qcom.h>

#include "adreno.h"
#include "adreno_a6xx.h"
#include "adreno_a6xx_hwsched.h"
#include "adreno_snapshot.h"
#include "kgsl_device.h"
#include "kgsl_trace.h"
#include "kgsl_util.h"

static size_t adreno_hwsched_snapshot_rb(struct kgsl_device *device, u8 *buf,
	size_t remain, void *priv)
{
	struct kgsl_snapshot_rb_v2 *header = (struct kgsl_snapshot_rb_v2 *)buf;
	u32 *data = (u32 *)(buf + sizeof(*header));
	struct kgsl_memdesc *rb = (struct kgsl_memdesc *)priv;

	if (remain < rb->size + sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "RB");
		return 0;
	}

	header->start = 0;
	header->end = rb->size >> 2;
	header->rptr = 0;
	header->rbsize = rb->size >> 2;
	header->count = rb->size >> 2;
	header->timestamp_queued = 0;
	header->timestamp_retired = 0;
	header->gpuaddr = rb->gpuaddr;
	header->id = 0;

	memcpy(data, rb->hostptr, rb->size);

	return rb->size + sizeof(*header);
}

static void a6xx_hwsched_snapshot_preemption_record(struct kgsl_device *device,
	struct kgsl_snapshot *snapshot, struct kgsl_memdesc *md, u64 offset)
{
	struct kgsl_snapshot_section_header *section_header =
		(struct kgsl_snapshot_section_header *)snapshot->ptr;
	u8 *dest = snapshot->ptr + sizeof(*section_header);
	struct kgsl_snapshot_gpu_object_v2 *header =
		(struct kgsl_snapshot_gpu_object_v2 *)dest;
	size_t section_size = sizeof(*section_header) + sizeof(*header) +
		A6XX_SNAPSHOT_CP_CTXRECORD_SIZE_IN_BYTES;

	if (snapshot->remain < section_size) {
		SNAPSHOT_ERR_NOMEM(device, "PREEMPTION RECORD");
		return;
	}

	section_header->magic = SNAPSHOT_SECTION_MAGIC;
	section_header->id = KGSL_SNAPSHOT_SECTION_GPU_OBJECT_V2;
	section_header->size = section_size;

	header->size = A6XX_SNAPSHOT_CP_CTXRECORD_SIZE_IN_BYTES >> 2;
	header->gpuaddr = md->gpuaddr + offset;
	header->ptbase =
		kgsl_mmu_pagetable_get_ttbr0(device->mmu.defaultpagetable);
	header->type = SNAPSHOT_GPU_OBJECT_GLOBAL;

	dest += sizeof(*header);

	memcpy(dest, md->hostptr + offset,
		A6XX_SNAPSHOT_CP_CTXRECORD_SIZE_IN_BYTES);

	snapshot->ptr += section_header->size;
	snapshot->remain -= section_header->size;
	snapshot->size += section_header->size;
}

static void snapshot_preemption_records(struct kgsl_device *device,
	struct kgsl_snapshot *snapshot, struct kgsl_memdesc *md)
{
	const struct adreno_a6xx_core *a6xx_core =
		to_a6xx_core(ADRENO_DEVICE(device));
	u64 ctxt_record_size = A6XX_CP_CTXRECORD_SIZE_IN_BYTES;
	u64 offset;

	if (a6xx_core->ctxt_record_size)
		ctxt_record_size = a6xx_core->ctxt_record_size;

	/* All preemption records exist as a single mem alloc entry */
	for (offset = 0; offset < md->size; offset += ctxt_record_size)
		a6xx_hwsched_snapshot_preemption_record(device, snapshot, md,
			offset);
}

void a6xx_hwsched_snapshot(struct adreno_device *adreno_dev,
	struct kgsl_snapshot *snapshot)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_hwsched_hfi *hw_hfi = to_a6xx_hwsched_hfi(adreno_dev);
	u32 i;

	a6xx_gmu_snapshot(adreno_dev, snapshot);

	for (i = 0; i < hw_hfi->mem_alloc_entries; i++) {
		struct mem_alloc_entry *entry = &hw_hfi->mem_alloc_table[i];

		if (entry->desc.mem_kind == MEMKIND_RB)
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_RB_V2,
				snapshot, adreno_hwsched_snapshot_rb,
				entry->gpu_md);

		if (entry->desc.mem_kind == MEMKIND_SCRATCH)
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_GPU_OBJECT_V2,
				snapshot, adreno_snapshot_global,
				entry->gpu_md);

		if (entry->desc.mem_kind == MEMKIND_PROFILE)
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_GPU_OBJECT_V2,
				snapshot, adreno_snapshot_global,
				entry->gpu_md);

		if (entry->desc.mem_kind == MEMKIND_CSW_SMMU_INFO)
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_GPU_OBJECT_V2,
				snapshot, adreno_snapshot_global,
				entry->gpu_md);

		if (entry->desc.mem_kind == MEMKIND_CSW_PRIV_NON_SECURE)
			snapshot_preemption_records(device, snapshot,
				entry->gpu_md);
	}

	adreno_hwsched_parse_fault_cmdobj(adreno_dev, snapshot);
}

static int a6xx_hwsched_gmu_first_boot(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	int level, ret = 0;

	trace_kgsl_pwr_request_state(device, KGSL_STATE_AWARE);

	a6xx_gmu_aop_send_acd_state(gmu, adreno_dev->acd_enabled);

	ret = a6xx_gmu_enable_gdsc(adreno_dev);
	if (ret)
		return ret;

	ret = a6xx_gmu_enable_clks(adreno_dev);
	if (ret)
		goto gdsc_off;

	ret = a6xx_gmu_load_fw(adreno_dev);
	if (ret)
		goto clks_gdsc_off;

	ret = a6xx_gmu_itcm_shadow(adreno_dev);
	if (ret)
		goto clks_gdsc_off;

	if (!test_bit(GMU_PRIV_PDC_RSC_LOADED, &gmu->flags)) {
		ret = a6xx_load_pdc_ucode(adreno_dev);
		if (ret)
			goto clks_gdsc_off;

		a6xx_load_rsc_ucode(adreno_dev);
		set_bit(GMU_PRIV_PDC_RSC_LOADED, &gmu->flags);
	}

	a6xx_gmu_register_config(adreno_dev);

	a6xx_gmu_version_info(adreno_dev);

	a6xx_gmu_irq_enable(adreno_dev);

	/* Vote for minimal DDR BW for GMU to init */
	level = pwr->pwrlevels[pwr->default_pwrlevel].bus_min;

	icc_set_bw(pwr->icc_path, 0, MBps_to_icc(pwr->ddr_table[level]));

	ret = a6xx_gmu_device_start(adreno_dev);
	if (ret)
		goto err;

	ret = a6xx_hwsched_hfi_start(adreno_dev);
	if (ret)
		goto err;

	icc_set_bw(pwr->icc_path, 0, 0);

	device->gmu_fault = false;

	trace_kgsl_pwr_set_state(device, KGSL_STATE_AWARE);

	return 0;

err:
	if (device->gmu_fault) {
		a6xx_gmu_suspend(adreno_dev);

		return ret;
	}

	a6xx_gmu_irq_disable(adreno_dev);

clks_gdsc_off:
	clk_bulk_disable_unprepare(gmu->num_clks, gmu->clks);

gdsc_off:
	/* Poll to make sure that the CX is off */
	a6xx_cx_regulator_disable_wait(gmu->cx_gdsc, device, 5000);

	a6xx_rdpm_cx_freq_update(gmu, 0);

	return ret;
}

static int a6xx_hwsched_gmu_boot(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	int ret = 0;

	trace_kgsl_pwr_request_state(device, KGSL_STATE_AWARE);

	ret = a6xx_gmu_enable_gdsc(adreno_dev);
	if (ret)
		return ret;

	ret = a6xx_gmu_enable_clks(adreno_dev);
	if (ret)
		goto gdsc_off;

	ret = a6xx_rscc_wakeup_sequence(adreno_dev);
	if (ret)
		goto clks_gdsc_off;

	ret = a6xx_gmu_load_fw(adreno_dev);
	if (ret)
		goto clks_gdsc_off;

	a6xx_gmu_register_config(adreno_dev);

	a6xx_gmu_irq_enable(adreno_dev);

	ret = a6xx_gmu_device_start(adreno_dev);
	if (ret)
		goto err;

	ret = a6xx_hwsched_hfi_start(adreno_dev);
	if (ret)
		goto err;

	device->gmu_fault = false;

	trace_kgsl_pwr_set_state(device, KGSL_STATE_AWARE);

	return 0;
err:
	if (device->gmu_fault) {
		a6xx_gmu_suspend(adreno_dev);

		return ret;
	}

	a6xx_gmu_irq_disable(adreno_dev);

clks_gdsc_off:
	clk_bulk_disable_unprepare(gmu->num_clks, gmu->clks);

gdsc_off:
	/* Poll to make sure that the CX is off */
	a6xx_cx_regulator_disable_wait(gmu->cx_gdsc, device, 5000);

	a6xx_rdpm_cx_freq_update(gmu, 0);

	return ret;
}

static void a6xx_hwsched_active_count_put(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (WARN_ON(!mutex_is_locked(&device->mutex)))
		return;

	if (WARN(atomic_read(&device->active_cnt) == 0,
		"Unbalanced get/put calls to KGSL active count\n"))
		return;

	if (atomic_dec_and_test(&device->active_cnt)) {
		kgsl_pwrscale_update_stats(device);
		kgsl_pwrscale_update(device);
		kgsl_start_idle_timer(device);
	}

	trace_kgsl_active_count(device,
		(unsigned long) __builtin_return_address(0));

	wake_up(&device->active_cnt_wq);
}

static int unregister_context_hwsched(int id, void *ptr, void *data)
{
	struct kgsl_context *context = ptr;

	/*
	 * We don't need to send the unregister hfi packet because
	 * we are anyway going to lose the gmu state of registered
	 * contexts. So just reset the flag so that the context
	 * registers with gmu on its first submission post slumber.
	 */
	context->gmu_registered = false;

	return 0;
}

static int a6xx_hwsched_notify_slumber(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct hfi_prep_slumber_cmd req;

	CMD_MSG_HDR(req, H2F_MSG_PREPARE_SLUMBER);

	req.freq = gmu->hfi.dcvs_table.gpu_level_num -
			pwr->default_pwrlevel - 1;
	req.bw = pwr->pwrlevels[pwr->default_pwrlevel].bus_freq;

	/* Disable the power counter so that the GMU is not busy */
	gmu_core_regwrite(device, A6XX_GMU_CX_GMU_POWER_COUNTER_ENABLE, 0);

	return a6xx_hfi_send_cmd_async(adreno_dev, &req);

}
static int a6xx_hwsched_gmu_power_off(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	int ret = 0;

	if (device->gmu_fault)
		goto error;

	/* Wait for the lowest idle level we requested */
	ret = a6xx_gmu_wait_for_lowest_idle(adreno_dev);
	if (ret)
		goto error;

	ret = a6xx_hwsched_notify_slumber(adreno_dev);
	if (ret)
		goto error;

	ret = a6xx_gmu_wait_for_idle(adreno_dev);
	if (ret)
		goto error;

	ret = a6xx_rscc_sleep_sequence(adreno_dev);

	a6xx_rdpm_mx_freq_update(gmu, 0);

	/* Now that we are done with GMU and GPU, Clear the GBIF */
	ret = a6xx_halt_gbif(adreno_dev);
	/* De-assert the halts */
	kgsl_regwrite(device, A6XX_GBIF_HALT, 0x0);

	a6xx_gmu_irq_disable(adreno_dev);

	a6xx_hwsched_hfi_stop(adreno_dev);

	clk_bulk_disable_unprepare(gmu->num_clks, gmu->clks);

	/* Poll to make sure that the CX is off */
	a6xx_cx_regulator_disable_wait(gmu->cx_gdsc, device, 5000);

	a6xx_rdpm_cx_freq_update(gmu, 0);

	return ret;

error:
	a6xx_hwsched_hfi_stop(adreno_dev);
	a6xx_gmu_suspend(adreno_dev);

	return ret;
}

static int a6xx_hwsched_gpu_boot(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	/* Clear any GPU faults that might have been left over */
	adreno_clear_gpu_fault(adreno_dev);

	ret = kgsl_mmu_start(device);
	if (ret)
		goto err;

	ret = a6xx_gmu_oob_set(device, oob_gpu);
	if (ret)
		goto err;

	adreno_clear_dcvs_counters(adreno_dev);

	/* Restore performance counter registers with saved values */
	adreno_perfcounter_restore(adreno_dev);

	a6xx_start(adreno_dev);

	/* Re-initialize the coresight registers if applicable */
	adreno_coresight_start(adreno_dev);

	adreno_perfcounter_start(adreno_dev);

	/* Clear FSR here in case it is set from a previous pagefault */
	kgsl_mmu_clear_fsr(&device->mmu);

	a6xx_enable_gpu_irq(adreno_dev);

	ret = a6xx_hwsched_cp_init(adreno_dev);
	if (ret) {
		a6xx_disable_gpu_irq(adreno_dev);
		goto err;
	}

	device->reset_counter++;
err:
	a6xx_gmu_oob_clear(device, oob_gpu);

	if (ret)
		a6xx_hwsched_gmu_power_off(adreno_dev);

	return ret;
}

static void hwsched_idle_timer(struct timer_list *t)
{
	struct kgsl_device *device = container_of(t, struct kgsl_device,
					idle_timer);

	kgsl_schedule_work(&device->idle_check_ws);
}

static int a6xx_hwsched_gmu_init(struct adreno_device *adreno_dev)
{
	int ret;

	ret = a6xx_gmu_parse_fw(adreno_dev);
	if (ret)
		return ret;

	ret = a6xx_gmu_memory_init(adreno_dev);
	if (ret)
		return ret;

	return a6xx_hwsched_hfi_init(adreno_dev);
}

static void a6xx_hwsched_touch_wakeup(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	int ret;

	/*
	 * Do not wake up a suspended device or until the first boot sequence
	 * has been completed.
	 */
	if (test_bit(GMU_PRIV_PM_SUSPEND, &gmu->flags) ||
		!test_bit(GMU_PRIV_FIRST_BOOT_DONE, &gmu->flags))
		return;

	if (test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags))
		goto done;

	trace_kgsl_pwr_request_state(device, KGSL_STATE_ACTIVE);

	ret = a6xx_hwsched_gmu_boot(adreno_dev);
	if (ret)
		return;

	ret = a6xx_hwsched_gpu_boot(adreno_dev);
	if (ret)
		return;

	kgsl_pwrscale_wake(device);

	set_bit(GMU_PRIV_GPU_STARTED, &gmu->flags);

	device->pwrctrl.last_stat_updated = ktime_get();
	device->state = KGSL_STATE_ACTIVE;

	trace_kgsl_pwr_set_state(device, KGSL_STATE_ACTIVE);

done:
	/*
	 * When waking up from a touch event we want to stay active long enough
	 * for the user to send a draw command.  The default idle timer timeout
	 * is shorter than we want so go ahead and push the idle timer out
	 * further for this special case
	 */
	mod_timer(&device->idle_timer, jiffies +
		msecs_to_jiffies(adreno_wake_timeout));
}

static int a6xx_hwsched_boot(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	if (test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags))
		return 0;

	trace_kgsl_pwr_request_state(device, KGSL_STATE_ACTIVE);

	adreno_hwsched_start(adreno_dev);

	ret = a6xx_hwsched_gmu_boot(adreno_dev);
	if (ret)
		return ret;

	ret = a6xx_hwsched_gpu_boot(adreno_dev);
	if (ret)
		return ret;

	kgsl_start_idle_timer(device);

	kgsl_pwrscale_wake(device);

	set_bit(GMU_PRIV_GPU_STARTED, &gmu->flags);

	device->pwrctrl.last_stat_updated = ktime_get();
	device->state = KGSL_STATE_ACTIVE;

	trace_kgsl_pwr_set_state(device, KGSL_STATE_ACTIVE);

	return ret;
}

static int a6xx_hwsched_first_boot(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	int ret;

	if (test_bit(GMU_PRIV_FIRST_BOOT_DONE, &gmu->flags))
		return a6xx_hwsched_boot(adreno_dev);

	adreno_hwsched_start(adreno_dev);

	ret = a6xx_microcode_read(adreno_dev);
	if (ret)
		return ret;

	ret = a6xx_init(adreno_dev);
	if (ret)
		return ret;

	ret = a6xx_hwsched_gmu_init(adreno_dev);
	if (ret)
		return ret;

	trace_kgsl_pwr_request_state(device, KGSL_STATE_ACTIVE);

	ret = a6xx_hwsched_gmu_first_boot(adreno_dev);
	if (ret)
		return ret;

	ret = a6xx_hwsched_gpu_boot(adreno_dev);
	if (ret)
		return ret;

	adreno_get_bus_counters(adreno_dev);

	adreno_dev->cooperative_reset = ADRENO_FEATURE(adreno_dev,
						 ADRENO_COOP_RESET);

	set_bit(GMU_PRIV_FIRST_BOOT_DONE, &gmu->flags);
	set_bit(GMU_PRIV_GPU_STARTED, &gmu->flags);

	device->pwrctrl.last_stat_updated = ktime_get();
	device->state = KGSL_STATE_ACTIVE;

	trace_kgsl_pwr_set_state(device, KGSL_STATE_ACTIVE);

	return 0;
}

static int a6xx_hwsched_power_off(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	int ret;

	if (!test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags))
		return 0;

	trace_kgsl_pwr_request_state(device, KGSL_STATE_SLUMBER);

	/* process any profiling results that are available */
	adreno_profile_process_results(ADRENO_DEVICE(device));

	if (!a6xx_hw_isidle(adreno_dev))
		dev_err(&gmu->pdev->dev, "GPU isn't idle before SLUMBER\n");

	ret = a6xx_gmu_oob_set(device, oob_gpu);
	if (ret) {
		a6xx_gmu_oob_clear(device, oob_gpu);
		goto no_gx_power;
	}

	kgsl_pwrscale_update_stats(device);

	/* Save active coresight registers if applicable */
	adreno_coresight_stop(adreno_dev);

	/* Save physical performance counter values before GPU power down*/
	adreno_perfcounter_save(adreno_dev);

	adreno_irqctrl(adreno_dev, 0);

	a6xx_gmu_oob_clear(device, oob_gpu);

no_gx_power:
	kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_OFF);

	a6xx_hwsched_gmu_power_off(adreno_dev);

	read_lock(&device->context_lock);
	idr_for_each(&device->context_idr, unregister_context_hwsched, NULL);
	read_unlock(&device->context_lock);

	if (!IS_ERR_OR_NULL(adreno_dev->gpu_llc_slice))
		llcc_slice_deactivate(adreno_dev->gpu_llc_slice);

	if (!IS_ERR_OR_NULL(adreno_dev->gpuhtw_llc_slice))
		llcc_slice_deactivate(adreno_dev->gpuhtw_llc_slice);

	clear_bit(GMU_PRIV_GPU_STARTED, &gmu->flags);

	device->state = KGSL_STATE_NONE;

	del_timer_sync(&device->idle_timer);

	kgsl_pwrscale_sleep(device);

	kgsl_pwrctrl_clear_l3_vote(device);

	trace_kgsl_pwr_set_state(device, KGSL_STATE_SLUMBER);

	return ret;
}

static void hwsched_idle_check(struct work_struct *work)
{
	struct kgsl_device *device = container_of(work,
					struct kgsl_device, idle_check_ws);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	mutex_lock(&device->mutex);

	if (test_bit(GMU_DISABLE_SLUMBER, &device->gmu_core.flags))
		goto done;

	if (!atomic_read(&device->active_cnt)) {
		a6xx_hwsched_power_off(adreno_dev);
	} else {
		kgsl_pwrscale_update(device);
		kgsl_start_idle_timer(device);
	}

done:
	mutex_unlock(&device->mutex);
}

static int a6xx_hwsched_first_open(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	/*
	 * Do the one time settings that need to happen when we
	 * attempt to boot the gpu the very first time
	 */
	ret = a6xx_hwsched_first_boot(adreno_dev);
	if (ret)
		return ret;

	/*
	 * A client that does a first_open but never closes the device
	 * may prevent us from going back to SLUMBER. So trigger the idle
	 * check by incrementing the active count and immediately releasing it.
	 */
	atomic_inc(&device->active_cnt);
	a6xx_hwsched_active_count_put(adreno_dev);

	return 0;
}

static int a6xx_hwsched_active_count_get(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	int ret = 0;

	if (WARN_ON(!mutex_is_locked(&device->mutex)))
		return -EINVAL;

	if (test_bit(GMU_PRIV_PM_SUSPEND, &gmu->flags))
		return -EINVAL;

	if ((atomic_read(&device->active_cnt) == 0))
		ret = a6xx_hwsched_boot(adreno_dev);

	if (ret == 0)
		atomic_inc(&device->active_cnt);

	trace_kgsl_active_count(device,
		(unsigned long) __builtin_return_address(0));

	return ret;
}

static int a6xx_hwsched_dcvs_set(struct adreno_device *adreno_dev,
		int gpu_pwrlevel, int bus_level)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct hfi_dcvstable_cmd *table = &gmu->hfi.dcvs_table;
	struct hfi_gx_bw_perf_vote_cmd req = {
		.ack_type = DCVS_ACK_BLOCK,
		.freq = INVALID_DCVS_IDX,
		.bw = INVALID_DCVS_IDX,
	};
	int ret = 0;

	if (!test_bit(GMU_PRIV_HFI_STARTED, &gmu->flags))
		return 0;

	/* Do not set to XO and lower GPU clock vote from GMU */
	if ((gpu_pwrlevel != INVALID_DCVS_IDX) &&
			(gpu_pwrlevel >= table->gpu_level_num - 1)) {
		dev_err(&gmu->pdev->dev, "Invalid gpu dcvs request: %d\n",
			gpu_pwrlevel);
		return -EINVAL;
	}

	if (gpu_pwrlevel < table->gpu_level_num - 1)
		req.freq = table->gpu_level_num - gpu_pwrlevel - 1;

	if (bus_level < pwr->ddr_table_count && bus_level > 0)
		req.bw = bus_level;

	/* GMU will vote for slumber levels through the sleep sequence */
	if ((req.freq == INVALID_DCVS_IDX) && (req.bw == INVALID_DCVS_IDX))
		return 0;

	CMD_MSG_HDR(req, H2F_MSG_GX_BW_PERF_VOTE);

	ret = a6xx_hfi_send_cmd_async(adreno_dev, &req);

	if (ret) {
		dev_err_ratelimited(&gmu->pdev->dev,
			"Failed to set GPU perf idx %d, bw idx %d\n",
			req.freq, req.bw);

		/*
		 * If this was a dcvs request along side an active gpu, request
		 * dispatcher based reset and recovery.
		 */
		if (test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags)) {

			adreno_get_gpu_halt(adreno_dev);

			adreno_hwsched_set_fault(adreno_dev);
		}

	}

	if (req.freq != INVALID_DCVS_IDX)
		a6xx_rdpm_mx_freq_update(gmu,
			gmu->hfi.dcvs_table.gx_votes[req.freq].freq);

	return ret;
}

static int a6xx_hwsched_clock_set(struct adreno_device *adreno_dev,
	u32 pwrlevel)
{
	return a6xx_hwsched_dcvs_set(adreno_dev, pwrlevel, INVALID_DCVS_IDX);
}

static int a6xx_hwsched_bus_set(struct adreno_device *adreno_dev, int buslevel,
	u32 ab)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int ret = 0;

	if (buslevel != pwr->cur_buslevel) {
		ret = a6xx_hwsched_dcvs_set(adreno_dev, INVALID_DCVS_IDX,
				buslevel);
		if (ret)
			return ret;

		pwr->cur_buslevel = buslevel;

		trace_kgsl_buslevel(device, pwr->active_pwrlevel, buslevel);
	}

	if (ab != pwr->cur_ab) {
		icc_set_bw(pwr->icc_path, MBps_to_icc(ab), 0);
		pwr->cur_ab = ab;
	}

	return ret;
}

static int a6xx_hwsched_pm_suspend(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	int ret;

	if (test_bit(GMU_PRIV_PM_SUSPEND, &gmu->flags))
		return 0;

	trace_kgsl_pwr_request_state(device, KGSL_STATE_SUSPEND);

	/* Halt any new submissions */
	reinit_completion(&device->halt_gate);

	mutex_unlock(&device->mutex);

	/* Flush any currently running instances of the dispatcher */
	kthread_flush_worker(&kgsl_driver.worker);

	mutex_lock(&device->mutex);

	/* This ensures that dispatcher doesn't submit any new work */
	adreno_dispatcher_halt(device);

	/**
	 * Wait for the dispatcher to retire everything by waiting
	 * for the active count to go to zero.
	 */
	ret = kgsl_active_count_wait(device, 0, msecs_to_jiffies(100));
	if (ret) {
		dev_err(device->dev, "Timed out waiting for the active count\n");
		goto err;
	}

	if (test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags)) {
		unsigned long wait = jiffies +
			msecs_to_jiffies(ADRENO_IDLE_TIMEOUT);

		do {
			if (a6xx_hw_isidle(adreno_dev))
				break;
		} while (time_before(jiffies, wait));

		if (!a6xx_hw_isidle(adreno_dev)) {
			dev_err(device->dev, "Timed out idling the gpu\n");
			ret = -ETIMEDOUT;
			goto err;
		}

		a6xx_hwsched_power_off(adreno_dev);
	}

	set_bit(GMU_PRIV_PM_SUSPEND, &gmu->flags);

	trace_kgsl_pwr_set_state(device, KGSL_STATE_SUSPEND);

	return 0;

err:
	adreno_dispatcher_unhalt(device);
	adreno_hwsched_start(adreno_dev);

	return ret;
}

static void a6xx_hwsched_pm_resume(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);

	if (WARN(!test_bit(GMU_PRIV_PM_SUSPEND, &gmu->flags),
		"resume invoked without a suspend\n"))
		return;

	adreno_dispatcher_unhalt(device);

	adreno_hwsched_start(adreno_dev);

	clear_bit(GMU_PRIV_PM_SUSPEND, &gmu->flags);
}

static void a6xx_hwsched_drain_ctxt_unregister(struct adreno_device *adreno_dev)
{
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);
	struct pending_cmd *cmd = NULL;

	read_lock(&hfi->msglock);

	list_for_each_entry(cmd, &hfi->msglist, node) {
		if (MSG_HDR_GET_ID(cmd->sent_hdr) == H2F_MSG_UNREGISTER_CONTEXT)
			complete(&cmd->complete);
	}

	read_unlock(&hfi->msglock);
}

void a6xx_hwsched_restart(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	/*
	 * Any pending context unregister packets will be lost
	 * since we hard reset the GMU. This means any threads waiting
	 * for context unregister hfi ack will timeout. Wake them
	 * to avoid false positive ack timeout messages later.
	 */
	a6xx_hwsched_drain_ctxt_unregister(adreno_dev);

	read_lock(&device->context_lock);
	idr_for_each(&device->context_idr, unregister_context_hwsched, NULL);
	read_unlock(&device->context_lock);


	if (!test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags))
		return;

	a6xx_hwsched_hfi_stop(adreno_dev);

	a6xx_disable_gpu_irq(adreno_dev);

	a6xx_gmu_suspend(adreno_dev);

	clear_bit(GMU_PRIV_GPU_STARTED, &gmu->flags);

	ret = a6xx_hwsched_boot(adreno_dev);

	BUG_ON(ret);
}

const struct adreno_power_ops a6xx_hwsched_power_ops = {
	.first_open = a6xx_hwsched_first_open,
	.last_close = a6xx_hwsched_power_off,
	.active_count_get = a6xx_hwsched_active_count_get,
	.active_count_put = a6xx_hwsched_active_count_put,
	.touch_wakeup = a6xx_hwsched_touch_wakeup,
	.pm_suspend = a6xx_hwsched_pm_suspend,
	.pm_resume = a6xx_hwsched_pm_resume,
	.gpu_clock_set = a6xx_hwsched_clock_set,
	.gpu_bus_set = a6xx_hwsched_bus_set,
};

int a6xx_hwsched_probe(struct platform_device *pdev,
	u32 chipid, const struct adreno_gpu_core *gpucore)
{
	struct adreno_device *adreno_dev;
	struct kgsl_device *device;
	struct a6xx_hwsched_device *a6xx_hwsched_dev;
	int ret;

	a6xx_hwsched_dev = devm_kzalloc(&pdev->dev, sizeof(*a6xx_hwsched_dev),
				GFP_KERNEL);
	if (!a6xx_hwsched_dev)
		return -ENOMEM;

	adreno_dev = &a6xx_hwsched_dev->a6xx_dev.adreno_dev;

	ret = a6xx_probe_common(pdev, adreno_dev, chipid, gpucore);
	if (ret)
		return ret;

	device = KGSL_DEVICE(adreno_dev);

	INIT_WORK(&device->idle_check_ws, hwsched_idle_check);

	timer_setup(&device->idle_timer, hwsched_idle_timer, 0);

	adreno_dev->irq_mask = A6XX_HWSCHED_INT_MASK;

	adreno_hwsched_init(adreno_dev);

	return 0;
}

static int a6xx_hwsched_bind(struct device *dev, struct device *master,
	void *data)
{
	struct kgsl_device *device = dev_get_drvdata(master);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int ret;

	ret = a6xx_gmu_probe(device, to_platform_device(dev));
	if (ret)
		goto error;

	ret = a6xx_hwsched_hfi_probe(adreno_dev);
	if (ret)
		goto error;

	set_bit(GMU_DISPATCH, &device->gmu_core.flags);

	if (ADRENO_FEATURE(adreno_dev, ADRENO_PREEMPTION))
		set_bit(ADRENO_DEVICE_PREEMPTION, &adreno_dev->priv);

	return 0;

error:
	a6xx_gmu_remove(device);

	return ret;
}

static void a6xx_hwsched_unbind(struct device *dev, struct device *master,
		void *data)
{
	struct kgsl_device *device = dev_get_drvdata(master);

	a6xx_gmu_remove(device);

	adreno_hwsched_dispatcher_close(ADRENO_DEVICE(device));
}

static const struct component_ops a6xx_hwsched_component_ops = {
	.bind = a6xx_hwsched_bind,
	.unbind = a6xx_hwsched_unbind,
};

static int a6xx_hwsched_probe_dev(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &a6xx_hwsched_component_ops);
}

static int a6xx_hwsched_remove_dev(struct platform_device *pdev)
{
	component_del(&pdev->dev, &a6xx_hwsched_component_ops);
	return 0;
}

static const struct of_device_id a6xx_gmu_match_table[] = {
	{ .compatible = "qcom,gpu-gmu" },
	{ },
};

struct platform_driver a6xx_hwsched_driver = {
	.probe = a6xx_hwsched_probe_dev,
	.remove = a6xx_hwsched_remove_dev,
	.driver = {
		.name = "adreno-a6xx-gmu",
		.of_match_table = a6xx_gmu_match_table,
	},
};
