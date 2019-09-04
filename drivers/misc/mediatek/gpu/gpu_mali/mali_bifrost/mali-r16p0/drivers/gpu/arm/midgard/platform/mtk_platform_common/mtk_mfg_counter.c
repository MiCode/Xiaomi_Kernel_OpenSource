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
static GPU_PMU *mali_pmus;
static int name_offset_table[MALI_HWC_TYPES];
static int name_index_table[MALI_HWC_TYPES];
static int perf_index_table[PERF_COUNTER_LAST];

static int mfg_is_power_on;
static int binited;
static uint32_t gpu_active;
static uint32_t active_cycle;
static uint32_t accmu;

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

static uint32_t _read_u_rate(int u_rate_numerator, int u_rate_denominator)
{
	uint32_t numerator_active, denominator_active, urate;
	numerator_active = denominator_active = urate = 0;
	if (!mali_pmus)
		return 0;

	numerator_active = mali_pmus[u_rate_numerator].value;
	denominator_active = mali_pmus[u_rate_denominator].value;
	if (denominator_active) {
		urate = _cal_urate(numerator_active, denominator_active);
	}

	return urate;
}

static uint32_t _read_var_u_rate(int u_rate_numerator, int u_rate_denominator)
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


static uint32_t _read_u_rate_w_loading(int u_rate_numerator, int u_rate_denominator)
{
	uint32_t numerator =0;

	if (!mali_pmus)
		return 0;

	numerator = mali_pmus[u_rate_numerator].value;

	return (!numerator || !active_cycle) ? 1 : _cal_urate(numerator, active_cycle);
}

static uint32_t _read_var_u_rate_w_loading(int u_rate_numerator, int u_rate_denominator)
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


/*
add more perf counter:
1.add counter in enum PERF_COUNTER_LAST
2.add counter in struct mfg_mtk_perf_index
*/
static struct {
	const char *name;
	const int index;
}mfg_mtk_perf_index[] = {
	{"GPU_ACTIVE", GPU_ACTIVE},
	{"EXEC_INSTR_COUNT", EXEC_INSTR_COUNT},
	{"EXEC_CORE_ACTIVE", EXEC_CORE_ACTIVE},
	{"EXEC_ACTIVE", EXEC_ACTIVE},
	{"FRAG_ACTIVE", FRAG_ACTIVE},
	{"TILER_ACTIVE", TILER_ACTIVE},
	{"TEX_FILT_NUM_OPERATIONS", TEX_FILT_NUM_OPERATIONS},
	{"LS_MEM_READ_FULL", LS_MEM_READ_FULL},
	{"LS_MEM_WRITE_FULL", LS_MEM_WRITE_FULL},
	{"LS_MEM_READ_SHORT", LS_MEM_READ_SHORT},
	{"LS_MEM_WRITE_SHORT", LS_MEM_WRITE_SHORT},
	{"L2_EXT_WRITE_BEATS", L2_EXT_WRITE_BEATS},
	{"L2_EXT_READ_BEATS", L2_EXT_READ_BEATS},
	{"L2_EXT_RRESP_0_127", L2_EXT_RRESP_0_127},
	{"L2_EXT_RRESP_128_191", L2_EXT_RRESP_128_191},
	{"L2_EXT_RRESP_192_255", L2_EXT_RRESP_192_255},
	{"L2_EXT_RRESP_256_319", L2_EXT_RRESP_256_319},
	{"L2_EXT_RRESP_320_383", L2_EXT_RRESP_320_383},
	{"L2_ANY_LOOKUP", L2_ANY_LOOKUP},
};
#define MFG_MTK_PERF_COUNTER_SIZE (ARRAY_SIZE(mfg_mtk_perf_index))

typedef uint32_t (*mfg_read_pfn)(int u_rate_numerator, int u_rate_denominator);
static struct {
	const char *name;
	int u_rate_numerator;
	int u_rate_denominator;
	mfg_read_pfn read;
} mfg_mtk_counters[] = {
	{"MTK_SHADER_U_RATE", EXEC_INSTR_COUNT, GPU_ACTIVE, _read_u_rate},
	{"MTK_ALU_U_RATE", EXEC_INSTR_COUNT, EXEC_ACTIVE, _read_u_rate},
	{"MTK_TEX_U_RATE", TEX_FILT_NUM_OPERATIONS, EXEC_ACTIVE, _read_u_rate},
	{"MTK_LSC_U_RATE", L2_ANY_LOOKUP, EXEC_ACTIVE, _read_u_rate},
	{"MTK_VAR_U_RATE", 0, 0, _read_var_u_rate},
	{"MTK_SHADER_U_RATE_W_LOADING", EXEC_CORE_ACTIVE, 0, _read_u_rate_w_loading},
	{"MTK_ALU_U_RATE_W_LOADING", EXEC_INSTR_COUNT, 0, _read_u_rate_w_loading},
	{"MTK_TEX_U_RATE_W_LOADING", TEX_FILT_NUM_OPERATIONS, 0, _read_u_rate_w_loading},
	{"MTK_LSC_U_RATE_W_LOADING", L2_ANY_LOOKUP, 0, _read_u_rate_w_loading},
	{"MTK_VAR_U_RATE_W_LOADING", 0, 0, _read_var_u_rate_w_loading},
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
	int tmp = map/4;
	int mask;
	mask = (info.bitmask[block_type]>>tmp) && 0x1;

	if(mask == 0)
		return 1;	//this couter disable

	return 0;
}



static void _mtk_mfg_init_counter(void)
{
	int empty_hwc_cnt, i, cnt;

	empty_hwc_cnt = 0;

	//get counter name and number of counter
	hardware_counter_names = kbase_gator_hwcnt_init_names(&cnt);
	if ((hardware_counter_names == NULL) || (cnt <= 0))
		return;

	// Default doesn't enable all HWC
	info.bitmask[0] = 0x57; 	//JM
	info.bitmask[1] = 0x2; 		//tiler
	info.bitmask[2] = 0xffff;	//shader
	info.bitmask[3] = 0x1fc0; 	//L2 & MMU
	handle = kbase_gator_hwcnt_init(&info);
	if (!handle) {
		pr_info("[GPU PMU]Error init hwcnt\n");
		return;
	}
	for (i = 0; i < cnt; i++) {
		const char *name = hardware_counter_names[i];

		if (name[0] == '\0')
			empty_hwc_cnt++;
	}
	if (!binited){
		number_of_hardware_counters = cnt - empty_hwc_cnt + MFG_MTK_COUNTER_SIZE;
		mali_pmus = kcalloc(number_of_hardware_counters, sizeof(GPU_PMU), GFP_KERNEL);
		if (!mali_pmus) {
			pr_info("[GPU PMU] fail to allocate mali_pmus\n");
			return;
		}
		name_offset_table[JM_BLOCK] = JM_BLOCK_NAME_POS;
		name_offset_table[TILER_BLOCK] = TILER_BLOCK_NAME_POS;
		name_offset_table[SHADER_BLOCK] = SHADER_BLOCK_NAME_POS;
		name_offset_table[MMU_L2_BLOCK] = MMU_BLOCK_NAME_POS;
		mfg_is_power_on = 0;
		binited = 1;

		// Dump PMU info
		pr_debug("[GPU PMU]bitmask[0] %x\n", info.bitmask[0]);
		pr_debug("[GPU PMU]bitmask[1] %x\n", info.bitmask[1]);
		pr_debug("[GPU PMU]bitmask[2] %x\n", info.bitmask[2]);
		pr_debug("[GPU PMU]bitmask[3] %x\n", info.bitmask[3]);
		pr_debug("[GPU PMU]gpu_id %d\n", info.gpu_id);
		pr_debug("[GPU PMU]nr_cores %d\n", info.nr_cores);
		pr_debug("[GPU PMU]nr_core_groups %d\n", info.nr_core_groups);
		pr_debug("[GPU PMU]nr_hwc_blocks %d\n", info.nr_hwc_blocks);
		for (i = 0; i < info.nr_hwc_blocks; i++)
			pr_debug("[GPU PMU]hwc_layout[%d] %d\n", i, info.hwc_layout[i]);

		pr_debug("[GPU PMU]num_of_counter %d\n", number_of_hardware_counters);
	}
	kbase_gator_instr_hwcnt_dump_irq(handle);
}


static int _mtk_mfg_update_counter(void)
{
	uint32_t success, ret, status, gpu_freq;
	static struct timeval tv_start, tv_end;
	static unsigned long long start_utime, end_utime, timd_diff_us;

	ret = timd_diff_us = gpu_freq = success = status = active_cycle = 0;
	status = kbase_gator_instr_hwcnt_dump_complete(handle, &success);
	//because irq is delay, no irq and bypass this point
	if (!status || !success){
		ret = PMU_NG;
		accmu++;
		return ret;
	}

	if (ret != PMU_NG) {
		u32 *hwcnt_data = (u32 *)info.kernel_dump_buffer;
		int i, j, cnt, block_type, name_ret;
		int name_offset, data_offset, nr_hwc_blocks;	//index data
		cnt = 0;
		do_gettimeofday(&tv_end);
		end_utime = tv_end.tv_sec * 1000000 + tv_end.tv_usec;
		timd_diff_us = (end_utime > start_utime) ? (end_utime - start_utime) : 0;
		gpu_freq = mt_gpufreq_get_cur_freq();
		nr_hwc_blocks = info.nr_hwc_blocks - info.nr_cores + 1;
		_mtk_mfg_reset_counter(1);
		name_ret = 0;
		//dump HW counter
		for (i = 0; i < nr_hwc_blocks; i++) {
			block_type = info.hwc_layout[i];
			if (block_type == RESERVED_BLOCK || info.bitmask[block_type] == 0)
				continue;
			cnt = name_index_table[block_type];
			name_offset = name_offset_table[block_type] * MALI_COUNTERS_PER_BLOCK;
			data_offset = i * MALI_COUNTERS_PER_BLOCK;
			for (j = 0; j < MALI_COUNTERS_PER_BLOCK; j++) {
				const char *name = hardware_counter_names[name_offset+j];
				if (name[0] == '\0')
					continue;
				else if(_mtk_check_map(block_type, j)){
					cnt++;
					continue;
				}

				mali_pmus[cnt].id = cnt;
				mali_pmus[cnt].overflow = 0;
				mali_pmus[cnt].value += hwcnt_data[data_offset + j];

				//gpu active=0, all counters=0
				if (strstr(mali_pmus[cnt].name, "GPU_ACTIVE")) {
					active_cycle = (uint32_t)div_u64((u64)(gpu_freq*timd_diff_us), 1000);
					gpu_active = mali_pmus[cnt].value;
					if (mali_pmus[cnt].value == 0){
						ret = PMU_RESET_VALUE;
						goto FINISH;
					}
				}

				//if special case, bypass
				if(strstr(mali_pmus[cnt].name, "TEX_FILT_NUM_OPERATIONS") ||
				    strstr(mali_pmus[cnt].name, "EXEC_INSTR_COUNT")){
					if(mali_pmus[cnt].value > gpu_active){
						ret = PMU_RESET_VALUE;
						goto FINISH;
					}
				}
				//DEBUG
				if (mali_pmus[cnt].name && (strstr(mali_pmus[cnt].name, "GPU_ACTIVE")
											|| strstr(mali_pmus[cnt].name, "EXEC_ACTIVE")
											|| strstr(mali_pmus[cnt].name, "FRAG_ACTIVE")
											|| strstr(mali_pmus[cnt].name, "TEX_FILT_NUM_OPERATIONS"))
				   )
					pr_debug("[PMU]id %d name %s value %d time %llu gpu_freq %d active_cycle %d\n",
					cnt, mali_pmus[cnt].name, mali_pmus[cnt].value, timd_diff_us,
					gpu_freq, active_cycle);
				cnt++;
			}
		}

		//dump u-rate
		for (i = 0; i < MFG_MTK_COUNTER_SIZE; i++) {
			int numerator, denominator;
			mali_pmus[cnt].id = cnt;
			if (mfg_mtk_counters[i].read) {
				numerator = perf_index_table[mfg_mtk_counters[i].u_rate_numerator];
				denominator = perf_index_table[mfg_mtk_counters[i].u_rate_denominator];
				mali_pmus[cnt].value = mfg_mtk_counters[i].read(numerator, denominator);
				if (mali_pmus[cnt].value == 0) {
					// We get incorrect value, we should pass this round capture
					mali_pmus[cnt].overflow = 0;
					ret = PMU_RESET_VALUE;
					goto FINISH;
				}
			}
			cnt++;
		}

	accmu = 1;
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

static int mali_get_gpu_pmu_init(GPU_PMU *pmus, int pmu_size, int *ret_size)
{
	int ret = PMU_OK;

	if (!binited)
		_mtk_mfg_init_counter();
	else{	//if met doesn't call stop function, we need to release first if init again
		kbase_gator_hwcnt_term(&info, handle);
		kbase_gator_hwcnt_term_names();
		_mtk_mfg_init_counter();
	}

	if (pmus) {
		int i, j, cnt, block_type;
		int nr_hwc_blocks, name_offset, data_offset;
		int mm_block=0;
		mutex_lock(&counter_info_lock);

		cnt = block_type = 0;
		nr_hwc_blocks = info.nr_hwc_blocks - info.nr_cores + 1;
		for (i = 0; i < nr_hwc_blocks; i++) {
			int first_flag=0;
			block_type = info.hwc_layout[i];
			if (block_type == RESERVED_BLOCK)
				continue;
			if (mm_block == 1 && block_type==MMU_L2_BLOCK)	//ignore same MM block
				continue;
			if (block_type == MMU_L2_BLOCK)
				mm_block = 1;
			name_offset = name_offset_table[block_type] * MALI_COUNTERS_PER_BLOCK;
			data_offset = i * MALI_COUNTERS_PER_BLOCK;
			for (j = 0; j < MALI_COUNTERS_PER_BLOCK; j++) {
				const char *name = hardware_counter_names[name_offset+j];
				if (name[0] == '\0')
					continue;
				if(first_flag==0){
					name_index_table[block_type] = cnt;
					first_flag = 1;
				}
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

		//get index of perf counter
		for(i = 0; i < MFG_MTK_PERF_COUNTER_SIZE; i++){
			int ret;
			int cnt = 0;
			ret = _find_name_pos(mfg_mtk_perf_index[i].name, &cnt);
			if(ret == -1){
				pr_info("[PMU] index fail:%s", mfg_mtk_perf_index[i].name);
			}
			else{
				perf_index_table[mfg_mtk_perf_index[i].index] = cnt;
			}
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

static int mali_get_gpu_pmu_swapnreset(GPU_PMU *pmus, int pmu_size)
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

void mali_gpu_pmu_stop(void)
{
	mutex_lock(&counter_info_lock);

	kbase_gator_hwcnt_term(&info, handle);
	kbase_gator_hwcnt_term_names();
	binited = 0;

	mutex_unlock(&counter_info_lock);
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

int mtk_mfg_update_counter(void)
{
	return	_mtk_mfg_update_counter();
}


int find_name_pos(const char *name, int *pos)
{
	return _find_name_pos(name, pos);
}


int get_mali_pmu_counter(int i)
{
	if(mali_pmus[i].value < 0)
		return 0;

	return mali_pmus[i].value;
}

// init but don't enable met
int gator_gpu_pmu_init()
{
	int ret = PMU_OK;
	int i, j, cnt, block_type;
	int nr_hwc_blocks, name_offset, data_offset;

	if (!binited){
		_mtk_mfg_init_counter();
	}
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
				mali_pmus[cnt].id = cnt;
				mali_pmus[cnt].name = name;
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
