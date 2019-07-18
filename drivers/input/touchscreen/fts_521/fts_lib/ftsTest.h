/*

**************************************************************************
**                        STMicroelectronics							**
**************************************************************************
**                        marco.cali@st.com								**
**************************************************************************
*                                                                        *
*						FTS API for MP test								 *
*                                                                        *
**************************************************************************
**************************************************************************

*/

/*!
* \file ftsTest.h
* \brief Contains all the definitions and structs related to the Mass Production Test
*/

#ifndef FTS_TEST_H
#define FTS_TEST_H

#include "ftsSoftware.h"

#ifndef LIMITS_H_FILE
#define LIMITS_FILE						"stm_fts_production_limits.csv"			/*Name of the Production Test Limit File*/
#else
#define LIMITS_FILE						"NULL"
#endif

#define WAIT_FOR_FRESH_FRAMES			200										/*Time in ms to wait after start to sensing before reading a frame*/
#define WAIT_AFTER_SENSEOFF				50										/*Time in ms to wait after stop sensing and before reading a frame from memory*/

#define NO_INIT							0										/*No Initialization required during the MP*/

#define RETRY_INIT_BOOT					3										/*number of retry of the initialization process at boot*/

/** @defgroup mp_test Mass Production Test
 * Mass production test API.
 * Mass Production Test (MP) should be executed at least one time in the life of every device \n
 * It used to verify that tit is not present any hardware damage and initialize some value of the chip in order to guarantee the working performance \n
 * The MP test is made up by 3 steps:
 * - ITO test = production_test_ito() \n
 * - Initialization = production_test_initialization() \n
 * - Data Test = production_test_data(), it is possible to select which items test thanks to the TestToDo struct\n
 * To execute the Data Test it is mandatory load some thresholds that are stored in the Limit File.
 * @{
 */

/** @defgroup limit_file Limit File
 * @ingroup mp_test
 * Production Test Limit File is a csv which contains thresholds of the data to test.
 * This file can be loaded from the file system or stored as a header file according to the LIMITS_H_FILE define \n
 * For each selectable test item there can be one or more associated labels which store the corresponding thresholds \n
 * @{
 */

/** @defgroup test_labels Test Items Labels
 * @ingroup limit_file
 * Labels present in the Limit File and associated to the test items of TestToDo
 * @{
 */
#define MS_RAW_MIN_MAX					"MS_RAW_DATA_MIN_MAX"
#define MS_RAW_EACH_NODE_MIN			"MS_RAW_DATA_EACH_MIN"
#define MS_RAW_EACH_NODE_MAX			"MS_RAW_DATA_EACH_MAX"
#define MS_RAW_GAP						"MS_RAW_DATA_GAP"
#define MS_RAW_ADJH						"MS_RAW_DATA_ADJ_HORIZONTAL"
#define MS_RAW_ADJV						"MS_RAW_DATA_ADJ_VERTICAL"
#define MS_RAW_ITO_ADJH					"MS_RAW_ITO_DATA_ADJ_HORIZONTAL"
#define MS_RAW_ITO_ADJV					"MS_RAW_ITO_DATA_ADJ_VERTICAL"
#define MS_RAW_LP_MIN_MAX				"MS_RAW_LOWPOWER_DATA_MIN_MAX"
#define MS_RAW_LP_GAP					"MS_RAW_LOWPOWER_DATA_GAP"
#define MS_RAW_LP_ADJH					"MS_RAW_LOWPOWER_DATA_ADJ_HORIZONTAL"
#define MS_RAW_LP_ADJV					"MS_RAW_LOWPOWER_DATA_ADJ_VERTICAL"
#define MS_CX1_MIN_MAX					"MS_TOUCH_ACTIVE_CX1_MIN_MAX"
#define MS_CX2_MAP_MIN					"MS_TOUCH_ACTIVE_CX2_MIN"
#define MS_CX2_MAP_MAX					"MS_TOUCH_ACTIVE_CX2_MAX"
#define MS_CX2_ADJH_MAP_MAX				"MS_TOUCH_ACTIVE_CX2_ADJ_HORIZONTAL"
#define MS_CX2_ADJV_MAP_MAX				"MS_TOUCH_ACTIVE_CX2_ADJ_VERTICAL"
#define MS_TOTAL_CX_MAP_MIN				"MS_TOUCH_ACTIVE_TOTAL_CX_MIN"
#define MS_TOTAL_CX_MAP_MAX				"MS_TOUCH_ACTIVE_TOTAL_CX_MAX"
#define MS_TOTAL_CX_ADJH_MAP_MAX		"MS_TOUCH_ACTIVE_TOTAL_CX_ADJ_HORIZONTAL"
#define MS_TOTAL_CX_ADJV_MAP_MAX		"MS_TOUCH_ACTIVE_TOTAL_CX_ADJ_VERTICAL"
#define MS_CX1_LP_MIN_MAX		"MS_TOUCH_LOWPOWER_CX1_MIN_MAX"
#define MS_CX2_LP_MAP_MIN		"MS_TOUCH_LOWPOWER_CX2_MIN"
#define MS_CX2_LP_MAP_MAX		"MS_TOUCH_LOWPOWER_CX2_MAX"
#define MS_CX2_ADJH_LP_MAP_MAX	"MS_TOUCH_LOWPOWER_CX2_ADJ_HORIZONTAL"
#define MS_CX2_ADJV_LP_MAP_MAX	"MS_TOUCH_LOWPOWER_CX2_ADJ_VERTICAL"
#define MS_TOTAL_CX_LP_MAP_MIN	"MS_TOUCH_LOWPOWER_TOTAL_CX_MIN"
#define MS_TOTAL_CX_LP_MAP_MAX	"MS_TOUCH_LOWPOWER_TOTAL_CX_MAX"
#define MS_TOTAL_CX_ADJH_LP_MAP_MAX "MS_TOUCH_LOWPOWER_TOTAL_CX_ADJ_HORIZONTAL"
#define MS_TOTAL_CX_ADJV_LP_MAP_MAX "MS_TOUCH_LOWPOWER_TOTAL_CX_ADJ_VERTICAL"
#define SS_RAW_FORCE_MIN_MAX			"SS_RAW_DATA_FORCE_MIN_MAX"
#define SS_RAW_SENSE_MIN_MAX			"SS_RAW_DATA_SENSE_MIN_MAX"
#define SS_RAW_FORCE_GAP				"SS_RAW_DATA_FORCE_GAP"
#define SS_RAW_SENSE_GAP				"SS_RAW_DATA_SENSE_GAP"
#define SS_RAW_LP_FORCE_MIN_MAX			"SS_RAW_LOWPOWER_DATA_FORCE_MIN_MAX"
#define SS_RAW_LP_SENSE_MIN_MAX			"SS_RAW_LOWPOWER_DATA_SENSE_MIN_MAX"
#define SS_RAW_LP_FORCE_GAP				"SS_RAW_LOWPOWER_DATA_FORCE_GAP"
#define SS_RAW_LP_SENSE_GAP				"SS_RAW_LOWPOWER_DATA_SENSE_GAP"
#define SS_IX1_FORCE_MIN_MAX			"SS_TOUCH_ACTIVE_IX1_FORCE_MIN_MAX"
#define SS_IX1_SENSE_MIN_MAX			"SS_TOUCH_ACTIVE_IX1_SENSE_MIN_MAX"
#define SS_CX1_FORCE_MIN_MAX			"SS_TOUCH_ACTIVE_CX1_FORCE_MIN_MAX"
#define SS_CX1_SENSE_MIN_MAX			"SS_TOUCH_ACTIVE_CX1_SENSE_MIN_MAX"
#define SS_IX2_FORCE_MAP_MIN			"SS_TOUCH_ACTIVE_IX2_FORCE_MIN"
#define SS_IX2_FORCE_MAP_MAX			"SS_TOUCH_ACTIVE_IX2_FORCE_MAX"
#define SS_IX2_SENSE_MAP_MIN			"SS_TOUCH_ACTIVE_IX2_SENSE_MIN"
#define SS_IX2_SENSE_MAP_MAX			"SS_TOUCH_ACTIVE_IX2_SENSE_MAX"
#define SS_IX2_FORCE_ADJV_MAP_MAX		"SS_TOUCH_ACTIVE_IX2_ADJ_VERTICAL"
#define SS_IX2_SENSE_ADJH_MAP_MAX		"SS_TOUCH_ACTIVE_IX2_ADJ_HORIZONTAL"
#define SS_CX2_FORCE_MAP_MIN			"SS_TOUCH_ACTIVE_CX2_FORCE_MIN"
#define SS_CX2_FORCE_MAP_MAX			"SS_TOUCH_ACTIVE_CX2_FORCE_MAX"
#define SS_CX2_SENSE_MAP_MIN			"SS_TOUCH_ACTIVE_CX2_SENSE_MIN"
#define SS_CX2_SENSE_MAP_MAX			"SS_TOUCH_ACTIVE_CX2_SENSE_MAX"
#define SS_CX2_FORCE_ADJV_MAP_MAX		"SS_TOUCH_ACTIVE_CX2_ADJ_VERTICAL"
#define SS_CX2_SENSE_ADJH_MAP_MAX		"SS_TOUCH_ACTIVE_CX2_ADJ_HORIZONTAL"

#define SS_TOTAL_IX_FORCE_MAP_MIN		"SS_TOUCH_ACTIVE_TOTAL_IX_FORCE_MIN"
#define SS_TOTAL_IX_FORCE_MAP_MAX		"SS_TOUCH_ACTIVE_TOTAL_IX_FORCE_MAX"
#define SS_TOTAL_IX_SENSE_MAP_MIN		"SS_TOUCH_ACTIVE_TOTAL_IX_SENSE_MIN"
#define SS_TOTAL_IX_SENSE_MAP_MAX		"SS_TOUCH_ACTIVE_TOTAL_IX_SENSE_MAX"
#define SS_TOTAL_IX_FORCE_ADJV_MAP_MAX	"SS_TOUCH_ACTIVE_TOTAL_IX_ADJ_VERTICAL"
#define SS_TOTAL_IX_SENSE_ADJH_MAP_MAX	"SS_TOUCH_ACTIVE_TOTAL_IX_ADJ_HORIZONTAL"
#define SS_TOTAL_CX_FORCE_MAP_MIN		"SS_TOUCH_ACTIVE_TOTAL_CX_FORCE_MIN"
#define SS_TOTAL_CX_FORCE_MAP_MAX		"SS_TOUCH_ACTIVE_TOTAL_CX_FORCE_MAX"
#define SS_TOTAL_CX_SENSE_MAP_MIN		"SS_TOUCH_ACTIVE_TOTAL_CX_SENSE_MIN"
#define SS_TOTAL_CX_SENSE_MAP_MAX		"SS_TOUCH_ACTIVE_TOTAL_CX_SENSE_MAX"
#define SS_TOTAL_CX_FORCE_ADJV_MAP_MAX	"SS_TOUCH_ACTIVE_TOTAL_CX_ADJ_VERTICAL"
#define SS_TOTAL_CX_SENSE_ADJH_MAP_MAX	"SS_TOUCH_ACTIVE_TOTAL_CX_ADJ_HORIZONTAL"

#define MS_KEY_RAW_MIN_MAX				"MS_KEY_RAW_DATA_MIN_MAX"
#define MS_KEY_CX1_MIN_MAX				"MS_KEY_CX1_MIN_MAX"
#define MS_KEY_CX2_MAP_MIN				"MS_KEY_CX2_MIN"
#define MS_KEY_CX2_MAP_MAX				"MS_KEY_CX2_MAX"
#define MS_KEY_TOTAL_CX_MAP_MIN			"MS_KEY_TOTAL_CX_MIN"
#define MS_KEY_TOTAL_CX_MAP_MAX			"MS_KEY_TOTAL_CX_MAX"

#define SS_IX1_FORCE_W                  "IX1_FORCE_W"
#define SS_IX2_FORCE_W                  "IX2_FORCE_W"
#define SS_IX1_SENSE_W                  "IX1_SENSE_W"
#define SS_IX2_SENSE_W                  "IX2_SENSE_W"
/** @}*/

/**
* Struct used to specify which test perform during the Mass Production Test.
* For each test item selected in this structure, there should be one or more labels associated in the Limit file from where load the thresholds
*/
typedef struct {
	int MutualRaw;																/*MS Raw min/Max test*/
	int MutualRawEachNode;
    int MutualRawGap;															/*MS Raw Gap(max-min) test*/
	int MutualRawAdj;															/*MS Raw Adjacent test*/
	int MutualRawLP;															/*MS Low Power Raw min/Max test*/
    int MutualRawGapLP;															/*MS Low Power Raw Gap(max-min) test*/
	int MutualRawAdjLP;															/*MS Low Power Raw Adjacent test*/
	int MutualRawAdjITO;														/*MS Raw Adjacent test during ITO test*/

	int MutualCx1;																/*MS Cx1 min/Max test*/
	int MutualCx2;																/*MS Cx2 min/Max (for each node) test*/
	int MutualCx2Adj;															/*MS Vertical and Horizontal Adj Cx2 min/Max (for each node) test*/
	int MutualCxTotal;															/*MS Total Cx min/Max (for each node) test*/
	int MutualCxTotalAdj;														/*MS Total vertical and Horizontal Adj Cx2 min/Max (for each node) test*/
	int MutualCx1LP;	/* /< MS LowPower Cx1 min/Max test */
	int MutualCx2LP;	/* /< MS LowPower Cx2 min/Max (for each node) test */
	int MutualCx2AdjLP;	/* /< MS LowPower Vertical and Horizontal Adj Cx2 min/Max
				 * (for each node) test */
	int MutualCxTotalLP;	/* /< MS Total LowPower Cx min/Max (for each node) test
				 * */
	int MutualCxTotalAdjLP;	/* /< MS Total LowPower vertical and Horizontal Adj Cx2
				 * min/Max (for each node) test */

	int MutualKeyRaw;															/*MS Raw Key min/Max test*/
	int MutualKeyCx1;															/*MS Cx1 Key min/Max test*/
	int MutualKeyCx2;															/*MS Cx2 Key min/Max (for each node) test*/
	int MutualKeyCxTotal;														/*MS Total Cx Key min/Max (for each node) test*/

	int SelfForceRaw;															/*SS Force Raw min/Max test*/
    int SelfForceRawGap;														/*SS Force Raw Gap(max-min) test*/
	int SelfForceRawLP;															/*SS Low Power Force Raw min/Max test*/
    int SelfForceRawGapLP;														/*SS Low Power Force Raw Gap(max-min) test*/

	int SelfForceIx1;															/*SS Force Ix1 min/Max test*/
	int SelfForceIx2;															/*SS Force Ix2 min/Max (for each node) test*/
	int SelfForceIx2Adj;														/*SS Vertical Adj Force Ix2 min/Max (for each node) test*/
	int SelfForceIxTotal;														/*SS Total Force Ix min/Max (for each node) test*/
	int SelfForceIxTotalAdj;													/*SS Total Vertical Adj Force Ix min/Max (for each node) test*/
	int SelfForceCx1;															/*SS Force Cx1 min/Max test*/
	int SelfForceCx2;															/*SS Force Cx2 min/Max (for each node) test*/
	int SelfForceCx2Adj;														/*SS Vertical Adj Force Cx2 min/Max (for each node) test*/
	int SelfForceCxTotal;														/*SS Total Force Cx min/Max (for each node) test*/
	int SelfForceCxTotalAdj;													/*SS Total Vertical Adj Force Cx min/Max (for each node) test*/

	int SelfSenseRaw;															/*SS Sense Raw min/Max test*/
    int SelfSenseRawGap;														/*SS Sense Raw Gap(max-min) test*/
	int SelfSenseRawLP;															/*SS Low Power Sense Raw min/Max test*/
    int SelfSenseRawGapLP;														/*SS Low Power Sense Raw Gap(max-min) test*/

	int SelfSenseIx1;															/*SS Sense Ix1 min/Max test*/
	int SelfSenseIx2;															/*SS Sense Ix2 min/Max (for each node) test*/
	int SelfSenseIx2Adj;														/*SS Horizontal Adj Sense Ix2 min/Max (for each node) test*/
	int SelfSenseIxTotal;														/*SS Total Horizontal Sense Ix min/Max (for each node) test*/
	int SelfSenseIxTotalAdj;													/*SS Total Horizontal Adj Sense Ix min/Max (for each node) test*/
	int SelfSenseCx1;															/*SS Sense Cx1 min/Max test*/
	int SelfSenseCx2;															/*SS Sense Cx2 min/Max (for each node) test*/
	int SelfSenseCx2Adj;														/*SS Horizontal Adj Sense Cx2 min/Max (for each node) test*/
	int SelfSenseCxTotal;														/*SS Total Sense Cx min/Max (for each node) test*/
	int SelfSenseCxTotalAdj;													/*SS Total Horizontal Adj Sense Cx min/Max (for each node) test*/

} TestToDo;

#define MAX_LIMIT_FILE_NAME					100									/*max number of chars of the limit file name*/

/**
 * Struct which store the data coming from a Production Limit File
 */
typedef struct {
	char *data;
	int size;
	char name[MAX_LIMIT_FILE_NAME];
} LimitFile;

int initTestToDo(void);
/**@}*/

/**@}*/

int computeAdjHoriz(i8 *data, int row, int column, u8  **result);
int computeAdjHorizTotal(short *data, int row, int column, u16  **result);
int computeAdjVert(i8 *data, int row, int column, u8  **result);
int computeAdjVertTotal(short *data, int row, int column, u16  **result);
int computeAdjHorizFromU(u8 *data, int row, int column, u8  **result);
int computeAdjHorizTotalFromU(u16 *data, int row, int column, u16  **result);
int computeAdjVertFromU(u8 *data, int row, int column, u8  **result);
int computeAdjVertTotalFromU(u16 *data, int row, int column, u16  **result);
int checkLimitsMinMax(short *data, int row, int column, int min, int max);
int checkLimitsMap(i8 *data, int row, int column, int *min, int *max);
int checkLimitsMapTotal(short *data, int row, int column, int *min, int *max);
int checkLimitsMapFromU(u8 *data, int row, int column, int *min, int *max);
int checkLimitsMapTotalFromU(u16 *data, int row, int column, int *min,
			     int *max);
int checkLimitsMapAdj(u8 *data, int row, int column, int *max);
int checkLimitsMapAdjTotal(u16 *data, int row, int column, int *max);

/**  @defgroup mp_api MP API
 * @ingroup mp_test
 * Functions to execute the MP test.
 * The parameters of these functions allow to customize their behavior in order to satisfy different scenarios
 * @{
 */
int production_test_ito(char *path_limits,TestToDo *todo);
int production_test_initialization(u8 type);
int production_test_main(char *pathThresholds, int stop_on_fail, int saveInit, TestToDo *todo);
int production_test_ms_raw(char *path_limits, int stop_on_fail, TestToDo *todo);
int production_test_ms_raw_lp(char *path_limits, int stop_on_fail, TestToDo *todo);
int production_test_ms_cx(char *path_limits, int stop_on_fail, TestToDo *todo);
int production_test_ms_cx_lp(char *path_limits, int stop_on_fail, TestToDo *todo);
int production_test_ss_raw(char *path_limits, int stop_on_fail, TestToDo *todo);
int production_test_ss_raw_lp(char *path_limits, int stop_on_fail, TestToDo *todo);
int production_test_ss_ix_cx(char *path_limits, int stop_on_fail, TestToDo *todo);
int production_test_data(char *path_limits, int stop_on_fail, TestToDo *todo);
int production_test_ms_key_cx(char *path_limits, int stop_on_fail, TestToDo *todo);
int production_test_ms_key_raw(char *path_limits);
int computeTotal(u8 *data, u8 main, int row, int column, int m, int n,
		 u16  **result);
/** @}*/

/**
 * @addtogroup limit_file
 * @{
 */
int parseProductionTestLimits(char *path, LimitFile *file, char *label, int **data, int *row, int *column);
int readLine(char *data, char *line, int size, int *n);
int getLimitsFile(char *path, LimitFile *file);
int freeLimitsFile(LimitFile *file);
int freeCurrentLimitsFile(void);
/**@}*/

#endif
