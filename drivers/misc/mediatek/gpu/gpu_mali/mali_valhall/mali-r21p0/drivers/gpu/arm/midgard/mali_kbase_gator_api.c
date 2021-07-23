/*
 *
 * (C) COPYRIGHT 2014-2018 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#include "mali_kbase.h"
#include "mali_kbase_hw.h"
#include "mali_kbase_mem_linux.h"
#include "mali_kbase_gator_api.h"
#include "mali_kbase_gator_hwcnt_names.h"
#include "mali_kbase_hwcnt_types.h"
#include "mali_kbase_hwcnt_gpu.h"
#include "mali_kbase_hwcnt_virtualizer.h"

#define MALI_MAX_CORES_PER_GROUP		4
#define MALI_MAX_NUM_BLOCKS_PER_GROUP	8
#define MALI_COUNTERS_PER_BLOCK			64
#define MALI_BYTES_PER_COUNTER			4

struct kbase_gator_hwcnt_handles {
	struct kbase_device *kbdev;
	struct kbase_hwcnt_virtualizer_client *hvcli;
	struct kbase_hwcnt_enable_map enable_map;
	struct kbase_hwcnt_dump_buffer dump_buf;
	struct work_struct dump_work;
	int dump_complete;
	spinlock_t dump_lock;
};

static void dump_worker(struct work_struct *work);

const char * const *kbase_gator_hwcnt_init_names(uint32_t *total_counters)
{
	const char * const *hardware_counters;
	struct kbase_device *kbdev;
	uint32_t product_id;
	uint32_t count;

	if (!total_counters)
		return NULL;

	/* Get the first device - it doesn't matter in this case */
	kbdev = kbase_find_device(-1);
	if (!kbdev)
		return NULL;

	product_id = kbdev->gpu_props.props.core_props.product_id;

	switch (GPU_ID2_MODEL_MATCH_VALUE(product_id)) {
	case GPU_ID2_PRODUCT_TMIX:
		hardware_counters = hardware_counters_mali_tMIx;
		count = ARRAY_SIZE(hardware_counters_mali_tMIx);
		break;
	case GPU_ID2_PRODUCT_THEX:
		hardware_counters = hardware_counters_mali_tHEx;
		count = ARRAY_SIZE(hardware_counters_mali_tHEx);
		break;
	case GPU_ID2_PRODUCT_TSIX:
		hardware_counters = hardware_counters_mali_tSIx;
		count = ARRAY_SIZE(hardware_counters_mali_tSIx);
		break;
	case GPU_ID2_PRODUCT_TDVX:
		hardware_counters = hardware_counters_mali_tSIx;
		count = ARRAY_SIZE(hardware_counters_mali_tSIx);
		break;
	case GPU_ID2_PRODUCT_TNOX:
		hardware_counters = hardware_counters_mali_tNOx;
		count = ARRAY_SIZE(hardware_counters_mali_tNOx);
		break;
	case GPU_ID2_PRODUCT_TGOX:
		hardware_counters = hardware_counters_mali_tGOx;
		count = ARRAY_SIZE(hardware_counters_mali_tGOx);
		break;
	case GPU_ID2_PRODUCT_TTRX:
		hardware_counters = hardware_counters_mali_tTRx;
		count = ARRAY_SIZE(hardware_counters_mali_tTRx);
		break;
	case GPU_ID2_PRODUCT_TNAX:
		hardware_counters = hardware_counters_mali_tNAx;
		count = ARRAY_SIZE(hardware_counters_mali_tNAx);
		break;
	case GPU_ID2_PRODUCT_TBEX:
		hardware_counters = hardware_counters_mali_tBEx;
		count = ARRAY_SIZE(hardware_counters_mali_tBEx);
		break;
	default:
		hardware_counters = NULL;
		count = 0;
		dev_err(kbdev->dev, "Unrecognized product ID: %u\n",
			product_id);
		break;
	}

	/* Release the kbdev reference. */
	kbase_release_device(kbdev);

	*total_counters = count;

	/* If we return a string array take a reference on the module (or fail). */
	if (hardware_counters && !try_module_get(THIS_MODULE))
		return NULL;

	return hardware_counters;
}
KBASE_EXPORT_SYMBOL(kbase_gator_hwcnt_init_names);

void kbase_gator_hwcnt_term_names(void)
{
	/* Release the module reference. */
	module_put(THIS_MODULE);
}
KBASE_EXPORT_SYMBOL(kbase_gator_hwcnt_term_names);

struct kbase_gator_hwcnt_handles *kbase_gator_hwcnt_init(struct kbase_gator_hwcnt_info *in_out_info)
{
	int errcode;
	struct kbase_gator_hwcnt_handles *hand;
	const struct kbase_hwcnt_metadata *metadata;
	struct kbase_hwcnt_physical_enable_map phys_map;
	uint32_t dump_size = 0, i = 0;
	uint32_t nr_l2, nr_sc_bits, j;
	uint64_t core_mask;

	if (!in_out_info)
		return NULL;

	hand = kzalloc(sizeof(*hand), GFP_KERNEL);
	if (!hand)
		return NULL;

	INIT_WORK(&hand->dump_work, dump_worker);
	spin_lock_init(&hand->dump_lock);

	/* Get the first device */
	hand->kbdev = kbase_find_device(-1);
	if (!hand->kbdev)
		goto free_hand;

	metadata = kbase_hwcnt_virtualizer_metadata(
		hand->kbdev->hwcnt_gpu_virt);
	if (!metadata)
		goto release_device;

	errcode = kbase_hwcnt_enable_map_alloc(metadata, &hand->enable_map);
	if (errcode)
		goto release_device;

	errcode = kbase_hwcnt_dump_buffer_alloc(metadata, &hand->dump_buf);
	if (errcode)
		goto free_enable_map;

	in_out_info->kernel_dump_buffer = hand->dump_buf.dump_buf;

	in_out_info->nr_cores = hand->kbdev->gpu_props.num_cores;
	in_out_info->nr_core_groups = hand->kbdev->gpu_props.num_core_groups;
	in_out_info->gpu_id = hand->kbdev->gpu_props.props.core_props.product_id;

	nr_l2 = hand->kbdev->gpu_props.props.l2_props.num_l2_slices;

	core_mask = hand->kbdev->gpu_props.props.coherency_info.group[0].core_mask;

	nr_sc_bits = fls64(core_mask);

	/* The job manager and tiler sets of counters
	 * are always present */
	in_out_info->hwc_layout = kmalloc(sizeof(enum hwc_type) * (2 + nr_sc_bits + nr_l2), GFP_KERNEL);

	if (!in_out_info->hwc_layout)
		goto free_dump_buf;

	dump_size = (2 + nr_sc_bits + nr_l2) * MALI_COUNTERS_PER_BLOCK * MALI_BYTES_PER_COUNTER;

	in_out_info->hwc_layout[i++] = JM_BLOCK;
	in_out_info->hwc_layout[i++] = TILER_BLOCK;

	for (j = 0; j < nr_l2; j++)
		in_out_info->hwc_layout[i++] = MMU_L2_BLOCK;

	while (core_mask != 0ull) {
		if ((core_mask & 1ull) != 0ull)
			in_out_info->hwc_layout[i++] = SHADER_BLOCK;
		else
			in_out_info->hwc_layout[i++] = RESERVED_BLOCK;
		core_mask >>= 1;
	}

	/* Calculated dump size must be the same as real dump size */
	if (WARN_ON(dump_size != metadata->dump_buf_bytes))
		goto free_layout;

	in_out_info->nr_hwc_blocks = i;
	in_out_info->size = dump_size;

	phys_map.jm_bm = in_out_info->bitmask[JM_BLOCK];
	phys_map.tiler_bm = in_out_info->bitmask[TILER_BLOCK];
	phys_map.shader_bm = in_out_info->bitmask[SHADER_BLOCK];
	phys_map.mmu_l2_bm = in_out_info->bitmask[MMU_L2_BLOCK];
	kbase_hwcnt_gpu_enable_map_from_physical(&hand->enable_map, &phys_map);
	errcode = kbase_hwcnt_virtualizer_client_create(
		hand->kbdev->hwcnt_gpu_virt, &hand->enable_map, &hand->hvcli);
	if (errcode) {
		dev_err(hand->kbdev->dev,
			"Failed to register gator with hwcnt virtualizer core");
		goto free_layout;
	}

	return hand;

free_layout:
	kfree(in_out_info->hwc_layout);
free_dump_buf:
	kbase_hwcnt_dump_buffer_free(&hand->dump_buf);
free_enable_map:
	kbase_hwcnt_enable_map_free(&hand->enable_map);
release_device:
	kbase_release_device(hand->kbdev);
free_hand:
	kfree(hand);
	return NULL;
}
KBASE_EXPORT_SYMBOL(kbase_gator_hwcnt_init);

void kbase_gator_hwcnt_term(struct kbase_gator_hwcnt_info *in_out_info, struct kbase_gator_hwcnt_handles *opaque_handles)
{
	if (in_out_info)
		kfree(in_out_info->hwc_layout);

	if (opaque_handles) {
		cancel_work_sync(&opaque_handles->dump_work);
		kbase_hwcnt_virtualizer_client_destroy(opaque_handles->hvcli);
		kbase_hwcnt_dump_buffer_free(&opaque_handles->dump_buf);
		kbase_hwcnt_enable_map_free(&opaque_handles->enable_map);
		kbase_release_device(opaque_handles->kbdev);
		kfree(opaque_handles);
	}
}
KBASE_EXPORT_SYMBOL(kbase_gator_hwcnt_term);

static void dump_worker(struct work_struct *work)
{
	int errcode;
	u64 ts_start_ns;
	u64 ts_end_ns;
	struct kbase_gator_hwcnt_handles *hand;

	hand = container_of(work, struct kbase_gator_hwcnt_handles, dump_work);
	errcode = kbase_hwcnt_virtualizer_client_dump(
		hand->hvcli, &ts_start_ns, &ts_end_ns, &hand->dump_buf);
	if (!errcode) {
		/* Patch the header to hide other client's counter choices */
		kbase_hwcnt_gpu_patch_dump_headers(
			&hand->dump_buf, &hand->enable_map);
		/* Zero all non-enabled counters (currently undefined values) */
		kbase_hwcnt_dump_buffer_zero_non_enabled(
			&hand->dump_buf, &hand->enable_map);
		spin_lock_bh(&hand->dump_lock);
		hand->dump_complete = 1;
		spin_unlock_bh(&hand->dump_lock);
	} else {
		schedule_work(&hand->dump_work);
	}
}

uint32_t kbase_gator_instr_hwcnt_dump_complete(
		struct kbase_gator_hwcnt_handles *opaque_handles,
		uint32_t * const success)
{

	if (opaque_handles && success) {
		*success = opaque_handles->dump_complete;
		opaque_handles->dump_complete = 0;
		return *success;
	}
	return 0;
}
KBASE_EXPORT_SYMBOL(kbase_gator_instr_hwcnt_dump_complete);

uint32_t kbase_gator_instr_hwcnt_dump_irq(struct kbase_gator_hwcnt_handles *opaque_handles)
{
	if (opaque_handles)
		schedule_work(&opaque_handles->dump_work);
	return 0;
}
KBASE_EXPORT_SYMBOL(kbase_gator_instr_hwcnt_dump_irq);
