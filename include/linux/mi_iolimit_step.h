enum throttle_type {
	BG_THROTTLE = 0,
	FG_THROTTLE,
	THROTTLE_TYPE_NR,
	NO_THROTTLE,
};

/* stime is time when the driver performs one IO
 * Start to limit step by step when the IO bandwidth of the process exceeds
 * right line of stime window, Every time step up need to wait for a while
 * to stabilize.
 *
 * Start lifting restrictions step by step When the IO bandwidth of the process is
 * less than right line of stime window, Reduce delay step by step, Same as above
 * Every time down step need to wait for a while to stabilize.
 *
 * delay time value
 * |
 * |
 * |           __
 * |        __
 * |      __
 * |    __
 * |  __
 * |__
 * |______________________time
 *
 */
typedef struct throttle {
	unsigned int iotime_threshold_up; /* right line of stime window */
	unsigned int iotime_threshold_down; /* left line of stime window */

	unsigned int delay_up_step; /* increase delay time to reduce IO bandwidth */
	unsigned int delay_down_step; /* reduce delay time to increase IO bandwidth */
	unsigned int delay_max; /* maximum delay value */

	unsigned int stable_time_speed_up; /* stability time when bandwidth is increased */
	unsigned int stable_time_slow_down; /* stability time with reduced bandwidth */

	int throttle_time; /* delay time value */
	unsigned long stable_start_time; /* start value of stable stage */
	unsigned long stable_time;
	bool stabling; /* indicates in stable phase */

	unsigned long window_low; /* left line of bandwidth window */
	unsigned long window_high; /* right line of bandwidth window */
} throttle_t;

typedef struct mi_throttle {
	bool throttle_init;
	throttle_t throttle[THROTTLE_TYPE_NR];
	spinlock_t throttle_lock;
} mi_throttle_t;


extern unsigned int sysctl_mi_iolimit;
extern unsigned long elapsed_jiffies(unsigned long start);
extern void mi_throttle_init(mi_throttle_t *p_mi_throttle);


