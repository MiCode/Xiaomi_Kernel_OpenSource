// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/spinlock.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <platform/mtk_mfg_counter.h>
#include <mali_kbase_gator_api.h>
#include <string.h>
#include <linux/math64.h>
#if IS_ENABLED(CONFIG_MTK_GPU_SWPM_SUPPORT)
#include <mali_kbase_vinstr.h>
#endif
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/uaccess.h>
#include <linux/random.h>

/*
 *  block position of Txx HWC
 *  It defines in Mali_kbase_hwcnt_names_txx.h
*/
#define JM_BLOCK_NAME_POS	0
#define TILER_BLOCK_NAME_POS	1
#define SHADER_BLOCK_NAME_POS	2
#define MMU_BLOCK_NAME_POS		3
//gpu stall counter
#if IS_ENABLED(CONFIG_MACH_MT6873) || IS_ENABLED(CONFIG_MACH_MT6853) || IS_ENABLED(CONFIG_MACH_MT6833)|| IS_ENABLED(CONFIG_MACH_MT6877)
#define GPU_STALL_ADD_BASE	0x1021C000
#else
#define GPU_STALL_ADD_BASE	0x1021E000
#endif
#define GPU_STALL_SIZE	0x1000
#define OFFSET_STALL_GPU_M0_WR_CNT	0x200
#define OFFSET_STALL_GPU_M0_RD_CNT	0x204
#define OFFSET_STALL_GPU_M1_WR_CNT	0x208
#define OFFSET_STALL_GPU_M1_RD_CNT	0x20c


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
static void __iomem *io_addr_gpu_stall;
static unsigned int	pre_stall_counters[4];

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
	static int shader_binited;
	const char *exec_core_active_str = "EXEC_CORE_ACTIVE";
	const char *gpu_active_str = "GPU_ACTIVE";
	int result = 0;
	uint32_t exec_core_active, gpu_active, value, value1, urate;

	exec_core_active = gpu_active = value = value1 = urate = 0;
	if (!mali_pmus)
		return 0;

	if (!shader_binited) {
		result |= _find_name_pos(exec_core_active_str, &pos_exec_core_active);
		result |= _find_name_pos(gpu_active_str, &pos_gpu_active);
		if (!result)
			shader_binited = 1;
	}

	if (shader_binited && info.nr_cores) {
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
	static int alu_binited;
	const char *exec_instr_count_str = "EXEC_INSTR_FMA";
	const char *exec_active_str = "EXEC_CORE_ACTIVE";
	int result = 0;
	uint32_t exec_instr_count, exec_active, value, urate;

	exec_instr_count = exec_active = value = urate = 0;
	if (!mali_pmus)
		return 0;

	if (!alu_binited) {
		result |= _find_name_pos(exec_instr_count_str, &pos_exec_instr_count);
		result |= _find_name_pos(exec_active_str, &pos_exec_active);
		if (!result)
			alu_binited = 1;
	}

	if (alu_binited) {
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
	static int tex_binited;
	const char *tex_coord_issue_str = "TEX_FILT_NUM_OPERATIONS";
	const char *exec_active_str = "EXEC_CORE_ACTIVE";
	int result = 0;
	uint32_t tex_coord_issue, exec_active, value, urate;

	tex_coord_issue = exec_active = value = urate = 0;
	if (!mali_pmus)
		return 0;

	if (!tex_binited) {
		result |= _find_name_pos(tex_coord_issue_str, &pos_tex_coord_issu);
		result |= _find_name_pos(exec_active_str, &pos_exec_active);
		if (!result)
			tex_binited = 1;
	}

	if (tex_binited) {
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
	static int lsc_binited;
	const char *ls_mem_read_full_Str = "LS_MEM_READ_FULL";
	const char *ls_mem_read_short_str = "LS_MEM_READ_SHORT";
	const char *ls_mem_write_full_str = "LS_MEM_WRITE_FULL";
	const char *ls_mem_write_short_str = "LS_MEM_WRITE_SHORT";
	const char *ls_mem_atomic_str = "LS_MEM_ATOMIC";
	const char *exec_active_str = "EXEC_CORE_ACTIVE";
	int result = 0;
	uint32_t lsc_active, exec_active, value, urate;

	lsc_active = exec_active = value = urate = 0;
	if (!mali_pmus)
		return 0;

	if (!lsc_binited) {
		result |= _find_name_pos(ls_mem_read_full_Str, &pos_ls_mem_read_full);
		result |= _find_name_pos(ls_mem_read_short_str, &pos_ls_mem_read_short);
		result |= _find_name_pos(ls_mem_write_full_str, &pos_ls_mem_write_full);
		result |= _find_name_pos(ls_mem_write_short_str, &pos_ls_mem_write_short);
		result |= _find_name_pos(ls_mem_atomic_str, &pos_ls_mem_atomic);
		result |= _find_name_pos(exec_active_str, &pos_exec_active);
		if (!result)
			lsc_binited = 1;
	}

	if (lsc_binited) {
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
	static int var_binited;
	const char *vary_slot_32_str = "VARY_SLOT_32";
	const char *vary_slot_16_str = "VARY_SLOT_16";
	const char *exec_active_str = "EXEC_CORE_ACTIVE";
	int result = 0;
	uint32_t var_active, exec_active, value, urate;

	var_active = exec_active = value = urate = 0;
	if (!mali_pmus)
		return 0;

	if (!var_binited) {
		result |= _find_name_pos(vary_slot_32_str, &pos_vary_slot_32);
		result |= _find_name_pos(vary_slot_16_str, &pos_vary_slot_16);
		result |= _find_name_pos(exec_active_str, &pos_exec_active);
		if (!result)
			var_binited = 1;
	}
	if (var_binited) {
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
	static int shader_loading_inited;
	const char *exec_core_active_str = "EXEC_CORE_ACTIVE";
	int result = 0;
	uint32_t exec_core_active, value;

	exec_core_active = value = 0;
	if (!mali_pmus)
		return 0;

	if (!shader_loading_inited) {
		result |= _find_name_pos(exec_core_active_str, &pos_exec_core_active);
		if (!result)
			shader_loading_inited = 1;
	}

	if (shader_loading_inited && info.nr_cores)
		exec_core_active = mali_pmus[pos_exec_core_active].value;

	return (!exec_core_active || !active_cycle) ? 1 : _cal_urate(exec_core_active, active_cycle);
}

static uint32_t _read_alu_u_rate_w_loading(void)
{
	static int pos_exec_instr_count;
	static int alu_loading_inited;
	const char *exec_instr_count_str = "EXEC_INSTR_FMA";
	int result = 0;
	uint32_t exec_instr_count, value;

	exec_instr_count = value = 0;
	if (!mali_pmus)
		return 0;

	if (!alu_loading_inited) {
		result |= _find_name_pos(exec_instr_count_str, &pos_exec_instr_count);
		if (!result)
			alu_loading_inited = 1;
	}

	if (alu_loading_inited && info.nr_cores)
		exec_instr_count = mali_pmus[pos_exec_instr_count].value;

	return (!exec_instr_count || !active_cycle) ? 1 : _cal_urate(exec_instr_count, active_cycle);
}

static uint32_t _read_tex_u_rate_w_loading(void)
{
	static int pos_tex_coord_issu;
	static int tex_loading_inited;
	const char *tex_coord_issue_str = "TEX_FILT_NUM_OPERATIONS";
	int result = 0;
	uint32_t tex_coord_issue, value;

	tex_coord_issue = value = 0;
	if (!mali_pmus)
		return 0;

	if (!tex_loading_inited) {
		result |= _find_name_pos(tex_coord_issue_str, &pos_tex_coord_issu);
		if (!result)
			tex_loading_inited = 1;
	}

	if (tex_loading_inited && info.nr_cores)
		tex_coord_issue = mali_pmus[pos_tex_coord_issu].value;

	return (!tex_coord_issue || !active_cycle) ? 1 : _cal_urate(tex_coord_issue, active_cycle);
}

static uint32_t _read_lsc_u_rate_w_loading(void)
{
	static int pos_ls_mem_read_full, pos_ls_mem_read_short;
	static int pos_ls_mem_write_full, pos_ls_mem_write_short, pos_ls_mem_atomic;
	static int lsc_loading_inited;
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

	if (!lsc_loading_inited) {
		result |= _find_name_pos(ls_mem_read_full_Str, &pos_ls_mem_read_full);
		result |= _find_name_pos(ls_mem_read_short_str, &pos_ls_mem_read_short);
		result |= _find_name_pos(ls_mem_write_full_str, &pos_ls_mem_write_full);
		result |= _find_name_pos(ls_mem_write_short_str, &pos_ls_mem_write_short);
		result |= _find_name_pos(ls_mem_atomic_str, &pos_ls_mem_atomic);
		if (!result)
			lsc_loading_inited = 1;
	}

	if (lsc_loading_inited) {
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
	static int var_loading_inited;
	const char *vary_slot_32_str = "VARY_SLOT_32";
	const char *vary_slot_16_str = "VARY_SLOT_16";
	int result = 0;
	uint32_t var_active, value;

	var_active = value = 0;
	if (!mali_pmus)
		return 0;

	if (!var_loading_inited) {
		result |= _find_name_pos(vary_slot_32_str, &pos_vary_slot_32);
		result |= _find_name_pos(vary_slot_16_str, &pos_vary_slot_16);
		if (!result)
			var_loading_inited = 1;
	}
	if (var_loading_inited) {
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
	{"MTK_ALU_FMA_U_RATE", _read_alu_u_rate},
	{"MTK_TEX_U_RATE", _read_tex_u_rate},
	{"MTK_LSC_U_RATE", _read_lsc_u_rate},
	{"MTK_VAR_U_RATE", _read_var_u_rate},
	{"MTK_SHADER_U_RATE_W_LOADING", _read_shader_u_rate_w_loading},
	{"MTK_ALU_FMA_U_RATE_W_LOADING", _read_alu_u_rate_w_loading},
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
		mali_pmus[i].value = 0;
		mali_pmus[i].overflow  = 0;
	}
}

static bool _mtk_check_map(int block_type, int map)
{
	int tmp = map / 4;
	int mask;
	mask = (info.bitmask[block_type] >> tmp) && 0x1;

	if (mask == 0)
		return 1;	//this couter disable

	return 0;
}


static void _mtk_mfg_init_counter(void)
{
	int empty_hwc_cnt, i, cnt;

	empty_hwc_cnt = 0;
	hardware_counter_names = kbase_gator_hwcnt_init_names(&cnt);
	if ((hardware_counter_names == NULL) || (cnt <= 0)) {
		return;
	}

	/* Default doesn't enable all HWC */
	info.bitmask[0] = 0x57; /* JM */
	info.bitmask[1] = 0x2; /* Tiler */
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
	if (!binited) {
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
		pr_debug("[GPU pmu]bitmask[0] %x\n", info.bitmask[0]);
		pr_debug("[GPU pmu]bitmask[1] %x\n", info.bitmask[1]);
		pr_debug("[GPU pmu]bitmask[2] %x\n", info.bitmask[2]);
		pr_debug("[GPU pmu]bitmask[3] %x\n", info.bitmask[3]);
		pr_debug("[GPU pmu]gpu_id %d\n", info.gpu_id);
		pr_debug("[GPU pmu]nr_cores %d\n", info.nr_cores);
		pr_debug("[GPU pmu]nr_core_groups %d\n", info.nr_core_groups);
		pr_debug("[GPU pmu]nr_hwc_blocks %d\n", info.nr_hwc_blocks);
		for (i = 0; i < info.nr_hwc_blocks; i++)
			pr_debug("[GPU pmu]hwc_layout[%d] %d\n", i, info.hwc_layout[i]);

		pr_debug("[GPU pmu]num_of_counter %d\n", number_of_hardware_counters);
		kbase_gator_instr_hwcnt_dump_irq(handle);
	}
}

static int _mtk_mfg_update_counter(void)
{
	uint32_t success, ret, status, gpu_freq;
	static struct timespec64 tv_start, tv_end;
	static unsigned long long start_utime, end_utime, timd_diff_us;
	int block[RESERVED_BLOCK + 1] = {0};
	ret = timd_diff_us = gpu_freq = active_cycle = 0;
	status = kbase_gator_instr_hwcnt_dump_complete(handle, &success);

	if (!status || !success) {
		ret = PMU_NG;
		return ret;
	}

	if (ret != PMU_NG) {
		u32 *hwcnt_data = (u32 *)info.kernel_dump_buffer;
		int shader_block, block_type, i, j, name_offset, data_offset, cnt, nr_hwc_blocks;

		nr_hwc_blocks = info.nr_hwc_blocks ;
		cnt = 0;
		ktime_get_real_ts64(&tv_end);
		end_utime = tv_end.tv_sec * 1000000 + (tv_end.tv_nsec / 1000);
		timd_diff_us = (end_utime > start_utime) ? (end_utime - start_utime) : 0;
#if defined(CONFIG_MTK_GPUFREQ_V2)
		gpu_freq = gpufreq_get_cur_freq(TARGET_DEFAULT)*1000;
#else
		gpu_freq = mt_gpufreq_get_cur_freq()*1000;
#endif
		_mtk_mfg_reset_counter(1);
		for (i = 0; i < nr_hwc_blocks; i++) {
			shader_block = 0;
			block_type = info.hwc_layout[i];
			if (block_type == RESERVED_BLOCK || block[block_type] == 1)
				continue;
			block[block_type] = 1;
			name_offset = name_offset_table[block_type] * MALI_COUNTERS_PER_BLOCK;
			data_offset = i * MALI_COUNTERS_PER_BLOCK;
			for (j = 0; j < MALI_COUNTERS_PER_BLOCK; j++) {
				const char *name = hardware_counter_names[name_offset+j];

				if (name[0] == '\0')
					continue;
				else if (_mtk_check_map(block_type, j)) {
					cnt++;
					continue;
				}
				mali_pmus[cnt].id = cnt;
				mali_pmus[cnt].overflow = 0;
				mali_pmus[cnt].value = hwcnt_data[data_offset + j];

				//gpu active=0, all counters=0
				if (strstr(mali_pmus[cnt].name, "GPU_ACTIVE")) {
					active_cycle = (uint32_t)div_u64((u64)(gpu_freq*timd_diff_us), 1000);
					if (mali_pmus[cnt].value == 0) {
						ret = PMU_RESET_VALUE;
						goto FINISH;
					}
				}
				/* DEBUG */
				/*
				if (mali_pmus[cnt].name && (strstr(mali_pmus[cnt].name, "GPU_ACTIVE")
				|| strstr(mali_pmus[cnt].name, "EXEC_ACTIVE")
				|| strstr(mali_pmus[cnt].name, "FRAG_ACTIVE")))
					pr_debug("[PMU]id %d name %s value %d time %llu gpu_freq %d active_cycle %d\n",
					cnt, mali_pmus[cnt].name, mali_pmus[cnt].value, timd_diff_us,
					gpu_freq, active_cycle)
				*/
				cnt++;
			}
		}

		for (i = 0; i < MFG_MTK_COUNTER_SIZE; i++) {
			mali_pmus[cnt].id = cnt;
			if (mfg_mtk_counters[i].read) {
				mali_pmus[cnt].value = mfg_mtk_counters[i].read();
				if (mali_pmus[cnt].value == 0) {
					//We get incorrect value, we should pass this round capture
					mali_pmus[cnt].overflow = 0;
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
		ktime_get_real_ts64(&tv_start);
		start_utime = tv_start.tv_sec * 1000000 + (tv_start.tv_nsec / 1000);
	}

	return ret;
}

static int mali_get_gpu_pmu_init(struct GPU_PMU *pmus, int pmu_size, int *ret_size)
{
	int ret = PMU_OK;
	int block[RESERVED_BLOCK + 1] = {0};

	if (!binited)
		_mtk_mfg_init_counter();

	if (pmus) {
		int i, j, cnt, block_type;
		int nr_hwc_blocks, name_offset, data_offset;

		mutex_lock(&counter_info_lock);
#if IS_ENABLED(CONFIG_MTK_GPU_SWPM_SUPPORT)
		if (MTK_get_mtk_pm() != pm_non)
			MTK_kbasep_vinstr_hwcnt_set_interval(0);
#endif
		cnt = block_type = 0;
		nr_hwc_blocks = info.nr_hwc_blocks;
		for (i = 0; i < nr_hwc_blocks; i++) {
			block_type = info.hwc_layout[i];
			if (block_type == RESERVED_BLOCK || block[block_type] == 1)
				continue;
			block[block_type] = 1;
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
	kbase_gator_hwcnt_term_names();

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
#if IS_ENABLED(CONFIG_MTK_GPU_SWPM_SUPPORT)
	if (MTK_get_mtk_pm() == pm_ltr)
		MTK_kbasep_vinstr_hwcnt_set_interval(8000000);
	else if (MTK_get_mtk_pm() == pm_swpm)
		MTK_kbasep_vinstr_hwcnt_set_interval(1000000);
#endif
	mutex_unlock(&counter_info_lock);

	return PMU_OK;
}

void mtk_mfg_counter_init(void)
{
	mtk_get_gpu_pmu_init_fp = mali_get_gpu_pmu_init;
	mtk_get_gpu_pmu_deinit_fp = mali_get_gpu_pmu_deinit;
	mtk_get_gpu_pmu_swapnreset_fp = mali_get_gpu_pmu_swapnreset;
	mtk_get_gpu_pmu_swapnreset_stop_fp = mali_get_gpu_pmu_swapnreset_stop;

	binited = 0;
	mfg_is_power_on = 0;

}

void mtk_mfg_counter_destroy(void)
{
	mtk_get_gpu_pmu_init_fp = NULL;
	mtk_get_gpu_pmu_swapnreset_fp = NULL;
}


// init but don't enable met
int gator_gpu_pmu_init()
{
	int ret = PMU_OK;
	int i, j, cnt, block_type;
	int nr_hwc_blocks, name_offset, data_offset;
	if (!binited) {
		_mtk_mfg_init_counter();
	}
	cnt = block_type = 0;
	nr_hwc_blocks = info.nr_hwc_blocks - info.nr_cores + 1;
	pr_debug("block num:%u, core:%u", info.nr_hwc_blocks, info.nr_cores);
	for (i = 0; i < info.nr_hwc_blocks; i++) {
		block_type = info.hwc_layout[i];
		pr_debug("block:%d", block_type);
		if (block_type == RESERVED_BLOCK)
			continue;
		name_offset = name_offset_table[block_type] * MALI_COUNTERS_PER_BLOCK;
		data_offset = i * MALI_COUNTERS_PER_BLOCK;
		for (j = 0; j < MALI_COUNTERS_PER_BLOCK; j++) {
			const char *name = hardware_counter_names[name_offset + j];
			if (name[0] == '\0')
				continue;
			mali_pmus[cnt].id = cnt;
			mali_pmus[cnt].name = name;
			pr_debug("%u:%s", data_offset + j, name);
			cnt++;
		}
	}
	for (i = 0; i < MFG_MTK_COUNTER_SIZE; i++) {
		mali_pmus[cnt].id = cnt;
		mali_pmus[cnt].name = mfg_mtk_counters[i].name;
		cnt++;
	}
	return ret;
}

int mtk_gpu_stall_create_subfs(void)
{
	io_addr_gpu_stall = ioremap(GPU_STALL_ADD_BASE, GPU_STALL_SIZE);
	if (!io_addr_gpu_stall) {
		pr_info("Failed to init GPU stall counters!!\n");
		return -ENODEV;
	}
	return 0;
}

void mtk_gpu_stall_delete_subfs(void)
{
	if (io_addr_gpu_stall) {
		iounmap(io_addr_gpu_stall);
		io_addr_gpu_stall = NULL;
	}
}

void mtk_gpu_stall_start(void)
{
	unsigned int value = 0x00000001;
	if (io_addr_gpu_stall) {
		writel(value, io_addr_gpu_stall + OFFSET_STALL_GPU_M0_WR_CNT);
		writel(value, io_addr_gpu_stall + OFFSET_STALL_GPU_M0_RD_CNT);
		writel(value, io_addr_gpu_stall + OFFSET_STALL_GPU_M1_WR_CNT);
		writel(value, io_addr_gpu_stall + OFFSET_STALL_GPU_M1_RD_CNT);
	}
}

void mtk_gpu_stall_stop(void)
{
	if (io_addr_gpu_stall) {
		writel(0x00000000, io_addr_gpu_stall + OFFSET_STALL_GPU_M0_WR_CNT);
		writel(0x00000000, io_addr_gpu_stall + OFFSET_STALL_GPU_M0_RD_CNT);
		writel(0x00000000, io_addr_gpu_stall + OFFSET_STALL_GPU_M1_WR_CNT);
		writel(0x00000000, io_addr_gpu_stall + OFFSET_STALL_GPU_M1_RD_CNT);
	}
}

void mtk_GPU_STALL_RAW(unsigned int *diff, int size)
{
	unsigned int stall_counters[4] = {0};
	int i;
#if IS_ENABLED(CONFIG_MACH_MT6877)
	for (i = 0; i < size; i++) {
			diff[i] = stall_counters[i];
	}
	for(i = 0; i < size; i++) {
			pre_stall_counters[i] = stall_counters[i];
	}
#else
	if (io_addr_gpu_stall) {
		stall_counters[0] = ((unsigned int)readl(io_addr_gpu_stall + OFFSET_STALL_GPU_M0_WR_CNT)) >> 1;
		stall_counters[1] = ((unsigned int)readl(io_addr_gpu_stall + OFFSET_STALL_GPU_M0_RD_CNT)) >> 1;
		stall_counters[2] = ((unsigned int)readl(io_addr_gpu_stall + OFFSET_STALL_GPU_M1_WR_CNT)) >> 1;
		stall_counters[3] = ((unsigned int)readl(io_addr_gpu_stall + OFFSET_STALL_GPU_M1_RD_CNT)) >> 1;

		for (i = 0; i < size; i++) {
			if(pre_stall_counters[i] > stall_counters[i]) {
				diff[i] = stall_counters[i] + (0xFFFFFFFF - pre_stall_counters[i]);
			}
			else {
				diff[i] = stall_counters[i] - pre_stall_counters[i];
			}
		}
		for(i = 0; i < size; i++) {
			pre_stall_counters[i] = stall_counters[i];
		}
	}
#endif
}

