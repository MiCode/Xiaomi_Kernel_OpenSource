/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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
perf-v7.c
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
#include "cp15_registers.h"

/*
DEFINITIONS AND DECLARATIONS FOR MODULE

This section contains definitions for constants, macros, types, variables
and other items needed by this module.
*/

/*
  Constant / Define Declarations
*/

#define PM_NUM_COUNTERS 4
#define PM_V7_ERR -1

/*------------------------------------------------------------------------
 * Global control bits
------------------------------------------------------------------------*/
#define PM_GLOBAL_ENABLE     (1<<0)
#define PM_EVENT_RESET       (1<<1)
#define PM_CYCLE_RESET       (1<<2)
#define PM_CLKDIV            (1<<3)
#define PM_GLOBAL_TRACE      (1<<4)
#define PM_DISABLE_PROHIBIT  (1<<5)

/*---------------------------------------------------------------------------
 * Enable and clear bits for each event/trigger
----------------------------------------------------------------------------*/
#define PM_EV0_ENABLE        (1<<0)
#define PM_EV1_ENABLE        (1<<1)
#define PM_EV2_ENABLE        (1<<2)
#define PM_EV3_ENABLE        (1<<3)
#define PM_COUNT_ENABLE      (1<<31)
#define PM_ALL_ENABLE        (0x8000000F)


/*-----------------------------------------------------------------------------
 * Overflow actions
------------------------------------------------------------------------------*/
#define PM_OVERFLOW_NOACTION	(0)
#define PM_OVERFLOW_HALT	(1)
#define PM_OVERFLOW_STOP	(2)
#define PM_OVERFLOW_SKIP	(3)

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
#define PM_EVT_SW_INCREMENT	0
#define PM_EVT_L1_I_MISS	1
#define PM_EVT_ITLB_MISS	2
#define PM_EVT_L1_D_MISS	3
#define PM_EVT_L1_D_ACCESS	4
#define PM_EVT_DTLB_MISS	5
#define PM_EVT_DATA_READ	6
#define PM_EVT_DATA_WRITE	7
#define PM_EVT_INSTRUCTION	8
#define PM_EVT_EXCEPTIONS	9
#define PM_EVT_EXCEPTION_RET	10
#define PM_EVT_CTX_CHANGE	11
#define PM_EVT_PC_CHANGE	12
#define PM_EVT_BRANCH		13
#define PM_EVT_RETURN		14
#define PM_EVT_UNALIGNED	15
#define PM_EVT_BRANCH_MISS	16
#define PM_EVT_EXTERNAL0	0x40
#define PM_EVT_EXTERNAL1	0x41
#define PM_EVT_EXTERNAL2	0x42
#define PM_EVT_EXTERNAL3	0x43
#define PM_EVT_TRACE0		0x44
#define PM_EVT_TRACE1		0x45
#define PM_EVT_TRACE2		0x46
#define PM_EVT_TRACE3		0x47
#define PM_EVT_PM0		0x48
#define PM_EVT_PM1		0x49
#define PM_EVT_PM2		0x4a
#define PM_EVT_PM3		0x4b
#define PM_EVT_LPM0_EVT0	0x4c
#define PM_EVT_LPM0_EVT1	0x4d
#define PM_EVT_LPM0_EVT2	0x4e
#define PM_EVT_LPM0_EVT3	0x4f
#define PM_EVT_LPM1_EVT0	0x50
#define PM_EVT_LPM1_EVT1	0x51
#define PM_EVT_LPM1_EVT2	0x52
#define PM_EVT_LPM1_EVT3	0x53
#define PM_EVT_LPM2_EVT0	0x54
#define PM_EVT_LPM2_EVT1	0x55
#define PM_EVT_LPM2_EVT2	0x56
#define PM_EVT_LPM2_EVT3	0x57
#define PM_EVT_L2_EVT0		0x58
#define PM_EVT_L2_EVT1		0x59
#define PM_EVT_L2_EVT2		0x5a
#define PM_EVT_L2_EVT3		0x5b
#define PM_EVT_VLP_EVT0		0x5c
#define PM_EVT_VLP_EVT1		0x5d
#define PM_EVT_VLP_EVT2		0x5e
#define PM_EVT_VLP_EVT3		0x5f

/*
Type Declarations
*/

/*--------------------------------------------------------------------------
A performance monitor trigger setup/initialization structure.  Contains
all of the fields necessary to setup a complex trigger with the internal
performance monitor.
---------------------------------------------------------------------------*/
struct pm_trigger_s {
  int index;
  int event_type;
  bool interrupt;
  bool overflow_enable;
  bool event_export;
  unsigned char overflow_action;
  unsigned char stop_index;
  unsigned char reload_index;
  unsigned char resume_index;
  unsigned char suspend_index;
  unsigned char start_index;
  bool  overflow_stop;
  unsigned char stop_condition;
  unsigned char reload_condition;
  unsigned char resume_condition;
  unsigned char suspend_condition;
  unsigned char start_condition;
};

/*
* Name and index place holder so we can display the event
*/
struct pm_name_s {
  unsigned long index;
  char          *name;
};

/*
Local Object Definitions
*/

unsigned long pm_cycle_overflow_count;
unsigned long pm_overflow_count[PM_NUM_COUNTERS];

/*---------------------------------------------------------------------------
Max number of events read from the config registers
---------------------------------------------------------------------------*/
static int pm_max_events;

/*--------------------------------------------------------------------------
Storage area for each of the triggers
*---------------------------------------------------------------------------*/
static struct pm_trigger_s pm_triggers[4];

/*--------------------------------------------------------------------------
Names and indexes of the events
--------------------------------------------------------------------------*/
static struct pm_name_s pm_names[] = {
  { PM_EVT_SW_INCREMENT,    "SW Increment"},
  { PM_EVT_L1_I_MISS,       "L1 I MISS"},
  { PM_EVT_ITLB_MISS,       "L1 ITLB MISS"},
  { PM_EVT_L1_D_MISS,       "L1 D MISS"},
  { PM_EVT_L1_D_ACCESS,     "L1 D ACCESS"},
  { PM_EVT_DTLB_MISS,       "DTLB MISS"},
  { PM_EVT_DATA_READ,       "DATA READ"},
  { PM_EVT_DATA_WRITE,      "DATA WRITE"},
  { PM_EVT_INSTRUCTION,     "INSTRUCTIONS"},
  { PM_EVT_EXCEPTIONS,      "EXCEPTIONS"},
  { PM_EVT_EXCEPTION_RET,   "EXCEPTION RETURN"},
  { PM_EVT_CTX_CHANGE,      "CTX CHANGE"},
  { PM_EVT_PC_CHANGE,       "PC CHANGE"},
  { PM_EVT_BRANCH,          "BRANCH"},
  { PM_EVT_RETURN,          "RETURN"},
  { PM_EVT_UNALIGNED,       "UNALIGNED"},
  { PM_EVT_BRANCH_MISS,     "BRANCH MISS"},
  { PM_EVT_EXTERNAL0,       "EXTERNAL 0"},
  { PM_EVT_EXTERNAL1,       "EXTERNAL 1"},
  { PM_EVT_EXTERNAL2,       "EXTERNAL 2"},
  { PM_EVT_EXTERNAL3,       "EXTERNAL 3"},
  { PM_EVT_TRACE0,          "TRACE 0"},
  { PM_EVT_TRACE1,          "TRACE 1"},
  { PM_EVT_TRACE2,          "TRACE 2"},
  { PM_EVT_TRACE3,          "TRACE 3"},
  { PM_EVT_PM0,             "PM0"},
  { PM_EVT_PM1,             "PM1"},
  { PM_EVT_PM2,             "PM2"},
  { PM_EVT_PM3,             "PM3"},
  { PM_EVT_LPM0_EVT0,       "LPM0 E0"},
  { PM_EVT_LPM0_EVT1,       "LPM0 E1"},
  { PM_EVT_LPM0_EVT2 ,      "LPM0 E2"},
  { PM_EVT_LPM0_EVT3,       "LPM0 E3"},
  { PM_EVT_LPM1_EVT0,       "LPM1 E0"},
  { PM_EVT_LPM1_EVT1,       "LPM1 E1"},
  { PM_EVT_LPM1_EVT2,       "LPM1 E2"},
  { PM_EVT_LPM1_EVT3,       "LPM1 E3"},
  { PM_EVT_LPM2_EVT0,       "LPM2 E0"},
  { PM_EVT_LPM2_EVT1 ,      "LPM2 E1"},
  { PM_EVT_LPM2_EVT2,       "LPM2 E2"},
  { PM_EVT_LPM2_EVT3,       "LPM2 E3"},
  { PM_EVT_L2_EVT0 ,        "L2 E0"},
  { PM_EVT_L2_EVT1,         "L2 E1"},
  { PM_EVT_L2_EVT2,         "L2 E2"},
  { PM_EVT_L2_EVT3 ,        "L2 E3"},
  { PM_EVT_VLP_EVT0 ,       "VLP E0"},
  { PM_EVT_VLP_EVT1,        "VLP E1"},
  { PM_EVT_VLP_EVT2,        "VLP E2"},
  { PM_EVT_VLP_EVT3,        "VLP E3"},
};

static int irqid;

/*
Function Definitions
*/

/*
FUNCTION  pm_find_event_name

DESCRIPTION Find the name associated with the event index passed and return
the pointer.

DEPENDENCIES

RETURN VALUE
Pointer to text string containing the name of the event or pointer to
an error string.  Either way access to the returned string will not
cause an access error.

SIDE EFFECTS
*/
char *pm_find_event_name(unsigned long index)
{
	unsigned long i = 0;

	while (pm_names[i].index != -1) {
		if (pm_names[i].index == index)
			return pm_names[i].name;
		i++;
	}
	return "BAD INDEX";
}

/*
FUNCTION  pm_group_stop

DESCRIPTION  Stop a group of the performance monitors.  Event monitor 0 is bit
0, event monitor 1 bit 1, etc.  The cycle count can also be disabled with
bit 31.  Macros are provided for all of the indexes including an ALL.

DEPENDENCIES

RETURN VALUE
None

SIDE EFFECTS
Stops the performance monitoring for the index passed.
*/
void pm_group_stop(unsigned long mask)
{
	WCP15_PMCNTENCLR(mask);
}

/*
FUNCTION  pm_group_start

DESCRIPTION  Start a group of the performance monitors.  Event monitor 0 is bit
0, event monitor 1 bit 1, etc.  The cycle count can also be enabled with
bit 31.  Macros are provided for all of the indexes including an ALL.

DEPENDENCIES

RETURN VALUE
None

SIDE EFFECTS
Starts the performance monitoring for the index passed.
*/
void pm_group_start(unsigned long mask)
{
	WCP15_PMCNTENSET(mask);
}

/*
FUNCTION pm_cycle_overflow_action

DESCRIPTION  Action to take for an overflow of the cycle counter.

DEPENDENCIES

RETURN VALUE
None

SIDE EFFECTS
Modify the state actions for overflow
*/
void pm_cycle_overflow_action(int action)
{
	unsigned long reg = 0;

	if ((action > PM_OVERFLOW_SKIP) || (action < 0))
		return;

	RCP15_PMACTLR(reg);
	reg &= ~(1<<30);   /*clear it*/
	WCP15_PMACTLR(reg | (action<<30));
}

/*
FUNCTION   pm_get_overflow

DESCRIPTION  Return the overflow condition for the index passed.

DEPENDENCIES

RETURN VALUE
0 no overflow
!0 (anything else) overflow;

SIDE EFFECTS
*/
unsigned long pm_get_overflow(int index)
{
  unsigned long overflow = 0;

/*
* Range check
*/
  if (index > pm_max_events)
	return PM_V7_ERR;
  RCP15_PMOVSR(overflow);

  return overflow & (1<<index);
}

/*
FUNCTION  pm_get_cycle_overflow

DESCRIPTION
Returns if the cycle counter has overflowed or not.

DEPENDENCIES

RETURN VALUE
0 no overflow
!0 (anything else) overflow;

SIDE EFFECTS
*/
unsigned long pm_get_cycle_overflow(void)
{
  unsigned long overflow = 0;

  RCP15_PMOVSR(overflow);
  return overflow & PM_COUNT_ENABLE;
}

/*
FUNCTION  pm_reset_overflow

DESCRIPTION Reset the cycle counter overflow bit.

DEPENDENCIES

RETURN VALUE
None

SIDE EFFECTS
*/
void pm_reset_overflow(int index)
{
  WCP15_PMOVSR(1<<index);
}

/*
FUNCTION  pm_reset_cycle_overflow

DESCRIPTION Reset the cycle counter overflow bit.

DEPENDENCIES

RETURN VALUE
None

SIDE EFFECTS
*/
void pm_reset_cycle_overflow(void)
{
  WCP15_PMOVSR(PM_COUNT_ENABLE);
}

/*
FUNCTION pm_get_cycle_count

DESCRIPTION  return the count in the cycle count register.

DEPENDENCIES

RETURN VALUE
The value in the cycle count register.

SIDE EFFECTS
*/
unsigned long pm_get_cycle_count(void)
{
  unsigned long cnt = 0;
  RCP15_PMCCNTR(cnt);
  return cnt;
}

/*
FUNCTION  pm_reset_cycle_count

DESCRIPTION  reset the value in the cycle count register

DEPENDENCIES

RETURN VALUE
NONE

SIDE EFFECTS
Resets the performance monitor cycle count register.
Any interrupts period based on this overflow will be changed
*/
void pm_reset_cycle_count(void)
{
  WCP15_PMCNTENCLR(PM_COUNT_ENABLE);
}

/*
FUNCTION  pm_cycle_div_64

DESCRIPTION  Set the cycle counter to count every 64th cycle instead of
every cycle when the value passed is 1, otherwise counts every cycle.

DEPENDENCIES

RETURN VALUE
none

SIDE EFFECTS
Changes the rate at which cycles are counted.  Anything that is reading
the cycle count (pmGetCyucleCount) may get different results.
*/
void pm_cycle_div_64(int enable)
{
	unsigned long enables = 0;

	RCP15_PMCR(enables);
	if (enable)
		WCP15_PMCR(enables | PM_CLKDIV);
	else
		WCP15_PMCR(enables & ~PM_CLKDIV);
}

/*
FUNCTION pm_enable_cycle_counter

DESCRIPTION  Enable the cycle counter.  Sets the bit in the enable register
so the performance monitor counter starts up counting.

DEPENDENCIES

RETURN VALUE
none

SIDE EFFECTS
*/
void pm_enable_cycle_counter(void)
{
/*
*  Enable the counter.
*/
  WCP15_PMCNTENSET(PM_COUNT_ENABLE);
}

/*
FUNCTION pm_disable_counter

DESCRIPTION  Disable a single counter based on the index passed.

DEPENDENCIES

RETURN VALUE
none

SIDE EFFECTS
Any triggers that are based on the stoped counter may not trigger...
*/
void pm_disable_counter(int index)
{
  /*
   * Range check
   */
  if (index > pm_max_events)
		return;
  WCP15_PMCNTENCLR(1<<index);
}

/*
FUNCTION pm_enable_counter

DESCRIPTION  Enable the counter with the index passed.

DEPENDENCIES

RETURN VALUE
none.

SIDE EFFECTS
*/
void pm_enable_counter(int index)
{
  /*
   * Range check
   */
  if (index > pm_max_events)
		return;
  WCP15_PMCNTENSET(1<<index);
}

/*
FUNCTION pm_set_count

DESCRIPTION  Set the number of events in a register, used for resets
passed.

DEPENDENCIES

RETURN VALUE
-1 if the index is out of range

SIDE EFFECTS
*/
int pm_set_count(int index, unsigned long new_value)
{
  unsigned long reg = 0;

/*
* Range check
*/
  if (index > pm_max_events)
		return PM_V7_ERR;

/*
* Lock, select the index and read the count...unlock
*/
  PM_LOCK();
  WCP15_PMSELR(index);
  WCP15_PMXEVCNTR(new_value);
  PM_UNLOCK();
  return reg;
}

int pm_reset_count(int index)
{
  return pm_set_count(index, 0);
}

/*
FUNCTION pm_get_count

DESCRIPTION  Return the number of events that have happened for the index
passed.

DEPENDENCIES

RETURN VALUE
-1 if the index is out of range
The number of events if inrange

SIDE EFFECTS
*/
unsigned long pm_get_count(int index)
{
  unsigned long reg = 0;

/*
* Range check
*/
  if (index > pm_max_events)
		return PM_V7_ERR;

/*
* Lock, select the index and read the count...unlock
*/
  PM_LOCK();
  WCP15_PMSELR(index);
  RCP15_PMXEVCNTR(reg);
  PM_UNLOCK();
  return reg;
}

/*
FUNCTION pm_show_event_info

DESCRIPTION Display (print) the information about the event at the index
passed.  Shows the index, name and count if a valid index is passed.  If
the index is not valid, then nothing is displayed.

DEPENDENCIES

RETURN VALUE
None

SIDE EFFECTS
*/
void pm_show_event_info(unsigned long index)
{
  unsigned long count;
  unsigned long event_type;

  if (index > pm_max_events)
		return;
  if (pm_triggers[index].index > pm_max_events)
		return;

  count = pm_get_count(index);
  event_type = pm_triggers[index].event_type;

  PRINT("Event %ld Trigger %s(%ld) count:%ld\n", index,
     pm_find_event_name(event_type), event_type, count);
}

/*
FUNCTION  pm_event_init

DESCRIPTION  Given the struct pm_trigger_s info passed, configure the event.
This can be a complex trigger or a simple trigger.  Any old values in the
event are lost.

DEPENDENCIES

RETURN VALUE
status

SIDE EFFECTS
stops and clears the event at the index passed.
*/
int pm_event_init(struct pm_trigger_s *data)
{
  unsigned long trigger;
  unsigned long actlr = 0;

  if (0 == data)
		return PM_V7_ERR;
  if (data->index > pm_max_events)
		return PM_V7_ERR;

  /*
   * Setup the trigger based ont he passed values
   */
  trigger = ((data->overflow_enable&1)<<31) |
	((data->event_export&1)<<30) |
	((data->stop_index&3)<<PM_STOP_SHIFT) |
	((data->reload_index&3)<<PM_RELOAD_SHIFT) |
	((data->resume_index&3)<<PM_RESUME_SHIFT) |
	((data->suspend_index&3)<<PM_SUSPEND_SHIFT) |
	((data->start_index&3)<<PM_START_SHIFT) |
	((data->overflow_stop&1)<<PM_STOPALL_SHIFT) |
	((data->stop_condition&7)<<PM_STOPCOND_SHIFT) |
	((data->reload_condition&7)<<PM_RELOADCOND_SHIFT) |
	((data->resume_condition&7)<<PM_RESUMECOND_SHIFT) |
	((data->suspend_condition&7)<<PM_SUSPENDCOND_SHIFT) |
	((data->start_condition&7)<<PM_STARTCOND_SHIFT);

  /*
   * Disable this counter while we are updating.
   */
  pm_disable_counter(data->index);

  /*
   * Lock, select the bank, set the trigger event and the event type
   * then unlock.
   */
  PM_LOCK();
  RCP15_PMACTLR(actlr);
  actlr &= ~(3<<(data->index<<1));
  WCP15_PMACTLR(actlr | ((data->overflow_action&3) << (data->index<<1)));
  WCP15_PMSELR(data->index);
  WCP15_PMXEVTYPER(data->event_type);
  WCP15_PMXEVCNTCR(trigger);
  PM_UNLOCK();

  /*
   * Make a copy of the trigger so we know what it is when/if it triggers.
   */
  memcpy(&pm_triggers[data->index], data, sizeof(*data));

  /*
   * We do not re-enable this here so events can be started together with
   * pm_group_start() that way an accurate measure can be taken...
   */

  return 0;
}

int pm_set_event(int index, unsigned long event)
{
  unsigned long reg = 0;

  /*
   * Range check
   */
  if (index > pm_max_events)
		return PM_V7_ERR;

  /*
   * Lock, select the index and read the count...unlock
   */
  PM_LOCK();
  WCP15_PMSELR(index);
  WCP15_PMXEVTYPER(event);
  PM_UNLOCK();
  return reg;
}

/*
FUNCTION  pm_set_local_iu

DESCRIPTION  Set the local IU triggers.  Note that the MSB determines if
  these are enabled or not.

DEPENDENCIES

RETURN VALUE
  NONE

SIDE EFFECTS
*/
void pm_set_local_iu(unsigned long value)
{
  WCP15_LPM0EVTYPER(value);
}

/*
FUNCTION  pm_set_local_iu

DESCRIPTION  Set the local IU triggers.  Note that the MSB determines if
  these are enabled or not.

DEPENDENCIES

RETURN VALUE
  NONE

SIDE EFFECTS
*/
void pm_set_local_xu(unsigned long value)
{
  WCP15_LPM1EVTYPER(value);
}

/*
FUNCTION  pm_set_local_su

DESCRIPTION  Set the local SU triggers.  Note that the MSB determines if
  these are enabled or not.

DEPENDENCIES

RETURN VALUE
  NONE

SIDE EFFECTS
*/
void pm_set_local_su(unsigned long value)
{
  WCP15_LPM2EVTYPER(value);
}

/*
FUNCTION  pm_set_local_l2

DESCRIPTION  Set the local L2 triggers.  Note that the MSB determines if
  these are enabled or not.

DEPENDENCIES

RETURN VALUE
  NONE

SIDE EFFECTS
*/
void pm_set_local_l2(unsigned long value)
{
  WCP15_L2LPMEVTYPER(value);
}

/*
FUNCTION  pm_set_local_vu

DESCRIPTION  Set the local VU triggers.  Note that the MSB determines if
  these are enabled or not.

DEPENDENCIES

RETURN VALUE
  NONE

SIDE EFFECTS
*/
void pm_set_local_vu(unsigned long value)
{
  WCP15_VLPMEVTYPER(value);
}

/*
FUNCTION  pm_isr

DESCRIPTION:
  Performance Monitor interrupt service routine to capture overflows

DEPENDENCIES

RETURN VALUE

SIDE EFFECTS
*/
static irqreturn_t pm_isr(int irq, void *d)
{
	int i;

	for (i = 0; i < PM_NUM_COUNTERS; i++) {
		if (pm_get_overflow(i)) {
			pm_overflow_count[i]++;
			pm_reset_overflow(i);
		}
	}

	if (pm_get_cycle_overflow()) {
		pm_cycle_overflow_count++;
		pm_reset_cycle_overflow();
	}

	return IRQ_HANDLED;
}


void pm_stop_all(void)
{
  WCP15_PMCNTENCLR(0xFFFFFFFF);
}

void pm_reset_all(void)
{
  WCP15_PMCR(0xF);
  WCP15_PMOVSR(PM_ALL_ENABLE);  /* overflow clear */
}

void pm_start_all(void)
{
  WCP15_PMCNTENSET(PM_ALL_ENABLE);
}

/*
FUNCTION pm_initialize

DESCRIPTION  Initialize the performanca monitoring for the v7 processor.
  Ensures the cycle count is running and the event counters are enabled.

DEPENDENCIES

RETURN VALUE
  NONE

SIDE EFFECTS
*/
void pm_initialize(void)
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

  irqid = INT_ARMQC_PERFMON;
  RCP15_PMCR(reg);
  imp = (reg>>24) & 0xFF;
  id  = (reg>>16) & 0xFF;
  pm_max_events = num = (reg>>11)  & 0xFF;
  PRINT("V7Performance Monitor Capabilities\n");
  PRINT("  Implementor %c(%d)\n", imp, imp);
  PRINT("  Id %d %x\n", id, id);
  PRINT("  Num Events %d %x\n", num, num);
  PRINT("\nCycle counter enabled by default...\n");

  /*
   * Global enable, ensure the global enable is set so all
   * subsequent actions take effect.  Also resets the counts
   */
  RCP15_PMCR(enables);
  WCP15_PMCR(enables | PM_GLOBAL_ENABLE | PM_EVENT_RESET |
      PM_CYCLE_RESET | PM_CLKDIV);

  /*
   * Enable access from user space
   */
  WCP15_PMUSERENR(1);
  WCP15_PMACTLR(1);

  /*
   * Install interrupt handler and the enable the interrupts
   */
  pm_reset_cycle_overflow();
  pm_reset_overflow(0);
  pm_reset_overflow(1);
  pm_reset_overflow(2);
  pm_reset_overflow(3);

	if (0 != request_irq(irqid, pm_isr, 0, "perfmon", 0))
		printk(KERN_ERR "%s:%d request_irq returned error\n",
		__FILE__, __LINE__);
  WCP15_PMINTENSET(PM_ALL_ENABLE);
  /*
   * Enable the cycle counter.  Default, count 1:1 no divisor.
   */
  pm_enable_cycle_counter();

}

void pm_free_irq(void)
{
	free_irq(irqid, 0);
}

void pm_deinitialize(void)
{
  unsigned long enables = 0;
  RCP15_PMCR(enables);
  WCP15_PMCR(enables & ~PM_GLOBAL_ENABLE);
}
