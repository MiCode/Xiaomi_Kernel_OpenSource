/*******************************************************************************
 * @file     timer.h
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
#ifndef _FSC_TIMER_H_
#define _FSC_TIMER_H_

#include "platform.h"

#define TIMER_DISABLE_COUNT 4   /* Should cause the timer to be disabled
                                 * during core_get_next_timeout calls.
                                 */

/* Struct object to contain the timer related members */
struct TimerObj {
  FSC_U32 starttime_;           /* Time-stamp when timer started */
  FSC_U32 period_;              /* Timer period */
  FSC_U8  disablecount_;        /* Auto-disable timers if they keep getting
                                 * checked and aren't explicitly disabled.
                                 */
};

/**
 * @brief Start the timer
 *
 * Records the current system timestamp as well as the time value
 *
 * @param Timer object
 * @param time non-zero, in units of (milliseconds / (platform scale value)).
 * @return None
 */
void TimerStart(struct TimerObj *obj, FSC_U32 time);

/**
 * @brief Restart the timer using the last used delay value.
 *
 * @param Timer object
 * @return None
 */
void TimerRestart(struct TimerObj *obj);

/**
 * @brief Set time and period to zero to indicate no current period.
 *
 * @param Timer object
 * @return TRUE if the timer is currently disabled
 */
void TimerDisable(struct TimerObj *obj);
FSC_BOOL TimerDisabled(struct TimerObj *obj);

/**
 * @brief Determine if timer has expired
 *
 * Check the current system time stamp against (start_time + time_period)
 *
 * @param Timer object
 * @return TRUE if time period has finished.
 */
FSC_BOOL TimerExpired(struct TimerObj *obj);

/**
 * @brief Returns the time remaining in microseconds, or zero if disabled/done.
 *
 * @param Timer object
 */
FSC_U32 TimerRemaining(struct TimerObj *obj);

#endif /* _FSC_TIMER_H_ */

