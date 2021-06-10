/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 *
 */


#define ENABLE_LOG_PERF 0

static inline uint64_t read_cntvct(void)
{
	uint64_t val;

	asm volatile("mrs %0, cntvct_el0" : "=r" (val));
	return val;
}


#if ENABLE_LOG_PERF == 1

#define LOG_MEASURE_START  0x7c40407c  /* |@@| */
#define LOG_MEASURE_END    0x7c2d2d7c  /* |--| */

static unsigned int received_tz_log_chars;
static unsigned int received_tz_log_lines;
static bool tz_log_timer_started;
static uint64_t start_log_timer_counter, end_log_timer_counter;

static void reset_tz_log_counter(void)
{
	received_tz_log_chars = 0;
	received_tz_log_lines = 0;
}

static void add_tz_log_counter(unsigned int num_chars)
{
	received_tz_log_chars += num_chars;
	received_tz_log_lines++;
}

static void measure_log_perf(const char *tag, int log_len, uint32_t log_prefix)
{
	if (log_len > 0) {
		if (log_prefix == LOG_MEASURE_START) {
			if (!tz_log_timer_started) {
				start_log_timer_counter = read_cntvct();
				tz_log_timer_started = true;
				reset_tz_log_counter();
			}
			add_tz_log_counter(log_len-1);
		}

		if (log_prefix == LOG_MEASURE_END) {
			if (!tz_log_timer_started)
				IMSG_WARN("[BUG] log timer not start yet!\n");
			else {
				end_log_timer_counter = read_cntvct();
				tz_log_timer_started = false;

				IMSG_PRINTK("%s[RLOG] Recv Log (%u)/(%u)\n",
						tag, received_tz_log_chars,
						received_tz_log_lines);

				IMSG_PRINTK("%s[RLOG] (%llu)~(%llu)/(%llu)\n",
						tag, start_log_timer_counter,
						end_log_timer_counter,
			end_log_timer_counter - start_log_timer_counter);
			}
		}
	}
}
#endif
