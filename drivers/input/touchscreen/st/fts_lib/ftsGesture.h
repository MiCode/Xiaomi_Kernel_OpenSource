/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * FTS Capacitive touch screen controller (FingerTipS)
 *
 * Copyright (C) 2016-2019, STMicroelectronics Limited.
 * Authors: AMG(Analog Mems Group) <marco.cali@st.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 *
 **************************************************************************
 **                        STMicroelectronics                            **
 **************************************************************************
 **                        marco.cali@st.com                             **
 **************************************************************************
 *                                                                        *
 *                     FTS Gesture Utilities                              *
 *                                                                        *
 **************************************************************************
 **************************************************************************
 */

#ifndef _FTS_GESTURE_H_
#define _FTS_GESTURE_H_

#include "ftsHardware.h"

#define GESTURE_MASK_SIZE              8

//max number of gestures coordinates reported
#define GESTURE_COORDS_REPORT_MAX      32

// for each custom gesture should be provided 30
//points (each point is a couple of x,y)
#define GESTURE_CUSTOM_POINTS          (30 * 2)

//fw support up to 5 custom gestures
#define GESTURE_CUSTOM_NUMBER          5

#ifdef FTM3

//number of points to transfer with each I2C transaction
#define TEMPLATE_CHUNK                 (10 * 2)
#else
//number of points to transfer with each I2C transaction
#define TEMPLATE_CHUNK                 (5 * 2)
#endif


//Gesture IDs
#define GES_ID_UNKNOWN                 0x00 //no meaningful gesture
#define GES_ID_DBLTAP                  0x01 //Double Tap
#define GES_ID_O                       0x02 //'O'
#define GES_ID_C                       0x03 //'C'
#define GES_ID_M                       0x04 //'M'
#define GES_ID_W                       0x05 //'W'
#define GES_ID_E                       0x06 //'e'
#define GES_ID_HFLIP_L2R               0x07 //Left to right line
#define GES_ID_HFLIP_R2L               0x08 //Right to left line
#define GES_ID_VFLIP_T2D               0x09 //Top to bottom line
#define GES_ID_VFLIP_D2T               0x0A //Bottom to Top line
#define GES_ID_L                       0x0B //'L'
#define GES_ID_F                       0x0C //'F'
#define GES_ID_V                       0x0D //'V'
#define GES_ID_AT                      0x0E //'@'
#define GES_ID_S                       0x0F //'S'
#define GES_ID_Z                       0x10 //'Z'
#define GES_ID_CUST1                   0x11 //Custom gesture 1
#define GES_ID_CUST2                   0x12 //Custom gesture 2
#define GES_ID_CUST3                   0x13 //Custom gesture 3
#define GES_ID_CUST4                   0x14 //Custom gesture 4
#define GES_ID_CUST5                   0x15 //Custom gesture 5
#define GES_ID_LEFTBRACE               0x20 //'<'
#define GES_ID_RIGHTBRACE              0x21 //'>'

#define GESTURE_CUSTOM_OFFSET          GES_ID_CUST1

//Command sub-type
#define GESTURE_ENABLE                 0x01
#define GESTURE_DISABLE                0x02
#define GESTURE_ENB_CHECK              0x03
#define GESTURE_START_ADD              0x10
#define GESTURE_DATA_ADD               0x11
#define        GESTURE_FINISH_ADD      0x12
#define GETURE_REMOVE_CUSTOM           0x13
#define GESTURE_CHECK_CUSTOM           0x14


//Event sub-type
#define EVENT_TYPE_ENB                 0x04
#define EVENT_TYPE_CHECK_ENB           0x03
#define EVENT_TYPE_GESTURE_DTC1        0x01
#define EVENT_TYPE_GESTURE_DTC2        0x02

int updateGestureMask(u8 *mask, int size, int en);
int disableGesture(u8 *mask, int size);
int enableGesture(u8 *mask, int size);
int startAddCustomGesture(u8 gestureID);
int finishAddCustomGesture(u8 gestureID);
int loadCustomGesture(u8 *template, u8 gestureID);
int reloadCustomGesture(void);
int enterGestureMode(int reload);
int addCustomGesture(u8 *data, int size, u8 gestureID);
int removeCustomGesture(u8 gestureID);
int isAnyGestureActive(void);
int gestureIDtoGestureMask(u8 id, u8 *mask);
int readGestureCoords(u8 *event);
int getGestureCoords(u16 *x, u16 *y);

#endif
