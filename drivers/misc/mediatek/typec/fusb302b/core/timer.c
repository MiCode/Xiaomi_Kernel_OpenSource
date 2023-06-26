/*******************************************************************************
 * @file     timer.c
 * @author   USB PD Firmware Team
 *
 * Copyright 2018 ON Semiconductor. All rights reserved.
 *
 * This software and/or documentation is licensed by ON Semiconductor under
 * limited terms and conditions. The terms and conditions pertaining to the
 * software and/or documentation are available at
 * http://www.onsemi.com/site/pdf/ONSEMI_T&C.pdf
 * ("ON Semiconductor Standard Terms and Conditions of Sale, Section 8 Software").
 *
 * DO NOT USE THIS SOFTWARE AND/OR DOCUMENTATION UNLESS YOU HAVE CAREFULLY
 * READ AND YOU AGREE TO THE LIMITED TERMS AND CONDITIONS. BY USING THIS
 * SOFTWARE AND/OR DOCUMENTATION, YOU AGREE TO THE LIMITED TERMS AND CONDITIONS.
 ******************************************************************************/
#include "timer.h"
#include "platform.h"

void TimerStart(struct TimerObj *obj, FSC_U32 time) {
  /* Grab the current time stamp and store the wait period. */
  /* Time must be > 0 */
  obj->starttime_ = platform_get_system_time();
  obj->period_ = time;

  obj->disablecount_ = TIMER_DISABLE_COUNT;

  if (obj->period_ == 0) obj->period_ = 1;
}

void TimerRestart(struct TimerObj *obj) {
  /* Grab the current time stamp for the next period. */
  obj->starttime_ = platform_get_system_time();
}

void TimerDisable(struct TimerObj *obj) {
  /* Zero means disabled */
  obj->starttime_ = obj->period_ = 0;
}

FSC_BOOL TimerDisabled(struct TimerObj *obj) {
  /* Zero means disabled */
  return (obj->period_ == 0) ? TRUE : FALSE;
}

FSC_BOOL TimerExpired(struct TimerObj *obj) {
  FSC_BOOL result = FALSE;

  if (TimerDisabled(obj)) {
      /* Disabled */
      /* TODO - possible cases where this return value might case issue? */
      result = FALSE;
  }
  else {
      /* Elapsed time >= period? */
      result = ((FSC_U32)(platform_get_system_time() - obj->starttime_) >=
               obj->period_) ? TRUE : FALSE;
  }

  /* Check for auto-disable if expired and not explicitly disabled */
  if (result) {
    if (obj->disablecount_-- == 0) {
      TimerDisable(obj);
    }
  }

  return result;
}

FSC_U32 TimerRemaining(struct TimerObj *obj)
{
  FSC_U32 currenttime = platform_get_system_time();

  if (TimerDisabled(obj)) {
    return 0;
  }

  /* If expired before it could be handled, return a minimum delay. */
  if (TimerExpired(obj)) {
    return 1;
  }

  /* Timer hasn't expired, so this should return a valid time left. */
  return (FSC_U32)(obj->starttime_ + obj->period_ - currenttime);
}

