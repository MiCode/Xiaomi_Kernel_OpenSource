/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
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

/*
perf-smp.c
DESCRIPTION
Manipulation, initialization of the ARMV7 Performance counter register.


EXTERNALIZED FUNCTIONS

INITIALIZATION AND SEQUENCING REQUIREMENTS
*/

/*
INCLUDE FILES FOR MODULE
*/
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/device.h>
#include <linux/interrupt.h>

#include <asm/io.h>
#include <asm/irq.h>
#include "l2_cp15_registers.h"

/*
DEFINITIONS AND DECLARATIONS FOR MODULE

This section contains definitions for constants, macros, types, variables
and other items needed by this module.
*/

/*
  Constant / Define Declarations
*/

#define PM_NUM_COUNTERS 4
#define L2_PM_ERR -1

/*------------------------------------------------------------------------
 * Global control bits
------------------------------------------------------------------------*/
#define PM_L2_GLOBAL_ENABLE     (1<<0)
#define PM_L2_EVENT_RESET       (1<<1)
#define PM_L2_CYCLE_RESET       (1<<2)
#define PM_L2_CLKDIV            (1<<3)
#define PM_L2_GLOBAL_TRACE      (1<<4)
#define PM_L2_DISABLE_PROHIBIT  (1<<5)

/*---------------------------------------------------------------------------
 * Enable and clear bits for each event/trigger
----------------------------------------------------------------------------*/
#define PM_L2EV0_ENABLE        (1<<0)
#define PM_L2EV1_ENABLE        (1<<1)
#define PM_L2EV2_ENABLE        (1<<2)
#define PM_L2EV3_ENABLE        (1<<3)
#define PM_L2_COUNT_ENABLE      (1<<31)
#define PM_L2_ALL_ENABLE        (0x8000000F)


/*-----------------------------------------------------------------------------
 * Overflow actions
------------------------------------------------------------------------------*/
#define PM_L2_OVERFLOW_NOACTION	(0)
#define PM_L2_OVERFLOW_HALT	(1)
#define PM_L2_OVERFLOW_STOP	(2)
#define PM_L2_OVERFLOW_SKIP	(3)

/*
 * Shifts for each trigger type
 */
#define PM_STOP_SHIFT		24
#define PM_RELOAD_SHIFT		22
#define PM_RESUME_SHIFT		20
#define PM_SUSPEND_SHIFT	18
#define PM_START_SHIFT		16
#define PM_STOPALL_SHIFT	15
#define PM_STOPCOND_SHIFT	12
#define PM_RELOADCOND_SHIFT	9
#define PM_RESUMECOND_SHIFT	6
#define PM_SUSPENDCOND_SHIFT	3
#define PM_STARTCOND_SHIFT	0


/*---------------------------------------------------------------------------
External control register.  What todo when various events happen.
Triggering events, etc.
----------------------------------------------------------------------------*/
#define PM_EXTTR0		0
#define PM_EXTTR1		1
#define PM_EXTTR2		2
#define PM_EXTTR3		3

#define PM_COND_NO_STOP		0
#define PM_COND_STOP_CNTOVRFLW	1
#define PM_COND_STOP_EXTERNAL	4
#define PM_COND_STOP_TRACE	5
#define PM_COND_STOP_EVOVRFLW	6
#define PM_COND_STOP_EVTYPER	7

/*--------------------------------------------------------------------------
Protect against concurrent access.  There is an index register that is
used to select the appropriate bank of registers.  If multiple processes
are writting this at different times we could have a mess...
---------------------------------------------------------------------------*/
#define PM_LOCK()
#define PM_UNLOCK()
#define PRINT printk

/*--------------------------------------------------------------------------
The Event definitions
--------------------------------------------------------------------------*/
#define L2PM_EVT_PM0_EVT0	0x00
#define L2PM_EVT_PM0_EVT1	0x01
#define L2PM_EVT_PM0_EVT2	0x02
#define L2PM_EVT_PM0_EVT3	0x03
#define L2PM_EVT_PM1_EVT0	0x04
#define L2PM_EVT_PM1_EVT1	0x05
#define L2PM_EVT_PM1_EVT2	0x06
#define L2PM_EVT_PM1_EVT3	0x07
#define L2PM_EVT_PM2_EVT0	0x08
#define L2PM_EVT_PM2_EVT1	0x09
#define L2PM_EVT_PM2_EVT2	0x0a
#define L2PM_EVT_PM2_EVT3	0x0b
#define L2PM_EVT_PM3_EVT0	0x0c
#define L2PM_EVT_PM3_EVT1	0x0d
#define L2PM_EVT_PM3_EVT2	0x0e
#define L2PM_EVT_PM3_EVT3	0x0f
#define L2PM_EVT_PM4_EVT0	0x10
#define L2PM_EVT_PM4_EVT1	0x11
#define L2PM_EVT_PM4_EVT2	0x12
#define L2PM_EVT_PM4_EVT3	0x13

/*
Type Declarations
*/

/*
Local Object Definitions
*/

unsigned long l2_pm_cycle_overflow_count;
unsigned long l2_pm_overflow_count[PM_NUM_COUNTERS];

/*---------------------------------------------------------------------------
Max number of events read from the config registers
---------------------------------------------------------------------------*/
static int pm_l2_max_events;

static int irqid;

/*
Function Definitions
*/

/*
FUNCTION  l2_pm_group_stop

DESCRIPTION  Stop a group of the performance monitors.  Event monitor 0 is bit
0, event monitor 1 bit 1, etc.  The cycle count can also be disable with
bit 31.  Macros are provided for all of the indexes including an ALL.

DEPENDENCIES

RETURN VALUE
None

SIDE EFFECTS
Stops the performance monitoring for the index passed.
*/
void pm_l2_group_stop(unsigned long mask)
{
	WCP15_L2PMCNTENCLR(mask);
}

/*
FUNCTION  l2_pm_group_start

DESCRIPTION  Start a group of the performance monitors.  Event monitor 0 is bit
0, event monitor 1 bit 1, etc.  The cycle count can also be enabled with
bit 31.  Macros are provided for all of the indexes including an ALL.

DEPENDENCIES

RETURN VALUE
None

SIDE EFFECTS
Starts the performance monitoring for the index passed.
*/
void pm_l2_group_start(unsigned long mask)
{
	WCP15_L2PMCNTENSET(mask);
}

/*
FUNCTION   l2_pm_get_overflow

DESCRIPTION  Return the overflow condition for the index passed.

DEPENDENCIES

RETURN VALUE
0 no overflow
!0 (anything else) overflow;

SIDE EFFECTS
*/
unsigned long l2_pm_get_overflow(int index)
{
  unsigned long overflow = 0;

/*
* Range check
*/
  if (index > pm_l2_max_events)
	return L2_PM_ERR;
  RCP15_L2PMOVSR(overflow);

  return overflow & (1<<index);
}

/*
FUNCTION  l2_pm_get_cycle_overflow

DESCRIPTION
Returns if the cycle counter has overflowed or not.

DEPENDENCIES

RETURN VALUE
0 no overflow
!0 (anything else) overflow;

SIDE EFFECTS
*/
unsigned long l2_pm_get_cycle_overflow(void)
{
  unsigned long overflow = 0;

  RCP15_L2PMOVSR(overflow);
  return overflow & PM_L2_COUNT_ENABLE;
}

/*
FUNCTION  l2_pm_reset_overflow

DESCRIPTION Reset the cycle counter overflow bit.

DEPENDENCIES

RETURN VALUE
None

SIDE EFFECTS
*/
void l2_pm_reset_overflow(int index)
{
  WCP15_L2PMOVSR(1<<index);
}

/*
FUNCTION  l2_pm_reset_cycle_overflow

DESCRIPTION Reset the cycle counter overflow bit.

DEPENDENCIES

RETURN VALUE
None

SIDE EFFECTS
*/
void l2_pm_reset_cycle_overflow(void)
{
  WCP15_L2PMOVSR(PM_L2_COUNT_ENABLE);
}

/*
FUNCTION l2_pm_get_cycle_count

DESCRIPTION  return the count in the cycle count register.

DEPENDENCIES

RETURN VALUE
The value in the cycle count register.

SIDE EFFECTS
*/
unsigned long l2_pm_get_cycle_count(void)
{
  unsigned long cnt = 0;
  RCP15_L2PMCCNTR(cnt);
  return cnt;
}

/*
FUNCTION  l2_pm_reset_cycle_count

DESCRIPTION  reset the value in the cycle count register

DEPENDENCIES

RETURN VALUE
NONE

SIDE EFFECTS
Resets the performance monitor cycle count register.
Any interrupts period based on this overflow will be changed
*/
void l2_pm_reset_cycle_count(void)
{
  WCP15_L2PMCNTENCLR(PM_L2_COUNT_ENABLE);
}

/*
FUNCTION  l2_pm_cycle_div_64

DESCRIPTION  Set the cycle counter to count every 64th cycle instead of
every cycle when the value passed is 1, otherwise counts every cycle.

DEPENDENCIES

RETURN VALUE
none

SIDE EFFECTS
Changes the rate at which cycles are counted.  Anything that is reading
the cycle count (pmGetCyucleCount) may get different results.
*/
void l2_pm_cycle_div_64(int enable)
{
	unsigned long enables = 0;

	RCP15_L2PMCR(enables);
	if (enable)
		WCP15_L2PMCR(enables | PM_L2_CLKDIV);
	else
		WCP15_L2PMCR(enables & ~PM_L2_CLKDIV);
}

/*
FUNCTION l2_pm_enable_cycle_counter

DESCRIPTION  Enable the cycle counter.  Sets the bit in the enable register
so the performance monitor counter starts up counting.

DEPENDENCIES

RETURN VALUE
none

SIDE EFFECTS
*/
void l2_pm_enable_cycle_counter(void)
{
/*
*  Enable the counter.
*/
  WCP15_L2PMCNTENSET(PM_L2_COUNT_ENABLE);
}

/*
FUNCTION l2_pm_disable_counter

DESCRIPTION  Disable a single counter based on the index passed.

DEPENDENCIES

RETURN VALUE
none

SIDE EFFECTS
Any triggers that are based on the stoped counter may not trigger...
*/
void l2_pm_disable_counter(int index)
{
  /*
   * Range check
   */
  if (index > pm_l2_max_events)
		return;
  WCP15_L2PMCNTENCLR(1<<index);
}

/*
FUNCTION l2_pm_enable_counter

DESCRIPTION  Enable the counter with the index passed.

DEPENDENCIES

RETURN VALUE
none.

SIDE EFFECTS
*/
void l2_pm_enable_counter(int index)
{
  /*
   * Range check
   */
  if (index > pm_l2_max_events)
		return;
  WCP15_L2PMCNTENSET(1<<index);
}

/*
FUNCTION l2_pm_set_count

DESCRIPTION  Set the number of events in a register, used for resets
passed.

DEPENDENCIES

RETURN VALUE
-1 if the index is out of range

SIDE EFFECTS
*/
int l2_pm_set_count(int index, unsigned long new_value)
{
  unsigned long reg = 0;

/*
* Range check
*/
  if (index > pm_l2_max_events)
		return L2_PM_ERR;

/*
* Lock, select the index and read the count...unlock
*/
  PM_LOCK();
  WCP15_L2PMSELR(index);
  WCP15_L2PMXEVCNTR(new_value);
  PM_UNLOCK();
  return reg;
}

int l2_pm_reset_count(int index)
{
  return l2_pm_set_count(index, 0);
}

/*
FUNCTION l2_pm_get_count

DESCRIPTION  Return the number of events that have happened for the index
passed.

DEPENDENCIES

RETURN VALUE
-1 if the index is out of range
The number of events if inrange

SIDE EFFECTS
*/
unsigned long l2_pm_get_count(int index)
{
  unsigned long reg = 0;

/*
* Range check
*/
  if (index > pm_l2_max_events)
		return L2_PM_ERR;

/*
* Lock, select the index and read the count...unlock
*/
  PM_LOCK();
  WCP15_L2PMSELR(index);
  RCP15_L2PMXEVCNTR(reg);
  PM_UNLOCK();
  return reg;
}

unsigned long get_filter_code(unsigned long event)
{
	if (event == 0x0 || event == 0x4 || event == 0x08
		|| event == 0x0c || event == 0x10)
			return 0x0001003f;
	else if (event == 0x1 || event == 0x5 || event == 0x09
		|| event == 0x0d || event == 0x11)
			return 0x0002003f;
	else if (event == 0x2 || event == 0x6 || event == 0x0a
		|| event == 0x0e || event == 0x12)
			return 0x0004003f;
	else if (event == 0x3 || event == 0x7 || event == 0x0b
		|| event == 0x0f || event == 0x13)
			return 0x0008003f;
	else
		return 0;
}

int l2_pm_set_event(int index, unsigned long event)
{
  unsigned long reg = 0;

  /*
   * Range check
   */
  if (index > pm_l2_max_events)
		return L2_PM_ERR;

  /*
   * Lock, select the index and read the count...unlock
   */
  PM_LOCK();
  WCP15_L2PMSELR(index);
  WCP15_L2PMXEVTYPER(event);
  /* WCP15_L2PMXEVFILTER(get_filter_code(event)); */
  WCP15_L2PMXEVFILTER(0x000f003f);
  PM_UNLOCK();
  return reg;
}

/*
FUNCTION  pm_set_local_bu

DESCRIPTION  Set the local BU triggers.  Note that the MSB determines if
  these are enabled or not.

DEPENDENCIES

RETURN VALUE
  NONE

SIDE EFFECTS
*/
void pm_set_local_bu(unsigned long value)
{
  WCP15_L2PMEVTYPER0(value);
}

/*
FUNCTION  pm_set_local_cb

DESCRIPTION  Set the local CB triggers.  Note that the MSB determines if
  these are enabled or not.

DEPENDENCIES

RETURN VALUE
  NONE

SIDE EFFECTS
*/
void pm_set_local_cb(unsigned long value)
{
  WCP15_L2PMEVTYPER1(value);
}

/*
FUNCTION  pm_set_local_mp

DESCRIPTION  Set the local MP triggers.  Note that the MSB determines if
  these are enabled or not.

DEPENDENCIES

RETURN VALUE
  NONE

SIDE EFFECTS
*/
void pm_set_local_mp(unsigned long value)
{
  WCP15_L2PMEVTYPER2(value);
}

/*
FUNCTION  pm_set_local_sp

DESCRIPTION  Set the local SP triggers.  Note that the MSB determines if
  these are enabled or not.

DEPENDENCIES

RETURN VALUE
  NONE

SIDE EFFECTS
*/
void pm_set_local_sp(unsigned long value)
{
  WCP15_L2PMEVTYPER3(value);
}

/*
FUNCTION  pm_set_local_scu

DESCRIPTION  Set the local SCU triggers.  Note that the MSB determines if
  these are enabled or not.

DEPENDENCIES

RETURN VALUE
  NONE

SIDE EFFECTS
*/
void pm_set_local_scu(unsigned long value)
{
  WCP15_L2PMEVTYPER4(value);
}

/*
FUNCTION  l2_pm_isr

DESCRIPTION:
  Performance Monitor interrupt service routine to capture overflows

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
static irqreturn_t l2_pm_isr(int irq, void *d)
{
	int i;

	for (i = 0; i < PM_NUM_COUNTERS; i++) {
		if (l2_pm_get_overflow(i)) {
			l2_pm_overflow_count[i]++;
			l2_pm_reset_overflow(i);
		}
	}

	if (l2_pm_get_cycle_overflow()) {
		l2_pm_cycle_overflow_count++;
		l2_pm_reset_cycle_overflow();
	}

	return IRQ_HANDLED;
}


void l2_pm_stop_all(void)
{
  WCP15_L2PMCNTENCLR(0xFFFFFFFF);
}

void l2_pm_reset_all(void)
{
  WCP15_L2PMCR(0xF);
  WCP15_L2PMOVSR(PM_L2_ALL_ENABLE);  /* overflow clear */
}

void l2_pm_start_all(void)
{
  WCP15_L2PMCNTENSET(PM_L2_ALL_ENABLE);
}

/*
FUNCTION l2_pm_initialize

DESCRIPTION  Initialize the performanca monitoring for the v7 processor.
  Ensures the cycle count is running and the event counters are enabled.

DEPENDENCIES

RETURN VALUE
  NONE

SIDE EFFECTS
*/
void l2_pm_initialize(void)
{
  unsigned long reg = 0;
  unsigned char imp;
  unsigned char id;
  unsigned char num;
  unsigned long enables = 0;
  static int initialized;

  if (initialized)
		return;
  initialized = 1;

  irqid = SC_SICL2PERFMONIRPTREQ;
  RCP15_L2PMCR(reg);
  imp = (reg>>24) & 0xFF;
  id  = (reg>>16) & 0xFF;
  pm_l2_max_events = num = (reg>>11)  & 0xFF;
  PRINT("V7 MP L2SCU Performance Monitor Capabilities\n");
  PRINT("  Implementor %c(%d)\n", imp, imp);
  PRINT("  Id %d %x\n", id, id);
  PRINT("  Num Events %d %x\n", num, num);
  PRINT("\nCycle counter enabled by default...\n");

  /*
   * Global enable, ensure the global enable is set so all
   * subsequent actions take effect.  Also resets the counts
   */
  RCP15_L2PMCR(enables);
  WCP15_L2PMCR(enables | PM_L2_GLOBAL_ENABLE | PM_L2_EVENT_RESET |
      PM_L2_CYCLE_RESET | PM_L2_CLKDIV);

  /*
   * Enable access from user space
   */

  /*
   * Install interrupt handler and the enable the interrupts
   */
  l2_pm_reset_cycle_overflow();
  l2_pm_reset_overflow(0);
  l2_pm_reset_overflow(1);
  l2_pm_reset_overflow(2);
  l2_pm_reset_overflow(3);
  l2_pm_reset_overflow(4);

	if (0 != request_irq(irqid, l2_pm_isr, 0, "l2perfmon", 0))
		printk(KERN_ERR "%s:%d request_irq returned error\n",
		__FILE__, __LINE__);
  WCP15_L2PMINTENSET(PM_L2_ALL_ENABLE);
  /*
   * Enable the cycle counter.  Default, count 1:1 no divisor.
   */
  l2_pm_enable_cycle_counter();

}

void l2_pm_free_irq(void)
{
	free_irq(irqid, 0);
}

void l2_pm_deinitialize(void)
{
  unsigned long enables = 0;
  RCP15_L2PMCR(enables);
  WCP15_L2PMCR(enables & ~PM_L2_GLOBAL_ENABLE);
}

