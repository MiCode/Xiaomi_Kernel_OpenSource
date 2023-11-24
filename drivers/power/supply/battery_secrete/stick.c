// SPDX-License-Identifier: GPL-2.0-only
/*
 * STICK ST1Wire driver
 * Copyright (C) 2020 ST Microelectronics S.A.
 */
#define DEBUG
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/gpio/consumer.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>		/* kfree() */
#include <linux/err.h>
#include <linux/io.h>
#include <linux/compat.h>

#include "stickapi/stick_types.h"
#include "battery_auth_class.h"

extern int stickapitest_init(int testnr);

#define DRIVER_VERSION "0.0.4.11"
#define STICK_ID_NAME "stick"
#define STICK_NODE_NAME "stick"

// set to false to remove the warnings from default logs.
// if debug is enabled, the warnigns will always be displayed.
#define WARNINGS_DISPLAYED_DEFAULT true

// test for aDSP ?  1. no IO def in DTS (remove DTS fully?). 2. use low-level iomap. 3. no misc device created
//#define TEST_ADSP 1

// Define the following values if the current version of kernel is at least this one
#define KERNEL_54_PLUS 1

#define OUTPUT_HIGH             (0x1 << 1)
#define OUTPUT_LOW              (0x0 << 1)


// Use timestamps to improve read_byte performance ?
#define READ_BYTE_USE_TIMESTAMPS 1

// Verbose messages for debug timestamps in stick_read_byte?
// #define READ_BYTE_USE_TIMESTAMPS_VERBOSE 1

// Use calibration to call GPIO read only at target time to try avoiding the transitions ?
//#define TRY_CALIBRATION 1

// for read bit, how often to sample GPIO state at maximum (in ns) ?
#define SAMPLING_PERIOD_LIMIT_NS 1100

// We have a real open drain ? (only 2 states, input or out low)
#define USE_OPEN_DRAIN 1

/* N17 code for HQ-306004 by liunianliang at 2023/07/05 start */
#define NO_BENCHMARK
#define TWEAK_HL_PRE 1256
#define TWEAK_HL_POST 837
#define TWEAK_LH_PRE 757
#define TWEAK_LH_POST 504
/* N17 code for HQ-306004 by liunianliang at 2023/07/05 end */

static bool enable_debug_log = true;
static bool enable_warnings_log = WARNINGS_DISPLAYED_DEFAULT;

static struct stick_device *g_sd;

#define GPIO_LOW 0
#define GPIO_HIGH 1

#define STICK_READ    gpiod_get_raw_value(g_sd->gpiod)

#define STICK_1WIRE_SET_IN           gpiod_direction_input(g_sd->gpiod)	//set gpio as input mode
#define STICK_1WIRE_SET_OUT_LOW      gpiod_direction_output(g_sd->gpiod, GPIO_LOW)	//set gpio to ooutput mode, and value is 0

#if 0
#define STICK_1WIRE_SET_OUT_HIGH     gpiod_direction_output(g_sd->gpiod, GPIO_HIGH)	//set gpio to output mode, and value is 1
#define STICK_1WIRE_SET_HIGH         gpiod_set_value(g_sd->gpiod, GPIO_HIGH);	//set gpio value to 1
#define STICK_1WIRE_SET_LOW          gpiod_set_value(g_sd->gpiod, GPIO_LOW);	//set gpio value to 0
#else
#define STICK_1WIRE_SET_OUT_HIGH STICK_1WIRE_SET_IN
#define STICK_1WIRE_SET_HIGH STICK_1WIRE_SET_OUT_HIGH
#define STICK_1WIRE_SET_LOW STICK_1WIRE_SET_OUT_LOW
#endif

#define DEV_DBG if (enable_debug_log) dev_dbg
#define DEV_INFO dev_info
#define DEV_WARN if (enable_warnings_log) dev_warn
#define DEV_ERR dev_err

/* STWire Status report definitions */
#define ST1WIRE_RECEIVE_TIMEOUT_LOW -5
#define ST1WIRE_RECEIVE_TIMEOUT_HIGH -4
#define ST1WIRE_ARBITRATION_LOST -3
#define ST1WIRE_RECEIVE_TIMEOUT -2
#define ST1WIRE_ACK_ERROR -1
#define ST1WIRE_OK 0

/* ---------  STWire timmings in us -------- */
#define IDLE 100
#define RECEIVE_TIMEOUT 10000
#define SLOW_INTERFRAME_DELAY 1000

#define SLOW_LONG_PULSE 14
#define SLOW_SHORT_PULSE 4
#define SLOW_WAIT_ACK 4
#define SLOW_ACK_PULSE 14
#define SLOW_START_PULSE 4 * (SLOW_LONG_PULSE + SLOW_SHORT_PULSE)	//4bit time,= 72us
#define SLOW_DELAY_INTER_BYTE 8 * (SLOW_LONG_PULSE + SLOW_SHORT_PULSE)	//8 bit time, = 144us
#define READHLOOP 14		//read 14 times of the GPIO line
#define READHTRH 8		//if there is more than 8, this is a bit1, bit0 not possiblie

#define SLOW_WAIT_RX 4
#define SLOW_WAIT_ACK_LOW_TIMEOUT 100
#define SLOW_WAIT_ACK_HIGH_TIMEOUT 100

#define HIBERNATE_CMD 0x02
#define ECHO_CMD 0x00
#define COLDRESET_DELAY 100000

#define STICK_EXEC_TIME_ECHO 6
#define STICK_EXEC_TIME_HIBERNATE 1

// For ST1Wire protocol we need to execute byte read or write without
// being interrupted so we can properly create or monitor the IO duty
// cycle.

// Set affinity to execute on core != 0 if available.
// This is to avoid being interrupted when TEE is handling an interrupt
// (assuming that s done on 0).
// This may need adaptation depending on target platform.

// Also make sure not to go to suspend and mask interrupts

/* N17 code for HQ-306004 by liunianliang at 2023/07/05 start */
struct cpumask mask;

#define START_UNINTERRUPTIBLE_SECTION \
{ \
    unsigned long flags; \
    pm_stay_awake(sd->dev); \
    if (num_online_cpus() > 1) { \
        cpumask_copy(&mask, cpu_online_mask); \
        cpumask_clear_cpu(0, &mask); \
        set_cpus_allowed_ptr(current, &mask); \
    } \
    sd->last_cpu = get_cpu(); \
    cond_resched(); \
    spin_lock_irqsave(&sd->lock, flags); \

#define END_UNINTERRUPTIBLE_SECTION \
    spin_unlock_irqrestore(&sd->lock, flags); \
    if (num_online_cpus() > 1) { \
        cpumask_set_cpu(0, &mask); \
        set_cpus_allowed_ptr(current, &mask); \
    } \
    put_cpu(); \
    pm_relax(sd->dev); \
}
/* N17 code for HQ-306004 by liunianliang at 2023/07/05 end */

//wait delay, must be < 5000, cannot be interrupted
void delay_us(int t)
{
	if (t > 0)
		udelay(t);
	else
		udelay(1);	// ensure a small break at least

}

//sleep wait, can be interrupted
void Delay_us_sleep(int T)
{
	if (T <= 10)
		udelay(T);
	else if (T <= 20000)
		usleep_range(T, 2 * T);
	else
		msleep(DIV_ROUND_UP(T, 1000));
}

struct stick_device {
	spinlock_t lock;
	struct mutex mutex;
	struct device *dev;

	bool isCmdSent;
	bool isHibernate;

	int tweak_ns_low_to_high_before_change;	// from out low, call STICK_1WIRE_SET_IN or HIGH, time spent in call before line toggles
	int tweak_ns_low_to_high_after_change;	// from out low, call STICK_1WIRE_SET_IN or HIGH, time spent in call after line toggles
	int tweak_ns_high_to_low_before_change;	// from in high, call STICK_1WIRE_SET_OUT_LOW or LOW, time spent in call before line toggles
	int tweak_ns_high_to_low_after_change;	// from in high, call STICK_1WIRE_SET_OUT_LOW or LOW, time spent in call after line toggles

	int bit_read;		// for debug only
	int last_cpu;		// for debug only
#ifdef READ_BYTE_USE_TIMESTAMPS
	struct timespec64 tv[20];	// for debug only
	int tvi;		// for debug only
	int counters[20];
	struct timespec64 prevtv[20];	// for debug only
	int prevtvi;		// for debug only
	struct timespec64 tvbitloop[16 * 50];	// reserve 50 for each loop; 16 loop per byte
	int tvbitloopidx;
#ifdef TRY_CALIBRATION
	int iscalib;
	int doCalib;
	struct timespec64 bitreftime[8];
	unsigned long calib0;	// delay in ns from call to set HIGH at end of sync, till 1st bit start.
	unsigned long calib1;	// delay in ns from rising edge to 1st call to stick_read (high).
	unsigned long calib2;	// delay in ns from rising edge to 2nd call to stick_read (bit val).
	unsigned long calib3;	// delay in ns from rising edge to 3rd call to stick_read (low).
	unsigned long calib4;	// delay in ns from rising edge to end of bit.
#define GPIO_CALIB_MAX 1000	// need to sample during 150us max. assume sampling at min 150ns
	struct timespec64 calibdatats[GPIO_CALIB_MAX];
	int calibdataval[GPIO_CALIB_MAX];
	int calibidx;
#endif				// TRY_CALIBRATION
#endif				// READ_BYTE_USE_TIMESTAMPS
	bool isOpen;
	/* Buffer for reading a full frame */
	uint8_t rxBuf[256];

	struct gpio_desc *gpiod;
/* N17 code for HQ-295595 by tongjiacheng at 20230509 start*/
	const char *auth_name;
/* N17 code for HQ-295595 by tongjiacheng at 20230509 end*/
	struct auth_device *auth_dev;
};

// delay value in low state, i.e. from a set out low till a set in high
void delay_us_low(int t, struct stick_device *sd)
{
	delay_us(((t * 1000) -
		  (sd->tweak_ns_high_to_low_after_change +
		   sd->tweak_ns_low_to_high_before_change)) / 1000);
}

// delay value in high state, i.e. from a set in high till a set out low
void delay_us_high(int t, struct stick_device *sd)
{
	delay_us(((t * 1000) -
		  (sd->tweak_ns_low_to_high_after_change +
		   sd->tweak_ns_high_to_low_before_change)) / 1000);
}

static void stick_write_bit1(struct stick_device *sd)
{
	//Long Pulse H, short Pulse L
	STICK_1WIRE_SET_OUT_HIGH;
	delay_us_high(SLOW_LONG_PULSE, sd);
	STICK_1WIRE_SET_LOW;
	delay_us_low(SLOW_SHORT_PULSE, sd);
}

//sync bit, to write bit1
static void stick_write_sync_bit(struct stick_device *sd)
{
	stick_write_bit1(sd);
}

static void stick_write_bit0(struct stick_device *sd)
{
	//Long Pulse L, short Pulse H
	STICK_1WIRE_SET_OUT_HIGH;
	delay_us_high(SLOW_SHORT_PULSE, sd);
	STICK_1WIRE_SET_LOW;
	delay_us_low(SLOW_LONG_PULSE, sd);
}

static inline void wait_active_delay(struct timespec64 *ref,
				     unsigned long _delay_)
{
	struct timespec64 end, tv;
	end.tv_sec = ref->tv_sec;
	end.tv_nsec = ref->tv_nsec;
	end.tv_nsec += _delay_;
	if (end.tv_nsec > 1000000000L) {
		end.tv_sec += 1;
		end.tv_nsec -= 1000000000L;
	}
	do {
		ktime_get_real_ts64(&tv);
	} while ((tv.tv_sec < end.tv_sec)
		 || (tv.tv_sec == end.tv_sec && tv.tv_nsec < end.tv_nsec));
}

#ifdef READ_BYTE_USE_TIMESTAMPS
#ifdef TRY_CALIBRATION
static ssize_t stick_read_bit_delay(struct stick_device *sd,
				    struct timespec64 *reftime)
{
	int s, b;

	// Wait calib_1 to sample the line state.
	wait_active_delay(reftime, sd->calib1);

	s = STICK_READ;
	if (s != 1)
		return ST1WIRE_RECEIVE_TIMEOUT_LOW;

	// Wait calib_2 to sample the line state.
	wait_active_delay(reftime, sd->calib2);

	b = STICK_READ;
	// save TS for debug
	ktime_get_real_ts64(&sd->tv[sd->tvi]);
	sd->tvi++;

	// Wait calib_3 to sample the line state.
	wait_active_delay(reftime, sd->calib3);

	s = STICK_READ;
	if (s != 0)
		return ST1WIRE_RECEIVE_TIMEOUT_HIGH;

	// Wait calib_4 to return.
	wait_active_delay(reftime, sd->calib4);
	return (ssize_t) b;
}
#endif				// TRY_CALIBRATION
#endif				// READ_BYTE_USE_TIMESTAMPS

//read a whole bit, and judging if it is bit 0, or bit 1.
// this is called when the line is high and will return just after the line is back to high.
static ssize_t stick_read_bit_loop(struct stick_device *sd)
{

	int countH = 0, countL = 0;

#ifdef READ_BYTE_USE_TIMESTAMPS
	ktime_get_real_ts64(&sd->tv[sd->tvi]);
	sd->tvi++;
#endif				// READ_BYTE_USE_TIMESTAMPS

	while (STICK_READ) {
#ifdef READ_BYTE_USE_TIMESTAMPS
		if (countH < 50)
			ktime_get_real_ts64(&sd->
					    tvbitloop[(sd->tvbitloopidx *
						       100) + countH]);
#endif				// READ_BYTE_USE_TIMESTAMPS
		countH++;

#ifdef READ_BYTE_USE_TIMESTAMPS
		// // if the last READ took > 3us, consider the line changed state.
		// if ((countH > 1) && (countH < 50)) {
		//     unsigned long ns = NSEC_PER_SEC * ( sd->tvbitloop[(sd->tvbitloopidx * 100) + countH - 1].tv_sec - sd->tvbitloop[(sd->tvbitloopidx * 100) + countH - 2].tv_sec );
		//     ns += sd->tvbitloop[(sd->tvbitloopidx * 100) + countH - 1].tv_nsec - sd->tvbitloop[(sd->tvbitloopidx * 100) + countH - 2].tv_nsec;
		//     if (ns > 3000)
		//         break;
		// }

		// don t scan faster than SAMPLING_PERIOD_LIMIT_NS to lower probability of errors
		wait_active_delay(&sd->tv[sd->tvi - 1],
				  countH * SAMPLING_PERIOD_LIMIT_NS);
		if (countH > (30000 / SAMPLING_PERIOD_LIMIT_NS))	// we should never stay over 30us in one state
			return ST1WIRE_RECEIVE_TIMEOUT_HIGH;
#endif				// READ_BYTE_USE_TIMESTAMPS

		//add below to avoid dead loop
		if (countH > RECEIVE_TIMEOUT)
			return ST1WIRE_RECEIVE_TIMEOUT_HIGH;
	}

	// if (countH == 0) {
	//     // started with the line low, but we enter if the line was high before, so acceptable
	//     return ST1WIRE_ARBITRATION_LOST;
	// }
#ifdef READ_BYTE_USE_TIMESTAMPS
	ktime_get_real_ts64(&sd->tv[sd->tvi]);
	sd->counters[sd->tvi] = countH;
	sd->tvi++;
#endif				// READ_BYTE_USE_TIMESTAMPS

	while (!STICK_READ) {
#ifdef READ_BYTE_USE_TIMESTAMPS
		if (countL < 50)
			ktime_get_real_ts64(&sd->
					    tvbitloop[(sd->tvbitloopidx *
						       100) + 50 +
						      countL]);
#endif				// READ_BYTE_USE_TIMESTAMPS
		countL++;
#ifdef READ_BYTE_USE_TIMESTAMPS
		// // if the last READ took > 3us, consider the line changed state.
		// if ((countL > 1) && (countL < 50)) {
		//     unsigned long ns = NSEC_PER_SEC * ( sd->tvbitloop[(sd->tvbitloopidx * 100) + 50 + countL - 1].tv_sec - sd->tvbitloop[(sd->tvbitloopidx * 100) + 50 + countL - 2].tv_sec );
		//     ns += sd->tvbitloop[(sd->tvbitloopidx * 100) + 50 + countL - 1].tv_nsec - sd->tvbitloop[(sd->tvbitloopidx * 100) + 50 + countL - 2].tv_nsec;
		//     if (ns > 3000)
		//         break;
		// }

		// don t scan faster than SAMPLING_PERIOD_LIMIT_NS to lower probability of errors
		wait_active_delay(&sd->tv[sd->tvi - 1],
				  countL * SAMPLING_PERIOD_LIMIT_NS);
		if (countL > (30000 / SAMPLING_PERIOD_LIMIT_NS))	// we should never stay over 30us in one state
			return ST1WIRE_RECEIVE_TIMEOUT_LOW;
#endif				// READ_BYTE_USE_TIMESTAMPS

		//add below to avoid dead loop
		if (countL > RECEIVE_TIMEOUT)
			return ST1WIRE_RECEIVE_TIMEOUT_LOW;
	}

#ifdef READ_BYTE_USE_TIMESTAMPS
	sd->counters[sd->tvi] = countL;
#endif				// READ_BYTE_USE_TIMESTAMPS

	// Return result
	if (countH >= countL)
		return 0x01;
	else
		return 0x00;
}

//after sending the last bit, set the io to high, waiting for STICK to set IO to low first, and to high later.
static ssize_t stick_read_ack_bit(struct stick_device *sd)
{

	int i = 0;

	//set IO to high
	STICK_1WIRE_SET_OUT_HIGH;
#ifndef USE_OPEN_DRAIN
	if (sd->tweak_ns_low_to_high_after_change < 1000)
		delay_us(1);
	//set IO from output to input status
	STICK_1WIRE_SET_IN;
#endif
	//wait for low
	while (STICK_READ) {
		i++;
		delay_us(1);
		if (i >= SLOW_WAIT_ACK_LOW_TIMEOUT) {
			return ST1WIRE_ACK_ERROR;
		}
	}
	// - Wait for a high level on STWire
	i = 0;
	while (!STICK_READ) {
		i++;
		delay_us(1);
		if (i >= SLOW_WAIT_ACK_HIGH_TIMEOUT) {
			return ST1WIRE_ACK_ERROR;
		}
	}
	return ST1WIRE_OK;
}

//after send the last bit, the io line is at high, to ack, need set the io to low first and then to high
static ssize_t stick_write_ack_bit(struct stick_device *sd)
{
	int j = 0;

	// wait for line high
	while (!STICK_READ) {
		j++;
		delay_us(1);
		//add below to avoid dead loop
		if (j > RECEIVE_TIMEOUT)
			return ST1WIRE_ARBITRATION_LOST;
	}

	delay_us(SLOW_WAIT_ACK -
		 (sd->tweak_ns_high_to_low_before_change / 1000));

	//set io to output and low
	STICK_1WIRE_SET_OUT_LOW;
	//keep low
	delay_us_low(SLOW_ACK_PULSE, sd);
	//set io to high
	STICK_1WIRE_SET_OUT_HIGH;

	return ST1WIRE_OK;
}

#ifdef READ_BYTE_USE_TIMESTAMPS
#ifdef TRY_CALIBRATION
// We are reading a 0x0F value when this function is called.
static ssize_t stick_read_byte_calib(struct stick_device *sd,
				     uint8_t * rcv_byte)
{
	ssize_t i = 0;
	int transitions = 0;
	int prevstate;
	unsigned long ns[16], zh, zl, oh, ol, bd, n;

	/* - Send sync bit('1') */
	stick_write_sync_bit(sd);

	// This is our starting point for calibration
	ktime_get_real_ts64(&sd->tv[1]);

	STICK_1WIRE_SET_HIGH;

	while (!STICK_READ);	// ensure the line is high
#ifndef USE_OPEN_DRAIN
	STICK_1WIRE_SET_IN;
#endif				// USE_OPEN_DRAIN
	prevstate = 1;

	// Now we sample the line until we get enough transitions
	do {
		ktime_get_real_ts64(&sd->calibdatats[sd->calibidx]);
		sd->calibdataval[sd->calibidx] = STICK_READ;
		if (sd->calibdataval[sd->calibidx] != prevstate) {
			transitions++;
			prevstate = sd->calibdataval[sd->calibidx];
		}
		sd->calibidx++;
	} while ((sd->calibidx < GPIO_CALIB_MAX) && (transitions < 16));

	// - Acknowledge the byte reception
	if (stick_write_ack_bit(sd) != ST1WIRE_OK) {
		return ST1WIRE_ACK_ERROR;
	}
	// Compute the calibration data, if we had all transitions and at least 10 acquisitions per bit
	if ((transitions == 16) && (sd->calibidx > 80)) {
		// We saw all the transitions, use this data for calibration
		prevstate = 1;
		transitions = 0;
		for (i = 0; (i < sd->calibidx) && (transitions < 16); i++) {
			if (sd->calibdataval[i] != prevstate) {
				// did we have an abnormal read here ? reject if > 1us
				n = (sd->calibdatats[i].tv_sec -
				     sd->calibdatats[i -
						     1].tv_sec) *
				    NSEC_PER_SEC;
				n += sd->calibdatats[i].tv_nsec -
				    sd->calibdatats[i - 1].tv_nsec;
				if (n > 1000)
					break;

				// the acquisition was OK, we use for computations the ts of the previous sample
				ns[transitions] =
				    (sd->calibdatats[i - 1].tv_sec -
				     sd->tv[1].tv_sec) * NSEC_PER_SEC;
				ns[transitions] +=
				    sd->calibdatats[i - 1].tv_nsec -
				    sd->tv[1].tv_nsec;
				prevstate = sd->calibdataval[i];
				transitions++;
			}
		}

		if (transitions == 16) {
			// average duration of '0' high state:
			zh = ((ns[2] - ns[1]) + (ns[4] - ns[3]) +
			      (ns[6] - ns[5])) / 3;
			// average duration of '0' low state:
			zl = ((ns[1] - ns[0]) + (ns[3] - ns[2]) +
			      (ns[5] - ns[4]) + (ns[7] - ns[6])) / 4;
			// average duration of '1' high state:
			oh = ((ns[8] - ns[7]) + (ns[10] - ns[9]) +
			      (ns[12] - ns[11]) + (ns[14] - ns[13])) / 4;
			// average duration of '1' low state:
			ol = ((ns[9] - ns[8]) + (ns[11] - ns[10]) +
			      (ns[13] - ns[12]) + (ns[15] - ns[14])) / 4;

			// average bit duration:
			bd = (ns[15] - ns[1]) / 7;

			if (ns[1] > bd) {
				sd->calib0 = ns[15] - (8 * bd);
				sd->calib1 = zh / 2;
				sd->calib2 = bd / 2;
				sd->calib3 = bd - (ol / 2);	// oh + (ol / 2);
				sd->calib4 = bd;

				sd->iscalib = 1;
			}
		}
	}
	//
	*rcv_byte = 0x0F;
	return ST1WIRE_OK;
}
#endif				// TRY_CALIBRATION
#endif				// READ_BYTE_USE_TIMESTAMPS

//read a byte and return it, if error return code value, needs to check
static ssize_t stick_read_byte(struct stick_device *sd, uint8_t * rcv_byte)
{
	ssize_t i = 0, bitRev, value = 0;
#ifdef READ_BYTE_USE_TIMESTAMPS
	memcpy(&sd->prevtv[0], &sd->tv[0], sizeof(sd->tv));
	sd->prevtvi = sd->tvi;
	memset(&sd->tv, 0, sizeof(sd->tv));
	memset(&sd->counters, 0, sizeof(sd->counters));
	ktime_get_real_ts64(&sd->tv[0]);
	memset(&sd->tvbitloop, 0, sizeof(sd->tvbitloop));
	sd->tvbitloopidx = 0;
#ifdef TRY_CALIBRATION
	memset(&sd->bitreftime, 0, sizeof(sd->bitreftime));
#endif				// TRY_CALIBRATION
#endif				// READ_BYTE_USE_TIMESTAMPS
	/* - Send sync bit('1') */
	stick_write_sync_bit(sd);
#ifdef READ_BYTE_USE_TIMESTAMPS
	ktime_get_real_ts64(&sd->tv[1]);
#endif				// READ_BYTE_USE_TIMESTAMPS

	STICK_1WIRE_SET_HIGH;

#ifdef READ_BYTE_USE_TIMESTAMPS
	ktime_get_real_ts64(&sd->tv[2]);
	sd->tvi = 3;
#ifdef TRY_CALIBRATION
	if (sd->iscalib) {
		// Compute the 8 bitreftime as follows:
		// bit 0: sd->tv[1] + sd->calib0
		// bit 1..7 : bin N-1 + calib4
		sd->bitreftime[0].tv_sec = sd->tv[1].tv_sec;
		sd->bitreftime[0].tv_nsec = sd->tv[1].tv_nsec;
		sd->bitreftime[0].tv_nsec += sd->calib0;
		if (sd->bitreftime[0].tv_nsec > 1000000000L) {
			sd->bitreftime[0].tv_sec += 1;
			sd->bitreftime[0].tv_nsec -= 1000000000L;
		}
		for (i = 1; i < 8; i++) {
			sd->bitreftime[i].tv_sec =
			    sd->bitreftime[i - 1].tv_sec;
			sd->bitreftime[i].tv_nsec =
			    sd->bitreftime[i - 1].tv_nsec;
			sd->bitreftime[i].tv_nsec += sd->calib4;
			if (sd->bitreftime[i].tv_nsec > 1000000000L) {
				sd->bitreftime[i].tv_sec += 1;
				sd->bitreftime[i].tv_nsec -= 1000000000L;
			}
		}
	}
#endif				// TRY_CALIBRATION
#endif				// READ_BYTE_USE_TIMESTAMPS

	// delay_us(SLOW_WAIT_RX - (sd->tweak_ns_low_to_high_after_change / 1000));
	while (!STICK_READ) {};	// ensure the line is high

#ifndef USE_OPEN_DRAIN
	STICK_1WIRE_SET_IN;
#endif				// USE_OPEN_DRAIN

#ifdef READ_BYTE_USE_TIMESTAMPS
#ifdef TRY_CALIBRATION
	if (sd->iscalib) {
		wait_active_delay(&sd->tv[1], sd->calib0);
	} else {
#endif				// TRY_CALIBRATION
		wait_active_delay(&sd->tv[2],
				  (SLOW_WAIT_RX * 1000) -
				  sd->tweak_ns_low_to_high_after_change);
#ifdef TRY_CALIBRATION
	}
#endif				// TRY_CALIBRATION
#endif				// READ_BYTE_USE_TIMESTAMPS

	for (i = 0; i < 8; i++) {
#ifdef READ_BYTE_USE_TIMESTAMPS
#ifdef TRY_CALIBRATION
		if (sd->iscalib) {
			bitRev =
			    stick_read_bit_delay(sd, &sd->bitreftime[i]);
		} else {
#endif				// TRY_CALIBRATION
			sd->tvbitloopidx = i;
#endif				// READ_BYTE_USE_TIMESTAMPS
			bitRev = stick_read_bit_loop(sd);
#ifdef READ_BYTE_USE_TIMESTAMPS
#ifdef TRY_CALIBRATION
		}		// (sd->iscalib)
#endif				// TRY_CALIBRATION
#endif				// READ_BYTE_USE_TIMESTAMPS
		if (bitRev == 0x01)
			value |= 0x01 << (7 - i);	// reads MSb first
		else if (bitRev < 0x00) {
			if ((i > 4)
			    && ((bitRev == ST1WIRE_RECEIVE_TIMEOUT_HIGH)
				|| (bitRev ==
				    ST1WIRE_RECEIVE_TIMEOUT_LOW))) {
				// We missed a few bits. Let s ignore that and let higher layer handle
#ifdef READ_BYTE_USE_TIMESTAMPS
				// we wait first until the byte is finished, and since STICK does not timeout, we take
				// margin (double time)
				wait_active_delay(&sd->tv[2],
						  SLOW_DELAY_INTER_BYTE *
						  1000 * 2);
#endif				// READ_BYTE_USE_TIMESTAMPS
				break;
			}
			*rcv_byte = value;	// for debug
			sd->bit_read = i;	// for debug
			return bitRev;	//return <0 , lost sync
		}
	}

#ifdef READ_BYTE_USE_TIMESTAMPS
	if (bitRev >= 0) {
		ktime_get_real_ts64(&sd->tv[sd->tvi]);
		// wait 1 bit duration in case we got a wrong reading.
		wait_active_delay(&sd->tv[sd->tvi],
				  (SLOW_LONG_PULSE +
				   SLOW_SHORT_PULSE) * 1000);
		sd->tvi++;
	}
#endif				// READ_BYTE_USE_TIMESTAMPS

	// - Acknowledge the byte reception
	if (stick_write_ack_bit(sd) != ST1WIRE_OK) {
		return ST1WIRE_ACK_ERROR;
	}
	//
	*rcv_byte = value;
	return ST1WIRE_OK;
}

static ssize_t stick_write_byte(struct stick_device *sd, uint8_t byte)
{

	uint32_t i = 0;
	ssize_t ret;
	//send sync bit 1
	stick_write_sync_bit(sd);
	// - Send Byte
	for (i = 0; i < 8; i++) {
		/* Mask each bit value */
		if (byte & (1 << (7 - i))) {
			/* - Send '1' */
			stick_write_bit1(sd);
		} else {
			/* - Send '0' */
			stick_write_bit0(sd);
		}
	}

    /*- Wait for Ack */
	ret = stick_read_ack_bit(sd);
	return ret;
}

static ssize_t stick_set_frame_start(struct stick_device *sd,
				     uint8_t byteval)
{
	int i = 0;
	int j = 0;
	int MAX_WAIT = 30;
	ssize_t ret;

	//make sure it is in idle status
	STICK_1WIRE_SET_IN;

	//check if in low status, else there is error with the status
	do {
		i = 0;
		while ((STICK_READ == GPIO_HIGH) && (i < IDLE)) {
			delay_us(1);
			i++;
		}
		if (i < IDLE) {
			j++;
			Delay_us_sleep(IDLE);
		}
	} while ((i < IDLE) && (j < MAX_WAIT));

	if (j == MAX_WAIT) {
		DEV_WARN(sd->dev, "%s : ST1WIRE_ARBITRATION_LOST\n",
			 __func__);
		return ST1WIRE_ARBITRATION_LOST;
	}

	START_UNINTERRUPTIBLE_SECTION
	    /* - Set bus to low level */
	    STICK_1WIRE_SET_OUT_LOW;
	delay_us_low(SLOW_START_PULSE, sd);
	/* - Set bus back to high level */
	STICK_1WIRE_SET_OUT_HIGH;
	delay_us_low(SLOW_DELAY_INTER_BYTE, sd);

	ret = stick_write_byte(sd, byteval);

	END_UNINTERRUPTIBLE_SECTION return ret;
}

static int stick_exit_hibernate_and_reset(struct stick_device *sd);

// Debug
static void dump_timestamps(struct stick_device *sd)
{
#ifdef READ_BYTE_USE_TIMESTAMPS
	int i = 0;
	unsigned long delta = 0;

	if (!enable_debug_log) {
		return;
	}

	DEV_WARN(sd->dev, "%s : %d timestamps recorded\n", __func__,
		 sd->tvi);
	for (i = 0; i < sd->tvi; i++) {
		if (i >= 1) {
			if (sd->tv[i].tv_sec > sd->tv[i - 1].tv_sec)
				delta =
				    NSEC_PER_SEC + sd->tv[i].tv_nsec -
				    sd->tv[i - 1].tv_nsec;
			else
				delta =
				    sd->tv[i].tv_nsec - sd->tv[i -
							       1].tv_nsec;
		}
		DEV_WARN(sd->dev,
			 "%s : tv[%2d]: %lld.%09ld   +%6ld  cnt:%d\n",
			 __func__, i, sd->tv[i].tv_sec, sd->tv[i].tv_nsec,
			 delta, sd->counters[i]);
	}
	delta = 0;
#ifdef TRY_CALIBRATION
	if (sd->iscalib) {
		DEV_WARN(sd->dev, "%s : [N-1] %d timestamps recorded\n",
			 __func__, sd->prevtvi);
		for (i = 0; i < sd->prevtvi; i++) {
			if (i >= 1) {
				if (sd->prevtv[i].tv_sec >
				    sd->prevtv[i - 1].tv_sec)
					delta =
					    NSEC_PER_SEC +
					    sd->prevtv[i].tv_nsec -
					    sd->prevtv[i - 1].tv_nsec;
				else
					delta =
					    sd->prevtv[i].tv_nsec -
					    sd->prevtv[i - 1].tv_nsec;
			}
			DEV_WARN(sd->dev,
				 "%s : [N-1] tv[%2d]: %lld.%09ld   +%6ld\n",
				 __func__, i, sd->prevtv[i].tv_sec,
				 sd->prevtv[i].tv_nsec, delta);
		}
		delta = 0;
		for (i = 0; i < 8; i++) {
			if (i >= 1) {
				if (sd->bitreftime[i].tv_sec >
				    sd->bitreftime[i - 1].tv_sec)
					delta =
					    NSEC_PER_SEC +
					    sd->bitreftime[i].tv_nsec -
					    sd->bitreftime[i - 1].tv_nsec;
				else
					delta =
					    sd->bitreftime[i].tv_nsec -
					    sd->bitreftime[i - 1].tv_nsec;
			} else {
				// compute delta vs sd->tv[1]
				if (sd->bitreftime[i].tv_sec >
				    sd->tv[1].tv_sec)
					delta =
					    NSEC_PER_SEC +
					    sd->bitreftime[i].tv_nsec -
					    sd->tv[1].tv_nsec;
				else
					delta =
					    sd->bitreftime[i].tv_nsec -
					    sd->tv[1].tv_nsec;
			}
			DEV_WARN(sd->dev,
				 "%s : bitreftime[%d]: %lld.%09ld   +%6ld\n",
				 __func__, i, sd->bitreftime[i].tv_sec,
				 sd->bitreftime[i].tv_nsec, delta);
		}
		return;
	}
#endif				// TRY_CALIBRATION
	DEV_WARN(sd->dev, "%s : ====== inner loop timestamps\n", __func__);
	for (i = 0; i < sd->bit_read; i++) {
		int j;
		DEV_WARN(sd->dev, "%s :   ====== bit %d, high loop\n",
			 __func__, i);
		for (j = (i * 100); j < (i * 100) + 50; j++) {
			if (j == i * 100) {
				DEV_WARN(sd->dev, "%s :    %lld.%09ld\n",
					 __func__, sd->tvbitloop[j].tv_sec,
					 sd->tvbitloop[j].tv_nsec);
			} else {
				if (sd->tvbitloop[j].tv_sec == 0)
					break;
				if (sd->tvbitloop[j].tv_sec >
				    sd->tvbitloop[j - 1].tv_sec)
					delta =
					    NSEC_PER_SEC +
					    sd->tvbitloop[j].tv_nsec -
					    sd->tvbitloop[j - 1].tv_nsec;
				else
					delta =
					    sd->tvbitloop[j].tv_nsec -
					    sd->tvbitloop[j - 1].tv_nsec;
				DEV_WARN(sd->dev,
					 "%s :    %lld.%09ld   +%5ld\n",
					 __func__, sd->tvbitloop[j].tv_sec,
					 sd->tvbitloop[j].tv_nsec, delta);
			}
		}
		DEV_WARN(sd->dev, "%s :   ====== bit %d, low loop\n",
			 __func__, i);
		for (j = (i * 100) + 50; j < (i * 100) + 100; j++) {
			if (j == (i * 100) + 50) {
				DEV_WARN(sd->dev, "%s :    %lld.%09ld\n",
					 __func__, sd->tvbitloop[j].tv_sec,
					 sd->tvbitloop[j].tv_nsec);
			} else {
				if (sd->tvbitloop[j].tv_sec == 0)
					break;
				if (sd->tvbitloop[j].tv_sec >
				    sd->tvbitloop[j - 1].tv_sec)
					delta =
					    NSEC_PER_SEC +
					    sd->tvbitloop[j].tv_nsec -
					    sd->tvbitloop[j - 1].tv_nsec;
				else
					delta =
					    sd->tvbitloop[j].tv_nsec -
					    sd->tvbitloop[j - 1].tv_nsec;
				DEV_WARN(sd->dev,
					 "%s :    %lld.%09ld   +%5ld\n",
					 __func__, sd->tvbitloop[j].tv_sec,
					 sd->tvbitloop[j].tv_nsec, delta);
			}
		}
	}
#endif				// READ_BYTE_USE_TIMESTAMPS
}

ssize_t stick_send_frame(struct stick_device *sd, uint8_t * buf,
			 size_t frame_length)
{
	// write these bytes to the STICK
	// was it a hibernate command? update isHibernate
	// return status code

	/* - Get bus Arbitration and send Start of frame */
	ssize_t ret;
	int i = 0;
	uint8_t ACKvalue;

	// reset hibernate flag.
	if (sd->isHibernate == true) {
		ret = stick_exit_hibernate_and_reset(sd);
		if (ret != ST1WIRE_OK) {
			DEV_WARN(sd->dev, "%s : Exit hibernate failed\n",
				 __func__);
			return ret;
		} else {
			sd->isHibernate = false;
		}
	}

	if (enable_debug_log) {
		DEV_DBG(sd->dev,
			"%s : send %zu bytes %02hhx%02hhx%02hhx%02hhx%02hhx...\n",
			__func__, frame_length,
			frame_length > 0 ? buf[0] : 0,
			frame_length > 1 ? buf[1] : 0,
			frame_length > 2 ? buf[2] : 0,
			frame_length > 3 ? buf[3] : 0,
			frame_length > 4 ? buf[4] : 0);
	}

	ret = stick_set_frame_start(sd, frame_length);
	if (ret == ST1WIRE_OK) {
		/* - Send Frame content */
		for (i = 0; i < frame_length; i++) {
			Delay_us_sleep(SLOW_DELAY_INTER_BYTE);	//8*18= 144us

			START_UNINTERRUPTIBLE_SECTION
			    ret = stick_write_byte(sd, buf[i]);

			END_UNINTERRUPTIBLE_SECTION
			    if (ret == ST1WIRE_ACK_ERROR) {
				DEV_WARN(sd->dev,
					 "%s : stick_write_byte(byte #%d) failed (cpu:%d)\n",
					 __func__, i, sd->last_cpu);
				break;
			}
		}

		/* - Get Frame Ack *///after send last byte, needs delay 8bit timing
		if (i == frame_length)	//all sent in success
		{
			Delay_us_sleep(SLOW_DELAY_INTER_BYTE);	//8*18= 144us

#ifdef READ_BYTE_USE_TIMESTAMPS
			if (enable_debug_log)
				DEV_DBG(sd->dev,
					"stick_read_byte(sd, &ACKvalue)\n");
#endif				// READ_BYTE_USE_TIMESTAMPS

			START_UNINTERRUPTIBLE_SECTION
			    ret = stick_read_byte(sd, &ACKvalue);

			END_UNINTERRUPTIBLE_SECTION
			    if ((ret != ST1WIRE_OK) || (ACKvalue != 0x20))
			{
				DEV_WARN(sd->dev,
					 "%s : stick_read_byte ACK failed (%d %d 0x%x) (cpu:%d)\n",
					 __func__, ret, sd->bit_read,
					 ACKvalue, sd->last_cpu);
				dump_timestamps(sd);
#ifdef READ_BYTE_USE_TIMESTAMPS
#ifdef TRY_CALIBRATION
				sd->iscalib = 0;
#endif				// TRY_CALIBRATION
#endif				// READ_BYTE_USE_TIMESTAMPS
				return ST1WIRE_ACK_ERROR;
			}
		}
	} else {
		DEV_WARN(sd->dev,
			 "%s : stick_set_frame_start(frame length) failed (cpu:%d)\n",
			 __func__, sd->last_cpu);
	}

	//command is sent in success, check if it was hibernate
	if ((ret == ST1WIRE_OK) && (buf[0] == HIBERNATE_CMD)) {
		sd->isHibernate = true;
	}
	// Delay in ST1Wire slow to allow STICK Vcc to stabilize
	Delay_us_sleep(SLOW_INTERFRAME_DELAY);

	return ret;
}

// Buf must always be a 256bytes buffer at least.
ssize_t stick_read_frame(struct stick_device * sd, uint8_t * buf)
{
	// read bytes from STICK, length is acquired during read.
	// return the length read successfully
	uint8_t i;
	ssize_t ret = ST1WIRE_ACK_ERROR;
	uint8_t rcv_byte, frame_length;

	/* - Get bus Arbitration and send Start of frame */
	ret = stick_set_frame_start(sd, 0x00);
	/* - Request Frame reception (frame length = 0x00) */

	if (ret != ST1WIRE_OK) {
		DEV_WARN(sd->dev,
			 "%s : Didn t get ACK bit from STICK to read answer (cpu:%d)\n",
			 __func__, sd->last_cpu);
		Delay_us_sleep(SLOW_INTERFRAME_DELAY);
		return ret;
	}

	/* - Get Request ACK */
	Delay_us_sleep(SLOW_DELAY_INTER_BYTE);

#ifdef READ_BYTE_USE_TIMESTAMPS
	if (enable_debug_log)
		DEV_DBG(sd->dev, "stick_read_byte(sd, &rcv_byte 0x20)\n");
#endif				// READ_BYTE_USE_TIMESTAMPS

	START_UNINTERRUPTIBLE_SECTION ret = stick_read_byte(sd, &rcv_byte);

	END_UNINTERRUPTIBLE_SECTION
	    if ((ret != ST1WIRE_OK) || (rcv_byte != 0x20)) {
		DEV_WARN(sd->dev,
			 "%s : Didn t get ACK byte from STICK to read answer (%d %d 0x%x) (cpu:%d)\n",
			 __func__, ret, sd->bit_read, rcv_byte,
			 sd->last_cpu);
		dump_timestamps(sd);
		if (ret != ST1WIRE_OK) {
#ifdef READ_BYTE_USE_TIMESTAMPS
#ifdef TRY_CALIBRATION
			sd->iscalib = 0;
#endif				// TRY_CALIBRATION
#endif				//  READ_BYTE_USE_TIMESTAMPS
			Delay_us_sleep(SLOW_INTERFRAME_DELAY);
			return ST1WIRE_ACK_ERROR;
		} else {
			// We read a value that was not 0x20. In case it was just a bit error, we continue.
			// We will fail anyway to read the length if we had a real desync
			DEV_DBG(sd->dev,
				"stick_read_byte ignores invalid ACK and try to read length\n");
		}
	}

	Delay_us_sleep(SLOW_DELAY_INTER_BYTE);

#ifdef READ_BYTE_USE_TIMESTAMPS
	if (enable_debug_log)
		DEV_DBG(sd->dev, "stick_read_byte(sd, &frame_length)\n");
#endif				// READ_BYTE_USE_TIMESTAMPS

	START_UNINTERRUPTIBLE_SECTION
	    /* - Get Frame length */
	    ret = stick_read_byte(sd, &frame_length);

	END_UNINTERRUPTIBLE_SECTION if (ret != ST1WIRE_OK) {
		DEV_WARN(sd->dev,
			 "%s : stick_read_byte (frame_length) failed (%d %d 0x%x) (cpu:%d)\n",
			 __func__, ret, sd->bit_read, frame_length,
			 sd->last_cpu);
		dump_timestamps(sd);
#ifdef READ_BYTE_USE_TIMESTAMPS
#ifdef TRY_CALIBRATION
		sd->iscalib = 0;
#endif				// TRY_CALIBRATION
#endif				// READ_BYTE_USE_TIMESTAMPS
		Delay_us_sleep(SLOW_INTERFRAME_DELAY);
		return ret;
	}

	if (frame_length > 0) {
		for (i = 0; i < frame_length; i++) {
			Delay_us_sleep(SLOW_DELAY_INTER_BYTE);

#ifdef READ_BYTE_USE_TIMESTAMPS
#ifdef TRY_CALIBRATION
			if ((sd->doCalib == 1) && (i == 2)) {
				DEV_DBG(sd->dev,
					"stick_read_byte_calib(sd, &buf[%d])\n",
					i);
				START_UNINTERRUPTIBLE_SECTION ret =
				    stick_read_byte_calib(sd, &buf[i]);
				END_UNINTERRUPTIBLE_SECTION if (sd->
								iscalib) {
					DEV_INFO(sd->dev,
						 "%s: calib done, %lu, %lu, %lu, %lu, %lu\n",
						 __func__, sd->calib0,
						 sd->calib1, sd->calib2,
						 sd->calib3, sd->calib4);
				} else {
					DEV_INFO(sd->dev,
						 "%s: calib NOT done, use default\n",
						 __func__);
				}
				continue;
			}
#endif				// TRY_CALIBRATION
#ifdef READ_BYTE_USE_TIMESTAMPS_VERBOSE
			if (enable_debug_log)
				DEV_DBG(sd->dev,
					"stick_read_byte(sd, &buf[%d])\n",
					i);
#endif				// READ_BYTE_USE_TIMESTAMPS_VERBOSE
#endif				// READ_BYTE_USE_TIMESTAMPS

			START_UNINTERRUPTIBLE_SECTION
			    ret = stick_read_byte(sd, &buf[i]);

			END_UNINTERRUPTIBLE_SECTION if (ret != ST1WIRE_OK) {
				DEV_WARN(sd->dev,
					 "%s : stick_read_byte(%d/%hhu) failed (%d %d 0x%x) (cpu:%d)\n",
					 __func__, i, frame_length, ret,
					 sd->bit_read, buf[i],
					 sd->last_cpu);
				dump_timestamps(sd);
#ifdef READ_BYTE_USE_TIMESTAMPS
#ifdef TRY_CALIBRATION
				sd->iscalib = 0;
#endif				// TRY_CALIBRATION
#endif				// READ_BYTE_USE_TIMESTAMPS
				Delay_us_sleep(SLOW_INTERFRAME_DELAY);
				return ret;
			}
		}
	}

	if (enable_debug_log) {
		DEV_DBG(sd->dev,
			"%s : read %hhu bytes: %02hhx%02hhx%02hhx%02hhx%02hhx\n",
			__func__, frame_length,
			frame_length > 0 ? buf[0] : 0,
			frame_length > 1 ? buf[1] : 0,
			frame_length > 2 ? buf[2] : 0,
			frame_length > 3 ? buf[3] : 0,
			frame_length > 4 ? buf[4] : 0);
	}
	// Purge any remaining byte in STICK output buffer
    /* N17 code for HQ-297568 by tongjiacheng at 20230601 start */
	if (!sd->isHibernate) {
		i = 0;
		while (ret == ST1WIRE_OK) {
			uint8_t dummy;
			Delay_us_sleep(SLOW_DELAY_INTER_BYTE);

	#ifdef READ_BYTE_USE_TIMESTAMPS
	#ifdef READ_BYTE_USE_TIMESTAMPS_VERBOSE
			if (enable_debug_log)
				DEV_DBG(sd->dev, "stick_read_byte(sd, &dummy)\n");
	#endif				// READ_BYTE_USE_TIMESTAMPS_VERBOSE
	#endif				// READ_BYTE_USE_TIMESTAMPS

			START_UNINTERRUPTIBLE_SECTION
				ret = stick_read_byte(sd, &dummy);

			END_UNINTERRUPTIBLE_SECTION if (ret == ST1WIRE_OK) {
	#ifdef READ_BYTE_USE_TIMESTAMPS
	#ifdef READ_BYTE_USE_TIMESTAMPS_VERBOSE
				DEV_DBG(sd->dev,
					"%s : stick_read_byte dummy read after end of frame: %02hhx\n",
					__func__, dummy);
	#endif				// READ_BYTE_USE_TIMESTAMPS_VERBOSE
	#endif				// READ_BYTE_USE_TIMESTAMPS
				i++;
			}
		}
		if (i > 0) {
			DEV_DBG(sd->dev,
				"%s : stick_read_byte found %d dummy bytes after end of frame\n",
				__func__, i);
		}
		// Delay in ST1Wire slow to allow STICK Vcc to stabilize
		Delay_us_sleep(SLOW_INTERFRAME_DELAY - SLOW_DELAY_INTER_BYTE);
	}
    /* N17 code for HQ-297568 by tongjiacheng at 20230601 end */
	return (ssize_t) frame_length;
}

/* N17 code for HQ-306004 by liunianliang at 2023/07/05 start */
#ifndef NO_BENCHMARK
static void stick_benchmark_io(struct stick_device *sd)
{
	struct timespec64 tv1, tv2;
	int i;
	unsigned long ns;
	unsigned long ns1 = 0, ns2 = 0, ns3 = 0, ns4 = 0, ns5 = 0, ns6 = 0;
	unsigned long ns1m = 0, ns2m = 0, ns3m = 0, ns4m = 0, ns5m =
	    0, ns6m = 0;
#define N_LOOPS 100
	// unsigned long nsreads[N_LOOPS];


	START_UNINTERRUPTIBLE_SECTION for (i = 0; i < N_LOOPS; i++) {
		ktime_get_real_ts64(&tv1);
		ktime_get_real_ts64(&tv2);
		if (tv1.tv_sec < tv2.tv_sec) {
			tv2.tv_nsec += NSEC_PER_SEC;
		} else {
			tv1.tv_sec += 1;
		}
		ns = tv2.tv_nsec - tv1.tv_nsec;
		ns1 += ns;
		if (ns > ns1m)
			ns1m = ns;

		ktime_get_real_ts64(&tv1);
		STICK_1WIRE_SET_OUT_LOW;
		ktime_get_real_ts64(&tv2);

		if (tv1.tv_sec < tv2.tv_sec) {
			tv2.tv_nsec += NSEC_PER_SEC;
		} else {
			tv1.tv_sec += 1;
		}
		ns = tv2.tv_nsec - tv1.tv_nsec;
		ns2 += ns;
		if (ns > ns2m)
			ns2m = ns;
		delay_us(1);
		ktime_get_real_ts64(&tv1);
		STICK_1WIRE_SET_IN;
		ktime_get_real_ts64(&tv2);

		if (tv1.tv_sec < tv2.tv_sec) {
			tv2.tv_nsec += NSEC_PER_SEC;
		} else {
			tv1.tv_sec += 1;
		}
		ns = tv2.tv_nsec - tv1.tv_nsec;
		ns3 += ns;
		if (ns > ns3m)
			ns3m = ns;
		ktime_get_real_ts64(&tv1);
		delay_us(1);
		ktime_get_real_ts64(&tv2);
		if (tv1.tv_sec < tv2.tv_sec) {
			tv2.tv_nsec += NSEC_PER_SEC;
		} else {
			tv1.tv_sec += 1;
		}
		ns = tv2.tv_nsec - tv1.tv_nsec;
		ns4 += ns;
		if (ns > ns4m)
			ns4m = ns;
		ktime_get_real_ts64(&tv1);
		(void) STICK_READ;
		ktime_get_real_ts64(&tv2);

		if (tv1.tv_sec < tv2.tv_sec) {
			tv2.tv_nsec += NSEC_PER_SEC;
		} else {
			tv1.tv_sec += 1;
		}
		ns = tv2.tv_nsec - tv1.tv_nsec;
		ns5 += ns;
		if (ns > ns5m)
			ns5m = ns;
		// nsreads[i] = ns;

		ktime_get_real_ts64(&tv1);
		(void) STICK_READ;
		(void) STICK_READ;
		(void) STICK_READ;

		(void) STICK_READ;
		(void) STICK_READ;

		ktime_get_real_ts64(&tv2);
		if (tv1.tv_sec < tv2.tv_sec) {
			tv2.tv_nsec += NSEC_PER_SEC;
		} else {
			tv1.tv_sec += 1;
		}
		ns = tv2.tv_nsec - tv1.tv_nsec;
		ns6 += ns;
		if (ns > ns6m)
			ns6m = ns;
	}

	END_UNINTERRUPTIBLE_SECTION ns = ns1 / N_LOOPS;
	DEV_INFO(sd->dev,
		 "Avg ktime_get_real_ts64 overhead: %lu ns (max: %lu ns)\n",
		 ns, ns1m);
	if (ns2 < ns1)
		ns2 = ns1;
	DEV_INFO(sd->dev,
		 "Avg STICK_1WIRE_SET_OUT_LOW duration: %lu ns (max: %lu ns)\n",
		 (ns2 - ns1) / N_LOOPS, ns2m - ns);
	// Assume the line toggles at 3/5 of this duration -- this is arbitrary
	sd->tweak_ns_high_to_low_before_change =
	    (ns2 - ns1) * 3 / (5 * N_LOOPS);
	sd->tweak_ns_high_to_low_after_change =
	    (ns2 - ns1) * 2 / (5 * N_LOOPS);
	if (ns3 < ns1)
		ns3 = ns1;
	DEV_INFO(sd->dev,
		 "Avg STICK_1WIRE_SET_IN duration: %lu ns (max: %lu ns)\n",
		 (ns3 - ns1) / N_LOOPS, ns3m - ns);

	// same assumption
	sd->tweak_ns_low_to_high_before_change =
	    (ns3 - ns1) * 3 / (5 * N_LOOPS);
	sd->tweak_ns_low_to_high_after_change =
	    (ns3 - ns1) * 2 / (5 * N_LOOPS);
	DEV_INFO(sd->dev,
		 "Avg delay_us(1) duration: %lu ns (max: %lu ns)\n",
		 (ns4 - ns1) / N_LOOPS, ns4m - ns);
	DEV_INFO(sd->dev,
		 "Avg STICK_READ duration: %lu ns (max: %lu ns)\n",
		 (ns5 - ns1) / N_LOOPS, ns5m - ns);
	DEV_INFO(sd->dev,
		 "Avg 5x STICK_READ duration: %lu ns (max: %lu ns)\n",
		 (ns6 - ns1) / N_LOOPS, ns6m - ns);

	// for (i = 0; i < N_LOOPS/10; i++) {
	//     DEV_INFO(sd->dev, "STICK_READ duration sampling: %ld; %ld; %ld; %ld; %ld; %ld; %ld; %ld; %ld; %ld\n",
	//         nsreads[i * 10], nsreads[i * 10 + 1], nsreads[i * 10 + 2], nsreads[i * 10 + 3], nsreads[i * 10 + 4],
	//         nsreads[i * 10 + 5], nsreads[i * 10 + 6], nsreads[i * 10 + 7], nsreads[i * 10 + 8], nsreads[i * 10 + 9]
	//     );
	// }
}
#endif
/* N17 code for HQ-306004 by liunianliang at 2023/07/05 end */

static void stick_cold_reset(struct stick_device *sd)
{
	// on ST1WIRE we just need to keep the IO low to go below POR
	STICK_1WIRE_SET_OUT_LOW;
	Delay_us_sleep(COLDRESET_DELAY);
	STICK_1WIRE_SET_OUT_HIGH;
	Delay_us_sleep(COLDRESET_DELAY);
	STICK_1WIRE_SET_IN;
	DEV_INFO(sd->dev, "COLD RESET done\n");
	// Chip is active after cold reset
	sd->isHibernate = false;
}

static int stick_set_hibernate(struct stick_device *sd)
{
	uint8_t hibernate_cmd[] = { HIBERNATE_CMD, 0xd3, 0x6A };
	int ret;

	// ssize_t stick_send_frame(struct stick_device *sd, uint8_t *buf, size_t frame_length)
	ret = stick_send_frame(sd, hibernate_cmd, sizeof(hibernate_cmd));
	if (ret != ST1WIRE_OK) {
		DEV_WARN(sd->dev,
			 "%s : failed to send hibernate command %d\n",
			 __func__, ret);
		return ret;
	}
	// Processing time for HIBERNATE
	Delay_us_sleep(1000 * STICK_EXEC_TIME_HIBERNATE);

	ret = stick_read_frame(sd, sd->rxBuf);
	if (ret <= 0) {
		DEV_WARN(sd->dev,
			 "%s : failed to read hibernate response %d\n",
			 __func__, ret);
		// return; We ignore the error to read answer, we will cold reset the chip after.
	}

	return ST1WIRE_OK;
}

static int stick_exit_hibernate_and_reset(struct stick_device *sd)
{
	uint8_t echo1_cmd[] = { ECHO_CMD, 0x00, 0x0F, 0x47 };
	int ret;
#ifdef READ_BYTE_USE_TIMESTAMPS
#ifdef TRY_CALIBRATION
	uint8_t echoCalib_cmd[] =
	    { ECHO_CMD, 0x00, 0x0F, 0x00, 0xFF, 0x8A, 0xC8 };
	int calibTries = 0;
#endif				// TRY_CALIBRATION
#endif				// READ_BYTE_USE_TIMESTAMPS

	// cold reset
	stick_cold_reset(sd);

	// send 1 byte echo command, check answer is OK
	ret = stick_send_frame(sd, echo1_cmd, sizeof(echo1_cmd));
	if (ret != ST1WIRE_OK) {
		DEV_WARN(sd->dev, "%s : failed to send ping command %d\n",
			 __func__, ret);
		return ret;
	}
	// Processing time for ECHO
	Delay_us_sleep(1000 * STICK_EXEC_TIME_ECHO);

	ret = stick_read_frame(sd, sd->rxBuf);
	if (ret <= 0) {
		DEV_WARN(sd->dev, "%s : failed to read ping response %d\n",
			 __func__, ret);
		return ret;
	}
#ifdef READ_BYTE_USE_TIMESTAMPS
#ifdef TRY_CALIBRATION
	do {
		// Try to calibrate
		ret =
		    stick_send_frame(sd, echoCalib_cmd,
				     sizeof(echoCalib_cmd));
		if (ret != ST1WIRE_OK) {
			DEV_WARN(sd->dev,
				 "%s : failed to send ping command %d\n",
				 __func__, ret);
			stick_cold_reset(sd);
			calibTries++;
			continue;
		}
		// Processing time for ECHO
		Delay_us_sleep(1000 * STICK_EXEC_TIME_ECHO);

		sd->doCalib = 1;
		ret = stick_read_frame(sd, sd->rxBuf);
		sd->doCalib = 0;
		if (ret <= 0) {
			DEV_WARN(sd->dev,
				 "%s : failed to read ping response %d, cold reset (retry #%d)\n",
				 __func__, ret, calibTries);
			sd->iscalib = 0;
			stick_cold_reset(sd);
		}
		calibTries++;
	} while (((ret <= 0) || (sd->iscalib == 0)) && (calibTries < 5));
#endif				// TRY_CALIBRATION
#endif				// READ_BYTE_USE_TIMESTAMPS

	return ST1WIRE_OK;
}

static struct platform_device *stick_pdev = NULL;

int stick_kernel_open(struct stick_device **psd, bool debug)
{
	struct stick_device *sd;
	struct device *dev;
	int ret = 0;

	if (stick_pdev == NULL) {
		DEV_WARN(NULL, "%s : no device probed\n", __func__);
		return -EAGAIN;
	}
	//DEV_WARN(NULL, "%p: stick_pdev addr in %s\n", stick_pdev, __func__ );

	sd = dev_get_drvdata(&stick_pdev->dev);
	if (sd == NULL) {
		DEV_ERR(&stick_pdev->dev, "%s : drvdata is null.\n",
			__func__);
		return -EINVAL;
	}

	dev = sd->dev;
	DEV_INFO(dev, "%s\n", __func__);

	mutex_lock(&sd->mutex);
	if (sd->isOpen) {
		ret = -EBUSY;
		DEV_WARN(sd->dev, "%s : device already open\n", __func__);
	} else {
		sd->isOpen = true;
	}
	mutex_unlock(&sd->mutex);
	*psd = sd;
	enable_debug_log = debug;
	enable_warnings_log = (debug ? true : WARNINGS_DISPLAYED_DEFAULT);

	return ret;
}

void stick_kernel_reset(struct stick_device *sd)
{
	stick_cold_reset(sd);
}

int stick_kernel_release(struct stick_device *sd)
{
	struct device *dev;
	int ret = 0;
	if (sd == NULL) {
		return -EINVAL;
	}
	dev = sd->dev;
	DEV_INFO(sd->dev, "%s\n", __func__);

	mutex_lock(&sd->mutex);
	sd->isOpen = false;
	if (sd->isHibernate == false) {
		if (stick_set_hibernate(sd) != ST1WIRE_OK) {
			DEV_WARN(sd->dev,
				 "%s : Error while setting STICK to hibernate\n",
				 __func__);
		}
	}
	mutex_unlock(&sd->mutex);

	return ret;
}

static int st_start_auth_battery(struct auth_device *auth_dev)
{
	int ret;

	ret = stickapitest_init(6);

	pr_info("%s authenticate_battery %s!\n", 
			__func__, (ret == STICK_OK) ? "success" : "fail");

	if (ret != STICK_OK)
		return -1;

	return 0;
}

/* N17 code for HQ-292265 by tongjiacheng at 20230515 start */
uint8_t page0_data[2];
/* N17 code for HQ-292265 by tongjiacheng at 20230515 end */
static int st_get_battery_id(struct auth_device *auth_dev, u8 *bat_id)
{
/* N17 code for HQ-292265 by tongjiacheng at 20230515 start */
/* N17 code for HQHW-4679 by wangtingting at 20230724 start */
	if (page0_data[0] == 0x47 || page0_data[0] == 0x45)	//冠宇(G)
		*bat_id = 0;
	else if (page0_data[0] == 0x4e && page0_data[1] == 0x56)	//NVT(NV)
		*bat_id = 1;
	else
		*bat_id = 0xff;
/* N17 code for HQHW-4679 by wangtingting at 20230724 end */
/* N17 code for HQ-292265 by tongjiacheng at 20230515 end */

	return 0;
}

static struct auth_ops st_auth_ops = {
	.auth_battery = st_start_auth_battery,
	.get_battery_id = st_get_battery_id,
};

static int stick_platform_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct stick_device *sd;

	DEV_INFO(dev, "%s\n", __func__);

	// Allocate the device structure
	sd = (struct stick_device *) devm_kzalloc(dev, sizeof(*sd),
						  GFP_KERNEL);
	if (sd == NULL) {
		DEV_ERR(dev, "%s : Cannot allocate memory\n", __func__);
		return -ENOMEM;
	}

	sd->dev = dev;
	mutex_init(&sd->mutex);
	spin_lock_init(&sd->lock);
/* N17 code for HQ-295595 by tongjiacheng at 20230509 start*/
	ret = of_property_read_string(pdev->dev.of_node, 
		"auth_name", &sd->auth_name);
	if (ret < 0) {
		pr_info("%s can not find auth name(%d)\n", __func__, ret);
		sd->auth_name = "third_supplier";
	}

	sd->auth_dev = auth_device_register(sd->auth_name, NULL, sd, &st_auth_ops);
	if (IS_ERR_OR_NULL(sd->auth_dev)) {
		DEV_ERR(sd->dev, "auth device register fail(%d)\n", PTR_ERR(sd->auth_dev));
		return PTR_ERR(sd->auth_dev);
	}
/* N17 code for HQ-295595 by tongjiacheng at 20230509 end*/
	sd->gpiod = sd->auth_dev->gpiod;

	// save the stick_device struct in platform_device
	dev_set_drvdata(dev, sd);

	g_sd = sd;
	stick_pdev = pdev;

	// Benchmark the IO speed of the platform. This can be removed in final version and constant hardcoded.
#ifdef NO_BENCHMARK
	// todo: replace by DTS instead of TWEAK_* ?
#if defined(TWEAK_HL_PRE) && defined(TWEAK_HL_POST) && defined(TWEAK_LH_PRE) && defined(TWEAK_LH_POST)
	// Platform GPIO latency is known
	sd->tweak_ns_high_to_low_before_change = TWEAK_HL_PRE;
	sd->tweak_ns_high_to_low_after_change = TWEAK_HL_POST;
	sd->tweak_ns_low_to_high_before_change = TWEAK_LH_PRE;
	sd->tweak_ns_low_to_high_after_change = TWEAK_LH_POST;
#else
#error "NO_BENCHMARK requires TWEAK_* to be defined"
#endif
#else
	// Measure platform GPIO latency to estimate the tweaks
	stick_benchmark_io(sd);
#endif

	mutex_lock(&sd->mutex);

	// Probe if STICK is connected
	if (stick_exit_hibernate_and_reset(sd) == ST1WIRE_OK) {
		// if STICK was found:

		// Set the chip back to hibernate
		if (stick_set_hibernate(sd) != ST1WIRE_OK) {
			DEV_WARN(sd->dev,
				 "%s : Error while setting STICK to hibernate\n",
				 __func__);
		}
		mutex_unlock(&sd->mutex);

	} else {
		mutex_unlock(&sd->mutex);
		DEV_WARN(sd->dev,
			 "%s : No answer from STICK, do not create device\n",
			 __func__);

		// Actually, return an error in that case so the driver is not loaded at all.
		ret = -ENODEV;
		goto freesd;
	}


	return 0;


freesd:
	//DEV_ERR(sd->dev, "%p: stick_pdev in probe when go to freesd\n",stick_pdev);
	mutex_destroy(&sd->mutex);
	devm_kfree(dev, sd);
	//stick_pdev = NULL;
	return ret;
}

static int stick_platform_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct stick_device *sd;


	DEV_INFO(dev, "%s\n", __func__);

	stick_pdev = NULL;

	sd = dev_get_drvdata(dev);

	mutex_destroy(&sd->mutex);
	auth_device_unregister(sd->auth_dev);
	devm_kfree(dev, sd);
	dev_set_drvdata(dev, NULL);

	return 0;
}

static const struct of_device_id stick_dev_of_match[] = {
	{
	 .compatible = "st,stick",
	 },
	{},
};

MODULE_DEVICE_TABLE(of, stick_dev_of_match);

static struct platform_driver stick_platform_driver = {
	.probe = stick_platform_probe,
	.remove = stick_platform_remove,
	.driver = {
		   .name = STICK_ID_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = stick_dev_of_match,
		   },
};

/* module load/unload record keeping */
static int __init stick_dev_init(void)
{
	return platform_driver_register(&stick_platform_driver);
}

module_init(stick_dev_init);

static void __exit stick_dev_exit(void)
{
	pr_info("%s : Unloading STICK driver\n", __func__);
	platform_driver_unregister(&stick_platform_driver);
}

module_exit(stick_dev_exit);

/* API for other kernel module stickapi */
EXPORT_SYMBOL(stick_kernel_open);
EXPORT_SYMBOL(stick_kernel_reset);
EXPORT_SYMBOL(stick_send_frame);
EXPORT_SYMBOL(stick_read_frame);
EXPORT_SYMBOL(stick_kernel_release);

MODULE_AUTHOR("STMicroelectronics");
MODULE_DESCRIPTION("STICK ST1Wire driver");
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" STICK_ID_NAME);
