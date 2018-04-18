/*

 **************************************************************************
 **                        STMicroelectronics 		                **
 **************************************************************************
 **                        marco.cali@st.com				**
 **************************************************************************
 *                                                                        *
 *               	FTS API for MP test				 *
 *                                                                        *
 **************************************************************************
 **************************************************************************

 */

#include "ftsCrossCompile.h"
#include "ftsCompensation.h"
#include "ftsError.h"
#include "ftsFrame.h"
#include "ftsHardware.h"
#include "ftsIO.h"
#include "ftsSoftware.h"
#include "ftsTest.h"
#include "ftsTime.h"
#include "ftsTool.h"
#include "../fts.h"

#include <linux/init.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <stdarg.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/serio.h>
#include <linux/time.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/power_supply.h>
#include <linux/firmware.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>

#ifdef LIMITS_H_FILE
#include <../fts_limits.h>
#endif


int computeAdjHoriz(u8 *data, int row, int column, u8 **result)
{
	int i, j;
	int size = row * (column - 1);

	if (column < 2) {
		log_error("%s computeAdjHoriz: ERROR %02X\n", tag, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	*result = (u8 *)kmalloc(size * sizeof(u8), GFP_KERNEL);

	if (*result == NULL) {
		log_error("%s computeAdjHoriz: ERROR %02X\n", tag, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	for (i = 0; i < row; i++) {
		for (j = 1; j < column; j++) {
			*(*result + (i * (column - 1) + (j - 1))) = abs(data[i * column + j] - data[i * column + (j - 1)]);
		}
	}

	return OK;
}

int computeAdjHorizTotal(u16 *data, int row, int column, u16 **result)
{
	int i, j;
	int size = row * (column - 1);

	if (column < 2) {
		log_error("%s computeAdjHorizTotal: ERROR %02X\n", tag, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	*result = (u16 *) kmalloc(size * sizeof(u16), GFP_KERNEL);

	if (*result == NULL) {
		log_error("%s computeAdjHorizTotal: ERROR %02X\n", tag, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	for (i = 0; i < row; i++) {
		for (j = 1; j < column; j++) {
			*(*result + (i * (column - 1) + (j - 1))) = abs(data[i * column + j] - data[i * column + (j - 1)]);
		}
	}

	return OK;
}

int computeAdjVert(u8 *data, int row, int column, u8 **result)
{
	int i, j;
	int size = (row - 1) * column;

	if (row < 2) {
		log_error("%s computeAdjVert: ERROR %02X\n", tag, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	*result = (u8 *) kmalloc(size * sizeof(u8), GFP_KERNEL);

	if (*result == NULL) {
		log_error("%s computeAdjVert: ERROR %02X\n", tag, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	for (i = 1; i < row; i++) {
		for (j = 0; j < column; j++) {
			*(*result + ((i - 1) * column + j)) = abs(data[i * column + j] - data[(i - 1) * column + j]);
		}
	}

	return OK;
}

int computeAdjVertTotal(u16 *data, int row, int column, u16 **result)
{
	int i, j;
	int size = (row - 1) * column;

	if (row < 2) {
		log_error("%s computeAdjVertTotal: ERROR %02X\n", tag, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	*result = (u16 *) kmalloc(size * sizeof(u16), GFP_KERNEL);

	if (*result == NULL) {
		log_error("%s computeAdjVertTotal: ERROR %02X\n", tag, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	for (i = 1; i < row; i++) {
		for (j = 0; j < column; j++) {
			*(*result + ((i - 1) * column + j)) = abs(data[i * column + j] - data[(i - 1) * column + j]);
		}
	}

	return OK;
}

int computeTotal(u8 *data, u8 main, int row, int column, int m, int n, u16 **result)
{
	int i, j;
	int size = (row) * (column);
	*result = (u16 *) kmalloc(size * sizeof(u16), GFP_KERNEL);

	if (*result == NULL) {
		log_error("%s computeTotal : ERROR %02X\n", tag, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	for (i = 0; i < row; i++) {
		for (j = 0; j < column; j++) {
			*(*result + (i * column + j)) = m * main + n * data[i * column + j];
		}
	}

	return OK;
}

int checkLimitsMinMax(short *data, int row, int column, int min, int max)
{
	int i, j;
	int count = 0;

	for (i = 0; i < row; i++) {
		for (j = 0; j < column; j++) {
			if (data[i * column + j] < min || data[i * column + j] > max) {
				log_error("%s checkLimitsMinMax: Node[%d,%d] = %d exceed limit [%d, %d]\n", tag, i, j, data[i * column + j], min, max);
				count++;
			}
		}
	}

	return count;
}

int checkLimitsGap(short *data, int row, int column, int threshold)
{
	int i, j;
	int min_node;
	int max_node;

	if (row == 0 || column == 0) {
		log_error("%s checkLimitsGap: invalid number of rows = %d or columns = %d  ERROR %02x\n", tag, row, column, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	min_node = data[0];
	max_node = data[0];

	for (i = 0; i < row; i++) {
		for (j = 0; j < column; j++) {
			if (data[i * column + j] < min_node) {
				min_node = data[i * column + j];
			} else {
				if (data[i * column + j] > max_node)
					max_node = data[i * column + j];
			}
		}
	}

	if (max_node - min_node > threshold) {
		log_error("%s checkLimitsGap: GAP = %d exceed limit  %d\n", tag, max_node - min_node, threshold);
		return ERROR_TEST_CHECK_FAIL;
	} else
		return OK;
}

int checkLimitsMap(u8 *data, int row, int column, int *min, int *max)
{
	int i, j;
	int count = 0;

	for (i = 0; i < row; i++) {
		for (j = 0; j < column; j++) {
			if (data[i * column + j] < min[i * column + j] || data[i * column + j] > max[i * column + j]) {
				log_error("%s checkLimitsMap: Node[%d,%d] = %d exceed limit [%d, %d]\n", tag, i, j, data[i * column + j], min[i * column + j], max[i * column + j]);
				count++;
			}
		}
	}

	return count;
}

int checkLimitsMapTotal(u16 *data, int row, int column, int *min, int *max)
{
	int i, j;
	int count = 0;

	for (i = 0; i < row; i++) {
		for (j = 0; j < column; j++) {
			if (data[i * column + j] < min[i * column + j] || data[i * column + j] > max[i * column + j]) {
				log_error("%s checkLimitsMapTotal: Node[%d,%d] = %d exceed limit [%d, %d]\n", tag, i, j, data[i * column + j], min[i * column + j], max[i * column + j]);
				count++;
			}
		}
	}

	return count;
}

int checkLimitsMapAdj(u8 *data, int row, int column, int *max)
{
	int i, j;
	int count = 0;

	for (i = 0; i < row; i++) {
		for (j = 0; j < column; j++) {
			if (data[i * column + j] > max[i * column + j]) {
				log_error("%s checkLimitsMapAdj: Node[%d,%d] = %d exceed limit > %d\n", tag, i, j, data[i * column + j], max[i * column + j]);
				count++;
			}
		}
	}

	return count;
}

int checkLimitsMapAdjTotal(u16 *data, int row, int column, int *max)
{
	int i, j;
	int count = 0;

	for (i = 0; i < row; i++) {
		for (j = 0; j < column; j++) {
			if (data[i * column + j] > max[i * column + j]) {
				log_error("%s checkLimitsMapAdjTotal: Node[%d,%d] = %d exceed limit > %d\n", tag, i, j, data[i * column + j], max[i * column + j]);
				count++;
			}
		}
	}

	return count;
}

int production_test_ito(void)
{
	int res = OK;
	u8 cmd;
	u8 readData[FIFO_EVENT_SIZE];
	int eventToSearch[2] = {EVENTID_ERROR_EVENT, EVENT_TYPE_ITO};
	log_debug("%s ITO Production test is starting...\n", tag);
	res = fts_system_reset();

	if (res < 0) {
		log_error("%s production_test_ito: ERROR %02X\n", tag, ERROR_PROD_TEST_ITO);
		return (res | ERROR_PROD_TEST_ITO);
	}

	cmd = FTS_CMD_ITO_CHECK;
	log_debug("%s ITO Check command sent...\n", tag);

	if (fts_writeFwCmd(&cmd, 1) < 0) {
		log_error("%s production_test_ito: ERROR %02X\n", tag, (ERROR_I2C_W | ERROR_PROD_TEST_ITO));
		return (ERROR_I2C_W | ERROR_PROD_TEST_ITO);
	}

	log_debug("%s Looking for ITO Event...\n", tag);
	res = pollForEvent(eventToSearch, 2, readData, TIMEOUT_ITO_TEST_RESULT);

	if (res < 0) {
		log_error("%s production_test_ito: ITO Production test failed... ERROR %02X\n", tag, ERROR_PROD_TEST_ITO);
		return (res | ERROR_PROD_TEST_ITO);
	}

	if (readData[2] != 0x00 || readData[3] != 0x00) {
		log_debug("%s ITO Production testes finished!.................FAILED  ERROR %02X\n", tag, (ERROR_TEST_CHECK_FAIL | ERROR_PROD_TEST_ITO));
		res = (ERROR_TEST_CHECK_FAIL | ERROR_PROD_TEST_ITO);
	} else {
		log_debug("%s ITO Production test finished!.................OK\n", tag);
		res = OK;
	}

	res |= fts_system_reset();

	if (res < 0) {
		log_error("%s production_test_ito: ERROR %02X\n", tag, ERROR_PROD_TEST_ITO);
		res = (res | ERROR_PROD_TEST_ITO);
	}

	return res;
}

int production_test_initialization(void)
{
	int res;
	u8 cmd;
	u8 readData[FIFO_EVENT_SIZE];
	int eventToSearch[2] = {EVENTID_STATUS_UPDATE, EVENT_TYPE_FULL_INITIALIZATION};
	log_debug("%s INITIALIZATION Production test is starting...\n", tag);
	res = fts_system_reset();

	if (res < 0) {
		log_error("%s production_test_initialization: ERROR %02X\n", tag, ERROR_PROD_TEST_INITIALIZATION);
		return (res | ERROR_PROD_TEST_INITIALIZATION);
	}

	log_debug("%s INITIALIZATION command sent...\n", tag);
	cmd = FTS_CMD_FULL_INITIALIZATION;

	if (fts_writeFwCmd(&cmd, 1) < 0) {
		log_error("%s production_test_initialization: ERROR %02X\n", tag, (ERROR_I2C_W | ERROR_PROD_TEST_INITIALIZATION));
		return (ERROR_I2C_W | ERROR_PROD_TEST_INITIALIZATION);
	}

	log_debug("%s Looking for INITIALIZATION Event...\n", tag);
	res = pollForEvent(eventToSearch, 2, readData, TIMEOUT_INITIALIZATION_TEST_RESULT);

	if (res < 0) {
		log_error("%s production_test_initialization: INITIALIZATION Production test failed... ERROR %02X\n", tag, ERROR_PROD_TEST_INITIALIZATION);
		return (res | ERROR_PROD_TEST_INITIALIZATION);
	}

	if (readData[2] != 0x00) {
		log_debug("%s INITIALIZATION Production testes finished!.................FAILED  ERROR %02X\n", tag, (ERROR_TEST_CHECK_FAIL | ERROR_PROD_TEST_INITIALIZATION));
		res = (ERROR_TEST_CHECK_FAIL | ERROR_PROD_TEST_INITIALIZATION);
	} else {
		log_debug("%s INITIALIZATION Production test.................OK\n", tag);
		res = OK;
	}

	log_debug("%s Refresh Chip Info...\n", tag);
	res |= readChipInfo(1);

	if (res < 0) {
		log_error("%s production_test_initialization: read chip info ERROR %02X\n", tag, ERROR_PROD_TEST_INITIALIZATION);
		res = (res | ERROR_PROD_TEST_INITIALIZATION);
	}

	return res;
}

int ms_compensation_tuning(void)
{
	int res;
	u8 cmd;
	u8 readData[FIFO_EVENT_SIZE];
	int eventToSearch[2] = {EVENTID_STATUS_UPDATE, EVENT_TYPE_MS_TUNING_CMPL};
	log_debug("%s MS INITIALIZATION command sent...\n", tag);
	cmd = FTS_CMD_MS_COMP_TUNING;

	if (fts_writeFwCmd(&cmd, 1) < 0) {
		log_error("%s ms_compensation_tuning 2: ERROR %02X\n", tag, (ERROR_I2C_W | ERROR_MS_TUNING));
		return (ERROR_I2C_W | ERROR_MS_TUNING);
	}

	log_debug("%s Looking for MS INITIALIZATION Event...\n", tag);
	res = pollForEvent(eventToSearch, 2, readData, TIMEOUT_INITIALIZATION_TEST_RESULT);

	if (res < 0) {
		log_error("%s ms_compensation_tuning: MS INITIALIZATION Production test failed... ERROR %02X\n", tag, ERROR_MS_TUNING);
		return (res | ERROR_MS_TUNING);
	}

	if (readData[2] != 0x00 || readData[3] != 0x00) {
		log_debug("%s MS INITIALIZATION Production test finished!.................FAILED  ERROR %02X\n", tag, ERROR_MS_TUNING);
		res = ERROR_MS_TUNING;
	} else {
		log_debug("%s MS INITIALIZATION Production test finished!.................OK\n", tag);
		res = OK;
	}

	return res;
}

int ss_compensation_tuning(void)
{
	int res;
	u8 cmd;
	u8 readData[FIFO_EVENT_SIZE];
	int eventToSearch[2] = {EVENTID_STATUS_UPDATE, EVENT_TYPE_SS_TUNING_CMPL};
	log_debug("%s SS INITIALIZATION command sent...\n", tag);
	cmd = FTS_CMD_SS_COMP_TUNING;

	if (fts_writeFwCmd(&cmd, 1) < 0) {
		log_error("%s ss_compensation_tuning 2: ERROR %02X\n", tag, (ERROR_I2C_W | ERROR_SS_TUNING));
		return (ERROR_I2C_W | ERROR_SS_TUNING);
	}

	log_debug("%s Looking for SS INITIALIZATION Event...\n", tag);
	res = pollForEvent(eventToSearch, 2, readData, TIMEOUT_INITIALIZATION_TEST_RESULT);

	if (res < 0) {
		log_error("%s ms_compensation_tuning: SS INITIALIZATION Production test failed... ERROR %02X\n", tag, ERROR_SS_TUNING);
		return (res | ERROR_SS_TUNING);
	}

	if (readData[2] != 0x00 || readData[3] != 0x00) {
		log_debug("%s SS INITIALIZATION Production test finished!.................FAILED  ERROR %02X\n", tag, ERROR_SS_TUNING);
		res = ERROR_SS_TUNING;
	} else {
		log_debug("%s SS INITIALIZATION Production test finished!.................OK\n", tag);
		res = OK;
	}

	return res;
}

int lp_timer_calibration(void)
{
	int res;
	u8 cmd;
	u8 readData[FIFO_EVENT_SIZE];
	int eventToSearch[2] = {EVENTID_STATUS_UPDATE, EVENT_TYPE_LPTIMER_TUNING_CMPL};
	log_debug("%s LP TIMER CALIBRATION command sent...\n", tag);
	cmd = FTS_CMD_LP_TIMER_CALIB;

	if (fts_writeFwCmd(&cmd, 1) < 0) {
		log_error("%s lp_timer_calibration 2: ERROR %02X\n", tag, (ERROR_I2C_W | ERROR_LP_TIMER_TUNING));
		return (ERROR_I2C_W | ERROR_LP_TIMER_TUNING);
	}

	log_debug("%s Looking for LP TIMER CALIBRATION Event...\n", tag);
	res = pollForEvent(eventToSearch, 2, readData, TIMEOUT_INITIALIZATION_TEST_RESULT);

	if (res < 0) {
		log_error("%s lp_timer_calibration: LP TIMER CALIBRATION Production test failed... ERROR %02X\n", tag, ERROR_LP_TIMER_TUNING);
		return (res | ERROR_LP_TIMER_TUNING);
	}

	if (readData[2] != 0x00 || readData[3] != 0x01) {
		log_debug("%s LP TIMER CALIBRATION Production test finished!.................FAILED  ERROR %02X\n", tag, ERROR_LP_TIMER_TUNING);
		res = ERROR_LP_TIMER_TUNING;
	} else {
		log_debug("%s LP TIMER CALIBRATION Production test finished!.................OK\n", tag);
		res = OK;
	}

	return res;
}

int save_cx_tuning(void)
{
	int res;
	u8 cmd;
	u8 readData[FIFO_EVENT_SIZE];
	int eventToSearch[2] = {EVENTID_STATUS_UPDATE, EVENT_TYPE_COMP_DATA_SAVED};
	log_debug("%s SAVE CX command sent...\n", tag);
	cmd = FTS_CMD_SAVE_CX_TUNING;

	if (fts_writeCmd(&cmd, 1) < 0) {
		log_error("%s save_cx_tuning 2: ERROR %02X\n", tag, (ERROR_I2C_W | ERROR_SAVE_CX_TUNING));
		return (ERROR_I2C_W | ERROR_SAVE_CX_TUNING);
	}

	log_debug("%s Looking for SAVE CX Event...\n", tag);
	res = pollForEvent(eventToSearch, 2, readData, TIMEOUT_INITIALIZATION_TEST_RESULT);

	if (res < 0) {
		log_error("%s save_cx_tuning: SAVE CX failed... ERROR %02X\n", tag, ERROR_SAVE_CX_TUNING);
		return (res | ERROR_SAVE_CX_TUNING);
	}

	if (readData[2] != 0x00 || readData[3] != 0x00) {
		log_debug("%s SAVE CX finished!.................FAILED  ERROR %02X\n", tag, ERROR_SAVE_CX_TUNING);
		res = ERROR_SAVE_CX_TUNING;
	} else {
		log_debug("%s SAVE CX finished!.................OK\n", tag);
		res = OK;
	}

	return res;
}

int production_test_splitted_initialization(int saveToFlash)
{
	int res;
	log_debug("%s Splitted Initialization test is starting...\n", tag);
	res = fts_system_reset();

	if (res < 0) {
		log_error("%s production_test_initialization: ERROR %02X\n", tag, ERROR_PROD_TEST_INITIALIZATION);
		return (res | ERROR_PROD_TEST_INITIALIZATION);
	}

	log_debug("%s MS INITIALIZATION TEST:\n", tag);
	res = ms_compensation_tuning();

	if (res < 0) {
		log_debug("%s production_test_splitted_initialization: MS INITIALIZATION TEST FAILED! ERROR %02X\n", tag, ERROR_PROD_TEST_INITIALIZATION);
		return (res | ERROR_PROD_TEST_INITIALIZATION);
	} else {
		log_debug("%s MS INITIALIZATION TEST OK!\n", tag);
		log_debug("%s\n", tag);
		log_debug("%s SS INITIALIZATION TEST:\n", tag);
		res = ss_compensation_tuning();

		if (res < 0) {
			log_debug("%s production_test_splitted_initialization: SS INITIALIZATION TEST FAILED! ERROR %02X\n", tag, ERROR_PROD_TEST_INITIALIZATION);
			return (res | ERROR_PROD_TEST_INITIALIZATION);
		} else {
			log_debug("%s SS INITIALIZATION TEST OK!\n", tag);
			log_debug("%s\n", tag);
			log_debug("%s LP INITIALIZATION TEST:\n", tag);
			res = lp_timer_calibration();

			if (res < 0) {
				log_debug("%s production_test_splitted_initialization: LP INITIALIZATION TEST FAILED! ERROR %02X\n", tag, ERROR_PROD_TEST_INITIALIZATION);
				return (res | ERROR_PROD_TEST_INITIALIZATION);
			} else {
				log_debug("%s LP INITIALIZATION TEST OK!\n", tag);

				if (saveToFlash) {
					log_debug("%s\n", tag);
					log_debug("%s SAVE CX TEST:\n", tag);
					res = save_cx_tuning();

					if (res < 0) {
						log_debug("%s  production_test_splitted_initialization: SAVE CX TEST FAILED! ERROR %02X\n", tag, res);
						return (res | ERROR_PROD_TEST_INITIALIZATION);
					} else {
						log_debug("%s SAVE CX TEST OK!\n", tag);
					}
				}

				log_debug("%s Refresh Chip Info...\n", tag);
				res |= readChipInfo(1);

				if (res < 0) {
					log_error("%s production_test_initialization: read chip info ERROR %02X\n", tag, ERROR_PROD_TEST_INITIALIZATION);
					res = (res | ERROR_PROD_TEST_INITIALIZATION);
				} else
					log_debug("%s Splitted Initialization test finished!.................OK\n", tag);

				return res;
			}
		}
	}
}

int production_test_main(const char *pathThresholds, int stop_on_fail, int saveInit, TestToDo *todo, u32 signature)
{
	int res, ret;
	log_debug("%s MAIN Production test is starting...\n", tag);
	log_debug("%s\n", tag);
	log_debug("%s ITO TEST:\n", tag);
	res = production_test_ito();

	if (res < 0) {
		log_error("%s Error during ITO TEST! ERROR %08X\n", tag, res);


	} else {
		log_debug("%s ITO TEST OK!\n", tag);
	}

	log_debug("%s\n", tag);
	log_debug("%s INITIALIZATION TEST :\n", tag);

	if (saveInit == 1) {
		res = production_test_initialization();

		if (res < 0) {
			log_error("%s Error during  INITIALIZATION TEST! ERROR %08X\n", tag, res);

			if (stop_on_fail)
				goto END;
		} else {
			log_debug("%s INITIALIZATION TEST OK!\n", tag);
		}
	} else
		log_error("%s INITIALIZATION TEST :................. SKIPPED \n", tag);

	log_debug("%s\n", tag);

	if (saveInit == 1) {
		log_debug("%s Cleaning up...\n", tag);
		ret = cleanUp(0);

		if (ret < 0) {
			log_error("%s production_test_main: clean up ERROR %02X\n", tag, ret);
			res |= ret;

			if (stop_on_fail)
				goto END;
		}

		log_debug("%s\n", tag);
	}

	log_debug("%s PRODUCTION DATA TEST:\n", tag);
	ret = production_test_data(pathThresholds, stop_on_fail, todo);

	if (ret < 0) {
		log_debug("%s Error during PRODUCTION DATA TEST! ERROR %08X\n", tag, ret);
	} else {
		log_debug("%s PRODUCTION DATA TEST OK!\n", tag);
	}

	res |= ret;

	if (ret == OK && saveInit == 1) {
		log_debug("%s SAVE FLAG:\n", tag);
		ret = save_mp_flag(signature);

		if (ret < OK)
			log_debug("%s SAVE FLAG:................. FAIL! ERROR %08X\n", tag, ret);
		else
			log_debug("%s SAVE FLAG:................. OK!\n", tag);

		res |= ret;
		ret = readChipInfo(1);

		if (ret < OK) {
			log_error("%s production_test_main: read chip info ERROR %08X\n", tag, ret);
		}

		res |= ret;
	}

	log_debug("%s\n", tag);
END:

	if (res < 0) {
		log_error("%s MAIN Production test finished.................FAILED\n", tag);
		return res;
	} else {
		log_debug("%s MAIN Production test finished.................OK\n", tag);
		return OK;
	}
}

int production_test_ms_raw(const char *path_limits, int stop_on_fail, TestToDo *todo)
{
	int ret, count_fail = 0;
	MutualSenseFrame msRawFrame;
	int *thresholds = NULL;
	int trows, tcolumns;
	log_debug("%s\n", tag);
	log_debug("%s MS RAW DATA TEST is starting...\n", tag);

	if (todo->MutualRaw == 1 || todo->MutualRawGap == 1) {
		ret = getMSFrame2(MS_TOUCH_ACTIVE, &msRawFrame);

		if (ret < 0) {
			log_error("%s production_test_data: getMSFrame failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA);
			return ERROR_PROD_TEST_DATA_MS_RAW;
		}

		print_frame_short("MS Raw frame =", array1dTo2d_short(msRawFrame.node_data, msRawFrame.node_data_size, msRawFrame.header.sense_node), msRawFrame.header.force_node, msRawFrame.header.sense_node);
		log_debug("%s MS RAW MIN MAX TEST: \n", tag);

		if (todo->MutualRaw == 1) {
			ret = parseProductionTestLimits(path_limits, MS_RAW_MIN_MAX, &thresholds, &trows, &tcolumns);

			if (ret < 0 || (trows != 1 || tcolumns != 2)) {
				log_error("%s production_test_data: parseProductionTestLimits MS_RAW_MIN_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_MS_RAW);
				ret = ERROR_PROD_TEST_DATA_MS_RAW;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMinMax(msRawFrame.node_data, msRawFrame.header.force_node, msRawFrame.header.sense_node, thresholds[0], thresholds[1]);

			if (ret != OK) {
				log_error("%s production_test_data: checkLimitsMinMax MS RAW failed... ERROR COUNT = %d\n", tag, ret);
				log_debug("%s MS RAW MIN MAX TEST:.................FAIL\n\n", tag);
				count_fail += 1;

				if (stop_on_fail == 1)
					goto ERROR;
			} else
				log_debug("%s MS RAW MIN MAX TEST:.................OK \n", tag);

			kfree(thresholds);
			thresholds = NULL;
		} else
			log_debug("%s MS RAW MIN MAX TEST:.................SKIPPED \n", tag);

		log_debug("%s\n", tag);
		log_debug("%s MS RAW GAP TEST:\n", tag);

		if (todo->MutualRawGap == 1) {
			ret = parseProductionTestLimits(path_limits, MS_RAW_GAP, &thresholds, &trows, &tcolumns);

			if (ret < 0 || (trows != 1 || tcolumns != 1)) {
				log_error("%s production_test_data: parseProductionTestLimits MS_RAW_GAP failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_MS_RAW);
				ret = ERROR_PROD_TEST_DATA_MS_RAW;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsGap(msRawFrame.node_data, msRawFrame.header.force_node, msRawFrame.header.sense_node, thresholds[0]);

			if (ret != OK) {
				log_error("%s production_test_data: checkLimitsGap MS RAW failed... ERROR = %02X\n", tag, ret);
				count_fail += 1;

				if (stop_on_fail == 1)
					goto ERROR;
			} else
				log_debug("%s MS RAW GAP TEST:.................OK\n\n", tag);

			kfree(thresholds);
			thresholds = NULL;
		} else
			log_debug("%s MS RAW GAP TEST:.................SKIPPED \n", tag);
	} else
		log_debug("%s MS RAW FRAME TEST:.................SKIPPED \n", tag);

	log_debug("%s\n", tag);
	log_debug("%s MS KEY RAW TEST:\n", tag);

	if (todo->MutualKeyRaw == 1) {
		ret = production_test_ms_key_raw(path_limits);

		if (ret < 0) {
			log_error("%s production_test_data: production_test_ms_key_raw failed... ERROR = %02X\n", tag, ret);
			count_fail += 1;

			if (count_fail == 1) {
				log_debug("%s MS RAW DATA TEST:.................FAIL fails_count = %d\n\n", tag, count_fail);
				goto ERROR_LIMITS;
			}
		}
	} else
		log_debug("%s MS KEY RAW TEST:.................SKIPPED \n", tag);

ERROR:
	log_debug("%s\n", tag);

	if (count_fail == 0) {
		kfree(msRawFrame.node_data);
		msRawFrame.node_data = NULL;
		log_debug("%s MS RAW DATA TEST finished!.................OK\n", tag);
		return OK;
	} else {
		if (msRawFrame.node_data != NULL) {
			kfree(msRawFrame.node_data);
			msRawFrame.node_data = NULL;
		}

		if (thresholds != NULL) {
			kfree(thresholds);
			thresholds = NULL;
		}

		log_debug("%s MS RAW DATA TEST:.................FAIL fails_count = %d\n\n", tag, count_fail);
		return ERROR_PROD_TEST_DATA_MS_RAW;
	}

ERROR_LIMITS:

	if (msRawFrame.node_data != NULL) {
		kfree(msRawFrame.node_data);
		msRawFrame.node_data = NULL;
	}

	if (thresholds != NULL) {
		kfree(thresholds);
		thresholds = NULL;
	}

	return ret;
}

int production_test_ms_key_raw(const char *path_limits)
{
	int ret;
	MutualSenseFrame msRawFrame;
	int *thresholds = NULL;
	int trows, tcolumns;
	log_debug("%s MS KEY RAW DATA TEST is starting...\n", tag);
	ret = getMSFrame2(MS_KEY, &msRawFrame);

	if (ret < 0) {
		log_error("%s production_test_data: getMSKeyFrame failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_KEY_RAW);
		return ERROR_PROD_TEST_DATA_KEY_RAW;
	}

	print_frame_short("MS Key Raw frame =", array1dTo2d_short(msRawFrame.node_data, msRawFrame.node_data_size, msRawFrame.header.sense_node), msRawFrame.header.force_node, msRawFrame.header.sense_node);
	ret = parseProductionTestLimits(path_limits, MS_KEY_RAW_MIN_MAX, &thresholds, &trows, &tcolumns);

	if (ret < 0 || (trows != 1 || tcolumns != 2)) {
		log_error("%s production_test_data: parseProductionTestLimits MS_KEY_RAW_MIN_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_KEY_RAW);
		ret = ERROR_PROD_TEST_DATA_KEY_RAW;
		goto ERROR_LIMITS;
	}

	ret = checkLimitsMinMax(msRawFrame.node_data, msRawFrame.header.force_node, msRawFrame.header.sense_node, thresholds[0], thresholds[1]);

	if (ret != OK) {
		log_error("%s production_test_data: checkLimitsMinMax MS KEY RAW failed... ERROR COUNT = %d\n", tag, ret);
		goto ERROR;
	} else
		log_debug("%s MS KEY RAW TEST:.................OK\n\n", tag);

	kfree(thresholds);
	thresholds = NULL;
	kfree(msRawFrame.node_data);
	msRawFrame.node_data = NULL;
	return OK;
ERROR:

	if (msRawFrame.node_data != NULL) {
		kfree(msRawFrame.node_data);
		msRawFrame.node_data = NULL;
	}

	if (thresholds != NULL) {
		kfree(thresholds);
		thresholds = NULL;
	}

	log_debug("%s MS KEY RAW TEST:.................FAIL\n\n", tag);
	return ERROR_PROD_TEST_DATA_KEY_RAW;
ERROR_LIMITS:

	if (msRawFrame.node_data != NULL) {
		kfree(msRawFrame.node_data);
		msRawFrame.node_data = NULL;
	}

	if (thresholds != NULL) {
		kfree(thresholds);
		thresholds = NULL;
	}

	return ret;
}

int production_test_ms_cx(const char *path_limits, int stop_on_fail, TestToDo *todo)
{
	int ret;
	int count_fail = 0;
	int *thresholds = NULL;
	int *thresholds_min = NULL;
	int *thresholds_max = NULL;
	int trows, tcolumns;
	MutualSenseData msCompData;
	u8 *adjhor = NULL;
	u8 *adjvert = NULL;
	u16 container;
	u16 *total_cx = NULL;
	u16 *total_adjhor = NULL;
	u16 *total_adjvert = NULL;
	log_debug("%s\n", tag);
	log_debug("%s MS CX Testes are starting...\n", tag);
	ret = readMutualSenseCompensationData(MS_TOUCH_ACTIVE, &msCompData);

	if (ret < 0) {
		log_error("%s production_test_data: readMutualSenseCompensationData failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_MS_CX);
		return ERROR_PROD_TEST_DATA_MS_CX;
	}

	print_frame_u8("MS Init Data (Cx2) =", array1dTo2d_u8(msCompData.node_data, msCompData.node_data_size, msCompData.header.sense_node), msCompData.header.force_node, msCompData.header.sense_node);
	log_debug("%s MS CX1 TEST:\n", tag);

	if (todo->MutualCx1 == 1) {
		ret = parseProductionTestLimits(path_limits, MS_CX1_MIN_MAX, &thresholds, &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != 2)) {
			log_error("%s production_test_data: parseProductionTestLimits MS_CX1_MIN_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_MS_CX);
			ret = ERROR_PROD_TEST_DATA_MS_CX;
			goto ERROR_LIMITS;
		}

		container = (u16) msCompData.cx1;
		ret = checkLimitsMinMax(&container, 1, 1, thresholds[0], thresholds[1]);

		if (ret != OK) {
			log_error("%s production_test_data: checkLimitsMinMax MS CX1 failed... ERROR COUNT = %d\n", tag, ret);
			log_debug("%s MS CX1 TEST:.................FAIL\n\n", tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			log_debug("%s MS CX1 TEST:.................OK\n\n", tag);
	} else
		log_debug("%s MS CX1 TEST:.................SKIPPED\n\n", tag);

	kfree(thresholds);
	thresholds = NULL;
	log_debug("%s MS CX2 MIN MAX TEST:\n", tag);

	if (todo->MutualCx2 == 1) {
		ret = parseProductionTestLimits(path_limits, MS_CX2_MAP_MIN, &thresholds_min, &trows, &tcolumns);

		if (ret < 0 || (trows != msCompData.header.force_node || tcolumns != msCompData.header.sense_node)) {
			log_error("%s production_test_data: parseProductionTestLimits MS_CX2_MAP_MIN failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_MS_CX);
			ret = ERROR_PROD_TEST_DATA_MS_CX;
			goto ERROR_LIMITS;
		}

		ret = parseProductionTestLimits(path_limits, MS_CX2_MAP_MAX, &thresholds_max, &trows, &tcolumns);

		if (ret < 0 || (trows != msCompData.header.force_node || tcolumns != msCompData.header.sense_node)) {
			log_error("%s production_test_data: parseProductionTestLimits MS_CX2_MAP_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_MS_CX);
			ret = ERROR_PROD_TEST_DATA_MS_CX;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMap(msCompData.node_data, msCompData.header.force_node, msCompData.header.sense_node, thresholds_min, thresholds_max);

		if (ret != OK) {
			log_error("%s production_test_data: checkLimitsMap MS CX2 MIN MAX failed... ERROR COUNT = %d\n", tag, ret);
			log_debug("%s MS CX2 MIN MAX TEST:.................FAIL\n\n", tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			log_debug("%s MS CX2 MIN MAX TEST:.................OK\n\n", tag);

		kfree(thresholds_min);
		thresholds_min = NULL;
		kfree(thresholds_max);
		thresholds_max = NULL;
	} else
		log_debug("%s MS CX2 MIN MAX TEST:.................SKIPPED\n\n", tag);

	log_debug("%s MS CX2 ADJ TEST:\n", tag);

	if (todo->MutualCx2Adj == 1) {
		log_debug("%s MS CX2 ADJ HORIZ TEST:\n", tag);
		ret = computeAdjHoriz(msCompData.node_data, msCompData.header.force_node, msCompData.header.sense_node, &adjhor);

		if (ret < 0) {
			log_error("%s production_test_data: computeAdjHoriz failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_MS_CX);
			ret = ERROR_PROD_TEST_DATA_MS_CX;
			goto ERROR_LIMITS;
		}

		log_debug("%s MS CX2 ADJ HORIZ computed!\n", tag);
		ret = parseProductionTestLimits(path_limits, MS_CX2_ADJH_MAP_MAX, &thresholds_max, &trows, &tcolumns);

		if (ret < 0 || (trows != msCompData.header.force_node || tcolumns != msCompData.header.sense_node - 1)) {
			log_error("%s production_test_data: parseProductionTestLimits MS_CX2_ADJH_MAP_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_MS_CX);
			ret = ERROR_PROD_TEST_DATA_MS_CX;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMapAdj(adjhor, msCompData.header.force_node, msCompData.header.sense_node - 1, thresholds_max);

		if (ret != OK) {
			log_error("%s production_test_data: checkLimitsMapAdj CX2 ADJH failed... ERROR COUNT = %d\n", tag, ret);
			log_debug("%s MS CX2 ADJ HORIZ TEST:.................FAIL\n\n", tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			log_debug("%s MS CX2 ADJ HORIZ TEST:.................OK\n\n", tag);

		kfree(thresholds_max);
		thresholds_max = NULL;
		kfree(adjhor);
		adjhor = NULL;
		log_debug("%s MS CX2 ADJ VERT TEST:\n", tag);
		ret = computeAdjVert(msCompData.node_data, msCompData.header.force_node, msCompData.header.sense_node, &adjvert);

		if (ret < 0) {
			log_error("%s production_test_data: computeAdjVert failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_MS_CX);
			ret = ERROR_PROD_TEST_DATA_MS_CX;
			goto ERROR_LIMITS;
		}

		log_debug("%s MS CX2 ADJ VERT computed!\n", tag);
		ret = parseProductionTestLimits(path_limits, MS_CX2_ADJV_MAP_MAX, &thresholds_max, &trows, &tcolumns);

		if (ret < 0 || (trows != msCompData.header.force_node - 1 || tcolumns != msCompData.header.sense_node)) {
			log_error("%s production_test_data: parseProductionTestLimits MS_CX2_ADJV_MAP_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_MS_CX);
			ret = ERROR_PROD_TEST_DATA_MS_CX;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMapAdj(adjvert, msCompData.header.force_node - 1, msCompData.header.sense_node - 1, thresholds_max);

		if (ret != OK) {
			log_error("%s production_test_data: checkLimitsMapAdj CX2 ADJV failed... ERROR COUNT = %d\n", tag, ret);
			log_debug("%s MS CX2 ADJ HORIZ TEST:.................FAIL\n\n", tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			log_debug("%s MS CX2 ADJ VERT TEST:.................OK\n\n", tag);

		kfree(thresholds_max);
		thresholds_max = NULL;
		kfree(adjvert);
		adjvert = NULL;
	} else
		log_debug("%s MS CX2 ADJ TEST:.................SKIPPED\n\n", tag);

	log_debug("%s MS TOTAL CX TEST:\n", tag);

	if (todo->MutualCxTotal == 1 || todo->MutualCxTotalAdj == 1) {
		ret = computeTotal(msCompData.node_data, msCompData.cx1, msCompData.header.force_node, msCompData.header.sense_node, CX1_WEIGHT, CX2_WEIGHT, &total_cx);

		if (ret < 0) {
			log_error("%s production_test_data: computeTotalCx failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_MS_CX);
			ret = ERROR_PROD_TEST_DATA_MS_CX;
			goto ERROR_LIMITS;
		}

		log_debug("%s MS TOTAL CX MIN MAX TEST:\n", tag);

		if (todo->MutualCxTotal == 1) {
			ret = parseProductionTestLimits(path_limits, MS_TOTAL_CX_MAP_MIN, &thresholds_min, &trows, &tcolumns);

			if (ret < 0 || (trows != msCompData.header.force_node || tcolumns != msCompData.header.sense_node)) {
				log_error("%s production_test_data: parseProductionTestLimits MS_TOTAL_CX_MAP_MIN failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_MS_CX);
				ret = ERROR_PROD_TEST_DATA_MS_CX;
				goto ERROR_LIMITS;
			}

			ret = parseProductionTestLimits(path_limits, MS_TOTAL_CX_MAP_MAX, &thresholds_max, &trows, &tcolumns);

			if (ret < 0 || (trows != msCompData.header.force_node || tcolumns != msCompData.header.sense_node)) {
				log_error("%s production_test_data: parseProductionTestLimits MS_TOTAL_CX_MAP_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_MS_CX);
				ret = ERROR_PROD_TEST_DATA_MS_CX;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMapTotal(total_cx, msCompData.header.force_node, msCompData.header.sense_node, thresholds_min, thresholds_max);

			if (ret != OK) {
				log_error("%s production_test_data: checkLimitsMap  MS TOTAL CX TEST failed... ERROR COUNT = %d\n", tag, ret);
				log_debug("%s MS TOTAL CX MIN MAX TEST:.................FAIL\n\n", tag);
				count_fail += 1;

				if (stop_on_fail)
					goto ERROR;
			} else
				log_debug("%s MS TOTAL CX MIN MAX TEST:.................OK\n\n", tag);

			kfree(thresholds_min);
			thresholds_min = NULL;
			kfree(thresholds_max);
			thresholds_max = NULL;
		} else
			log_debug("%s MS TOTAL CX MIN MAX TEST:.................SKIPPED\n\n", tag);

		log_debug("%s MS TOTAL CX ADJ TEST:\n", tag);

		if (todo->MutualCxTotalAdj == 1) {
			log_debug("%s MS TOTAL CX ADJ HORIZ TEST:\n", tag);
			ret = computeAdjHorizTotal(total_cx, msCompData.header.force_node, msCompData.header.sense_node, &total_adjhor);

			if (ret < 0) {
				log_error("%s production_test_data: computeAdjHoriz failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_MS_CX);
				ret = ERROR_PROD_TEST_DATA_MS_CX;
				goto ERROR_LIMITS;
			}

			log_debug("%s MS TOTAL CX ADJ HORIZ computed!\n", tag);
			ret = parseProductionTestLimits(path_limits, MS_TOTAL_CX_ADJH_MAP_MAX, &thresholds_max, &trows, &tcolumns);

			if (ret < 0 || (trows != msCompData.header.force_node || tcolumns != msCompData.header.sense_node - 1)) {
				log_error("%s production_test_data: parseProductionTestLimits MS_TOTAL_CX_ADJH_MAP_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_MS_CX);
				ret = ERROR_PROD_TEST_DATA_MS_CX;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMapAdjTotal(total_adjhor, msCompData.header.force_node, msCompData.header.sense_node - 1, thresholds_max);

			if (ret != OK) {
				log_error("%s production_test_data: checkLimitsMapAdj MS TOTAL CX ADJH failed... ERROR COUNT = %d\n", tag, ret);
				log_debug("%s MS TOTAL CX ADJ HORIZ TEST:.................FAIL\n\n", tag);
				count_fail += 1;

				if (stop_on_fail)
					goto ERROR;
			} else
				log_debug("%s MS TOTAL CX ADJ HORIZ TEST:.................OK\n\n", tag);

			kfree(thresholds_max);
			thresholds_max = NULL;
			kfree(total_adjhor);
			total_adjhor = NULL;
			log_debug("%s MS TOTAL CX ADJ VERT TEST:\n", tag);
			ret = computeAdjVertTotal(total_cx, msCompData.header.force_node, msCompData.header.sense_node, &total_adjvert);

			if (ret < 0) {
				log_error("%s production_test_data: computeAdjVert failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_MS_CX);
				ret = ERROR_PROD_TEST_DATA_MS_CX;
				goto ERROR_LIMITS;
			}

			log_debug("%s MS TOTAL CX ADJ VERT computed!\n", tag);
			ret = parseProductionTestLimits(path_limits, MS_TOTAL_CX_ADJV_MAP_MAX, &thresholds_max, &trows, &tcolumns);

			if (ret < 0 || (trows != msCompData.header.force_node - 1 || tcolumns != msCompData.header.sense_node)) {
				log_error("%s production_test_data: parseProductionTestLimits MS_TOTAL_CX_ADJV_MAP_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_MS_CX);
				ret = ERROR_PROD_TEST_DATA_MS_CX;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMapAdjTotal(total_adjvert, msCompData.header.force_node - 1, msCompData.header.sense_node - 1, thresholds_max);

			if (ret != OK) {
				log_error("%s production_test_data: checkLimitsMapAdj MS TOTAL CX ADJV failed... ERROR COUNT = %d\n", tag, ret);
				log_debug("%s MS TOTAL CX ADJ HORIZ TEST:.................FAIL\n", tag);
				count_fail += 1;

				if (stop_on_fail)
					goto ERROR;
			} else
				log_debug("%s MS TOTAL CX ADJ VERT TEST:.................OK\n", tag);

			kfree(thresholds_max);
			thresholds_max = NULL;
			kfree(total_adjvert);
			total_adjvert = NULL;
		} else
			log_debug("%s MS TOTAL CX ADJ TEST:.................SKIPPED\n", tag);

		kfree(total_cx);
		total_cx = NULL;
	} else
		log_debug("%s MS TOTAL CX TEST:.................SKIPPED\n", tag);

	if ((todo->MutualKeyCx1 | todo->MutualKeyCx2 | todo->MutualKeyCxTotal) == 1) {
		ret = production_test_ms_key_cx(path_limits, stop_on_fail, todo);

		if (ret < 0) {
			count_fail += 1;
			log_error("%s production_test_data: production_test_ms_key_cx failed... ERROR = %02X\n", tag, ret);
			log_debug("%s MS CX testes finished!.................FAILED  fails_count = %d\n\n", tag, count_fail);
			return ret;
		}
	} else
		log_debug("%s MS KEY CX TEST:.................SKIPPED\n", tag);

ERROR:
	log_debug("%s\n", tag);

	if (count_fail == 0) {
		log_debug("%s MS CX testes finished!.................OK\n", tag);
		kfree(msCompData.node_data);
		msCompData.node_data = NULL;
		return OK;
	} else {
		log_debug("%s MS CX testes finished!.................FAILED  fails_count = %d\n\n", tag, count_fail);

		if (thresholds != NULL) {
			kfree(thresholds);
			thresholds = NULL;
		}

		if (thresholds_min != NULL) {
			kfree(thresholds_min);
			thresholds_min = NULL;
		}

		if (thresholds_max != NULL) {
			kfree(thresholds_max);
			thresholds_max = NULL;
		}

		if (adjhor != NULL) {
			kfree(adjhor);
			adjhor = NULL;
		}

		if (adjvert != NULL) {
			kfree(adjvert);
			adjvert = NULL;
		}

		if (total_cx != NULL) {
			kfree(total_cx);
			total_cx = NULL;
		}

		if (total_adjhor != NULL) {
			kfree(total_adjhor);
			total_adjhor = NULL;
		}

		if (total_adjvert != NULL) {
			kfree(total_adjvert);
			total_adjvert = NULL;
		}

		if (msCompData.node_data != NULL) {
			kfree(msCompData.node_data);
			msCompData.node_data = NULL;
		}

		return ERROR_PROD_TEST_DATA_MS_CX;
	}

ERROR_LIMITS:

	if (thresholds != NULL) {
		kfree(thresholds);
		thresholds = NULL;
	}

	if (thresholds_min != NULL) {
		kfree(thresholds_min);
		thresholds_min = NULL;
	}

	if (thresholds_max != NULL) {
		kfree(thresholds_max);
		thresholds_max = NULL;
	}

	if (adjhor != NULL) {
		kfree(adjhor);
		adjhor = NULL;
	}

	if (adjvert != NULL) {
		kfree(adjvert);
		adjvert = NULL;
	}

	if (total_cx != NULL) {
		kfree(total_cx);
		total_cx = NULL;
	}

	if (total_adjhor != NULL) {
		kfree(total_adjhor);
		total_adjhor = NULL;
	}

	if (total_adjvert != NULL) {
		kfree(total_adjvert);
		total_adjvert = NULL;
	}

	if (msCompData.node_data != NULL) {
		kfree(msCompData.node_data);
		msCompData.node_data = NULL;
	}

	return ret;
}

int production_test_ms_key_cx(const char *path_limits, int stop_on_fail, TestToDo *todo)
{
	int ret;
	int count_fail = 0;
	int num_keys = 0;
	int *thresholds = NULL;
	int *thresholds_min = NULL;
	int *thresholds_max = NULL;
	int trows, tcolumns;
	MutualSenseData msCompData;
	u16 container;
	u16 *total_cx = NULL;
	log_debug("%s MS KEY CX Testes are starting...\n", tag);
	ret = readMutualSenseCompensationData(MS_KEY, &msCompData);

	if (ret < 0) {
		log_error("%s production_test_data: readMutualSenseCompensationData failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_MS_KEY_CX);
		return ERROR_PROD_TEST_DATA_MS_KEY_CX;
	}

	if (msCompData.header.force_node > msCompData.header.sense_node)
		num_keys = msCompData.header.force_node;
	else
		num_keys = msCompData.header.sense_node;

	print_frame_u8("MS Key Init Data (Cx2) =", array1dTo2d_u8(msCompData.node_data, msCompData.node_data_size, msCompData.header.sense_node), 1, msCompData.header.sense_node);
	log_debug("%s MS KEY CX1 TEST:\n", tag);

	if (todo->MutualKeyCx1 == 1) {
		ret = parseProductionTestLimits(path_limits, MS_KEY_CX1_MIN_MAX, &thresholds, &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != 2)) {
			log_error("%s production_test_data: parseProductionTestLimits MS_KEY_CX1_MIN_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_MS_KEY_CX);
			ret = ERROR_PROD_TEST_DATA_MS_KEY_CX;
			goto ERROR_LIMITS;
		}

		container = (u16) msCompData.cx1;
		ret = checkLimitsMinMax(&container, 1, 1, thresholds[0], thresholds[1]);

		if (ret != OK) {
			log_error("%s production_test_data: checkLimitsMinMax MS CX1 failed... ERROR COUNT = %d\n", tag, ret);
			log_debug("%s MS KEY CX1 TEST:.................FAIL\n\n", tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			log_debug("%s MS KEY CX1 TEST:.................OK\n\n", tag);
	} else
		log_debug("%s MS KEY CX1 TEST:.................SKIPPED\n\n", tag);

	kfree(thresholds);
	thresholds = NULL;
	log_debug("%s MS KEY CX2 TEST:\n", tag);

	if (todo->MutualKeyCx2 == 1) {
		ret = parseProductionTestLimits(path_limits, MS_KEY_CX2_MAP_MIN, &thresholds_min, &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != num_keys)) {
			log_error("%s production_test_data: parseProductionTestLimits MS_KEY_CX2_MAP_MIN failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_MS_KEY_CX);
			ret = ERROR_PROD_TEST_DATA_MS_KEY_CX;
			goto ERROR_LIMITS;
		}

		ret = parseProductionTestLimits(path_limits, MS_KEY_CX2_MAP_MAX, &thresholds_max, &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != num_keys)) {
			log_error("%s production_test_data: parseProductionTestLimits MS_KEY_CX2_MAP_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_MS_KEY_CX);
			ret = ERROR_PROD_TEST_DATA_MS_KEY_CX;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMap(msCompData.node_data, 1, num_keys, thresholds_min, thresholds_max);

		if (ret != OK) {
			log_error("%s production_test_data: checkLimitsMap MS KEY CX2 failed... ERROR COUNT = %d\n", tag, ret);
			log_debug("%s MS KEY CX2 TEST:.................FAIL\n\n", tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			log_debug("%s MS KEY CX2 TEST:.................OK\n\n", tag);

		kfree(thresholds_min);
		thresholds_min = NULL;
		kfree(thresholds_max);
		thresholds_max = NULL;
	} else
		log_debug("%s MS CX2 TEST:.................SKIPPED\n\n", tag);

	log_debug("%s MS KEY TOTAL CX TEST:\n", tag);

	if (todo->MutualKeyCxTotal == 1) {
		ret = computeTotal(msCompData.node_data, msCompData.cx1, 1, num_keys, CX1_WEIGHT, CX2_WEIGHT, &total_cx);

		if (ret < 0) {
			log_error("%s production_test_data: computeTotalCx failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_MS_KEY_CX);
			ret = ERROR_PROD_TEST_DATA_MS_KEY_CX;
			goto ERROR_LIMITS;
		}

		ret = parseProductionTestLimits(path_limits, MS_KEY_TOTAL_CX_MAP_MIN, &thresholds_min, &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != num_keys)) {
			log_error("%s production_test_data: parseProductionTestLimits MS_KEY_TOTAL_CX_MAP_MIN failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_MS_KEY_CX);
			ret = ERROR_PROD_TEST_DATA_MS_KEY_CX;
			goto ERROR_LIMITS;
		}

		ret = parseProductionTestLimits(path_limits, MS_KEY_TOTAL_CX_MAP_MAX, &thresholds_max, &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != num_keys)) {
			log_error("%s production_test_data: parseProductionTestLimits MS_KEY_TOTAL_CX_MAP_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_MS_KEY_CX);
			ret = ERROR_PROD_TEST_DATA_MS_KEY_CX;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMapTotal(total_cx, 1, num_keys, thresholds_min, thresholds_max);

		if (ret != OK) {
			log_error("%s production_test_data: checkLimitsMap  MS TOTAL KEY CX TEST failed... ERROR COUNT = %d\n", tag, ret);
			log_debug("%s MS KEY TOTAL CX TEST:.................FAIL\n\n", tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			log_debug("%s MS KEY TOTAL CX TEST:.................OK\n\n", tag);

		kfree(thresholds_min);
		thresholds_min = NULL;
		kfree(thresholds_max);
		thresholds_max = NULL;
		kfree(total_cx);
		total_cx = NULL;
	} else
		log_debug("%s MS KEY TOTAL CX TEST:.................SKIPPED\n", tag);

ERROR:
	log_debug("%s\n", tag);

	if (count_fail == 0) {
		log_debug("%s MS KEY CX testes finished!.................OK\n", tag);
		kfree(msCompData.node_data);
		msCompData.node_data = NULL;
		return OK;
	} else {
		log_debug("%s MS Key CX testes finished!.................FAILED  fails_count = %d\n\n", tag, count_fail);

		if (thresholds != NULL) {
			kfree(thresholds);
			thresholds = NULL;
		}

		if (thresholds_min != NULL) {
			kfree(thresholds_min);
			thresholds_min = NULL;
		}

		if (thresholds_max != NULL) {
			kfree(thresholds_max);
			thresholds_max = NULL;
		}

		if (msCompData.node_data != NULL) {
			kfree(msCompData.node_data);
			msCompData.node_data = NULL;
		}

		if (total_cx != NULL) {
			kfree(total_cx);
			total_cx = NULL;
		}

		return ERROR_PROD_TEST_DATA_MS_KEY_CX;
	}

ERROR_LIMITS:

	if (thresholds != NULL) {
		kfree(thresholds);
		thresholds = NULL;
	}

	if (thresholds_min != NULL) {
		kfree(thresholds_min);
		thresholds_min = NULL;
	}

	if (thresholds_max != NULL) {
		kfree(thresholds_max);
		thresholds_max = NULL;
	}

	if (msCompData.node_data != NULL) {
		kfree(msCompData.node_data);
		msCompData.node_data = NULL;
	}

	if (total_cx != NULL) {
		kfree(total_cx);
		total_cx = NULL;
	}

	return ret;
}

int production_test_ss_raw(const char *path_limits, int stop_on_fail, TestToDo *todo)
{
	int ret;
	int count_fail = 0;
	int rows, columns;
	SelfSenseFrame ssRawFrame;
	int *thresholds = NULL;
	int trows, tcolumns;
	log_debug("%s\n", tag);
	log_debug("%s SS RAW Testes are starting...\n", tag);
	log_debug("%s Getting SS Frame...\n", tag);
	ret = getSSFrame2(SS_TOUCH, &ssRawFrame);

	if (ret < 0) {
		log_error("%s production_test_data: getSSFrame failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_RAW);
		return ERROR_PROD_TEST_DATA_SS_RAW;
	}

	log_debug("%s SS RAW (PROXIMITY) FORCE TEST: \n", tag);

	if (todo->SelfForceRaw == 1 || todo->SelfForceRawGap == 1) {
		columns = 1;
		rows = ssRawFrame.header.force_node;
		print_frame_short("SS Raw force frame =", array1dTo2d_short(ssRawFrame.force_data, rows * columns, columns), rows, columns);
		log_debug("%s SS RAW (PROXIMITY) FORCE MIN MAX TEST: \n", tag);

		if (todo->SelfForceRaw == 1) {
			ret = parseProductionTestLimits(path_limits, SS_RAW_FORCE_MIN_MAX, &thresholds, &trows, &tcolumns);

			if (ret < 0 || (trows != 1 || tcolumns != 2)) {
				log_error("%s production_test_data: parseProductionTestLimits SS_RAW_FORCE_MIN_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_RAW);
				ret = ERROR_PROD_TEST_DATA_SS_RAW;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMinMax(ssRawFrame.force_data, rows, columns, thresholds[0], thresholds[1]);

			if (ret != OK) {
				log_error("%s production_test_data: checkLimitsMinMax SS RAW (PROXIMITY) FORCE failed... ERROR COUNT = %d\n", tag, ret);
				log_debug("%s SS RAW (PROXIMITY) FORCE MIN MAX TEST:.................FAIL\n\n", tag);
				count_fail += 1;

				if (stop_on_fail) {
					ret = ERROR_PROD_TEST_DATA_SS_RAW;
					goto ERROR_LIMITS;
				}
			} else
				log_debug("%s SS RAW (PROXIMITY) FORCE MIN MAX TEST:.................OK\n\n", tag);

			kfree(thresholds);
			thresholds = NULL;
		} else
			log_debug("%s SS RAW (PROXIMITY) FORCE MIN MAX TEST:.................SKIPPED\n\n", tag);

		log_debug("%s\n", tag);
		log_debug("%s SS RAW (PROXIMITY) FORCE GAP TEST: \n", tag);

		if (todo->SelfForceRawGap == 1) {
			ret = parseProductionTestLimits(path_limits, SS_RAW_FORCE_GAP, &thresholds, &trows, &tcolumns);

			if (ret < 0 || (trows != 1 || tcolumns != 1)) {
				log_error("%s production_test_data: parseProductionTestLimits SS_RAW_FORCE_GAP failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_RAW);
				ret = ERROR_PROD_TEST_DATA_SS_RAW;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsGap(ssRawFrame.force_data, rows, columns, thresholds[0]);

			if (ret != OK) {
				log_error("%s production_test_data: checkLimitsGap SS RAW (PROXIMITY) FORCE GAP failed... ERROR = %02X\n", tag, ret);
				log_debug("%s SS RAW (PROXIMITY) FORCE GAP TEST:.................FAIL\n\n", tag);
				count_fail += 1;

				if (stop_on_fail) {
					ret = ERROR_PROD_TEST_DATA_SS_RAW;
					goto ERROR_LIMITS;
				}
			} else
				log_debug("%s SS RAW (PROXIMITY) FORCE GAP TEST:.................OK\n\n", tag);

			kfree(thresholds);
			thresholds = NULL;
		} else
			log_debug("%s SS RAW (PROXIMITY) FORCE GAP TEST:.................SKIPPED\n\n", tag);

		kfree(ssRawFrame.force_data);
		ssRawFrame.force_data = NULL;
	} else
		log_debug("%s SS RAW (PROXIMITY) FORCE TEST:.................SKIPPED\n\n", tag);

	log_debug("%s\n", tag);
	log_debug("%s SS RAW (PROXIMITY) SENSE TEST: \n", tag);

	if (todo->SelfSenseRaw == 1 || todo->SelfSenseRawGap == 1) {
		columns = ssRawFrame.header.sense_node;
		rows = 1;
		print_frame_short("SS Raw sense frame =", array1dTo2d_short(ssRawFrame.sense_data, rows * columns, columns), rows, columns);
		log_debug("%s SS RAW (PROXIMITY) SENSE MIN MAX TEST: \n", tag);

		if (todo->SelfSenseRaw == 1) {
			ret = parseProductionTestLimits(path_limits, SS_RAW_SENSE_MIN_MAX, &thresholds, &trows, &tcolumns);

			if (ret < 0 || (trows != 1 || tcolumns != 2)) {
				log_error("%s production_test_data: parseProductionTestLimits SS_RAW_SENSE_MIN_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_RAW);
				ret = ERROR_PROD_TEST_DATA_SS_RAW;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMinMax(ssRawFrame.sense_data, rows, columns, thresholds[0], thresholds[1]);

			if (ret != OK) {
				log_error("%s production_test_data: checkLimitsMinMax SS RAW (PROXIMITY) SENSE failed... ERROR COUNT = %d\n", tag, ret);
				log_debug("%s SS RAW (PROXIMITY) SENSE MIN MAX TEST:.................FAIL\n", tag);
				count_fail += 1;

				if (stop_on_fail) {
					ret = ERROR_PROD_TEST_DATA_SS_RAW;
					goto ERROR_LIMITS;
				}
			} else
				log_debug("%s SS RAW (PROXIMITY) SENSE MIN MAX TEST:.................OK\n", tag);

			kfree(thresholds);
			thresholds = NULL;
		} else
			log_debug("%s SS RAW (PROXIMITY) SENSE MIN MAX TEST:.................SKIPPED\n", tag);

		log_debug("%s\n", tag);
		log_debug("%s SS RAW (PROXIMITY) SENSE GAP TEST: \n", tag);

		if (todo->SelfSenseRawGap == 1) {
			ret = parseProductionTestLimits(path_limits, SS_RAW_SENSE_GAP, &thresholds, &trows, &tcolumns);

			if (ret < 0 || (trows != 1 || tcolumns != 1)) {
				log_error("%s production_test_data: parseProductionTestLimits SS_RAW_SENSE_GAP failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_RAW);
				ret = ERROR_PROD_TEST_DATA_SS_RAW;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsGap(ssRawFrame.sense_data, rows, columns, thresholds[0]);

			if (ret != OK) {
				log_error("%s production_test_data: checkLimitsGap SS RAW (PROXIMITY) SENSE GAP failed... ERROR = %02X\n", tag, ret);
				log_debug("%s SS RAW (PROXIMITY) SENSE GAP TEST:.................FAIL\n", tag);
				count_fail += 1;

				if (stop_on_fail) {
					ret = ERROR_PROD_TEST_DATA_SS_RAW;
					goto ERROR_LIMITS;
				}
			} else
				log_debug("%s SS RAW (PROXIMITY) SENSE GAP TEST:.................OK\n", tag);

			kfree(thresholds);
			thresholds = NULL;
		} else
			log_debug("%s SS RAW (PROXIMITY) SENSE GAP TEST:.................SKIPPED\n", tag);

		kfree(ssRawFrame.sense_data);
		ssRawFrame.sense_data = NULL;
	}

	log_debug("%s\n", tag);

	if (count_fail == 0) {
		log_debug("%s SS RAW testes finished!.................OK\n\n", tag);
		return OK;
	} else {
		log_debug("%s SS RAW testes finished!.................FAILED  fails_count = %d\n\n", tag, count_fail);
		return ERROR_PROD_TEST_DATA_SS_RAW;
	}

ERROR_LIMITS:

	if (ssRawFrame.force_data != NULL) {
		kfree(ssRawFrame.force_data);
		ssRawFrame.force_data = NULL;
	}

	if (ssRawFrame.sense_data != NULL) {
		kfree(ssRawFrame.sense_data);
		ssRawFrame.sense_data = NULL;
	}

	if (thresholds != NULL) {
		kfree(thresholds);
		thresholds = NULL;
	}

	return ret;
}

int production_test_ss_ix_cx(const char *path_limits, int stop_on_fail, TestToDo *todo)
{
	int ret;
	int count_fail = 0;
	int *thresholds = NULL;
	int trows, tcolumns;
	int *thresholds_min = NULL;
	int *thresholds_max = NULL;
	SelfSenseData ssCompData;
	u8 *adjhor = NULL;
	u8 *adjvert = NULL;
	u16 container;
	int *ix1_w = NULL;
	int *ix2_w = NULL;
	u16 *total_ix = NULL;
	u16 *total_cx = NULL;
	u16 *total_adjhor = NULL;
	u16 *total_adjvert = NULL;
	log_debug("%s\n", tag);
	log_debug("%s SS IX CX testes are starting... \n", tag);
	ret = readSelfSenseCompensationData(SS_TOUCH, &ssCompData);

	if (ret < 0) {
		log_error("%s production_test_data: readSelfSenseCompensationData failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
		return  ERROR_PROD_TEST_DATA_SS_IX_CX;
	}

	print_frame_u8("SS Init Data Ix2_fm = ", array1dTo2d_u8(ssCompData.ix2_fm, ssCompData.header.force_node, ssCompData.header.force_node), 1, ssCompData.header.force_node);
	print_frame_u8("SS Init Data Cx2_fm = ", array1dTo2d_u8(ssCompData.cx2_fm, ssCompData.header.force_node, ssCompData.header.force_node), 1, ssCompData.header.force_node);
	print_frame_u8("SS Init Data Ix2_sn = ", array1dTo2d_u8(ssCompData.ix2_sn, ssCompData.header.sense_node, ssCompData.header.sense_node), 1, ssCompData.header.sense_node);
	print_frame_u8("SS Init Data Cx2_sn = ", array1dTo2d_u8(ssCompData.cx2_sn, ssCompData.header.sense_node, ssCompData.header.sense_node), 1, ssCompData.header.sense_node);
	log_debug("%s SS IX1 FORCE TEST: \n", tag);

	if (todo->SelfForceIx1 == 1) {
		ret = parseProductionTestLimits(path_limits, SS_IX1_FORCE_MIN_MAX, &thresholds, &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != 2)) {
			log_error("%s production_test_data: parseProductionTestLimits SS_IX1_FORCE_MIN_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
			goto ERROR_LIMITS;
		}

		container = (u16) ssCompData.f_ix1;
		ret = checkLimitsMinMax(&container, 1, 1, thresholds[0], thresholds[1]);

		if (ret != OK) {
			log_error("%s production_test_data: checkLimitsMinMax SS IX1 FORCE TEST failed... ERROR COUNT = %d\n", tag, ret);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			log_debug("%s SS IX1 FORCE TEST:.................OK\n\n", tag);
	} else
		log_debug("%s SS IX1 FORCE TEST:.................SKIPPED\n\n", tag);

	kfree(thresholds);
	thresholds = NULL;
	log_debug("%s SS IX2 FORCE MIN MAX TEST: \n", tag);

	if (todo->SelfForceIx2 == 1) {
		ret = parseProductionTestLimits(path_limits, SS_IX2_FORCE_MAP_MIN, &thresholds_min, &trows, &tcolumns);

		if (ret < 0 || (trows != ssCompData.header.force_node || tcolumns != 1)) {
			log_error("%s production_test_data: parseProductionTestLimits SS_IX2_FORCE_MAP_MIN failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
			goto ERROR_LIMITS;
		}

		ret = parseProductionTestLimits(path_limits, SS_IX2_FORCE_MAP_MAX, &thresholds_max, &trows, &tcolumns);

		if (ret < 0 || (trows != ssCompData.header.force_node || tcolumns != 1)) {
			log_error("%s production_test_data: parseProductionTestLimits SS_IX2_FORCE_MAP_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMap(ssCompData.ix2_fm, ssCompData.header.force_node, 1, thresholds_min, thresholds_max);

		if (ret != OK) {
			log_error("%s production_test_data: checkLimitsMap SS IX2 FORCE failed... ERROR COUNT = %d\n", tag, ret);
			log_debug("%s SS IX2 FORCE MIN MAX TEST:.................FAIL\n\n", tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			log_debug("%s SS IX2 FORCE MIN MAX TEST:.................OK\n\n", tag);

		kfree(thresholds_min);
		thresholds_min = NULL;
		kfree(thresholds_max);
		thresholds_max = NULL;
	} else
		log_debug("%s SS IX2 FORCE MIN MAX TEST:.................SKIPPED\n\n", tag);

	log_debug("%s SS IX2 FORCE ADJ TEST: \n", tag);

	if (todo->SelfForceIx2Adj == 1) {
		log_debug("%s SS IX2 FORCE ADJVERT TEST: \n", tag);
		ret = computeAdjVert(ssCompData.ix2_fm, ssCompData.header.force_node, 1, &adjvert);

		if (ret < 0) {
			log_error("%s production_test_data: computeAdjVert SS IX2 FORCE ADJV failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
			goto ERROR_LIMITS;
		}

		log_debug("%s SS IX2 FORCE ADJV computed!\n", tag);
		ret = parseProductionTestLimits(path_limits, SS_IX2_FORCE_ADJV_MAP_MAX, &thresholds_max, &trows, &tcolumns);

		if (ret < 0 || (trows != ssCompData.header.force_node - 1 || tcolumns != 1)) {
			log_error("%s production_test_data: parseProductionTestLimits SS_IX2_FORCE_ADJV_MAP_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMapAdj(adjvert, ssCompData.header.force_node - 1, 1, thresholds_max);

		if (ret != OK) {
			log_error("%s production_test_data: checkLimitsMap SS IX2 FORCE failed... ERROR COUNT = %d\n", tag, ret);
			log_debug("%s SS IX2 FORCE ADJV TEST:.................FAIL\n\n", tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			log_debug("%s SS IX2 FORCE ADJV TEST:.................OK\n\n", tag);

		kfree(thresholds_max);
		thresholds_max = NULL;
		kfree(adjvert);
		adjvert = NULL;
	} else
		log_debug("%s SS IX2 FORCE ADJ TEST:.................SKIPPED\n\n", tag);

	log_debug("%s SS TOTAL IX FORCE TEST: \n", tag);

	if (todo->SelfForceIxTotal == 1 || todo->SelfForceIxTotalAdj == 1) {
		log_debug("%s Reading TOTAL IX FORCE Weights... \n", tag);
		ret = parseProductionTestLimits(path_limits, SS_IX1_FORCE_W, &ix1_w, &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != 1)) {
			log_error("%s production_test_data: parseProductionTestLimits SS_IX1_FORCE_W failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			return ERROR_PROD_TEST_DATA_SS_IX_CX;
		}

		ret = parseProductionTestLimits(path_limits, SS_IX2_FORCE_W, &ix2_w, &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != 1)) {
			log_error("%s production_test_data: parseProductionTestLimits SS_IX1_FORCE_W failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			return ERROR_PROD_TEST_DATA_SS_IX_CX;
		}

		log_debug("%s Weights: IX1_W = %d   IX2_W = %d \n", tag, *ix1_w, *ix2_w);
		ret = computeTotal(ssCompData.ix2_fm, ssCompData.f_ix1, ssCompData.header.force_node, 1, *ix1_w, *ix2_w, &total_ix);

		if (ret < 0) {
			log_error("%s production_test_data: computeTotal Ix Force failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
			goto ERROR_LIMITS;
		}

		kfree(ix1_w);
		ix1_w = NULL;
		kfree(ix2_w);
		ix2_w = NULL;
		log_debug("%s SS TOTAL IX FORCE MIN MAX TEST: \n", tag);

		if (todo->SelfForceIxTotal == 1) {
			ret = parseProductionTestLimits(path_limits, SS_TOTAL_IX_FORCE_MAP_MIN, &thresholds_min, &trows, &tcolumns);

			if (ret < 0 || (trows != ssCompData.header.force_node || tcolumns != 1)) {
				log_error("%s production_test_data: parseProductionTestLimits SS_TOTAL_IX_FORCE_MAP_MIN failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
				ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
				goto ERROR_LIMITS;
			}

			ret = parseProductionTestLimits(path_limits, SS_TOTAL_IX_FORCE_MAP_MAX, &thresholds_max, &trows, &tcolumns);

			if (ret < 0 || (trows != ssCompData.header.force_node || tcolumns != 1)) {
				log_error("%s production_test_data: parseProductionTestLimits SS_TOTAL_IX_FORCE_MAP_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
				ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMapTotal(total_ix, ssCompData.header.force_node, 1, thresholds_min, thresholds_max);

			if (ret != OK) {
				log_error("%s production_test_data: checkLimitsMap  SS TOTAL IX FORCE failed... ERROR COUNT = %d\n", tag, ret);
				log_debug("%s SS TOTAL IX FORCE MIN MAX TEST:.................FAIL\n\n", tag);
				count_fail += 1;

				if (stop_on_fail)
					goto ERROR;
			} else
				log_debug("%s SS TOTAL IX FORCE MIN MAX TEST:.................OK\n\n", tag);

			kfree(thresholds_min);
			thresholds_min = NULL;
			kfree(thresholds_max);
			thresholds_max = NULL;
		} else
			log_debug("%s SS TOTAL IX FORCE MIN MAX TEST:.................SKIPPED\n", tag);

		log_debug("%s SS TOTAL IX FORCE ADJ TEST: \n", tag);

		if (todo->SelfForceIxTotalAdj == 1) {
			log_debug("%s SS TOTAL IX FORCE ADJVERT TEST: \n", tag);
			ret = computeAdjVertTotal(total_ix, ssCompData.header.force_node, 1, &total_adjvert);

			if (ret < 0) {
				log_error("%s production_test_data: computeAdjVert SS TOTAL IX FORCE ADJV failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
				ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
				goto ERROR_LIMITS;
			}

			log_debug("%s SS TOTAL IX FORCE ADJV computed!\n", tag);
			ret = parseProductionTestLimits(path_limits, SS_TOTAL_IX_FORCE_ADJV_MAP_MAX, &thresholds_max, &trows, &tcolumns);

			if (ret < 0 || (trows != ssCompData.header.force_node - 1 || tcolumns != 1)) {
				log_error("%s production_test_data: parseProductionTestLimits SS_TOTAL_IX_FORCE_ADJV_MAP_MAX... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
				ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMapAdjTotal(total_adjvert, ssCompData.header.force_node - 1, 1, thresholds_max);

			if (ret != OK) {
				log_error("%s production_test_data: checkLimitsMap SS TOTAL IX FORCE failed... ERROR COUNT = %d\n", tag, ret);
				log_debug("%s SS TOTAL IX FORCE ADJV TEST:.................FAIL\n\n", tag);
				count_fail += 1;

				if (stop_on_fail)
					goto ERROR;
			} else
				log_debug("%s SS TOTAL IX FORCE ADJV TEST:.................OK\n\n", tag);

			kfree(thresholds_max);
			thresholds_max = NULL;
			kfree(total_adjvert);
			total_adjvert = NULL;
		} else
			log_debug("%s SS TOTAL IX FORCE ADJ TEST:.................SKIPPED \n", tag);

		kfree(total_ix);
		total_ix = NULL;
	} else
		log_debug("%s SS TOTAL IX FORCE TEST:.................SKIPPED\n\n", tag);

	log_debug("%s SS IX1 SENSE TEST: \n", tag);

	if (todo->SelfSenseIx1 == 1) {
		ret = parseProductionTestLimits(path_limits, SS_IX1_SENSE_MIN_MAX, &thresholds, &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != 2)) {
			log_error("%s production_test_data: parseProductionTestLimits SS_IX1_SENSE_MIN_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
			goto ERROR_LIMITS;
		}

		container = (u16) ssCompData.s_ix1;
		ret = checkLimitsMinMax(&container, 1, 1, thresholds[0], thresholds[1]);

		if (ret != OK) {
			log_error("%s production_test_data: checkLimitsMinMax SS IX1 SENSE TEST failed... ERROR COUNT = %d\n", tag, ret);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			log_debug("%s SS IX1 SENSE TEST:.................OK\n\n", tag);
	} else
		log_debug("%s SS IX1 SENSE TEST:.................SKIPPED\n\n", tag);

	kfree(thresholds);
	thresholds = NULL;
	log_debug("%s SS IX2 SENSE MIN MAX TEST: \n", tag);

	if (todo->SelfSenseIx2 == 1) {
		ret = parseProductionTestLimits(path_limits, SS_IX2_SENSE_MAP_MIN, &thresholds_min, &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != ssCompData.header.sense_node)) {
			log_error("%s production_test_data: parseProductionTestLimits SS_IX2_SENSE_MAP_MIN failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
			goto ERROR_LIMITS;
		}

		ret = parseProductionTestLimits(path_limits, SS_IX2_SENSE_MAP_MAX, &thresholds_max, &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != ssCompData.header.sense_node)) {
			log_error("%s production_test_data: parseProductionTestLimits SS_IX2_SENSE_MAP_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMap(ssCompData.ix2_sn, 1, ssCompData.header.sense_node, thresholds_min, thresholds_max);

		if (ret != OK) {
			log_error("%s production_test_data: checkLimitsMap SS IX2 SENSE failed... ERROR COUNT = %d\n", tag, ret);
			log_debug("%s SS IX2 SENSE MIN MAX TEST:.................FAIL\n\n", tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			log_debug("%s SS IX2 SENSE MIN MAX TEST:.................OK\n\n", tag);

		kfree(thresholds_min);
		thresholds_min = NULL;
		kfree(thresholds_max);
		thresholds_max = NULL;
	} else
		log_debug("%s SS IX2 SENSE MIN MAX TEST:.................SKIPPED\n\n", tag);

	log_debug("%s SS IX2 SENSE ADJ TEST: \n", tag);

	if (todo->SelfSenseIx2Adj == 1) {
		log_debug("%s SS IX2 SENSE ADJHORIZ TEST: \n", tag);
		ret = computeAdjHoriz(ssCompData.ix2_sn, 1, ssCompData.header.sense_node, &adjhor);

		if (ret < 0) {
			log_error("%s production_test_data: computeAdjHoriz SS IX2 SENSE ADJH failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
			goto ERROR_LIMITS;
		}

		log_debug("%s SS IX2 SENSE ADJ HORIZ computed!\n", tag);
		ret = parseProductionTestLimits(path_limits, SS_IX2_SENSE_ADJH_MAP_MAX, &thresholds_max, &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != ssCompData.header.sense_node - 1)) {
			log_error("%s production_test_data: parseProductionTestLimits SS_IX2_SENSE_ADJH_MAP_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMapAdj(adjhor, 1, ssCompData.header.sense_node - 1, thresholds_max);

		if (ret != OK) {
			log_error("%s production_test_data: checkLimitsMapAdj SS IX2 SENSE ADJH failed... ERROR COUNT = %d\n", tag, ret);
			log_debug("%s SS IX2 SENSE ADJH TEST:.................FAIL\n\n", tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			log_debug("%s SS IX2 SENSE ADJH TEST:.................OK\n\n", tag);

		kfree(thresholds_max);
		thresholds_max = NULL;
		kfree(adjhor);
		adjhor = NULL;
	} else
		log_debug("%s SS IX2 SENSE ADJ TEST:.................SKIPPED \n", tag);

	log_debug("%s SS TOTAL IX SENSE TEST: \n", tag);

	if (todo->SelfSenseIxTotal == 1 || todo->SelfSenseIxTotalAdj == 1) {
		log_debug("%s Reading TOTAL IX SENSE Weights... \n", tag);
		ret = parseProductionTestLimits(path_limits, SS_IX1_SENSE_W, &ix1_w, &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != 1)) {
			log_error("%s production_test_data: parseProductionTestLimits SS_IX1_SENSE_W failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
			goto ERROR_LIMITS;
		}

		ret = parseProductionTestLimits(path_limits, SS_IX2_SENSE_W, &ix2_w, &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != 1)) {
			log_error("%s production_test_data: parseProductionTestLimits SS_IX1_SENSE_W failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
			goto ERROR_LIMITS;
		}

		log_debug("%s Weights: IX1_W = %d   IX2_W = %d \n", tag, *ix1_w, *ix2_w);
		ret = computeTotal(ssCompData.ix2_sn, ssCompData.s_ix1, 1, ssCompData.header.sense_node, *ix1_w, *ix2_w, &total_ix);

		if (ret < 0) {
			log_error("%s production_test_data: computeTotal Ix Sense failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
			goto ERROR_LIMITS;
		}

		kfree(ix1_w);
		ix1_w = NULL;
		kfree(ix2_w);
		ix2_w = NULL;
		log_debug("%s SS TOTAL IX SENSE MIN MAX TEST: \n", tag);

		if (todo->SelfSenseIxTotal == 1) {
			ret = parseProductionTestLimits(path_limits, SS_TOTAL_IX_SENSE_MAP_MIN, &thresholds_min, &trows, &tcolumns);

			if (ret < 0 || (trows != 1 || tcolumns != ssCompData.header.sense_node)) {
				log_error("%s production_test_data: parseProductionTestLimits SS_TOTAL_IX_SENSE_MAP_MIN failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
				ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
				goto ERROR_LIMITS;
			}

			ret = parseProductionTestLimits(path_limits, SS_TOTAL_IX_SENSE_MAP_MAX, &thresholds_max, &trows, &tcolumns);

			if (ret < 0 || (trows != 1 || tcolumns != ssCompData.header.sense_node)) {
				log_error("%s production_test_data: parseProductionTestLimits SS_TOTAL_IX_SENSE_MAP_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
				ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMapTotal(total_ix, 1, ssCompData.header.sense_node, thresholds_min, thresholds_max);

			if (ret != OK) {
				log_error("%s production_test_data: checkLimitsMap SS TOTAL IX SENSE failed... ERROR COUNT = %d\n", tag, ret);
				log_debug("%s SS TOTAL IX SENSE MIN MAX TEST:.................FAIL\n\n", tag);
				count_fail += 1;

				if (stop_on_fail)
					goto ERROR;
			} else
				log_debug("%s SS TOTAL IX SENSE MIN MAX TEST:.................OK\n\n", tag);

			kfree(thresholds_min);
			thresholds_min = NULL;
			kfree(thresholds_max);
			thresholds_max = NULL;
		} else
			log_debug("%s SS TOTAL IX SENSE MIN MAX TEST:.................SKIPPED \n", tag);

		log_debug("%s SS TOTAL IX SENSE ADJ TEST: \n", tag);

		if (todo->SelfSenseIxTotalAdj == 1) {
			log_debug("%s SS TOTAL IX SENSE ADJHORIZ TEST: \n", tag);
			ret = computeAdjHorizTotal(total_ix, 1, ssCompData.header.sense_node, &total_adjhor);

			if (ret < 0) {
				log_error("%s production_test_data: computeAdjHoriz SS TOTAL IX SENSE ADJH failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
				ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
				goto ERROR_LIMITS;
			}

			log_debug("%s SS TOTAL IX SENSE ADJ HORIZ computed!\n", tag);
			ret = parseProductionTestLimits(path_limits, SS_TOTAL_IX_SENSE_ADJH_MAP_MAX, &thresholds_max, &trows, &tcolumns);

			if (ret < 0 || (trows != 1 || tcolumns != ssCompData.header.sense_node - 1)) {
				log_error("%s production_test_data: parseProductionTestLimits SS_TOTAL_IX_SENSE_ADJH_MAP_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
				ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMapAdjTotal(total_adjhor, 1, ssCompData.header.sense_node - 1, thresholds_max);

			if (ret != OK) {
				log_error("%s production_test_data: checkLimitsMapAdj SS TOTAL IX SENSE ADJH failed... ERROR COUNT = %d\n", tag, ret);
				log_debug("%s SS TOTAL IX SENSE ADJH TEST:.................FAIL\n\n", tag);
				count_fail += 1;

				if (stop_on_fail)
					goto ERROR;
			} else
				log_debug("%s SS TOTAL IX SENSE ADJH TEST:.................OK\n\n", tag);

			kfree(thresholds_max);
			thresholds_max = NULL;
			kfree(total_adjhor);
			total_adjhor = NULL;
		} else
			log_debug("%s SS TOTAL IX SENSE ADJ TEST:.................SKIPPED \n", tag);

		kfree(total_ix);
		total_ix = NULL;
	} else
		log_debug("%s SS TOTAL IX SENSE TEST:.................SKIPPED \n", tag);

	log_debug("%s SS CX1 FORCE TEST: \n", tag);

	if (todo->SelfForceCx1 == 1) {
		ret = parseProductionTestLimits(path_limits, SS_CX1_FORCE_MIN_MAX, &thresholds, &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != 2)) {
			log_error("%s production_test_data: parseProductionTestLimits SS_CX1_FORCE_MIN_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
			goto ERROR_LIMITS;
		}

		container = (u16) ssCompData.f_cx1;
		ret = checkLimitsMinMax(&container, 1, 1, thresholds[0], thresholds[1]);

		if (ret != OK) {
			log_error("%s production_test_data: checkLimitsMinMax SS CX1 FORCE TEST failed... ERROR COUNT = %d\n", tag, ret);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			log_debug("%s SS CX1 FORCE TEST:.................OK\n\n", tag);

		kfree(thresholds);
		thresholds = NULL;
	} else
		log_debug("%s SS CX1 FORCE TEST:.................SKIPPED\n\n", tag);

	log_debug("%s SS CX2 FORCE MIN MAX TEST: \n", tag);

	if (todo->SelfForceCx2 == 1) {
		ret = parseProductionTestLimits(path_limits, SS_CX2_FORCE_MAP_MIN, &thresholds_min, &trows, &tcolumns);

		if (ret < 0 || (trows != ssCompData.header.force_node || tcolumns != 1)) {
			log_error("%s production_test_data: parseProductionTestLimits SS_CX2_FORCE_MAP_MIN failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
			goto ERROR_LIMITS;
		}

		ret = parseProductionTestLimits(path_limits, SS_CX2_FORCE_MAP_MAX, &thresholds_max, &trows, &tcolumns);

		if (ret < 0 || (trows != ssCompData.header.force_node || tcolumns != 1)) {
			log_error("%s production_test_data: parseProductionTestLimits SS_CX2_FORCE_MAP_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMap(ssCompData.cx2_fm, ssCompData.header.force_node, 1, thresholds_min, thresholds_max);

		if (ret != OK) {
			log_error("%s production_test_data: checkLimitsMap SS CX2 FORCE failed... ERROR COUNT = %d\n", tag, ret);
			log_debug("%s SS CX2 FORCE MIN MAX TEST:.................FAIL\n\n", tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			log_debug("%s SS CX2 FORCE MIN MAX TEST:.................OK\n\n", tag);

		kfree(thresholds_min);
		thresholds_min = NULL;
		kfree(thresholds_max);
		thresholds_max = NULL;
	} else
		log_debug("%s SS CX2 FORCE MIN MAX TEST:.................SKIPPED \n", tag);

	log_debug("%s SS CX2 FORCE ADJ TEST: \n", tag);

	if (todo->SelfForceCx2Adj == 1) {
		log_debug("%s SS CX2 FORCE ADJVERT TEST: \n", tag);
		ret = computeAdjVert(ssCompData.cx2_fm, ssCompData.header.force_node, 1, &adjvert);

		if (ret < 0) {
			log_error("%s production_test_data: computeAdjVert SS CX2 FORCE ADJV failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
			goto ERROR_LIMITS;
		}

		log_debug("%s SS CX2 FORCE ADJV computed!\n", tag);
		ret = parseProductionTestLimits(path_limits, SS_CX2_FORCE_ADJV_MAP_MAX, &thresholds_max, &trows, &tcolumns);

		if (ret < 0 || (trows != ssCompData.header.force_node - 1 || tcolumns != 1)) {
			log_error("%s production_test_data: parseProductionTestLimits SS_CX2_FORCE_ADJV_MAP_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMapAdj(adjvert, ssCompData.header.force_node - 1, 1, thresholds_max);

		if (ret != OK) {
			log_error("%s production_test_data: checkLimitsMap SS IX2 FORCE failed... ERROR COUNT = %d\n", tag, ret);
			log_debug("%s SS CX2 FORCE ADJV TEST:.................FAIL\n\n", tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			log_debug("%s SS CX2 FORCE ADJV TEST:.................OK\n\n", tag);

		kfree(thresholds_max);
		thresholds_max = NULL;
		kfree(adjvert);
		adjvert = NULL;
	} else
		log_debug("%s SS CX2 FORCE ADJ TEST:.................SKIPPED\n\n", tag);

	log_debug("%s SS TOTAL CX FORCE TEST: \n", tag);

	if (todo->SelfForceCxTotal == 1 || todo->SelfForceCxTotalAdj == 1) {
		ret = computeTotal(ssCompData.cx2_fm, ssCompData.f_cx1, ssCompData.header.force_node, 1, CX1_WEIGHT, CX2_WEIGHT, &total_cx);

		if (ret < 0) {
			log_error("%s production_test_data: computeTotal Cx Force failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			return ERROR_PROD_TEST_DATA_SS_IX_CX;
		}

		log_debug("%s SS TOTAL CX FORCE MIN MAX TEST: \n", tag);

		if (todo->SelfForceCxTotal == 1) {
			ret = parseProductionTestLimits(path_limits, SS_TOTAL_CX_FORCE_MAP_MIN, &thresholds_min, &trows, &tcolumns);

			if (ret < 0 || (trows != ssCompData.header.force_node || tcolumns != 1)) {
				log_error("%s production_test_data: parseProductionTestLimits SS_TOTAL_CX_FORCE_MAP_MIN failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
				ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
				goto ERROR_LIMITS;
			}

			ret = parseProductionTestLimits(path_limits, SS_TOTAL_CX_FORCE_MAP_MAX, &thresholds_max, &trows, &tcolumns);

			if (ret < 0 || (trows != ssCompData.header.force_node || tcolumns != 1)) {
				log_error("%s production_test_data: parseProductionTestLimits SS_TOTAL_CX_FORCE_MAP_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
				ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMapTotal(total_cx, ssCompData.header.force_node, 1, thresholds_min, thresholds_max);

			if (ret != OK) {
				log_error("%s production_test_data: checkLimitsMap SS TOTAL FORCE failed... ERROR COUNT = %d\n", tag, ret);
				log_debug("%s SS TOTAL FORCE MIN MAX TEST:.................FAIL\n\n", tag);
				count_fail += 1;

				if (stop_on_fail)
					goto ERROR;
			} else
				log_debug("%s SS TOTAL FORCE MIN MAX TEST:.................OK\n\n", tag);

			kfree(thresholds_min);
			thresholds_min = NULL;
			kfree(thresholds_max);
			thresholds_max = NULL;
		} else
			log_debug("%s SS TOTAL CX FORCE MIN MAX TEST:.................SKIPPED \n", tag);

		log_debug("%s SS TOTAL CX FORCE ADJ TEST: \n", tag);

		if (todo->SelfForceCxTotalAdj == 1) {
			log_debug("%s SS TOTAL CX FORCE ADJVERT TEST: \n", tag);
			ret = computeAdjVertTotal(total_cx, ssCompData.header.force_node, 1, &total_adjvert);

			if (ret < 0) {
				log_error("%s production_test_data: computeAdjVert SS TOTAL CX FORCE ADJV failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
				ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
				goto ERROR_LIMITS;
			}

			log_debug("%s SS TOTAL CX FORCE ADJV computed!\n", tag);
			ret = parseProductionTestLimits(path_limits, SS_TOTAL_CX_FORCE_ADJV_MAP_MAX, &thresholds_max, &trows, &tcolumns);

			if (ret < 0 || (trows != ssCompData.header.force_node - 1 || tcolumns != 1)) {
				log_error("%s production_test_data: parseProductionTestLimits SS_TOTAL_CX_FORCE_ADJV_MAP_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
				ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMapAdjTotal(total_adjvert, ssCompData.header.force_node - 1, 1, thresholds_max);

			if (ret != OK) {
				log_error("%s production_test_data: checkLimitsMap SS TOTAL CX FORCE failed... ERROR COUNT = %d\n", tag, ret);
				log_debug("%s SS TOTAL CX FORCE ADJV TEST:.................FAIL\n\n", tag);
				count_fail += 1;

				if (stop_on_fail)
					goto ERROR;
			} else
				log_debug("%s SS TOTAL CX FORCE ADJV TEST:.................OK\n\n", tag);

			kfree(thresholds_max);
			thresholds_max = NULL;
			kfree(total_adjvert);
			total_adjvert = NULL;
		} else
			log_debug("%s SS TOTAL CX FORCE ADJ TEST:.................SKIPPED \n", tag);

		kfree(total_cx);
		total_cx = NULL;
	} else
		log_debug("%s SS TOTAL CX FORCE TEST:.................SKIPPED\n\n", tag);

	log_debug("%s SS CX1 SENSE TEST: \n", tag);

	if (todo->SelfSenseCx1 == 1) {
		ret = parseProductionTestLimits(path_limits, SS_CX1_SENSE_MIN_MAX, &thresholds, &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != 2)) {
			log_error("%s production_test_data: parseProductionTestLimits SS_CX1_SENSE_MIN_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
			goto ERROR_LIMITS;
		}

		container = (u16) ssCompData.s_cx1;
		ret = checkLimitsMinMax(&container, 1, 1, thresholds[0], thresholds[1]);

		if (ret != OK) {
			log_error("%s production_test_data: checkLimitsMinMax SS CX1 SENSE TEST failed... ERROR COUNT = %d\n", tag, ret);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			log_debug("%s SS CX1 SENSE TEST:.................OK\n\n", tag);

		kfree(thresholds);
		thresholds = NULL;
	} else
		log_debug("%s SS CX1 SENSE TEST:.................SKIPPED\n\n", tag);

	log_debug("%s SS CX2 SENSE MIN MAX TEST: \n", tag);

	if (todo->SelfSenseCx2 == 1) {
		ret = parseProductionTestLimits(path_limits, SS_CX2_SENSE_MAP_MIN, &thresholds_min, &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != ssCompData.header.sense_node)) {
			log_error("%s production_test_data: parseProductionTestLimits SS_CX2_SENSE_MAP_MIN failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
			goto ERROR_LIMITS;
		}

		ret = parseProductionTestLimits(path_limits, SS_CX2_SENSE_MAP_MAX, &thresholds_max, &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != ssCompData.header.sense_node)) {
			log_error("%s production_test_data: parseProductionTestLimits SS_CX2_SENSE_MAP_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMap(ssCompData.cx2_sn, 1, ssCompData.header.sense_node, thresholds_min, thresholds_max);

		if (ret != OK) {
			log_error("%s production_test_data: checkLimitsMap SS CX2 SENSE failed... ERROR COUNT = %d\n", tag, ret);
			log_debug("%s SS CX2 SENSE MIN MAX TEST:.................FAIL\n\n", tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			log_debug("%s SS CX2 SENSE MIN MAX TEST:.................OK\n\n", tag);

		kfree(thresholds_min);
		thresholds_min = NULL;
		kfree(thresholds_max);
		thresholds_max = NULL;
	} else
		log_debug("%s SS CX2 SENSE MIN MAX TEST:.................SKIPPED \n", tag);

	log_debug("%s SS CX2 SENSE ADJ TEST: \n", tag);

	if (todo->SelfSenseCx2Adj == 1) {
		log_debug("%s SS CX2 SENSE ADJHORIZ TEST: \n", tag);
		ret = computeAdjHoriz(ssCompData.ix2_sn, 1, ssCompData.header.sense_node, &adjhor);

		if (ret < 0) {
			log_error("%s production_test_data: computeAdjHoriz SS CX2 SENSE ADJH failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
			goto ERROR_LIMITS;
		}

		log_debug("%s SS CX2 SENSE ADJH computed!\n", tag);
		ret = parseProductionTestLimits(path_limits, SS_CX2_SENSE_ADJH_MAP_MAX, &thresholds_max, &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != ssCompData.header.sense_node - 1)) {
			log_error("%s production_test_data: parseProductionTestLimits SS_IX2_SENSE_MAP_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMapAdj(adjhor, 1, ssCompData.header.sense_node - 1, thresholds_max);

		if (ret != OK) {
			log_error("%s production_test_data: checkLimitsMapAdj SS CX2 SENSE ADJH failed... ERROR COUNT = %d\n", tag, ret);
			log_debug("%s SS CX2 SENSE ADJH TEST:.................FAIL\n\n", tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			log_debug("%s SS CX2 SENSE ADJH TEST:.................OK\n", tag);

		kfree(thresholds_max);
		thresholds_max = NULL;
		kfree(adjhor);
		adjhor = NULL;
	} else
		log_debug("%s SS CX2 SENSE ADJ TEST:.................SKIPPED\n\n", tag);

	log_debug("%s SS TOTAL CX SENSE TEST: \n", tag);

	if (todo->SelfSenseCxTotal == 1 || todo->SelfSenseCxTotalAdj == 1) {
		ret = computeTotal(ssCompData.cx2_sn, ssCompData.s_cx1, 1, ssCompData.header.sense_node, CX1_WEIGHT, CX2_WEIGHT, &total_cx);

		if (ret < 0) {
			log_error("%s production_test_data: computeTotal Cx Sense failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
			ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
			goto ERROR_LIMITS;
		}

		log_debug("%s SS TOTAL CX SENSE MIN MAX TEST: \n", tag);

		if (todo->SelfSenseCxTotal == 1) {
			ret = parseProductionTestLimits(path_limits, SS_TOTAL_CX_SENSE_MAP_MIN, &thresholds_min, &trows, &tcolumns);

			if (ret < 0 || (trows != 1 || tcolumns != ssCompData.header.sense_node)) {
				log_error("%s production_test_data: parseProductionTestLimits SS_TOTAL_CX_SENSE_MAP_MIN failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
				ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
				goto ERROR_LIMITS;
			}

			ret = parseProductionTestLimits(path_limits, SS_TOTAL_CX_SENSE_MAP_MAX, &thresholds_max, &trows, &tcolumns);

			if (ret < 0 || (trows != 1 || tcolumns != ssCompData.header.sense_node)) {
				log_error("%s production_test_data: parseProductionTestLimits SS_TOTAL_CX_SENSE_MAP_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
				ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMapTotal(total_cx, 1, ssCompData.header.sense_node, thresholds_min, thresholds_max);

			if (ret != OK) {
				log_error("%s production_test_data: checkLimitsMap SS TOTAL CX SENSE failed... ERROR COUNT = %d\n", tag, ret);
				log_debug("%s SS TOTAL CX SENSE MIN MAX TEST:.................FAIL\n\n", tag);
				count_fail += 1;

				if (stop_on_fail)
					goto ERROR;
			} else
				log_debug("%s SS TOTAL CX SENSE MIN MAX TEST:.................OK\n\n", tag);

			kfree(thresholds_min);
			thresholds_min = NULL;
			kfree(thresholds_max);
			thresholds_max = NULL;
		} else
			log_debug("%s SS TOTAL CX SENSE MIN MAX TEST:.................SKIPPED \n", tag);

		log_debug("%s SS TOTAL CX SENSE ADJ TEST: \n", tag);

		if (todo->SelfSenseCxTotalAdj == 1) {
			log_debug("%s SS TOTAL CX SENSE ADJHORIZ TEST: \n", tag);
			ret = computeAdjHorizTotal(total_cx, 1, ssCompData.header.sense_node, &total_adjhor);

			if (ret < 0) {
				log_error("%s production_test_data: computeAdjHoriz SS TOTAL CX SENSE ADJH failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
				ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
				goto ERROR_LIMITS;
			}

			log_debug("%s SS TOTAL CX SENSE ADJ HORIZ computed!\n", tag);
			ret = parseProductionTestLimits(path_limits, SS_TOTAL_CX_SENSE_ADJH_MAP_MAX, &thresholds_max, &trows, &tcolumns);

			if (ret < 0 || (trows != 1 || tcolumns != ssCompData.header.sense_node - 1)) {
				log_error("%s production_test_data: parseProductionTestLimits SS_TOTAL_CX_SENSE_ADJH_MAP_MAX failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
				ret = ERROR_PROD_TEST_DATA_SS_IX_CX;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMapAdjTotal(total_adjhor, 1, ssCompData.header.sense_node - 1, thresholds_max);

			if (ret != OK) {
				log_error("%s production_test_data: checkLimitsMapAdj SS TOTAL CX SENSE ADJH failed... ERROR COUNT = %d\n", tag, ret);
				log_debug("%s SS TOTAL CX SENSE ADJH TEST:.................FAIL\n\n", tag);
				count_fail += 1;

				if (stop_on_fail)
					goto ERROR;
			} else
				log_debug("%s SS TOTAL CX SENSE ADJH TEST:.................OK\n\n", tag);

			kfree(thresholds_max);
			thresholds_max = NULL;
			kfree(total_adjhor);
			total_adjhor = NULL;
		} else
			log_debug("%s SS TOTAL CX SENSE ADJ TEST:.................SKIPPED \n", tag);

		kfree(total_cx);
		total_cx = NULL;
	} else
		log_debug("%s SS TOTAL CX SENSE TEST:.................SKIPPED \n", tag);

ERROR:
	log_debug("%s\n", tag);

	if (count_fail == 0) {
		kfree(ssCompData.ix2_fm);
		ssCompData.ix2_fm = NULL;
		kfree(ssCompData.ix2_sn);
		ssCompData.ix2_sn = NULL;
		kfree(ssCompData.cx2_fm);
		ssCompData.cx2_fm = NULL;
		kfree(ssCompData.cx2_sn);
		ssCompData.cx2_sn = NULL;
		log_debug("%s SS IX CX testes finished!.................OK\n\n", tag);
		return OK;
	} else {
		log_debug("%s SS IX CX testes finished!.................FAILED  fails_count = %d\n\n", tag, count_fail);

		if (thresholds != NULL) {
			kfree(thresholds);
			thresholds = NULL;
		}

		if (thresholds_min != NULL) {
			kfree(thresholds_min);
			thresholds_min = NULL;
		}

		if (thresholds_max != NULL) {
			kfree(thresholds_max);
			thresholds_max = NULL;
		}

		if (adjhor != NULL) {
			kfree(adjhor);
			adjhor = NULL;
		}

		if (adjvert != NULL) {
			kfree(adjvert);
			adjvert = NULL;
		}

		if (ix1_w != NULL) {
			kfree(ix1_w);
			ix1_w = NULL;
		}

		if (ix2_w != NULL) {
			kfree(ix2_w);
			ix2_w = NULL;
		}

		if (total_ix != NULL) {
			kfree(total_ix);
			total_ix = NULL;
		}

		if (total_cx != NULL) {
			kfree(total_cx);
			total_cx = NULL;
		}

		if (total_adjhor != NULL) {
			kfree(total_adjhor);
			total_adjhor = NULL;
		}

		if (total_adjvert != NULL) {
			kfree(total_adjvert);
			total_adjvert = NULL;
		}

		if (ssCompData.ix2_fm != NULL) {
			kfree(ssCompData.ix2_fm);
			ssCompData.ix2_fm = NULL;
		}

		if (ssCompData.ix2_sn != NULL) {
			kfree(ssCompData.ix2_sn);
			ssCompData.ix2_sn = NULL;
		}

		if (ssCompData.cx2_fm != NULL) {
			kfree(ssCompData.cx2_fm);
			ssCompData.cx2_fm = NULL;
		}

		if (ssCompData.cx2_sn != NULL) {
			kfree(ssCompData.cx2_sn);
			ssCompData.cx2_sn = NULL;
		}

		return ERROR_PROD_TEST_DATA_SS_IX_CX;
	}

ERROR_LIMITS:

	if (thresholds != NULL) {
		kfree(thresholds);
		thresholds = NULL;
	}

	if (thresholds_min != NULL) {
		kfree(thresholds_min);
		thresholds_min = NULL;
	}

	if (thresholds_max != NULL) {
		kfree(thresholds_max);
		thresholds_max = NULL;
	}

	if (adjhor != NULL) {
		kfree(adjhor);
		adjhor = NULL;
	}

	if (adjvert != NULL) {
		kfree(adjvert);
		adjvert = NULL;
	}

	if (ix1_w != NULL) {
		kfree(ix1_w);
		ix1_w = NULL;
	}

	if (ix2_w != NULL) {
		kfree(ix2_w);
		ix2_w = NULL;
	}

	if (total_ix != NULL) {
		kfree(total_ix);
		total_ix = NULL;
	}

	if (total_cx != NULL) {
		kfree(total_cx);
		total_cx = NULL;
	}

	if (total_adjhor != NULL) {
		kfree(total_adjhor);
		total_adjhor = NULL;
	}

	if (total_adjvert != NULL) {
		kfree(total_adjvert);
		total_adjvert = NULL;
	}

	if (ssCompData.ix2_fm != NULL) {
		kfree(ssCompData.ix2_fm);
		ssCompData.ix2_fm = NULL;
	}

	if (ssCompData.ix2_sn != NULL) {
		kfree(ssCompData.ix2_sn);
		ssCompData.ix2_sn = NULL;
	}

	if (ssCompData.cx2_fm != NULL) {
		kfree(ssCompData.cx2_fm);
		ssCompData.cx2_fm = NULL;
	}

	if (ssCompData.cx2_sn != NULL) {
		kfree(ssCompData.cx2_sn);
		ssCompData.cx2_sn = NULL;
	}

	return ret;
}

int production_test_data(const char *path_limits, int stop_on_fail, TestToDo *todo)
{
	int res = OK, ret;

	if (todo == NULL) {
		log_error("%s production_test_data: No TestToDo specified!! ERROR = %02X\n", tag, (ERROR_OP_NOT_ALLOW | ERROR_PROD_TEST_DATA));
		return ERROR_OP_NOT_ALLOW;
	}

	log_debug("%s DATA Production test is starting...\n", tag);
	ret = production_test_ms_raw(path_limits, stop_on_fail, todo);
	res |= ret;

	if (ret < 0) {
		log_error("%s production_test_data: production_test_ms_raw failed... ERROR = %02X\n", tag, ret);

		if (stop_on_fail == 1)
			goto END;
	}

	ret = production_test_ms_cx(path_limits, stop_on_fail, todo);
	res |= ret;

	if (ret < 0) {
		log_error("%s production_test_data: production_test_ms_cx failed... ERROR = %02X\n", tag, ret);

		if (stop_on_fail == 1)
			goto END;
	}

	ret = production_test_ss_raw(path_limits, stop_on_fail, todo);
	res |= ret;

	if (ret < 0) {
		log_error("%s production_test_data: production_test_ss_raw failed... ERROR = %02X\n", tag, ret);

		if (stop_on_fail == 1)
			goto END;
	}

	ret = production_test_ss_ix_cx(path_limits, stop_on_fail, todo);
	res |= ret;

	if (ret < 0) {
		log_error("%s production_test_data: production_test_ss_ix_cx failed... ERROR = %02X\n", tag, ret);

		if (stop_on_fail == 1)
			goto END;
	}

END:

	if (res < OK)
		log_debug("%s DATA Production test failed!\n", tag);
	else
		log_debug("%s DATA Production test finished!\n", tag);

	return res;
}


int save_mp_flag(u32 signature)
{
	int res = -1;
	int i;
	u8 cmd[6] = {FTS_CMD_WRITE_MP_FLAG, 0x00, 0x00, 0x00, 0x00, 0x00};
	u32ToU8(signature, &cmd[2]);
	log_debug("%s Starting Saving Flag with signature = %08X ...\n", tag, signature);

	for (i = 0; i < SAVE_FLAG_RETRY && res < OK; i++) {
		log_debug("%s Attempt number %d to save mp flag !\n", tag, i + 1);
		log_debug("%s Command write flag sent...\n", tag);
		res = fts_writeFwCmd(cmd, 6);

		if (res >= OK)
			res = save_cx_tuning();
	}

	if (res < OK) {
		log_error("%s save_mp_flag: ERROR %08X ...\n", tag, signature);
		return res;
	} else {
		log_error("%s Saving Flag DONE!\n", tag);
		return OK;
	}
}


int parseProductionTestLimits(const char *path, char *label, int **data, int *row, int *column)
{
	int find = 0;
	char *token = NULL;
	int i, j, z;
	char *line2 = NULL;
	char line[800];
	int fd = -1;
	char *buf = NULL;
	int n, size, pointer = 0, ret = OK;
	char *data_file = NULL;
#ifndef LIMITS_H_FILE
	const struct firmware *fw = NULL;
	struct device *dev = NULL;

	dev = getDev();
	if (dev != NULL)
		fd = request_firmware(&fw, path, dev);
#else
	fd = 0;
#endif
	if (fd == 0) {
#ifndef LIMITS_H_FILE
		size = fw->size;
		data_file = (char *)fw->data;
		log_debug("%s Start to reading %s...\n", tag, path);
#else
		size = LIMITS_SIZE_NAME;
		data_file = (char *)(LIMITS_ARRAY_NAME);
#endif
		log_debug("%s The size of the limits file is %d bytes...\n", tag, size);
		while (find == 0) {
			if (readLine(&data_file[pointer], line, size - pointer, &n) < 0) {
				find = -1;
				break;
			}
			pointer += n;
			if (line[0] == '*') {
				line2 = kstrdup(line, GFP_KERNEL);
				if (line2 == NULL) {
					log_error("%s parseProductionTestLimits: kstrdup ERROR %02X\n", tag, ERROR_ALLOC);
					ret = ERROR_ALLOC;
					goto END;
				}
				buf = line2;
				line2 += 1;
				token = strsep(&line2, ",");
				if (strcmp(token, label) == 0) {
					find = 1;
					token = strsep(&line2, ",");

					if (token != NULL) {
						sscanf(token, "%d", row);
						log_debug("%s Row = %d\n", tag, *row);
					} else {
						log_error("%s parseProductionTestLimits 1: ERROR %02X\n", tag, ERROR_FILE_PARSE);
						ret = ERROR_FILE_PARSE;
						goto END;
					}
					token = strsep(&line2, ",");
					if (token != NULL) {
						sscanf(token, "%d", column);
						log_debug("%s Column = %d\n", tag, *column);
					} else {
						log_error("%s parseProductionTestLimits 2: ERROR %02X\n", tag, ERROR_FILE_PARSE);
						ret = ERROR_FILE_PARSE;
						goto END;
					}
					kfree(buf);
					buf = NULL;
					*data = (int *)kmalloc(((*row) * (*column)) * sizeof(int), GFP_KERNEL);
					j = 0;
					if (*data == NULL) {
						log_error("%s parseProductionTestLimits: ERROR %02X\n", tag, ERROR_ALLOC);
						ret = ERROR_ALLOC;
						goto END;
					}
					for (i = 0; i < *row; i++) {
						if (readLine(&data_file[pointer], line, size - pointer, &n) < 0) {
							log_error("%s parseProductionTestLimits : ERROR %02X\n", tag, ERROR_FILE_READ);
							ret = ERROR_FILE_READ;
							goto END;
						}
						pointer += n;
						line2 = kstrdup(line, GFP_KERNEL);
						if (line2 == NULL) {
							log_error("%s parseProductionTestLimits: kstrdup ERROR %02X\n", tag, ERROR_ALLOC);
							ret = ERROR_ALLOC;
							goto END;
						}
						buf = line2;
						token = strsep(&line2, ",");
						for (z = 0; (z < *column) && (token != NULL); z++) {
							sscanf(token, "%d", ((*data) + j));
							j++;
							token = strsep(&line2, ",");
						}
						kfree(buf);
						buf = NULL;
					}
					if (j == ((*row) * (*column))) {
						log_debug("%s READ DONE!\n", tag);
						ret = OK;
						goto END;
					}

					log_error("%s parseProductionTestLimits 3: ERROR %02X\n", tag, ERROR_FILE_PARSE);
					ret = ERROR_FILE_PARSE;
					goto END;
				}

				kfree(buf);
				buf = NULL;
			}
		}

		log_error("%s parseProductionTestLimits: ERROR %02X\n", tag, ERROR_LABEL_NOT_FOUND);
		ret = ERROR_LABEL_NOT_FOUND;
END:
		if (buf != NULL) {
			kfree(buf);
			buf = NULL;
		}
#ifndef LIMITS_H_FILE
		release_firmware(fw);
#endif
		return ret;
	} else {
		log_error("%s parseProductionTestLimits: ERROR %02X\n", tag, ERROR_FILE_NOT_FOUND);
		return ERROR_FILE_NOT_FOUND;
	}
}



int readLine(char *data, char *line, int size, int *n)
{
	int i = 0;
	int err = -1;

	if (size < 1)
		return err;

	while (data[i] != '\n' && i < size) {
		line[i] = data[i];
		i++;
	}

	*n = i + 1;
	line[i] = '\0';
	return OK;
}



