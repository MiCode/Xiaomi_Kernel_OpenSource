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
 *                        FTS API for MP test                            **
 *                                                                        *
 **************************************************************************
 **************************************************************************
 */

#ifndef __FTS_TEST_H
#define __FTS_TEST_H

#include "ftsSoftware.h"

#define LIMITS_FILE                    "stm_fts_production_limits.csv"

#define WAIT_FOR_FRESH_FRAMES          100 //ms
#define WAIT_AFTER_SENSEOFF            50 //ms

#define TIMEOUT_ITO_TEST_RESULT        200 //ms
#define TIMEOUT_INITIALIZATION_TEST_RESULT 5000 //ms

//LABELS PRODUCTION TEST LIMITS FILE
#define MS_RAW_MIN_MAX                 "MS_RAW_DATA_MIN_MAX"
#define MS_RAW_GAP                     "MS_RAW_DATA_GAP"
#define MS_CX1_MIN_MAX                 "MS_TOUCH_ACTIVE_CX1_MIN_MAX"
#define MS_CX2_MAP_MIN                 "MS_TOUCH_ACTIVE_CX2_MIN"
#define MS_CX2_MAP_MAX                 "MS_TOUCH_ACTIVE_CX2_MAX"
#define MS_CX2_ADJH_MAP_MAX            "MS_TOUCH_ACTIVE_CX2_ADJ_HORIZONTAL"
#define MS_CX2_ADJV_MAP_MAX            "MS_TOUCH_ACTIVE_CX2_ADJ_VERTICAL"
#define MS_TOTAL_CX_MAP_MIN            "MS_TOUCH_ACTIVE_TOTAL_CX_MIN"
#define MS_TOTAL_CX_MAP_MAX            "MS_TOUCH_ACTIVE_TOTAL_CX_MAX"
#define MS_TOTAL_CX_ADJH_MAP_MAX       "MS_TOUCH_ACTIVE_TOTAL_CX_ADJ_HORIZONTAL"
#define MS_TOTAL_CX_ADJV_MAP_MAX       "MS_TOUCH_ACTIVE_TOTAL_CX_ADJ_VERTICAL"
#define SS_RAW_FORCE_MIN_MAX           "SS_RAW_DATA_FORCE_MIN_MAX"
#define SS_RAW_SENSE_MIN_MAX           "SS_RAW_DATA_SENSE_MIN_MAX"
#define SS_RAW_FORCE_GAP               "SS_RAW_DATA_FORCE_GAP"
#define SS_RAW_SENSE_GAP               "SS_RAW_DATA_SENSE_GAP"
#define SS_IX1_FORCE_MIN_MAX           "SS_TOUCH_ACTIVE_IX1_FORCE_MIN_MAX"
#define SS_IX1_SENSE_MIN_MAX           "SS_TOUCH_ACTIVE_IX1_SENSE_MIN_MAX"
#define SS_CX1_FORCE_MIN_MAX           "SS_TOUCH_ACTIVE_CX1_FORCE_MIN_MAX"
#define SS_CX1_SENSE_MIN_MAX           "SS_TOUCH_ACTIVE_CX1_SENSE_MIN_MAX"
#define SS_IX2_FORCE_MAP_MIN           "SS_TOUCH_ACTIVE_IX2_FORCE_MIN"
#define SS_IX2_FORCE_MAP_MAX           "SS_TOUCH_ACTIVE_IX2_FORCE_MAX"
#define SS_IX2_SENSE_MAP_MIN           "SS_TOUCH_ACTIVE_IX2_SENSE_MIN"
#define SS_IX2_SENSE_MAP_MAX           "SS_TOUCH_ACTIVE_IX2_SENSE_MAX"
#define SS_IX2_FORCE_ADJV_MAP_MAX      "SS_TOUCH_ACTIVE_IX2_ADJ_VERTICAL"
#define SS_IX2_SENSE_ADJH_MAP_MAX      "SS_TOUCH_ACTIVE_IX2_ADJ_HORIZONTAL"
#define SS_CX2_FORCE_MAP_MIN           "SS_TOUCH_ACTIVE_CX2_FORCE_MIN"
#define SS_CX2_FORCE_MAP_MAX           "SS_TOUCH_ACTIVE_CX2_FORCE_MAX"
#define SS_CX2_SENSE_MAP_MIN           "SS_TOUCH_ACTIVE_CX2_SENSE_MIN"
#define SS_CX2_SENSE_MAP_MAX           "SS_TOUCH_ACTIVE_CX2_SENSE_MAX"
#define SS_CX2_FORCE_ADJV_MAP_MAX      "SS_TOUCH_ACTIVE_CX2_ADJ_VERTICAL"
#define SS_CX2_SENSE_ADJH_MAP_MAX      "SS_TOUCH_ACTIVE_CX2_ADJ_HORIZONTAL"

// TOTAL SS
#define SS_TOTAL_IX_FORCE_MAP_MIN      "SS_TOUCH_ACTIVE_TOTAL_IX_FORCE_MIN"
#define SS_TOTAL_IX_FORCE_MAP_MAX      "SS_TOUCH_ACTIVE_TOTAL_IX_FORCE_MAX"
#define SS_TOTAL_IX_SENSE_MAP_MIN      "SS_TOUCH_ACTIVE_TOTAL_IX_SENSE_MIN"
#define SS_TOTAL_IX_SENSE_MAP_MAX      "SS_TOUCH_ACTIVE_TOTAL_IX_SENSE_MAX"
#define SS_TOTAL_IX_FORCE_ADJV_MAP_MAX "SS_TOUCH_ACTIVE_TOTAL_IX_ADJ_VERTICAL"
#define SS_TOTAL_IX_SENSE_ADJH_MAP_MAX "SS_TOUCH_ACTIVE_TOTAL_IX_ADJ_HORIZONTAL"
#define SS_TOTAL_CX_FORCE_MAP_MIN      "SS_TOUCH_ACTIVE_TOTAL_CX_FORCE_MIN"
#define SS_TOTAL_CX_FORCE_MAP_MAX      "SS_TOUCH_ACTIVE_TOTAL_CX_FORCE_MAX"
#define SS_TOTAL_CX_SENSE_MAP_MIN      "SS_TOUCH_ACTIVE_TOTAL_CX_SENSE_MIN"
#define SS_TOTAL_CX_SENSE_MAP_MAX      "SS_TOUCH_ACTIVE_TOTAL_CX_SENSE_MAX"
#define SS_TOTAL_CX_FORCE_ADJV_MAP_MAX "SS_TOUCH_ACTIVE_TOTAL_CX_ADJ_VERTICAL"
#define SS_TOTAL_CX_SENSE_ADJH_MAP_MAX "SS_TOUCH_ACTIVE_TOTAL_CX_ADJ_HORIZONTAL"

//KEYS
#define MS_KEY_RAW_MIN_MAX              "MS_KEY_RAW_DATA_MIN_MAX"
#define MS_KEY_CX1_MIN_MAX              "MS_KEY_CX1_MIN_MAX"
#define MS_KEY_CX2_MAP_MIN              "MS_KEY_CX2_MIN"
#define MS_KEY_CX2_MAP_MAX              "MS_KEY_CX2_MAX"
#define MS_KEY_TOTAL_CX_MAP_MIN         "MS_KEY_TOTAL_CX_MIN"
#define MS_KEY_TOTAL_CX_MAP_MAX         "MS_KEY_TOTAL_CX_MAX"

//CONSTANT TOTAL IX
#define SS_IX1_FORCE_W                  "IX1_FORCE_W"
#define SS_IX2_FORCE_W                  "IX2_FORCE_W"
#define SS_IX1_SENSE_W                  "IX1_SENSE_W"
#define SS_IX2_SENSE_W                  "IX2_SENSE_W"


#define SAVE_FLAG_RETRY                 3


struct TestToDo {
	int MutualRaw;
	int MutualRawGap;
	int MutualCx1;
	int MutualCx2;
	int MutualCx2Adj;
	int MutualCxTotal;
	int MutualCxTotalAdj;

	int MutualKeyRaw;
	int MutualKeyCx1;
	int MutualKeyCx2;
	int MutualKeyCxTotal;

	int SelfForceRaw;
	int SelfForceRawGap;
	int SelfForceIx1;
	int SelfForceIx2;
	int SelfForceIx2Adj;
	int SelfForceIxTotal;
	int SelfForceIxTotalAdj;
	int SelfForceCx1;
	int SelfForceCx2;
	int SelfForceCx2Adj;
	int SelfForceCxTotal;
	int SelfForceCxTotalAdj;

	int SelfSenseRaw;
	int SelfSenseRawGap;
	int SelfSenseIx1;
	int SelfSenseIx2;
	int SelfSenseIx2Adj;
	int SelfSenseIxTotal;
	int SelfSenseIxTotalAdj;
	int SelfSenseCx1;
	int SelfSenseCx2;
	int SelfSenseCx2Adj;
	int SelfSenseCxTotal;
	int SelfSenseCxTotalAdj;

};

int computeAdjHoriz(u8 *data, int row, int column, u8 **result);
int computeAdjHorizTotal(u16 *data, int row, int column, u16 **result);
int computeAdjVert(u8 *data, int row, int column, u8 **result);
int computeAdjVertTotal(u16 *data, int row, int column, u16 **result);
int computeTotal(u8 *data, u8 main, int row, int column, int m,
	int n, u16 **result);
int checkLimitsMinMax(short *data, int row, int column, int min, int max);
int checkLimitsMap(u8 *data, int row, int column, int *min, int *max);
int checkLimitsMapTotal(u16 *data, int row, int column, int *min, int *max);
int checkLimitsMapAdj(u8 *data, int row, int column, int *max);
int checkLimitsMapAdjTotal(u16 *data, int row, int column, int *max);
int production_test_ito(void);
int production_test_initialization(void);
int ms_compensation_tuning(void);
int ss_compensation_tuning(void);
int lp_timer_calibration(void);
int save_cx_tuning(void);
int production_test_split_initialization(int saveToFlash);
int production_test_main(char *pathThresholds, int stop_on_fail, int saveInit,
	struct TestToDo *todo, u32 signature);
int production_test_ms_raw(char *path_limits, int stop_on_fail,
	struct TestToDo *todo);
int production_test_ms_cx(char *path_limits, int stop_on_fail,
	struct TestToDo *todo);
int production_test_ss_raw(char *path_limits, int stop_on_fail,
	struct TestToDo *todo);
int production_test_ss_ix_cx(char *path_limits, int stop_on_fail,
	struct TestToDo *todo);
int production_test_data(char *path_limits, int stop_on_fail,
	struct TestToDo *todo);
int production_test_ms_key_cx(char *path_limits, int stop_on_fail,
	struct TestToDo *todo);
int production_test_ms_key_raw(char *path_limits);
int save_mp_flag(u32 signature);
int parseProductionTestLimits(char *path, char *label, int **data,
	int *row, int *column);
int readLine(char *data, char *line, int size, int *n);
#endif
