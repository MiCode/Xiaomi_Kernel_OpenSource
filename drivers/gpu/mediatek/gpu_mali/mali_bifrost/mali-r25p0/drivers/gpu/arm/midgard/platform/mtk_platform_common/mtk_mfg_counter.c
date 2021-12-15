/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/spinlock.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <platform/mtk_mfg_counter.h>
#include <mali_kbase_gator_api.h>
#include <string.h>
#include <linux/math64.h>


#define MALI_HWC_TYPES					4
#define MALI_COUNTERS_PER_BLOCK			64
/*
 *  block position of TMix HWC
 *  It defines in Mali_kbase_hwcnt_names_tmix.h
*/
#define JM_BLOCK_NAME_POS	0
#define TILER_BLOCK_NAME_POS	1
#define SHADER_BLOCK_NAME_POS	2
#define MMU_BLOCK_NAME_POS		3

static DEFINE_MUTEX(counter_info_lock);

static const char *const *hardware_counter_names;
static int number_of_hardware_counters;
static struct kbase_gator_hwcnt_info info;
static struct kbase_gator_hwcnt_handles *handle;
static struct GPU_PMU *mali_pmus;
static int name_offset_table[MALI_HWC_TYPES];
static int mfg_is_power_on;
static int binited;
static uint32_t active_cycle;

static int _find_name_pos(const char *name, int *pos)
{
	int i;

	if (!name || !pos)
		return -1;

	for (i = 0; i < number_of_hardware_counters; i++) {
		if (strstr(mali_pmus[i].name, name))
			break;
	}
	*pos = (i == number_of_hardware_counters) ? -1 : i;

	return (i == number_of_hardware_counters) ? -1 : 0;
}

static uint32_t _cal_urate(uint32_t fractions, uint32_t denominator)
{
	uint32_t value, urate;

	urate = value = 0;
	if (!denominator)
		return 0;
	if (fractions >= denominator)
		urate = 100;
	else {
		value = denominator / 100;
		if (!value)
			value = 1;
		urate = fractions / value;
		if (urate > 100)
			urate = 100;
		else if (!urate)
			urate = 1;
	}

	return urate;
}

static uint32_t _read_shader_u_rate(void)
{
	static int pos_exec_core_active, pos_gpu_active;
	static int binited;
	const char *exec_core_active_str = "EXEC_CORE_ACTIVE";
	const char *gpu_active_str = "GPU_ACTIVE";
	int result = 0;
	uint32_t exec_core_active, gpu_active, value, value1, urate;

	exec_core_active = gpu_active = value = value1 = urate = 0;
	if (!mali_pmus)
		return 0;

	if (!binited) {
		result |= _find_name_pos(exec_core_active_str, &pos_exec_core_active);
		result |= _find_name_pos(gpu_active_str, &pos_gpu_active);
		if (!result)
			binited = 1;
	}

	if (binited && info.nr_cores) {
		exec_core_active = mali_pmus[pos_exec_core_active].value;
		gpu_active = mali_pmus[pos_gpu_active].value;

		if (gpu_active) {
			value = exec_core_active / info.nr_cores;
			urate = _cal_urate(value, gpu_active);
		}
	}

	return urate;
}

static uint32_t _read_alu_u_rate(void)
{
	static int pos_exec_instr_count, pos_exec_active;
	static int binited;
	const char *exec_instr_count_str = "EXEC_INSTR_COUNT";
	const char *exec_active_str = "EXEC_ACTIVE";
	int result = 0;
	uint32_t exec_instr_count, exec_active, value, urate;

	exec_instr_count = exec_active = value = urate = 0;
	if (!mali_pmus)
		return 0;

	if (!binited) {
		result |= _find_name_pos(exec_instr_count_str, &pos_exec_instr_count);
		result |= _find_name_pos(exec_active_str, &pos_exec_active);
		if (!result)
			binited = 1;
	}

	if (binited) {
		exec_instr_count = mali_pmus[pos_exec_instr_count].value;
		exec_active = mali_pmus[pos_exec_active].value;

		if (exec_active)
			urate = _cal_urate(exec_instr_count, exec_active);
	}

	return urate;
}

static uint32_t _read_tex_u_rate(void)
{
	static int pos_tex_coord_issu, pos_exec_active;
	static int binited;
	const char *tex_coord_issue_str = "TEX_COORD_ISSUE";
	const char *exec_active_str = "EXEC_ACTIVE";
	int result = 0;
	uint32_t tex_coord_issue, exec_active, value, urate;

	tex_coord_issue = exec_active = value = urate = 0;
	if (!mali_pmus)
		return 0;

	if (!binited) {
		result |= _find_name_pos(tex_coord_issue_str, &pos_tex_coord_issu);
		result |= _find_name_pos(exec_active_str, &pos_exec_active);
		if (!result)
			binited = 1;
	}

	if (binited) {
		tex_coord_issue = mali_pmus[pos_tex_coord_issu].value;
		exec_active = mali_pmus[pos_exec_active].value;

		if (exec_active)
			urate = _cal_urate(tex_coord_issue, exec_active);
	}

	return urate;
}

static uint32_t _read_lsc_u_rate(void)
{
	static int pos_ls_mem_read_full, pos_ls_mem_read_short, pos_exec_active;
	static int pos_ls_mem_write_full, pos_ls_mem_write_short, pos_ls_mem_atomic;
	static int binited;
	const char *ls_mem_read_full_Str = "LS_MEM_READ_FULL";
	const char *ls_mem_read_short_str = "LS_MEM_READ_SHORT";
	const char *ls_mem_write_full_str = "LS_MEM_WRITE_FULL";
	const char *ls_mem_write_short_str = "LS_MEM_WRITE_SHORT";
	const char *ls_mem_atomic_str = "LS_MEM_ATOMIC";
	const char *exec_active_str = "EXEC_ACTIVE";
	int result = 0;
	uint32_t lsc_active, exec_active, value, urate;

	lsc_active = exec_active = value = urate = 0;
	if (!mali_pmus)
		return 0;

	if (!binited) {
		result |= _find_name_pos(ls_mem_read_full_Str, &pos_ls_mem_read_full);
		result |= _find_name_pos(ls_mem_read_short_str, &pos_ls_mem_read_short);
		result |= _find_name_pos(ls_mem_write_full_str, &pos_ls_mem_write_full);
		result |= _find_name_pos(ls_mem_write_short_str, &pos_ls_mem_write_short);
		result |= _find_name_pos(ls_mem_atomic_str, &pos_ls_mem_atomic);
		result |= _find_name_pos(exec_active_str, &pos_exec_active);
		if (!result)
			binited = 1;
	}

	if (binited) {
		lsc_active = mali_pmus[pos_ls_mem_read_full].value + mali_pmus[pos_ls_mem_read_short].value +
			mali_pmus[pos_ls_mem_write_full].value + mali_pmus[pos_ls_mem_write_short].value +
			mali_pmus[pos_ls_mem_atomic].value;
		exec_active = mali_pmus[pos_exec_active].value;

		if (exec_active) {
			/* Check overflow */
			if (lsc_active < (mali_pmus[pos_ls_mem_read_full].value
			+ mali_pmus[pos_ls_mem_write_full].value))
				urate = 0;
			else
				urate = _cal_urate(lsc_active, exec_active);
		}
	}

	return urate;
}

static uint32_t _read_var_u_rate(void)
{
	static int pos_vary_slot_32, pos_vary_slot_16, pos_exec_active;
	static int binited;
	const char *vary_slot_32_str = "VARY_SLOT_32";
	const char *vary_slot_16_str = "VARY_SLOT_16";
	const char *exec_active_str = "EXEC_ACTIVE";
	int result = 0;
	uint32_t var_active, exec_active, value, urate;

	var_active = exec_active = value = urate = 0;
	if (!mali_pmus)
		return 0;

	if (!binited) {
		result |= _find_name_pos(vary_slot_32_str, &pos_vary_slot_32);
		result |= _find_name_pos(vary_slot_16_str, &pos_vary_slot_16);
		result |= _find_name_pos(exec_active_str, &pos_exec_active);
		if (!result)
			binited = 1;
	}
	if (binited) {
		var_active = mali_pmus[pos_vary_slot_32].value + mali_pmus[pos_vary_slot_16].value;
		exec_active = mali_pmus[pos_exec_active].value;

		if (exec_active) {
			if (var_active < mali_pmus[pos_vary_slot_32].value)
				urate = 0;
			else
				urate = _cal_urate(var_active, exec_active);
		}
	}

	return urate;
}

static uint32_t _read_shader_u_rate_w_loading(void)
{
	static int pos_exec_core_active;
	static int binited;
	const char *exec_core_active_str = "EXEC_CORE_ACTIVE";
	int result = 0;
	uint32_t exec_core_active, value;

	exec_core_active = value = 0;
	if (!mali_pmus)
		return 0;

	if (!binited) {
		result |= _find_name_pos(exec_core_active_str, &pos_exec_core_active);
		if (!result)
			binited = 1;
	}

	if (binited && info.nr_cores)
		exec_core_active = mali_pmus[pos_exec_core_active].value;

	return (!exec_core_active || !active_cycle) ? 1 : _cal_urate(exec_core_active, active_cycle);
}

static uint32_t _read_alu_u_rate_w_loading(void)
{
	static int pos_exec_instr_count;
	static int binited;
	const char *exec_instr_count_str = "EXEC_INSTR_COUNT";
	int result = 0;
	uint32_t exec_instr_count, value;

	exec_instr_count = value = 0;
	if (!mali_pmus)
		return 0;

	if (!binited) {
		result |= _find_name_pos(exec_instr_count_str, &pos_exec_instr_count);
		if (!result)
			binited = 1;
	}

	if (binited && info.nr_cores)
		exec_instr_count = mali_pmus[pos_exec_instr_count].value;

	return (!exec_instr_count || !active_cycle) ? 1 : _cal_urate(exec_instr_count, active_cycle);
}

static uint32_t _read_tex_u_rate_w_loading(void)
{
	static int pos_tex_coord_issu;
	static int binited;
	const char *tex_coord_issue_str = "TEX_COORD_ISSUE";
	int result = 0;
	uint32_t tex_coord_issue, value;

	tex_coord_issue = value = 0;
	if (!mali_pmus)
		return 0;

	if (!binited) {
		result |= _find_name_pos(tex_coord_issue_str, &pos_tex_coord_issu);
		if (!result)
			binited = 1;
	}

	if (binited && info.nr_cores)
		tex_coord_issue = mali_pmus[pos_tex_coord_issu].value;

	return (!tex_coord_issue || !active_cycle) ? 1 : _cal_urate(tex_coord_issue, active_cycle);
}

static uint32_t _read_lsc_u_rate_w_loading(void)
{
	static int pos_ls_mem_read_full, pos_ls_mem_read_short;
	static int pos_ls_mem_write_full, pos_ls_mem_write_short, pos_ls_mem_atomic;
	static int binited;
	const char *ls_mem_read_full_Str = "LS_MEM_READ_FULL";
	const char *ls_mem_read_short_str = "LS_MEM_READ_SHORT";
	const char *ls_mem_write_full_str = "LS_MEM_WRITE_FULL";
	const char *ls_mem_write_short_str = "LS_MEM_WRITE_SHORT";
	const char *ls_mem_atomic_str = "LS_MEM_ATOMIC";
	int result = 0;
	uint32_t lsc_active, value;

	lsc_active = value = 0;
	if (!mali_pmus)
		return 0;

	if (!binited) {
		result |= _find_name_pos(ls_mem_read_full_Str, &pos_ls_mem_read_full);
		result |= _find_name_pos(ls_mem_read_short_str, &pos_ls_mem_read_short);
		result |= _find_name_pos(ls_mem_write_full_str, &pos_ls_mem_write_full);
		result |= _find_name_pos(ls_mem_write_short_str, &pos_ls_mem_write_short);
		result |= _find_name_pos(ls_mem_atomic_str, &pos_ls_mem_atomic);
		if (!result)
			binited = 1;
	}

	if (binited) {
		lsc_active = mali_pmus[pos_ls_mem_read_full].value + mali_pmus[pos_ls_mem_read_short].value +
			mali_pmus[pos_ls_mem_write_full].value + mali_pmus[pos_ls_mem_write_short].value +
			mali_pmus[pos_ls_mem_atomic].value;

		/* Check overflow */
		if (lsc_active < (mali_pmus[pos_ls_mem_read_full].value + mali_pmus[pos_ls_mem_write_full].value))
			lsc_active = 0;
	}

	return (!lsc_active || !active_cycle) ? 1 : _cal_urate(lsc_active, active_cycle);
}

static uint32_t _read_var_u_rate_w_loading(void)
{
	static int pos_vary_slot_32, pos_vary_slot_16;
	static int binited;
	const char *vary_slot_32_str = "VARY_SLOT_32";
	const char *vary_slot_16_str = "VARY_SLOT_16";
	int result = 0;
	uint32_t var_active, value;

	var_active = value = 0;
	if (!mali_pmus)
		return 0;

	if (!binited) {
		result |= _find_name_pos(vary_slot_32_str, &pos_vary_slot_32);
		result |= _find_name_pos(vary_slot_16_str, &pos_vary_slot_16);
		if (!result)
			binited = 1;
	}
	if (binited) {
		var_active = mali_pmus[pos_vary_slot_32].value + mali_pmus[pos_vary_slot_16].value;

		if (var_active < mali_pmus[pos_vary_slot_32].value)
			var_active = 0;
	}

	return (!var_active || !active_cycle) ? 1 : _cal_urate(var_active, active_cycle);
}

typedef uint32_t (*mfg_read_pfn)(void);
static struct {
	const char *name;
	mfg_read_pfn read;
} mfg_mtk_counters[] = {
	{"MTK_SHADER_U_RATE", _read_shader_u_rate},
	{"MTK_ALU_U_RATE", _read_alu_u_rate},
	{"MTK_TEX_U_RATE", _read_tex_u_rate},
	{"MTK_LSC_U_RATE", _read_lsc_u_rate},
	{"MTK_VAR_U_RATE", _read_var_u_rate},
	{"MTK_SHADER_U_RATE_W_LOADING", _read_shader_u_rate_w_loading},
	{"MTK_ALU_U_RATE_W_LOADING", _read_alu_u_rate_w_loading},
	{"MTK_TEX_U_RATE_W_LOADING", _read_tex_u_rate_w_loading},
	{"MTK_LSC_U_RATE_W_LOADING", _read_lsc_u_rate_w_loading},
	{"MTK_VAR_U_RATE_W_LOADING", _read_var_u_rate_w_loading},
};
#define MFG_MTK_COUNTER_SIZE (ARRAY_SIZE(mfg_mtk_counters))

static void _mtk_mfg_reset_counter(int ret)
{
	int i;

	if (!binited || !mali_pmus || !ret)
		return;

	for (i = 0; i < number_of_hardware_counters; i++) {
		if (ret == 1)
			mali_pmus[i].value = 0;
		mali_pmus[i].overflow  = 0;
	}
}

static void _mtk_mfg_init_counter(void)
{
	int empty_hwc_cnt, i, cnt;

	empty_hwc_cnt = 0;
	hardware_counter_names = kbase_gator_hwcnt_init_names(&cnt);
	if ((hardware_counter_names == NULL) || (cnt <= 0))
		return;

	/* Default doesn't enable all HWC */
	info.bitmask[0] = 0x57; /* JM */
	info.bitmask[1] = 0; /* Tiler */
	info.bitmask[2] = 0xffff; /* Shader */
	info.bitmask[3] = 0x19CF; /* L2 & MMU */
	handle = kbase_gator_hwcnt_init(&info);
	if (!handle) {
		pr_info("[PMU]Error init hwcnt\n");
		return;
	}

	for (i = 0; i < cnt; i++) {
		const char *name = hardware_counter_names[i];

		if (name[0] == '\0')
			empty_hwc_cnt++;
	}
	number_of_hardware_counters = cnt - empty_hwc_cnt + MFG_MTK_COUNTER_SIZE;
	mali_pmus = kcalloc(number_of_hardware_counters, sizeof(struct GPU_PMU), GFP_KERNEL);
	if (!mali_pmus) {
		pr_info("[PMU] fail to allocate mali_pmus\n");
		return;
	}


	name_offset_table[JM_BLOCK] = JM_BLOCK_NAME_POS;
	name_offset_table[TILER_BLOCK] = TILER_BLOCK_NAME_POS;
	name_offset_table[SHADER_BLOCK] = SHADER_BLOCK_NAME_POS;
	name_offset_table[MMU_L2_BLOCK] = MMU_BLOCK_NAME_POS;
	mfg_is_power_on = 0;
	binited = 1;

	/* Dump PMU info */
	pr_debug("bitmask[0] %x\n", info.bitmask[0]);
	pr_debug("bitmask[1] %x\n", info.bitmask[1]);
	pr_debug("bitmask[2] %x\n", info.bitmask[2]);
	pr_debug("bitmask[3] %x\n", info.bitmask[3]);
	pr_debug("gpu_id %d\n", info.gpu_id);
	pr_debug("nr_cores %d\n", info.nr_cores);
	pr_debug("nr_core_groups %d\n", info.nr_core_groups);
	pr_debug("nr_hwc_blocks %d\n", info.nr_hwc_blocks);
	for (i = 0; i < info.nr_hwc_blocks; i++)
		pr_debug("hwc_layout[%d] %d\n", i, info.hwc_layout[i]);

	pr_debug("num_of_counter %d\n", number_of_hardware_counters);
}

static int _mtk_mfg_update_counter(void)
{
	uint32_t success, ret, status, gpu_freq;
	static struct timeval tv_start, tv_end;
	static unsigned long long start_utime, end_utime, timd_diff_us;

	ret = timd_diff_us = gpu_freq = active_cycle = 0;
	status = kbase_gator_instr_hwcnt_dump_complete(handle, &success);

	if (!status || !success)
		ret = PMU_NG;

	if (ret != PMU_NG) {

		u32 *hwcnt_data = (u32 *)info.kernel_dump_buffer;
		int shader_block, block_type, i, j, name_offset, data_offset, cnt, nr_hwc_blocks;

		nr_hwc_blocks = info.nr_hwc_blocks - info.nr_cores + 1;
		cnt = 0;
		do_gettimeofday(&tv_end);
		end_utime = tv_end.tv_sec * 1000000 + tv_end.tv_usec;
		timd_diff_us = (end_utime > start_utime) ? (end_utime - start_utime) : 0;
		gpu_freq = mt_gpufreq_get_cur_freq();

		for (i = 0; i < nr_hwc_blocks; i++) {
			shader_block = 0;
			block_type = info.hwc_layout[i];
			if (block_type == RESERVED_BLOCK)
				continue;
			if (block_type == SHADER_BLOCK)
				shader_block = 1;

			name_offset = name_offset_table[block_type] * MALI_COUNTERS_PER_BLOCK;
			data_offset = i * MALI_COUNTERS_PER_BLOCK;
			for (j = 0; j < MALI_COUNTERS_PER_BLOCK; j++) {
				const char *name = hardware_counter_names[name_offset+j];

				if (name[0] == '\0')
					continue;

				mali_pmus[cnt].id = cnt;
				mali_pmus[cnt].overflow = 0;
				mali_pmus[cnt].value = hwcnt_data[data_offset + j];
				if (shader_block)
					mali_pmus[cnt].value /= info.nr_cores;

				if (strstr(mali_pmus[cnt].name, "GPU_ACTIVE")) {
					if ((mali_pmus[cnt].value != 0)
					&& ((timd_diff_us > 0) && (gpu_freq > 0)))
						active_cycle = (u32)(gpu_freq * div_u64((u64)timd_diff_us, 1000));
					else {
						ret = PMU_RESET_VALUE;
						goto FINISH;
					}
				}
				/* DEBUG */
				if (mali_pmus[cnt].name && (strstr(mali_pmus[cnt].name, "GPU_ACTIVE")
				|| strstr(mali_pmus[cnt].name, "EXEC_ACTIVE")
				|| strstr(mali_pmus[cnt].name, "FRAG_ACTIVE")))
					pr_debug("[PMU]id %d name %s value %d time %llu gpu_freq %d active_cycle %d\n",
					cnt, mali_pmus[cnt].name, mali_pmus[cnt].value, timd_diff_us,
					gpu_freq, active_cycle);

				cnt++;
			}
		}

		for (i = 0; i < MFG_MTK_COUNTER_SIZE; i++) {
			mali_pmus[cnt].id = cnt;
			if (mfg_mtk_counters[i].read) {
				mali_pmus[cnt].value = mfg_mtk_counters[i].read();
				if (mali_pmus[cnt].value == 0) {
					/* We get incorrect value, we should pass this round capture */
					mali_pmus[cnt].overflow = 0;
					mali_pmus[cnt].value = (uint32_t)-1;
					ret = PMU_RESET_VALUE;
					goto FINISH;
				}
			}
			cnt++;
		}
	}
FINISH:
	if (handle) {
		kbase_gator_instr_hwcnt_dump_irq(handle);
		do_gettimeofday(&tv_start);
		start_utime = tv_start.tv_sec * 1000000 + tv_start.tv_usec;
	}

	return ret;
}

static void gpu_power_change_notify_mfg_counter(int power_on)
{
	mutex_lock(&counter_info_lock);

	/* update before power off */
	if (!power_on) {
		_mtk_mfg_update_counter();
		_mtk_mfg_reset_counter(PMU_OK);
	}

	mfg_is_power_on = power_on;

	mutex_unlock(&counter_info_lock);
}

static int mali_get_gpu_pmu_init(struct GPU_PMU *pmus, int pmu_size, int *ret_size)
{
	int ret = PMU_OK;

	if (!binited)
		_mtk_mfg_init_counter();

	if (pmus) {
		int i, j, cnt, block_type;
		int nr_hwc_blocks, name_offset, data_offset;

		mutex_lock(&counter_info_lock);

		cnt = block_type = 0;
		nr_hwc_blocks = info.nr_hwc_blocks - info.nr_cores + 1;
		for (i = 0; i < nr_hwc_blocks; i++) {
			block_type = info.hwc_layout[i];

			if (block_type == RESERVED_BLOCK)
				continue;

			name_offset = name_offset_table[block_type] * MALI_COUNTERS_PER_BLOCK;
			data_offset = i * MALI_COUNTERS_PER_BLOCK;
			for (j = 0; j < MALI_COUNTERS_PER_BLOCK; j++) {
				const char *name = hardware_counter_names[name_offset+j];

				if (name[0] == '\0')
					continue;
				pmus[cnt].id = cnt;
				mali_pmus[cnt].id = cnt;
				pmus[cnt].name = name;
				mali_pmus[cnt].name = name;
				cnt++;
			}
		}

		for (i = 0; i < MFG_MTK_COUNTER_SIZE; i++) {
			pmus[cnt].id = cnt;
			mali_pmus[cnt].id = cnt;
			pmus[cnt].name = mfg_mtk_counters[i].name;
			mali_pmus[cnt].name = mfg_mtk_counters[i].name;
			cnt++;
		}
		mutex_unlock(&counter_info_lock);
	}

	if (ret_size) {
		mutex_lock(&counter_info_lock);

		*ret_size = number_of_hardware_counters;

		mutex_unlock(&counter_info_lock);
	}

	return ret;
}

static int mali_get_gpu_pmu_swapnreset(struct GPU_PMU *pmus, int pmu_size)
{
	int i, ret;

	ret = PMU_OK;
	if (!binited) {
		pr_info("[PMU] not inited, call mtk_get_gpu_pmu_init first\n");
		return PMU_NG;
	}

	if (pmus) {
		mutex_lock(&counter_info_lock);

		/* update if gpu power on */
		if (1) {
			ret = _mtk_mfg_update_counter();
			if (ret == PMU_RESET_VALUE) {
			_mtk_mfg_reset_counter(ret);
			ret = PMU_OK;
			}
		}

		if (!ret) {
			for (i = 0; i < pmu_size; i++) {
				pmus[i].id = mali_pmus[i].id;
				pmus[i].name = mali_pmus[i].name;
				pmus[i].value = mali_pmus[i].value;
				pmus[i].overflow = mali_pmus[i].overflow;
			}
		}

		mutex_unlock(&counter_info_lock);
	}

	return ret;
}

static int mali_get_gpu_pmu_swapnreset_stop(void)
{
	if (!binited) {
		pr_info("[PMU] not inited, call mtk_get_gpu_pmu_init first\n");
		return PMU_NG;
	}

	mutex_lock(&counter_info_lock);

	kbase_gator_hwcnt_term(&info, handle);

	mutex_unlock(&counter_info_lock);

	return PMU_OK;
}

int mali_get_gpu_pmu_deinit(void)
{
	if (!binited) {
		pr_info("[PMU] not inited, call mtk_get_gpu_pmu_init first\n");
		return PMU_NG;
	}

	mutex_lock(&counter_info_lock);

	kfree(mali_pmus);
	binited = 0;
	mfg_is_power_on = 0;

	mutex_unlock(&counter_info_lock);

	return PMU_OK;
}

void mtk_mfg_counter_init(void)
{
	mtk_get_gpu_pmu_init_fp = mali_get_gpu_pmu_init;
	mtk_get_gpu_pmu_deinit_fp = mali_get_gpu_pmu_deinit;
	mtk_get_gpu_pmu_swapnreset_fp = mali_get_gpu_pmu_swapnreset;
	mtk_get_gpu_pmu_swapnreset_stop_fp = mali_get_gpu_pmu_swapnreset_stop;

	mtk_register_gpu_power_change("mfg_counter", gpu_power_change_notify_mfg_counter);
	binited = 0;
	mfg_is_power_on = 0;

}

void mtk_mfg_counter_destroy(void)
{
	mtk_unregister_gpu_power_change("mfg_counter");

	mtk_get_gpu_pmu_init_fp = NULL;
	mtk_get_gpu_pmu_swapnreset_fp = NULL;
}
