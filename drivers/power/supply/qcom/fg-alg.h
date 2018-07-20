/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __FG_ALG_H__
#define __FG_ALG_H__

#define BUCKET_COUNT		8
#define BUCKET_SOC_PCT		(256 / BUCKET_COUNT)
#define MAX_CC_STEPS		20
#define MAX_TTF_SAMPLES		10

struct cycle_counter {
	void		*data;
	bool		started[BUCKET_COUNT];
	u16		count[BUCKET_COUNT];
	u8		last_soc[BUCKET_COUNT];
	char		str_buf[BUCKET_COUNT * 8];
	int		id;
	int		last_bucket;
	struct mutex	lock;
	int (*restore_count)(void *data, u16 *buf, int num_bytes);
	int (*store_count)(void *data, u16 *buf, int id, int num_bytes);
};

struct cl_params {
	int	min_start_soc;
	int	max_start_soc;
	int	max_temp;
	int	min_temp;
	int	max_cap_inc;
	int	max_cap_dec;
	int	max_cap_limit;
	int	min_cap_limit;
	int	skew_decipct;
};

struct cap_learning {
	void			*data;
	int			init_cc_soc_sw;
	int			cc_soc_max;
	int64_t			nom_cap_uah;
	int64_t			init_cap_uah;
	int64_t			final_cap_uah;
	int64_t			learned_cap_uah;
	bool			active;
	struct mutex		lock;
	struct cl_params	dt;
	int (*get_learned_capacity)(void *data, int64_t *learned_cap_uah);
	int (*store_learned_capacity)(void *data, int64_t learned_cap_uah);
	int (*get_cc_soc)(void *data, int *cc_soc_sw);
	int (*prime_cc_soc)(void *data, u32 cc_soc_sw);
};

enum ttf_mode {
	TTF_MODE_NORMAL = 0,
	TTF_MODE_QNOVO,
};

enum ttf_param {
	TTF_MSOC = 0,
	TTF_VBAT,
	TTF_IBAT,
	TTF_FCC,
	TTF_MODE,
	TTF_ITERM,
	TTF_RBATT,
	TTF_VFLOAT,
	TTF_CHG_TYPE,
	TTF_CHG_STATUS,
	TTF_VALID,
};

struct ttf_circ_buf {
	int	arr[MAX_TTF_SAMPLES];
	int	size;
	int	head;
};

struct ttf_cc_step_data {
	int arr[MAX_CC_STEPS];
	int sel;
};

struct ttf_pt {
	s32 x;
	s32 y;
};

struct ttf {
	void			*data;
	struct ttf_circ_buf	ibatt;
	struct ttf_circ_buf	vbatt;
	struct ttf_cc_step_data	cc_step;
	struct mutex		lock;
	int			mode;
	int			last_ttf;
	int			input_present;
	int			iterm_delta;
	int			period_ms;
	s64			last_ms;
	struct delayed_work	ttf_work;
	int (*get_ttf_param)(void *data, enum ttf_param, int *val);
	int (*awake_voter)(void *data, bool vote);
};

int restore_cycle_count(struct cycle_counter *counter);
void clear_cycle_count(struct cycle_counter *counter);
void cycle_count_update(struct cycle_counter *counter, int batt_soc,
		int charge_status, bool charge_done, bool input_present);
int get_cycle_count(struct cycle_counter *counter, int *count);
int get_cycle_counts(struct cycle_counter *counter, const char **buf);
int cycle_count_init(struct cycle_counter *counter);
void cap_learning_abort(struct cap_learning *cl);
void cap_learning_update(struct cap_learning *cl, int batt_temp,
		int batt_soc, int charge_status, bool charge_done,
		bool input_present, bool qnovo_en);
int cap_learning_init(struct cap_learning *cl);
int cap_learning_post_profile_init(struct cap_learning *cl,
		int64_t nom_cap_uah);
void ttf_update(struct ttf *ttf, bool input_present);
int ttf_get_time_to_empty(struct ttf *ttf, int *val);
int ttf_get_time_to_full(struct ttf *ttf, int *val);
int ttf_tte_init(struct ttf *ttf);

#endif
