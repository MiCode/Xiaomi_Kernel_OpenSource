#ifndef _AW8622_HAPTIC_H_
#define _AW8622_HAPTIC_H_

struct aw8622_effect_state {
	int effect_idx;
	int duration;
	int secs;
	unsigned long nsces;
	bool is_shock_stop;
};

struct waveform_data_info {
	bool is_loaded;
	const char *waveform_name;
	unsigned int waveform_period; // The time of the whole waveform unit is ms
	unsigned int sample_freq;
	unsigned int sample_nums;
	unsigned int us_time_len; //unit us	
	unsigned int len;
	unsigned char *data;
};

struct aw8622_haptic {

	/* Hardware info */
	unsigned int pwm_ch;
	struct device *dev;
	int hwen_gpio;
	struct pinctrl *ppinctrl_pwm;

	unsigned int default_pwm_freq;
	unsigned int h_l_period;


	/* Vibration waveform data field */ 
	struct delayed_work load_waveform_work;
	struct delayed_work hw_off_work;
	unsigned int wave_sample_period; //wave sample period is ns
	struct waveform_data_info *p_waveform_data;
	int waveform_data_nums;
	unsigned int wave_max_len;
	bool is_malloc_wavedata_info;
	int cur_load_idx;
	unsigned int load_idx_offset;

	bool is_malloc_dma_memory;
	dma_addr_t wave_phy;
	void *wave_vir;
	unsigned dma_len;
	

	
	spinlock_t	spin_lock;
	

	/* Vibration control field */
	bool is_actived;
	bool is_real_play;
	bool is_power_on;
	bool is_wavefrom_ready;

	bool is_hwen;
	int effect_idx;
	unsigned int duration;
	unsigned int interval;
	unsigned int center_freq;

	struct workqueue_struct *aw8622_wq;
	struct work_struct play_work;
	struct work_struct stop_play_work;
	struct work_struct test_work;
	unsigned int test_cnt;
	struct mutex mutex_lock;
	struct hrtimer timer;
	struct aw8622_effect_state effect_state;


};

#define LONG_SHOCK_BIT_NUMS_PER_SAMPLED_VALE	(80)

#define WAVEFORM_DATA_OFFSET		(12)
#define BIT_NUMS_PER_SAMPLED_VALE	(250)
#define BIT_NUMS_PER_BYTE			(8)
#define WAVEFORM_MAX_SAMPLE_VAL		(127)
#define WAVEFORM_MIN_SAMPLE_VAL		(-127)

#define MAX_NUMS_NONNEGATIVE_SIGNEC_8BIT	(128)	//The number of non-negative integers that a signed 8bit of data can represent
#define MAX_NUMS_POSITIVE_SIGNEC_8BIT		(128)
#define MAX_COUNT_SIGNEC_8BIT				(255)

#endif