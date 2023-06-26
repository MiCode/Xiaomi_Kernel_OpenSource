#ifndef __AW87XXX_MONITOR_H__
#define __AW87XXX_MONITOR_H__

/********************************************
*
* vmax of matching about capacity
*
*********************************************/

#define AW87XXX_MONITOR_DEFAULT_FLAG		0
#define AW87XXX_MONITOR_DEFAULT_TIMER_VAL	30000
#define AW87XXX_MONITOR_DEFAULT_TIMER_COUNT	2

#define AW87XXX_VBAT_CAPACITY_MIN	0
#define AW87XXX_VBAT_CAPACITY_MAX	100
#define AW_VMAX_INIT_VAL		(0xFFFFFFFF)


enum aw_monitor_first_enter {
	AW_FIRST_ENTRY = 0,
	AW_NOT_FIRST_ENTRY = 1,
};

struct vmax_single_config {
	uint32_t min_thr;
	uint32_t vmax;
};

struct vmax_config {
	int vmax_cfg_num;
	struct vmax_single_config vmax_cfg_total[];
};

struct aw87xxx_monitor {
	uint8_t first_entry;
	uint8_t timer_cnt;
	uint8_t cfg_update_flag;
	uint8_t update_num;
	uint32_t monitor_flag;
	uint32_t timer_cnt_max;
	uint32_t timer_val;
	uint32_t vbat_sum;
	uint32_t custom_capacity;
	uint32_t pre_vmax;

	struct delayed_work work;
	struct vmax_config *vmax_cfg;
};

/**********************************************************
 * aw87xxx monitor function
***********************************************************/
void aw87xxx_monitor_start(struct aw87xxx_monitor *monitor);
void aw87xxx_monitor_stop(struct aw87xxx_monitor *monitor);
void aw87xxx_monitor_init(struct aw87xxx_monitor *monitor);
void aw87xxx_parse_monitor_dt(struct aw87xxx_monitor *monitor);


#endif
