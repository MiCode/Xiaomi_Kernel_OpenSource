// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include "gsp_coef_cal.h"
#include "gsp_sin_cos.h"

/*
 * we use "Least Recently Used(LRU)"
 * to implement the coef-matrix cache policy
 */

static int64_t div64_s64_s64(int64_t dividend, int64_t divisor)
{
	int8_t sign = 1;
	int64_t dividend_tmp = dividend;
	int64_t divisor_tmp = divisor;
	int64_t ret = 0;

	if (divisor == 0)
		return 0;

	if ((dividend >> 63) & 0x1) {
		sign *= -1;
		dividend_tmp = dividend * (-1);
	}
	if ((divisor >> 63) & 0x1) {
		sign *= -1;
		divisor_tmp = divisor * (-1);
	}
	ret = div64_s64(dividend_tmp, divisor_tmp);
	ret *= sign;

	return ret;
}

static int32_t sin_32(int32_t index_in)
{
	int sin_cnt = 4096 * INTERPOLATION_STEP;
	int index   = (index_in % sin_cnt) / INTERPOLATION_STEP;
	int depart  = (index_in / sin_cnt) % 4;
	int sin_value_up;
	int sin_value_down;
	int sin_value;
	int index_diff;

	index_in = index_in % sin_cnt;

	if ((depart == 1) || (depart == 3)) {
		index = 4096 - 1 - index;
		index_in = 4096 * 128 - 1 - index_in;
	}

	if (index == 4095) {
		sin_value_down = sin_table[index];
		sin_value_up = sin_value_down;
		index_diff = 0;
	} else {
		if ((depart == 1) || (depart == 3)) {
			sin_value_down = sin_table[index - 1];
			sin_value_up = sin_table[index];
			index_diff = index_in - (index - 1) * 128;
		} else {
			sin_value_down = sin_table[index];
			sin_value_up = sin_table[index + 1];
			index_diff = index_in - index * 128;
		}
	}

	sin_value = sin_value_down +
		(sin_value_up - sin_value_down) * index_diff / 128;

	if ((depart == 2) || depart == 3)
		sin_value *= -1;

	return sin_value;
}

/*
 * normalize the array
 */

static void normalize_inter_fix(int64_t *f_data, int32_t *i_data, int tap)
{
	int i;
	int64_t sum_val;

	sum_val = 0;

	for (i = 0; i < tap; i++)
		sum_val += f_data[i];

	for (i = 0; i < tap; i++)
		i_data[i] = (int32_t)div64_s64_s64(f_data[i] * 256, sum_val);
}

static void adjust_coef_inter(int32_t *coef, int tap)
{
	int32_t	i, midi, maxi;
	int32_t	tmpi, tmp_S, tmp_val;
	int32_t sum_val = 0;

	/* sum value */
	for (i = 0; i < tap; i++)
		sum_val += coef[i];

	if (sum_val != 256) {
		tmpi = sum_val - 256;
		tmp_val = 1 - 2 * (tmpi < 0);
		midi = tap >> 1;

		/* tmpi is odd */
		if ((tmpi & 1) == 1) {
			coef[midi] = coef[midi] - tmp_val;
			tmpi -= tmp_val;
		}

		tmp_S = abs(tmpi >> 1);

		/* tap is odd */
		if ((tap & 1) == 1) {
			for (i = 0; i < tmp_S; i++) {
				coef[midi - (i + 1)] =
					coef[midi - (i + 1)] - tmp_val;
				coef[midi + (i + 1)] =
					coef[midi + (i + 1)] - tmp_val;
			}
		} else {
		/* tap is even */
			for (i = 0; i < tmp_S; i++) {
				coef[midi - (i + 1)] =
					coef[midi - (i + 1)] - tmp_val;
				coef[midi + i] = coef[midi + i] - tmp_val;
			}
		}
	}

	/* find max */
	maxi = coef[0];
	midi = 0;
	for (i = 1; i < tap; i++) {
		if (coef[i] > maxi) {
			maxi = coef[i];
			midi = i;
		}
	}

	if (coef[midi] > 255) {
		coef[midi + 1] = coef[midi + 1] + coef[midi] - 255;
		coef[midi] = 255;
	}
}

/*
 * func:cache_coef_hit_check
 * desc:find the entry have the same in_w in_h out_w out_h
 * return:if hit,return the entry pointer; else return null;
 */
static struct coef_entry *gsp_r8p0_coef_cache_hit_check(
		struct gsp_r8p0_core *core,
		uint16_t in_w, uint16_t in_h,
		uint16_t out_w, uint16_t out_h,
		uint16_t hor_tap, uint16_t ver_tap)
{
	static uint32_t total_cnt = 1;
	static uint32_t hit_cnt = 1;
	struct coef_entry *pos = NULL;

	total_cnt++;
	list_for_each_entry(pos, &core->coef_list, list) {
		if (pos->in_w == in_w
		   && pos->in_h == in_h
		   && pos->out_w == out_w
		   && pos->out_h == out_h
		   && pos->hor_tap == hor_tap
		   && pos->ver_tap == ver_tap) {
			hit_cnt++;
			GSP_DEBUG("hit, hit_ratio:%d percent.\n",
				hit_cnt * 100 / total_cnt);

			return pos;
		}
	}
	GSP_DEBUG("miss.\n");

	return NULL;
}

static struct coef_entry *gsp_r9p0_coef_cache_hit_check(
		struct gsp_r9p0_core *core,
		uint16_t in_w, uint16_t in_h,
		uint16_t out_w, uint16_t out_h,
		uint16_t hor_tap, uint16_t ver_tap)
{
	static uint32_t total_cnt = 1;
	static uint32_t hit_cnt = 1;
	struct coef_entry *pos = NULL;

	total_cnt++;
	list_for_each_entry(pos, &core->coef_list, list) {
		if (pos->in_w == in_w
		   && pos->in_h == in_h
		   && pos->out_w == out_w
		   && pos->out_h == out_h
		   && pos->hor_tap == hor_tap
		   && pos->ver_tap == ver_tap) {
			hit_cnt++;
			GSP_DEBUG("hit, hit_ratio:%d percent.\n",
				hit_cnt * 100 / total_cnt);

			return pos;
		}
	}
	GSP_DEBUG("miss.\n");

	return NULL;
}

static inline void gsp_r8p0_coef_cache_move_to_head(
		struct gsp_r8p0_core *core,
		struct coef_entry *entry)
{
	list_del(&entry->list);
	list_add(&entry->list, &core->coef_list);
}

static inline void gsp_r9p0_coef_cache_move_to_head(
		struct gsp_r9p0_core *core,
		struct coef_entry *entry)
{
	list_del(&entry->list);
	list_add(&entry->list, &core->coef_list);
}

static uint8_t _InitPool(void *buffer_ptr,
		 uint32_t buffer_size,
		 struct GSC_MEM_POOL *pool_ptr)
{
	if (NULL == buffer_ptr || 0 == buffer_size || NULL == pool_ptr)
		return 0;

	if (buffer_size < MIN_POOL_SIZE)
		return 0;

	pool_ptr->begin_addr = (ulong) buffer_ptr;
	pool_ptr->total_size = buffer_size;
	pool_ptr->used_size = 0;

	return 1;
}

static void *_Allocate(uint32_t size,
		   uint32_t align_shift,
		   struct GSC_MEM_POOL *pool_ptr)
{
	ulong begin_addr = 0;
	ulong temp_addr = 0;

	if (pool_ptr == NULL) {
		GSP_ERR("GSP_Allocate:%d error!\n", __LINE__);
		return NULL;
	}
	begin_addr = pool_ptr->begin_addr;
	temp_addr = begin_addr + pool_ptr->used_size;
	temp_addr =
		(((temp_addr + (1UL << align_shift) - 1)
		>> align_shift) << align_shift);
	if (temp_addr + size > begin_addr + pool_ptr->total_size) {
		GSP_ERR("GSP_Allocate err:%d,temp_addr:0x%08x,size:%d, "
		, __LINE__, (unsigned int)temp_addr, size);
		GSP_ERR("begin_addr:0x%08x,total_size:%d,used_size:%d\n"
		, (unsigned int)begin_addr
		, (unsigned int)pool_ptr->total_size
		, (unsigned int)pool_ptr->used_size);

		return NULL;
	}
	pool_ptr->used_size = (temp_addr + size) - begin_addr;
	memset((void *)temp_addr, 0, size);

	return (void *)temp_addr;
}

static void calc_coef(int tap, int32_t (*scaler_coef)[MAX_TAP],
	int in_size, int out_size, struct GSC_MEM_POOL *pool_ptr)
{
	int fix_scl = 1 << FIX_POINT;
	int fp_Q;
	int i, j, k;

	/* scaler settings */
	int n_phase = MAX_PHASE;
	int coef_length = tap * n_phase;
	int mid_i = coef_length >> 1;
	/* -0.5 or -0.75 */
	int a = (int)(-0.5 * fix_scl);

	int64_t absx, absx2, absx3;
	int N, M;

	enum scale_kernel_type kernel_type;
	enum scale_win_type win_type;

	int64_t *coef =
		_Allocate(MAX_COEF_LEN * sizeof(int64_t), 3, pool_ptr);

	int64_t *tmp_coef =
		_Allocate(MAX_TAP * sizeof(int64_t), 3, pool_ptr);

	int32_t *normalized_coef =
		_Allocate(MAX_TAP * sizeof(int32_t), 2, pool_ptr);

	if (in_size < out_size) {
		kernel_type = GSP_SCL_TYPE_BI_CUBIC;
		win_type = GSP_SCL_WIN_RECT;
		fp_Q = 0;
		N = M = 1;
	} else {
#ifdef SIN_SCL_EN
		kernel_type = GSP_SCL_TYPE_SINC;
		win_type = GSP_SCL_WIN_SINC;
		fp_Q = 10;
		N = out_size;
		M = in_size;
#else
		kernel_type = GSP_SCL_TYPE_BI_CUBIC;
		win_type = GSP_SCL_WIN_RECT;
		fp_Q = 0;
		N = M = 1;
#endif /* SIN_SCL_EN */
	}

	/* sinc kernel, for down scaling */
	if (kernel_type == GSP_SCL_TYPE_SINC) {
		coef[mid_i] =
			sin_32((int)((int64_t) 4096 * 256 / n_phase * N / M));

		for (i = 0; i < mid_i; i++) {
			coef[mid_i + i + 1] = (sin_32((int)((int64_t)
				(i + 1) * 4096 * 256 / n_phase * N / M))
				*((int64_t)1 << fp_Q) / (i + 1)) >> fp_Q;
			coef[mid_i - (i + 1)] = coef[mid_i + i + 1];
		}
	} else if (kernel_type == GSP_SCL_TYPE_BI_CUBIC) {
		/* bi-cubic kernel, for up scaling */
		N = N << fp_Q;

		coef[mid_i] = 1 << (FIX_POINT * 4);

		for (i = 0; i < mid_i; i++) {
			/* 4*1 + fp_Q*1 */
			absx = (i + 1) * N / M;
			/* 4*2 + fp_Q*2 */
			absx2 = absx * absx;
			/* 4*3 + fp_Q*3 */
			absx3 = absx2 * absx;

			if (absx <= (fix_scl << fp_Q))
				coef[mid_i + i + 1] = ((a + 2 * fix_scl) * absx3
				- (((a + 3 * fix_scl) * absx2)
				<< (FIX_POINT + fp_Q))
				+ ((int64_t)1 << (FIX_POINT * 4 + 3 * fp_Q)))
				>> (3 * fp_Q);
			else if ((absx <= ((2 * fix_scl) << fp_Q)))
				coef[mid_i + i + 1] = (a * absx3
				- ((5 * a * absx2) << (FIX_POINT + fp_Q))
				+ ((8 * a * absx) << (FIX_POINT * 2 + 2 * fp_Q))
				- ((int64_t)(4 * a)
				<< (FIX_POINT * 3 + 3 * fp_Q)))
				>> (3 * fp_Q);
			else
				coef[mid_i + i + 1] = 0;

			coef[mid_i - (i + 1)] = coef[mid_i + i + 1];
		}
	}

	/* hamming window */
	if (win_type == GSP_SCL_WIN_SINC) {
		for (i = -1; i < mid_i; i++) {
			if (-1 == i)
				coef[mid_i + i + 1] *= (sin_32((int)
				((int64_t)4096 * 256
				* out_size / in_size / mid_i))
				* mid_i);
			else
				coef[mid_i + i + 1] *= (sin_32((int)((int64_t)
				(i + 1) * 4096 * 256
				* out_size / in_size / mid_i))
				* ((int64_t)mid_i << fp_Q) / (i + 1)) >> fp_Q;

			coef[mid_i - (i + 1)] = coef[mid_i + i + 1];
		}
	}

	for (i = 0; i < n_phase; i++) {
		k = 0;
		for (j = i + 1; j < coef_length + 1; j += n_phase)
			tmp_coef[k++] = coef[j];

		normalize_inter_fix(tmp_coef, normalized_coef, tap);
		adjust_coef_inter(normalized_coef, tap);

		for (k = 0; k < tap; k++)
			scaler_coef[n_phase - i - 1][k] = normalized_coef[k];
	}
}

uint32_t *gsp_r8p0_gen_block_scaler_coef(struct gsp_r8p0_core *core,
				 uint32_t in_sz_x,
				 uint32_t in_sz_y,
				 uint32_t ouf_sz_x,
				 uint32_t ouf_sz_y,
				 uint32_t hor_tap,
				 uint32_t ver_tap)
{
	struct GSC_MEM_POOL pool = { 0 };
	struct coef_entry *entry = NULL;
	int32_t icnt = 0;
	int32_t jcnt = 0;
	int32_t (*coeff_array_hor)[MAX_TAP] = NULL;
	int32_t (*coeff_array_ver)[MAX_TAP] = NULL;
	int32_t (*scaling_reg_buf_hor)[MAX_TAP / 2] = NULL;
	int32_t (*scaling_reg_buf_ver)[MAX_TAP / 2] = NULL;

	if (core->cache_coef_init_flag == 1) {
		entry = gsp_r8p0_coef_cache_hit_check(core,
			in_sz_x, in_sz_y, ouf_sz_x, ouf_sz_y, hor_tap, ver_tap);
		if (entry) {
			gsp_r8p0_coef_cache_move_to_head(core, entry);
			return entry->coef;
		}
	}

	/* init pool and allocate static array */
	if (!_InitPool(core->coef_buf_pool, MIN_POOL_SIZE, &pool)) {
		GSP_ERR("GSP_Gen_Block_Ccaler_Coef: _InitPool error!\n");
		return NULL;
	}

	coeff_array_hor = (int32_t (*)[MAX_TAP])
		_Allocate((MAX_PHASE * MAX_TAP * sizeof(int32_t)), 2, &pool);
	coeff_array_ver = (int32_t (*)[MAX_TAP])
		_Allocate((MAX_PHASE * MAX_TAP * sizeof(int32_t)), 2, &pool);

	calc_coef(8 - 2 * hor_tap, coeff_array_hor, in_sz_x, ouf_sz_x, &pool);
	calc_coef(8 - 2 * ver_tap, coeff_array_ver, in_sz_y, ouf_sz_y, &pool);

	scaling_reg_buf_hor = (int32_t (*)[MAX_TAP / 2])_Allocate
		((MAX_PHASE * MAX_TAP / 2 * sizeof(int32_t)), 2, &pool);
	scaling_reg_buf_ver = (int32_t (*)[MAX_TAP / 2])_Allocate
		((MAX_PHASE * MAX_TAP / 2 * sizeof(int32_t)), 2, &pool);

	for (icnt = 0; icnt < MAX_PHASE; icnt++)
		for (jcnt = 0; jcnt < MAX_TAP; jcnt += 2)
			scaling_reg_buf_hor[icnt][jcnt / 2] =
			(coeff_array_hor[icnt][jcnt] & 0xFFFF) |
			((coeff_array_hor[icnt][jcnt + 1] & 0xFFFF) << 16);

	for (icnt = 0; icnt < MAX_PHASE; icnt++)
		for (jcnt = 0; jcnt < MAX_TAP; jcnt += 2)
			scaling_reg_buf_ver[icnt][jcnt / 2] =
			(coeff_array_ver[icnt][jcnt] & 0xFFFF) |
			((coeff_array_ver[icnt][jcnt + 1] & 0xFFFF) << 16);

	if (core->cache_coef_init_flag == 1) {
		entry = list_entry(core->coef_list.prev,
				struct coef_entry, list);
		if (entry->in_w == 0)
			GSP_DEBUG("add.\n");
		else
			GSP_DEBUG("swap.\n");

		if ((ulong)scaling_reg_buf_hor & MEM_OPS_ADDR_ALIGN_MASK
			|| (ulong)&entry->coef[0] & MEM_OPS_ADDR_ALIGN_MASK) {
			GSP_DEBUG("memcpy use none 8B alignment address.");
		}
		memcpy((void *)&entry->coef[0], (void *)scaling_reg_buf_hor,
				MAX_PHASE * MAX_TAP * 4 / 2);

		if ((ulong)scaling_reg_buf_ver & MEM_OPS_ADDR_ALIGN_MASK
			|| (ulong)&entry->coef[64] & MEM_OPS_ADDR_ALIGN_MASK) {
			GSP_DEBUG("memcpy use none 8B alignment address.");
		}
		memcpy((void *)&entry->coef[64], (void *)scaling_reg_buf_ver,
			MAX_PHASE * MAX_TAP * 4 / 2);
		gsp_r8p0_coef_cache_move_to_head(core, entry);

		LIST_SET_ENTRY_KEY(entry, in_sz_x, in_sz_y, ouf_sz_x, ouf_sz_y,
			hor_tap, ver_tap);
	}

	return entry->coef;
}

uint32_t *gsp_r9p0_gen_block_scaler_coef(struct gsp_r9p0_core *core,
				 uint32_t in_sz_x,
				 uint32_t in_sz_y,
				 uint32_t ouf_sz_x,
				 uint32_t ouf_sz_y,
				 uint32_t hor_tap,
				 uint32_t ver_tap)
{
	struct GSC_MEM_POOL pool = { 0 };
	struct coef_entry *entry = NULL;
	int32_t icnt = 0;
	int32_t jcnt = 0;
	int32_t (*coeff_array_hor)[MAX_TAP] = NULL;
	int32_t (*coeff_array_ver)[MAX_TAP] = NULL;
	int32_t (*scaling_reg_buf_hor)[MAX_TAP / 2] = NULL;
	int32_t (*scaling_reg_buf_ver)[MAX_TAP / 2] = NULL;

	if (core->cache_coef_init_flag == 1) {
		entry = gsp_r9p0_coef_cache_hit_check(core,
			in_sz_x, in_sz_y, ouf_sz_x, ouf_sz_y, hor_tap, ver_tap);
		if (entry) {
			gsp_r9p0_coef_cache_move_to_head(core, entry);
			return entry->coef;
		}
	}

	/* init pool and allocate static array */
	if (!_InitPool(core->coef_buf_pool, MIN_POOL_SIZE, &pool)) {
		GSP_ERR("GSP_Gen_Block_Ccaler_Coef: _InitPool error!\n");
		return NULL;
	}

	coeff_array_hor = (int32_t (*)[MAX_TAP])
		_Allocate((MAX_PHASE * MAX_TAP * sizeof(int32_t)), 2, &pool);
	coeff_array_ver = (int32_t (*)[MAX_TAP])
		_Allocate((MAX_PHASE * MAX_TAP * sizeof(int32_t)), 2, &pool);

	calc_coef(8 - 2 * hor_tap, coeff_array_hor, in_sz_x, ouf_sz_x, &pool);
	calc_coef(8 - 2 * ver_tap, coeff_array_ver, in_sz_y, ouf_sz_y, &pool);

	scaling_reg_buf_hor = (int32_t (*)[MAX_TAP / 2])_Allocate
		((MAX_PHASE * MAX_TAP / 2 * sizeof(int32_t)), 2, &pool);
	scaling_reg_buf_ver = (int32_t (*)[MAX_TAP / 2])_Allocate
		((MAX_PHASE * MAX_TAP / 2 * sizeof(int32_t)), 2, &pool);

	for (icnt = 0; icnt < MAX_PHASE; icnt++)
		for (jcnt = 0; jcnt < MAX_TAP; jcnt += 2)
			scaling_reg_buf_hor[icnt][jcnt / 2] =
			(coeff_array_hor[icnt][jcnt] & 0xFFFF) |
			((coeff_array_hor[icnt][jcnt + 1] & 0xFFFF) << 16);

	for (icnt = 0; icnt < MAX_PHASE; icnt++)
		for (jcnt = 0; jcnt < MAX_TAP; jcnt += 2)
			scaling_reg_buf_ver[icnt][jcnt / 2] =
			(coeff_array_ver[icnt][jcnt] & 0xFFFF) |
			((coeff_array_ver[icnt][jcnt + 1] & 0xFFFF) << 16);

	if (core->cache_coef_init_flag == 1) {
		entry = list_entry(core->coef_list.prev,
				struct coef_entry, list);
		if (entry->in_w == 0)
			GSP_DEBUG("add.\n");
		else
			GSP_DEBUG("swap.\n");

		if ((ulong)scaling_reg_buf_hor & MEM_OPS_ADDR_ALIGN_MASK
			|| (ulong)&entry->coef[0] & MEM_OPS_ADDR_ALIGN_MASK) {
			GSP_DEBUG("memcpy use none 8B alignment address.");
		}
		memcpy((void *)&entry->coef[0], (void *)scaling_reg_buf_hor,
				MAX_PHASE * MAX_TAP * 4 / 2);

		if ((ulong)scaling_reg_buf_ver & MEM_OPS_ADDR_ALIGN_MASK
			|| (ulong)&entry->coef[64] & MEM_OPS_ADDR_ALIGN_MASK) {
			GSP_DEBUG("memcpy use none 8B alignment address.");
		}
		memcpy((void *)&entry->coef[64], (void *)scaling_reg_buf_ver,
			MAX_PHASE * MAX_TAP * 4 / 2);
		gsp_r9p0_coef_cache_move_to_head(core, entry);

		LIST_SET_ENTRY_KEY(entry, in_sz_x, in_sz_y, ouf_sz_x, ouf_sz_y,
			hor_tap, ver_tap);
	}

	return entry->coef;
}

