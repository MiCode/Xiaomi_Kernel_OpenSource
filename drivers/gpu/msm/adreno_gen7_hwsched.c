// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/interconnect.h>
#include <linux/soc/qcom/llcc-qcom.h>

#include "adreno.h"
#include "adreno_gen7.h"
#include "adreno_gen7_hwsched.h"
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

static void gen7_hwsched_snapshot_preemption_record(struct kgsl_device *device,
	struct kgsl_snapshot *snapshot, struct kgsl_memdesc *md, u64 offset)
{
	struct kgsl_snapshot_section_header *section_header =
		(struct kgsl_snapshot_section_header *)snapshot->ptr;
	u8 *dest = snapshot->ptr + sizeof(*section_header);
	struct kgsl_snapshot_gpu_object_v2 *header =
		(struct kgsl_snapshot_gpu_object_v2 *)dest;
	const struct adreno_gen7_core *gen7_core = to_gen7_core(ADRENO_DEVICE(device));
	u64 ctxt_record_size = GEN7_CP_CTXRECORD_SIZE_IN_BYTES;
	size_t section_size;

	if (gen7_core->ctxt_record_size)
		ctxt_record_size = gen7_core->ctxt_record_size;

	ctxt_record_size = min_t(u64, ctxt_record_size, device->snapshot_ctxt_record_size);

	section_size = sizeof(*section_header) + sizeof(*header) + ctxt_record_size;
	if (snapshot->remain < section_size) {
		SNAPSHOT_ERR_NOMEM(device, "PREEMPTION RECORD");
		return;
	}

	section_header->magic = SNAPSHOT_SECTION_MAGIC;
	section_header->id = KGSL_SNAPSHOT_SECTION_GPU_OBJECT_V2;
	section_header->size = section_size;

	header->size = ctxt_record_size >> 2;
	header->gpuaddr = md->gpuaddr + offset;
	header->ptbase =
		kgsl_mmu_pagetable_get_ttbr0(device->mmu.defaultpagetable);
	header->type = SNAPSHOT_GPU_OBJECT_GLOBAL;

	dest += sizeof(*header);

	memcpy(dest, md->hostptr + offset, ctxt_record_size);

	snapshot->ptr += section_header->size;
	snapshot->remain -= section_header->size;
	snapshot->size += section_header->size;
}

static void snapshot_preemption_records(struct kgsl_device *device,
	struct kgsl_snapshot *snapshot, struct kgsl_memdesc *md)
{
	const struct adreno_gen7_core *gen7_core =
		to_gen7_core(ADRENO_DEVICE(device));
	u64 ctxt_record_size = GEN7_CP_CTXRECORD_SIZE_IN_BYTES;
	u64 offset;

	if (gen7_core->ctxt_record_size)
		ctxt_record_size = gen7_core->ctxt_record_size;

	/* All preemption records exist as a single mem alloc entry */
	for (offset = 0; offset < md->size; offset += ctxt_record_size)
		gen7_hwsched_snapshot_preemption_record(device, snapshot, md,
			offset);
}

static u32 gen7_copy_gpu_global(struct adreno_device *adreno_dev,
	void *out, u64 gpuaddr, u32 size)
{
	struct gen7_hwsched_hfi *hw_hfi = to_gen7_hwsched_hfi(adreno_dev);
	u64 offset;
	u32 i;

	for (i = 0; i < hw_hfi->mem_alloc_entries; i++) {
		struct kgsl_memdesc *md = hw_hfi->mem_alloc_table[i].md;

		if (md && (gpuaddr >= md->gpuaddr) &&
			((gpuaddr + size) <= (md->gpuaddr + md->size))) {
			offset = gpuaddr - md->gpuaddr;
			memcpy(out, md->hostptr + offset, size);
			return size;
		}
	}

	return 0;
}

static size_t adreno_hwsched_snapshot_rb_payload(struct kgsl_device *device,
	u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_rb_v2 *header = (struct kgsl_snapshot_rb_v2 *)buf;
	u32 *data = (u32 *)(buf + sizeof(*header));
	struct payload_section *payload = (struct payload_section *)priv;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	u32 size = gen7_hwsched_parse_payload(payload, KEY_RB_SIZEDWORDS) << 2;
	u64 lo, hi, gpuaddr;

	lo = gen7_hwsched_parse_payload(payload, KEY_RB_GPUADDR_LO);
	hi = gen7_hwsched_parse_payload(payload, KEY_RB_GPUADDR_HI);
	gpuaddr = hi << 32 | lo;

	/* If the gpuaddress and size don't match any allocation, then abort */
	if ((remain < size + sizeof(*header)) ||
		!gen7_copy_gpu_global(adreno_dev, data, gpuaddr, size)) {
		SNAPSHOT_ERR_NOMEM(device, "RB");
		return 0;
	}

	header->start = 0;
	header->end = size >> 2;
	header->rptr = gen7_hwsched_parse_payload(payload, KEY_RB_RPTR);
	header->wptr = gen7_hwsched_parse_payload(payload, KEY_RB_WPTR);
	header->rbsize = size >> 2;
	header->count = size >> 2;
	header->timestamp_queued = gen7_hwsched_parse_payload(payload,
			KEY_RB_QUEUED_TS);
	header->timestamp_retired = gen7_hwsched_parse_payload(payload,
			KEY_RB_RETIRED_TS);
	header->gpuaddr = gpuaddr;
	header->id = gen7_hwsched_parse_payload(payload, KEY_RB_ID);

	return size + sizeof(*header);
}

static bool parse_payload_rb(struct adreno_device *adreno_dev,
	struct kgsl_snapshot *snapshot)
{
	struct hfi_context_bad_cmd *cmd = adreno_dev->hwsched.ctxt_bad;
	u32 i = 0, payload_bytes;
	void *start;
	bool ret = false;

	/* Skip if we didn't receive a context bad HFI */
	if (!cmd->hdr)
		return false;

	payload_bytes = (MSG_HDR_GET_SIZE(cmd->hdr) << 2) -
			offsetof(struct hfi_context_bad_cmd, payload);

	start = &cmd->payload[0];

	while (i < payload_bytes) {
		struct payload_section *payload = start + i;

		if (payload->type == PAYLOAD_RB) {
			kgsl_snapshot_add_section(KGSL_DEVICE(adreno_dev),
				KGSL_SNAPSHOT_SECTION_RB_V2,
				snapshot, adreno_hwsched_snapshot_rb_payload,
				payload);
			ret = true;
		}

		i += sizeof(*payload) + (payload->dwords << 2);
	}

	return ret;
}

void gen7_hwsched_snapshot(struct adreno_device *adreno_dev,
	struct kgsl_snapshot *snapshot)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_hwsched_hfi *hw_hfi = to_gen7_hwsched_hfi(adreno_dev);
	bool skip_memkind_rb = false;
	u32 i;

	gen7_gmu_snapshot(adreno_dev, snapshot);

	adreno_hwsched_parse_fault_cmdobj(adreno_dev, snapshot);

	/*
	 * First try to dump ringbuffers using context bad HFI payloads
	 * because they have all the ringbuffer parameters. If ringbuffer
	 * payloads are not present, fall back to dumping ringbuffers
	 * based on MEMKIND_RB
	 */
	if (parse_payload_rb(adreno_dev, snapshot))
		skip_memkind_rb = true;

	for (i = 0; i < hw_hfi->mem_alloc_entries; i++) {
		struct hfi_mem_alloc_entry *entry = &hw_hfi->mem_alloc_table[i];

		if (entry->desc.mem_kind == HFI_MEMKIND_RB && !skip_memkind_rb)
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_RB_V2,
				snapshot, adreno_hwsched_snapshot_rb,
				entry->md);

		if (entry->desc.mem_kind == HFI_MEMKIND_SCRATCH)
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_GPU_OBJECT_V2,
				snapshot, adreno_snapshot_global,
				entry->md);

		if (entry->desc.mem_kind == HFI_MEMKIND_PROFILE)
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_GPU_OBJECT_V2,
				snapshot, adreno_snapshot_global,
				entry->md);

		if (entry->desc.mem_kind == HFI_MEMKIND_CSW_SMMU_INFO)
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_GPU_OBJECT_V2,
				snapshot, adreno_snapshot_global,
				entry->md);

		if (entry->desc.mem_kind == HFI_MEMKIND_CSW_PRIV_NON_SECURE)
			snapshot_preemption_records(device, snapshot,
				entry->md);
	}

	adreno_hwsched_parse_fault_cmdobj(adreno_dev, snapshot);
}

static int gen7_hwsched_gmu_first_boot(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	int level, ret = 0;

	trace_kgsl_pwr_request_state(device, KGSL_STATE_AWARE);

	gen7_gmu_aop_send_acd_state(gmu, adreno_dev->acd_enabled);

	ret = gen7_gmu_enable_gdsc(adreno_dev);
	if (ret)
		return ret;

	ret = gen7_gmu_enable_clks(adreno_dev);
	if (ret)
		goto gdsc_off;

	ret = gen7_gmu_load_fw(adreno_dev);
	if (ret)
		goto clks_gdsc_off;

	ret = gen7_gmu_itcm_shadow(adreno_dev);
	if (ret)
		goto clks_gdsc_off;

	if (!test_bit(GMU_PRIV_PDC_RSC_LOADED, &gmu->flags)) {
		ret = gen7_load_pdc_ucode(adreno_dev);
		if (ret)
			goto clks_gdsc_off;

		gen7_load_rsc_ucode(adreno_dev);
		set_bit(GMU_PRIV_PDC_RSC_LOADED, &gmu->flags);
	}

	gen7_gmu_register_config(adreno_dev);

	gen7_gmu_version_info(adreno_dev);

	gen7_gmu_irq_enable(adreno_dev);

	/* Vote for minimal DDR BW for GMU to init */
	level = pwr->pwrlevels[pwr->default_pwrlevel].bus_min;

	icc_set_bw(pwr->icc_path, 0, kBps_to_icc(pwr->ddr_table[level]));

	ret = gen7_gmu_device_start(adreno_dev);
	if (ret)
		goto err;

	ret = gen7_hwsched_hfi_start(adreno_dev);
	if (ret)
		goto err;

	icc_set_bw(pwr->icc_path, 0, 0);

	device->gmu_fault = false;

	if (ADRENO_FEATURE(adreno_dev, ADRENO_BCL))
		adreno_dev->bcl_enabled = true;

	trace_kgsl_pwr_set_state(device, KGSL_STATE_AWARE);

	return 0;

err:
	if (device->gmu_fault) {
		gen7_gmu_suspend(adreno_dev);

		return ret;
	}

	gen7_gmu_irq_disable(adreno_dev);

clks_gdsc_off:
	clk_bulk_disable_unprepare(gmu->num_clks, gmu->clks);

gdsc_off:
	/* Poll to make sure that the CX is off */
	gen7_cx_regulator_disable_wait(gmu->cx_gdsc, device, 5000);

	gen7_rdpm_cx_freq_update(gmu, 0);

	return ret;
}

static int gen7_hwsched_gmu_boot(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	int ret = 0;

	trace_kgsl_pwr_request_state(device, KGSL_STATE_AWARE);

	ret = gen7_gmu_enable_gdsc(adreno_dev);
	if (ret)
		return ret;

	ret = gen7_gmu_enable_clks(adreno_dev);
	if (ret)
		goto gdsc_off;

	ret = gen7_rscc_wakeup_sequence(adreno_dev);
	if (ret)
		goto clks_gdsc_off;

	ret = gen7_gmu_load_fw(adreno_dev);
	if (ret)
		goto clks_gdsc_off;

	gen7_gmu_register_config(adreno_dev);

	gen7_gmu_irq_enable(adreno_dev);

	ret = gen7_gmu_device_start(adreno_dev);
	if (ret)
		goto err;

	ret = gen7_hwsched_hfi_start(adreno_dev);
	if (ret)
		goto err;

	device->gmu_fault = false;

	trace_kgsl_pwr_set_state(device, KGSL_STATE_AWARE);

	return 0;
err:
	if (device->gmu_fault) {
		gen7_gmu_suspend(adreno_dev);

		return ret;
	}

	gen7_gmu_irq_disable(adreno_dev);

clks_gdsc_off:
	clk_bulk_disable_unprepare(gmu->num_clks, gmu->clks);

gdsc_off:
	/* Poll to make sure that the CX is off */
	gen7_cx_regulator_disable_wait(gmu->cx_gdsc, device, 5000);

	gen7_rdpm_cx_freq_update(gmu, 0);

	return ret;
}

void gen7_hwsched_active_count_put(struct adreno_device *adreno_dev)
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

static int gen7_hwsched_notify_slumber(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct hfi_prep_slumber_cmd req;
	int ret;

	ret = CMD_MSG_HDR(req, H2F_MSG_PREPARE_SLUMBER);
	if (ret)
		return ret;

	req.freq = gmu->hfi.dcvs_table.gpu_level_num -
			pwr->default_pwrlevel - 1;
	req.bw = pwr->pwrlevels[pwr->default_pwrlevel].bus_freq;

	/* Disable the power counter so that the GMU is not busy */
	gmu_core_regwrite(device, GEN7_GMU_CX_GMU_POWER_COUNTER_ENABLE, 0);

	return gen7_hfi_send_cmd_async(adreno_dev, &req);

}
static int gen7_hwsched_gmu_power_off(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	int ret = 0;

	if (device->gmu_fault)
		goto error;

	/* Wait for the lowest idle level we requested */
	ret = gen7_gmu_wait_for_lowest_idle(adreno_dev);
	if (ret)
		goto error;

	ret = gen7_hwsched_notify_slumber(adreno_dev);
	if (ret)
		goto error;

	ret = gen7_gmu_wait_for_idle(adreno_dev);
	if (ret)
		goto error;

	ret = gen7_rscc_sleep_sequence(adreno_dev);

	gen7_rdpm_mx_freq_update(gmu, 0);

	/* Now that we are done with GMU and GPU, Clear the GBIF */
	ret = gen7_halt_gbif(adreno_dev);

	gen7_gmu_irq_disable(adreno_dev);

	gen7_hwsched_hfi_stop(adreno_dev);

	clk_bulk_disable_unprepare(gmu->num_clks, gmu->clks);

	/* Poll to make sure that the CX is off */
	gen7_cx_regulator_disable_wait(gmu->cx_gdsc, device, 5000);

	gen7_rdpm_cx_freq_update(gmu, 0);

	return ret;

error:
	gen7_hwsched_hfi_stop(adreno_dev);
	gen7_gmu_suspend(adreno_dev);

	return ret;
}

static int gen7_hwsched_gpu_boot(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	/* Clear any GPU faults that might have been left over */
	adreno_clear_gpu_fault(adreno_dev);

	ret = kgsl_mmu_start(device);
	if (ret)
		goto err;

	ret = gen7_gmu_oob_set(device, oob_gpu);
	if (ret)
		goto err;

	/* Clear the busy_data stats - we're starting over from scratch */
	memset(&adreno_dev->busy_data, 0, sizeof(adreno_dev->busy_data));

	gen7_start(adreno_dev);

	/* Re-initialize the coresight registers if applicable */
	adreno_coresight_start(adreno_dev);

	adreno_perfcounter_start(adreno_dev);

	/* Clear FSR here in case it is set from a previous pagefault */
	kgsl_mmu_clear_fsr(&device->mmu);

	gen7_enable_gpu_irq(adreno_dev);

	ret = gen7_hwsched_cp_init(adreno_dev);
	if (ret) {
		gen7_disable_gpu_irq(adreno_dev);
		goto err;
	}

	device->reset_counter++;
err:
	gen7_gmu_oob_clear(device, oob_gpu);

	if (ret)
		gen7_hwsched_gmu_power_off(adreno_dev);

	return ret;
}

static void hwsched_idle_timer(struct timer_list *t)
{
	struct kgsl_device *device = container_of(t, struct kgsl_device,
					idle_timer);

	kgsl_schedule_work(&device->idle_check_ws);
}

static int gen7_hwsched_gmu_init(struct adreno_device *adreno_dev)
{
	int ret;

	ret = gen7_gmu_parse_fw(adreno_dev);
	if (ret)
		return ret;

	ret = gen7_gmu_memory_init(adreno_dev);
	if (ret)
		return ret;

	return gen7_hwsched_hfi_init(adreno_dev);
}

static void gen7_hwsched_touch_wakeup(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
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

	ret = gen7_hwsched_gmu_boot(adreno_dev);
	if (ret)
		return;

	ret = gen7_hwsched_gpu_boot(adreno_dev);
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
	 * for the user to send a draw command. The default idle timer timeout
	 * is shorter than we want so go ahead and push the idle timer out
	 * further for this special case
	 */
	mod_timer(&device->idle_timer, jiffies +
		msecs_to_jiffies(adreno_wake_timeout));
}

static int gen7_hwsched_boot(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	if (test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags))
		return 0;

	trace_kgsl_pwr_request_state(device, KGSL_STATE_ACTIVE);

	adreno_hwsched_start(adreno_dev);

	ret = gen7_hwsched_gmu_boot(adreno_dev);
	if (ret)
		return ret;

	ret = gen7_hwsched_gpu_boot(adreno_dev);
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

static int gen7_hwsched_first_boot(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	int ret;

	if (test_bit(GMU_PRIV_FIRST_BOOT_DONE, &gmu->flags))
		return gen7_hwsched_boot(adreno_dev);

	adreno_hwsched_start(adreno_dev);

	ret = gen7_microcode_read(adreno_dev);
	if (ret)
		return ret;

	ret = gen7_init(adreno_dev);
	if (ret)
		return ret;

	ret = gen7_hwsched_gmu_init(adreno_dev);
	if (ret)
		return ret;

	trace_kgsl_pwr_request_state(device, KGSL_STATE_ACTIVE);

	ret = gen7_hwsched_gmu_first_boot(adreno_dev);
	if (ret)
		return ret;

	ret = gen7_hwsched_gpu_boot(adreno_dev);
	if (ret)
		return ret;

	adreno_get_bus_counters(adreno_dev);

	adreno_dev->cooperative_reset = ADRENO_FEATURE(adreno_dev,
						 ADRENO_COOP_RESET);

	set_bit(GMU_PRIV_FIRST_BOOT_DONE, &gmu->flags);
	set_bit(GMU_PRIV_GPU_STARTED, &gmu->flags);

	/*
	 * There is a possible deadlock scenario during kgsl firmware reading
	 * (request_firmware) and devfreq update calls. During first boot, kgsl
	 * device mutex is held and then request_firmware is called for reading
	 * firmware. request_firmware internally takes dev_pm_qos_mtx lock.
	 * Whereas in case of devfreq update calls triggered by thermal/bcl or
	 * devfreq sysfs, it first takes the same dev_pm_qos_mtx lock and then
	 * tries to take kgsl device mutex as part of get_dev_status/target
	 * calls. This results in deadlock when both thread are unable to acquire
	 * the mutex held by other thread. Enable devfreq updates now as we are
	 * done reading all firmware files.
	 */
	device->pwrscale.devfreq_enabled = true;

	device->pwrctrl.last_stat_updated = ktime_get();
	device->state = KGSL_STATE_ACTIVE;

	trace_kgsl_pwr_set_state(device, KGSL_STATE_ACTIVE);

	return 0;
}

static int gen7_hwsched_power_off(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	int ret = 0;

	if (!test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags))
		return 0;

	trace_kgsl_pwr_request_state(device, KGSL_STATE_SLUMBER);

	/* process any profiling results that are available */
	adreno_profile_process_results(ADRENO_DEVICE(device));

	if (!gen7_hw_isidle(adreno_dev)) {
		dev_err(&gmu->pdev->dev, "GPU isn't idle before SLUMBER\n");
		gmu_core_fault_snapshot(device);
		goto no_gx_power;
	}

	ret = gen7_gmu_oob_set(device, oob_gpu);
	if (ret) {
		gen7_gmu_oob_clear(device, oob_gpu);
		goto no_gx_power;
	}

	kgsl_pwrscale_update_stats(device);

	/* Save active coresight registers if applicable */
	adreno_coresight_stop(adreno_dev);

	adreno_irqctrl(adreno_dev, 0);

	gen7_gmu_oob_clear(device, oob_gpu);

no_gx_power:
	kgsl_pwrctrl_irq(device, false);

	gen7_hwsched_gmu_power_off(adreno_dev);

	adreno_hwsched_unregister_contexts(adreno_dev);

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
		gen7_hwsched_power_off(adreno_dev);
	} else {
		kgsl_pwrscale_update(device);
		kgsl_start_idle_timer(device);
	}

done:
	mutex_unlock(&device->mutex);
}

static int gen7_hwsched_first_open(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	/*
	 * Do the one time settings that need to happen when we
	 * attempt to boot the gpu the very first time
	 */
	ret = gen7_hwsched_first_boot(adreno_dev);
	if (ret)
		return ret;

	/*
	 * A client that does a first_open but never closes the device
	 * may prevent us from going back to SLUMBER. So trigger the idle
	 * check by incrementing the active count and immediately releasing it.
	 */
	atomic_inc(&device->active_cnt);
	gen7_hwsched_active_count_put(adreno_dev);

	return 0;
}

int gen7_hwsched_active_count_get(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	int ret = 0;

	if (WARN_ON(!mutex_is_locked(&device->mutex)))
		return -EINVAL;

	if (test_bit(GMU_PRIV_PM_SUSPEND, &gmu->flags))
		return -EINVAL;

	if ((atomic_read(&device->active_cnt) == 0))
		ret = gen7_hwsched_boot(adreno_dev);

	if (ret == 0)
		atomic_inc(&device->active_cnt);

	trace_kgsl_active_count(device,
		(unsigned long) __builtin_return_address(0));

	return ret;
}

static int gen7_hwsched_dcvs_set(struct adreno_device *adreno_dev,
		int gpu_pwrlevel, int bus_level)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct hfi_dcvstable_cmd *table = &gmu->hfi.dcvs_table;
	struct hfi_gx_bw_perf_vote_cmd req = {
		.ack_type = DCVS_ACK_BLOCK,
		.freq = INVALID_DCVS_IDX,
		.bw = INVALID_DCVS_IDX,
	};
	int ret;

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

	ret = CMD_MSG_HDR(req, H2F_MSG_GX_BW_PERF_VOTE);
	if (ret)
		return ret;

	ret = gen7_hfi_send_cmd_async(adreno_dev, &req);

	if (ret) {
		dev_err_ratelimited(&gmu->pdev->dev,
			"Failed to set GPU perf idx %d, bw idx %d\n",
			req.freq, req.bw);

		/*
		 * If this was a dcvs request along side an active gpu, request
		 * dispatcher based reset and recovery.
		 */
		if (test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags))
			adreno_hwsched_fault(adreno_dev, ADRENO_HARD_FAULT);
	}

	if (req.freq != INVALID_DCVS_IDX)
		gen7_rdpm_mx_freq_update(gmu,
			gmu->hfi.dcvs_table.gx_votes[req.freq].freq);

	return ret;
}

static int gen7_hwsched_clock_set(struct adreno_device *adreno_dev,
	u32 pwrlevel)
{
	return gen7_hwsched_dcvs_set(adreno_dev, pwrlevel, INVALID_DCVS_IDX);
}

static void scale_gmu_frequency(struct adreno_device *adreno_dev, int buslevel)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	static unsigned long prev_freq;
	unsigned long freq = GMU_FREQ_MIN;

	if (!gmu->perf_ddr_bw)
		return;

	/*
	 * Scale the GMU if DDR is at a CX corner at which GMU can run at
	 * 500 Mhz
	 */
	if (pwr->ddr_table[buslevel] >= gmu->perf_ddr_bw)
		freq = GMU_FREQ_MAX;

	if (prev_freq == freq)
		return;

	if (kgsl_clk_set_rate(gmu->clks, gmu->num_clks, "gmu_clk", freq)) {
		dev_err(&gmu->pdev->dev, "Unable to set the GMU clock to %ld\n",
			freq);
		return;
	}

	gen7_rdpm_cx_freq_update(gmu, freq / 1000);

	trace_kgsl_gmu_pwrlevel(freq, prev_freq);

	prev_freq = freq;
}

static int gen7_hwsched_bus_set(struct adreno_device *adreno_dev, int buslevel,
	u32 ab)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int ret = 0;

	if (buslevel != pwr->cur_buslevel) {
		ret = gen7_hwsched_dcvs_set(adreno_dev, INVALID_DCVS_IDX,
				buslevel);
		if (ret)
			return ret;

		scale_gmu_frequency(adreno_dev, buslevel);

		pwr->cur_buslevel = buslevel;

		trace_kgsl_buslevel(device, pwr->active_pwrlevel, buslevel);
	}

	if (ab != pwr->cur_ab) {
		icc_set_bw(pwr->icc_path, MBps_to_icc(ab), 0);
		pwr->cur_ab = ab;
	}

	return ret;
}

static int gen7_hwsched_pm_suspend(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	int ret;

	if (test_bit(GMU_PRIV_PM_SUSPEND, &gmu->flags))
		return 0;

	trace_kgsl_pwr_request_state(device, KGSL_STATE_SUSPEND);

	/* Halt any new submissions */
	reinit_completion(&device->halt_gate);

	/**
	 * Wait for the dispatcher to retire everything by waiting
	 * for the active count to go to zero.
	 */
	ret = kgsl_active_count_wait(device, 0, msecs_to_jiffies(100));
	if (ret) {
		dev_err(device->dev, "Timed out waiting for the active count\n");
		goto err;
	}

	ret = adreno_hwsched_idle(adreno_dev);
	if (ret)
		goto err;

	gen7_hwsched_power_off(adreno_dev);

	adreno_get_gpu_halt(adreno_dev);

	set_bit(GMU_PRIV_PM_SUSPEND, &gmu->flags);

	trace_kgsl_pwr_set_state(device, KGSL_STATE_SUSPEND);

	return 0;

err:
	adreno_hwsched_start(adreno_dev);

	return ret;
}

static void gen7_hwsched_pm_resume(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);

	if (WARN(!test_bit(GMU_PRIV_PM_SUSPEND, &gmu->flags),
		"resume invoked without a suspend\n"))
		return;

	adreno_put_gpu_halt(adreno_dev);

	adreno_hwsched_start(adreno_dev);

	clear_bit(GMU_PRIV_PM_SUSPEND, &gmu->flags);
}

void gen7_hwsched_handle_watchdog(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 mask;

	/* Temporarily mask the watchdog interrupt to prevent a storm */
	gmu_core_regread(device, GEN7_GMU_AO_HOST_INTERRUPT_MASK,
		&mask);
	gmu_core_regwrite(device, GEN7_GMU_AO_HOST_INTERRUPT_MASK,
			(mask | GMU_INT_WDOG_BITE));

	gen7_gmu_send_nmi(adreno_dev, false);

	dev_err_ratelimited(&gmu->pdev->dev,
			"GMU watchdog expired interrupt received\n");

	adreno_hwsched_fault(adreno_dev, ADRENO_HARD_FAULT);
}

static void gen7_hwsched_drain_ctxt_unregister(struct adreno_device *adreno_dev)
{
	struct gen7_hwsched_hfi *hfi = to_gen7_hwsched_hfi(adreno_dev);
	struct pending_cmd *cmd = NULL;

	read_lock(&hfi->msglock);

	list_for_each_entry(cmd, &hfi->msglist, node) {
		if (MSG_HDR_GET_ID(cmd->sent_hdr) == H2F_MSG_UNREGISTER_CONTEXT)
			complete(&cmd->complete);
	}

	read_unlock(&hfi->msglock);
}

int gen7_hwsched_reset(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	int ret;

	/*
	 * Any pending context unregister packets will be lost
	 * since we hard reset the GMU. This means any threads waiting
	 * for context unregister hfi ack will timeout. Wake them
	 * to avoid false positive ack timeout messages later.
	 */
	gen7_hwsched_drain_ctxt_unregister(adreno_dev);

	adreno_hwsched_unregister_contexts(adreno_dev);

	if (!test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags))
		return 0;

	gen7_hwsched_hfi_stop(adreno_dev);

	gen7_disable_gpu_irq(adreno_dev);

	gen7_gmu_suspend(adreno_dev);

	/*
	 * In some corner cases, it is possible that GMU put TS_RETIRE
	 * on the msgq after we have turned off gmu interrupts. Hence,
	 * drain the queue one last time before we reboot the GMU.
	 */
	gen7_hwsched_process_msgq(adreno_dev);

	clear_bit(GMU_PRIV_GPU_STARTED, &gmu->flags);

	ret = gen7_hwsched_boot(adreno_dev);

	BUG_ON(ret);

	return ret;
}

const struct adreno_power_ops gen7_hwsched_power_ops = {
	.first_open = gen7_hwsched_first_open,
	.last_close = gen7_hwsched_power_off,
	.active_count_get = gen7_hwsched_active_count_get,
	.active_count_put = gen7_hwsched_active_count_put,
	.touch_wakeup = gen7_hwsched_touch_wakeup,
	.pm_suspend = gen7_hwsched_pm_suspend,
	.pm_resume = gen7_hwsched_pm_resume,
	.gpu_clock_set = gen7_hwsched_clock_set,
	.gpu_bus_set = gen7_hwsched_bus_set,
};

const struct adreno_hwsched_ops gen7_hwsched_ops = {
	.submit_cmdobj = gen7_hwsched_submit_cmdobj,
	.preempt_count = gen7_hwsched_preempt_count_get,
};

int gen7_hwsched_probe(struct platform_device *pdev,
		u32 chipid, const struct adreno_gpu_core *gpucore)
{
	struct adreno_device *adreno_dev;
	struct kgsl_device *device;
	struct gen7_hwsched_device *gen7_hwsched_dev;
	int ret;

	gen7_hwsched_dev = devm_kzalloc(&pdev->dev, sizeof(*gen7_hwsched_dev),
				GFP_KERNEL);
	if (!gen7_hwsched_dev)
		return -ENOMEM;

	adreno_dev = &gen7_hwsched_dev->gen7_dev.adreno_dev;

	ret = gen7_probe_common(pdev, adreno_dev, chipid, gpucore);
	if (ret)
		return ret;

	device = KGSL_DEVICE(adreno_dev);

	INIT_WORK(&device->idle_check_ws, hwsched_idle_check);

	timer_setup(&device->idle_timer, hwsched_idle_timer, 0);

	adreno_dev->irq_mask = GEN7_HWSCHED_INT_MASK;

	if (ADRENO_FEATURE(adreno_dev, ADRENO_PREEMPTION))
		set_bit(ADRENO_DEVICE_PREEMPTION, &adreno_dev->priv);

	return adreno_hwsched_init(adreno_dev, &gen7_hwsched_ops);
}

int gen7_hwsched_add_to_minidump(struct adreno_device *adreno_dev)
{
	struct gen7_device *gen7_dev = container_of(adreno_dev,
					struct gen7_device, adreno_dev);
	struct gen7_hwsched_device *gen7_hwsched = container_of(gen7_dev,
					struct gen7_hwsched_device, gen7_dev);
	int ret;

	ret = kgsl_add_va_to_minidump(adreno_dev->dev.dev, KGSL_HWSCHED_DEVICE,
			(void *)(gen7_hwsched), sizeof(struct gen7_hwsched_device));
	if (ret)
		return ret;

	ret = kgsl_add_va_to_minidump(adreno_dev->dev.dev, KGSL_GMU_LOG_ENTRY,
			gen7_dev->gmu.gmu_log->hostptr, gen7_dev->gmu.gmu_log->size);
	if (ret)
		return ret;

	ret = kgsl_add_va_to_minidump(adreno_dev->dev.dev, KGSL_HFIMEM_ENTRY,
			gen7_dev->gmu.hfi.hfi_mem->hostptr, gen7_dev->gmu.hfi.hfi_mem->size);

	return ret;
}
