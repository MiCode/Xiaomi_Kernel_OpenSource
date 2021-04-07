#define IO_LIMIT_MASK			1
#define IO_BUFFER_SPEED_MASK		2
#define IO_CFQ_DISPATCH_SPEED_MASK	4
#define IO_CFQ_SELECTQ_SPEED_MASK	8
#define IO_DEBUG_MASK			16

#define GROUP_TOP              11

enum throttle_type {
        BG_THROTTLE = 0,
        FG_THROTTLE,
        URGENT_THROTTLE,
        THROTTLE_TYPE_NR,
        NO_THROTTLE,
};

typedef struct throttle {
	unsigned int iotime_threshold_up;
	unsigned int iotime_threshold_down;

	unsigned int delay_up_step;
	unsigned int delay_down_step;
	unsigned int delay_max;

	unsigned int stable_time_speed_up;
	unsigned int stable_time_slow_down;

	unsigned int throttle_time;
	unsigned long stable_start_time;
	unsigned long stable_time;
	bool stabling;

	unsigned long window_low;
	unsigned long window_high;
} throttle_t;

typedef struct mi_throttle {
	bool throttle_urgent;
	bool throttle_init;
	throttle_t throttle[THROTTLE_TYPE_NR];
	spinlock_t throttle_lock;
} mi_throttle_t;


extern int task_type(struct task_struct *task);
extern unsigned long filemap_range_nr_page(struct address_space *mapping,
                           pgoff_t index, pgoff_t end);
extern unsigned int sysctl_mi_iolimit;
extern unsigned long elapsed_jiffies(unsigned long start);
extern void mi_throttle_init(mi_throttle_t *p_mi_throttle);


