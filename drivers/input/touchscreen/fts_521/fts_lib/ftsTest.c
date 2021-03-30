/*

 **************************************************************************
 **                        STMicroelectronics							 **
 **************************************************************************
 **                        marco.cali@st.com							 **
 **************************************************************************
 *                                                                        *
 *							FTS API for MP test							  *
 *                                                                        *
 **************************************************************************
 **************************************************************************

 */

/*!
 * \file ftsTest.c
 * \brief Contains all the functions related to the Mass Production Test
 */

#include "ftsCompensation.h"
#include "ftsCore.h"
#include "ftsError.h"
#include "ftsFrame.h"
#include "ftsHardware.h"
#include "ftsIO.h"
#include "ftsSoftware.h"
#include "ftsTest.h"
#include "ftsTime.h"
#include "ftsTool.h"
#include "../fts.h"

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <stdarg.h>
#include <linux/serio.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/firmware.h>

#ifdef LIMITS_H_FILE
#include "../fts_limits.h"
#endif

TestToDo tests;
static LimitFile limit_file;

/**
 * Initialize the testToDo variable with the default tests to perform during the Mass Production Test
 * @return OK
 */
int initTestToDo(void)
{
	/*** Initialize Limit File ***/
	limit_file.size = 0;
	limit_file.data = NULL;
	strlcpy(limit_file.name, " ", MAX_LIMIT_FILE_NAME);
	tests.SelfHoverForceRaw = 0;		/* /< SS Hover Force Raw min/Max test */
	tests.SelfHoverSenceRaw = 0;		/* /< SS Hover Sence Raw min/Max test */
	tests.SelfHoverForceIxTotal = 0;	/* /< SS Hover Total Force Ix min/Max (for each node)* test */
	tests.SelfHoverSenceIxTotal = 0;
	tests.MutualRawAdjITO = 1;
	tests.MutualRaw = 1;
	tests.MutualRawEachNode = 1;
	tests.MutualRawGap = 0;
	tests.MutualRawAdj = 0;
	tests.MutualRawLP = 0;
	tests.MutualRawGapLP = 0;
	tests.MutualRawAdjLP = 0;
	tests.MutualCx1 = 0;
	tests.MutualCx2 = 0;
	tests.MutualCx2Adj = 0;
	tests.MutualCxTotal = 0;
	tests.MutualCxTotalAdj = 0;

	tests.MutualCx1LP = 0;
	tests.MutualCx2LP = 1;
	tests.MutualCx2AdjLP = 1;
	tests.MutualCxTotalLP = 0;
	tests.MutualCxTotalAdjLP = 0;
#ifdef PHONE_KEY
	tests.MutualKeyRaw = 0;
#else
	tests.MutualKeyRaw = 0;
#endif
	tests.MutualKeyCx1 = 0;
	tests.MutualKeyCx2 = 0;
#ifdef PHONE_KEY
	tests.MutualKeyCxTotal = 0;
#else
	tests.MutualKeyCxTotal = 0;
#endif
	tests.SelfForceRaw = 1;
	tests.SelfForceRawMap = 1;
	tests.SelfForceRawGap = 0;
	tests.SelfForceRawLP = 0;
	tests.SelfForceRawGapLP = 0;
	tests.SelfForceIx1 = 0;
	tests.SelfForceIx2 = 0;
	tests.SelfForceIx2Adj = 0;
	tests.SelfForceIxTotal = 1;
	tests.SelfForceIxTotalAdj = 0;
	tests.SelfForceCx1 = 0;
	tests.SelfForceCx2 = 0;
	tests.SelfForceCx2Adj = 0;
	tests.SelfForceCxTotal = 0;
	tests.SelfForceCxTotalAdj = 0;
	tests.SelfSenseRaw = 1;
	tests.SelfSenseRawMap = 1;
	tests.SelfSenseRawGap = 0;
	tests.SelfSenseRawLP = 0;
	tests.SelfSenseRawGapLP = 0;
	tests.SelfSenseIx1 = 0;
	tests.SelfSenseIx2 = 0;
	tests.SelfSenseIx2Adj = 0;
	tests.SelfSenseIxTotal = 1;
	tests.SelfSenseIxTotalAdj = 0;
	tests.SelfSenseCx1 = 0;
	tests.SelfSenseCx2 = 0;
	tests.SelfSenseCx2Adj = 0;
	tests.SelfSenseCxTotal = 0;
	tests.SelfSenseCxTotalAdj = 0;
	return OK;
}

/**
 * Compute the Horizontal adjacent matrix doing the abs of the difference between the column i with the i-1 one. \n
 * Both the original data matrix and the adj matrix are disposed as 1 dimension array one row after the other \n
 * The resulting matrix has one column less than the starting original one \n
 * @param data pointer to the array of signed bytes containing the original data
 * @param row number of rows of the original data
 * @param column number of columns of the original data
 * @param result pointer of a pointer to an array of unsigned bytes which will contain the adj matrix
 * @return OK if success or an error code which specify the type of error encountered
 */
int computeAdjHoriz(i8 *data, int row, int column, u8 **result)
{
	int i, j;
	int size = row * (column - 1);

	if (column < 2) {
		logError(1, "%s computeAdjHoriz: ERROR % 08X\n", tag,
			 ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	*result = (u8 *) kmalloc(size * sizeof(u8), GFP_KERNEL);

	if (*result == NULL) {
		logError(1, "%s computeAdjHoriz: ERROR %08X\n", tag,
			 ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	for (i = 0; i < row; i++) {
		for (j = 1; j < column; j++) {
			*(*result + (i * (column - 1) + (j - 1))) =
				abs(data[i * column + j] -
				    data[i * column + (j - 1)]);
		}
	}

	return OK;
}

/**
 * Compute the Horizontal adjacent matrix of short values doing the abs of the difference between the column i with the i-1 one.
 * Both the original data matrix and the adj matrix are disposed as 1 dimension array one row after the other \n
 * The resulting matrix has one column less than the starting original one \n
 * @param data pointer to the array of signed bytes containing the original data
 * @param row number of rows of the original data
 * @param column number of columns of the original data
 * @param result pointer of a pointer to an array of unsigned bytes which will contain the adj matrix
 * @return OK if success or an error code which specify the type of error encountered
 */
int computeAdjHorizTotal(short *data, int row, int column, u16 **result)
{
	int i, j;
	int size = row * (column - 1);

	if (column < 2) {
		logError(1, "%s computeAdjHorizTotal: ERROR % 08X\n", tag,
			 ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	*result = (u16 *) kmalloc(size * sizeof(u16), GFP_KERNEL);

	if (*result == NULL) {
		logError(1, "%s computeAdjHorizTotal: ERROR %08X\n", tag,
			 ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	for (i = 0; i < row; i++) {
		for (j = 1; j < column; j++) {
			*(*result + (i * (column - 1) + (j - 1))) =
				abs(data[i * column + j] -
				    data[i * column + (j - 1)]);
		}
	}

	return OK;
}

/**
 * Compute the Vertical adjacent matrix doing the abs of the difference between the row i with the i-1 one.
 * Both the original data matrix and the adj matrix are disposed as 1 dimension array one row after the other. \n
 * The resulting matrix has one column less than the starting original one \n
 * @param data pointer to the array of signed bytes containing the original data
 * @param row number of rows of the original data
 * @param column number of columns of the original data
 * @param result pointer of a pointer to an array of unsigned bytes which will contain the adj matrix
 * @return OK if success or an error code which specify the type of error encountered
 */
int computeAdjVert(i8 *data, int row, int column, u8 **result)
{
	int i, j;
	int size = (row - 1) * (column);

	if (row < 2) {
		logError(1, "%s computeAdjVert: ERROR % 08X\n", tag,
			 ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	*result = (u8 *) kmalloc(size * sizeof(u8), GFP_KERNEL);

	if (*result == NULL) {
		logError(1, "%s computeAdjVert: ERROR %08X\n", tag,
			 ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	for (i = 1; i < row; i++) {
		for (j = 0; j < column; j++) {
			*(*result + ((i - 1) * column + j)) =
				abs(data[i * column + j] -
				    data[(i - 1) * column + j]);
		}
	}

	return OK;
}

/**
 * Compute the Vertical adjacent matrix of short values doing the abs of the difference between the row i with the i-1 one.
 * Both the original data matrix and the adj matrix are disposed as 1 dimension array one row after the other. \n
 * The resulting matrix has one column less than the starting original one \n
 * @param data pointer to the array of signed bytes containing the original data
 * @param row number of rows of the original data
 * @param column number of columns of the original data
 * @param result pointer of a pointer to an array of unsigned bytes which will contain the adj matrix
 * @return OK if success or an error code which specify the type of error encountered
 */
int computeAdjVertTotal(short *data, int row, int column, u16 **result)
{
	int i, j;
	int size = (row - 1) * (column);

	if (row < 2) {
		logError(1, "%s computeAdjVertTotal: ERROR %08X\n", tag,
			 ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	*result = (u16 *) kmalloc(size * sizeof(u16), GFP_KERNEL);

	if (*result == NULL) {
		logError(1, "%s computeAdjVertTotal: ERROR %08X\n", tag,
			 ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	for (i = 1; i < row; i++) {
		for (j = 0; j < column; j++) {
			*(*result + ((i - 1) * column + j)) =
				abs(data[i * column + j] -
				    data[(i - 1) * column + j]);
		}
	}

	return OK;
}

/**
 * Compute the Horizontal adjacent matrix doing the abs of the difference between the column i with the i-1 one. \n
 * Both the original data matrix and the adj matrix are disposed as 1 dimension array one row after the other \n
 * The resulting matrix has one column less than the starting original one \n
 * @param data pointer to the array of unsigned bytes containing the original data
 * @param row number of rows of the original data
 * @param column number of columns of the original data
 * @param result pointer of a pointer to an array of unsigned bytes which will contain the adj matrix
 * @return OK if success or an error code which specify the type of error encountered
 */
int computeAdjHorizFromU(u8 *data, int row, int column, u8 **result)
{
	int i, j;
	int size = row * (column - 1);

	if (column < 2) {
		logError(1, "%s computeAdjHoriz: ERROR %08X\n", tag,
			 ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	*result = (u8 *) kmalloc(size * sizeof(u8), GFP_KERNEL);

	if (*result == NULL) {
		logError(1, "%s computeAdjHoriz: ERROR %08X\n", tag,
			 ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	for (i = 0; i < row; i++) {
		for (j = 1; j < column; j++) {
			*(*result + (i * (column - 1) + (j - 1))) =
				abs(data[i * column + j] -
				    data[i * column + (j - 1)]);
		}
	}

	return OK;
}

/**
 * Compute the Horizontal adjacent matrix of u16 values doing the abs of the difference between the column i with the i-1 one.
 * Both the original data matrix and the adj matrix are disposed as 1 dimension array one row after the other \n
 * The resulting matrix has one column less than the starting original one \n
 * @param data pointer to the array of unsigned bytes containing the original data
 * @param row number of rows of the original data
 * @param column number of columns of the original data
 * @param result pointer of a pointer to an array of unsigned bytes which will contain the adj matrix
 * @return OK if success or an error code which specify the type of error encountered
 */
int computeAdjHorizTotalFromU(u16 *data, int row, int column, u16 **result)
{
	int i, j;
	int size = row * (column - 1);

	if (column < 2) {
		logError(1, "%s computeAdjHorizTotal: ERROR %08X\n", tag,
			 ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	*result = (u16 *) kmalloc(size * sizeof(u16), GFP_KERNEL);

	if (*result == NULL) {
		logError(1, "%s computeAdjHorizTotal: ERROR %08X\n", tag,
			 ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	for (i = 0; i < row; i++) {
		for (j = 1; j < column; j++) {
			*(*result + (i * (column - 1) + (j - 1))) =
				abs(data[i * column + j] -
				    data[i * column + (j - 1)]);
		}
	}

	return OK;
}

/**
 * Compute the Vertical adjacent matrix doing the abs of the difference between the row i with the i-1 one.
 * Both the original data matrix and the adj matrix are disposed as 1 dimension array one row after the other. \n
 * The resulting matrix has one column less than the starting original one \n
 * @param data pointer to the array of unsigned bytes containing the original data
 * @param row number of rows of the original data
 * @param column number of columns of the original data
 * @param result pointer of a pointer to an array of unsigned bytes which will contain the adj matrix
 * @return OK if success or an error code which specify the type of error encountered
 */
int computeAdjVertFromU(u8 *data, int row, int column, u8 **result)
{
	int i, j;
	int size = (row - 1) * (column);

	if (row < 2) {
		logError(1, "%s computeAdjVert: ERROR %08X\n", tag,
			 ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	*result = (u8 *) kmalloc(size * sizeof(u8), GFP_KERNEL);

	if (*result == NULL) {
		logError(1, "%s computeAdjVert: ERROR %08X\n", tag,
			 ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	for (i = 1; i < row; i++) {
		for (j = 0; j < column; j++) {
			*(*result + ((i - 1) * column + j)) =
				abs(data[i * column + j] -
				    data[(i - 1) * column + j]);
		}
	}

	return OK;
}

/**
 * Compute the Vertical adjacent matrix of u16 values doing the abs of the difference between the row i with the i-1 one.
 * Both the original data matrix and the adj matrix are disposed as 1 dimension array one row after the other. \n
 * The resulting matrix has one column less than the starting original one \n
 * @param data pointer to the array of unsigned bytes containing the original data
 * @param row number of rows of the original data
 * @param column number of columns of the original data
 * @param result pointer of a pointer to an array of unsigned bytes which will contain the adj matrix
 * @return OK if success or an error code which specify the type of error encountered
 */
int computeAdjVertTotalFromU(u16 *data, int row, int column, u16 **result)
{
	int i, j;
	int size = (row - 1) * (column);

	if (row < 2) {
		logError(1, "%s computeAdjVertTotal: ERROR %08X\n", tag,
			 ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	*result = (u16 *) kmalloc(size * sizeof(u16), GFP_KERNEL);

	if (*result == NULL) {
		logError(1, "%s computeAdjVertTotal: ERROR %08X\n", tag,
			 ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	for (i = 1; i < row; i++) {
		for (j = 0; j < column; j++) {
			*(*result + ((i - 1) * column + j)) =
				abs(data[i * column + j] -
				    data[(i - 1) * column + j]);
		}
	}

	return OK;
}

int computeTotal(u8 *data, u8 main, int row, int column, int m, int n,
		 u16 **result)
{
	int i, j;
	int size = (row) * (column);
	*result = (u16 *) kmalloc(size * sizeof(u16), GFP_KERNEL);

	if (*result == NULL) {
		logError(1, "%s computeTotal : ERROR %02X\n", tag, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	for (i = 0; i < row; i++) {
		for (j = 0; j < column; j++) {
			*(*result + (i * column + j)) =
				m * main + n * data[i * column + j];
		}
	}

	return OK;
}

/**
 * Check that each value of a matrix of short doesn't exceed a min and a Max value (these values are included in the interval). \n
 * The matrix is stored as 1 dimension array one row after the other. \n
 * @param data pointer to the array of short containing the data to check
 * @param row number of rows of data
 * @param column number of columns of data
 * @param min minimum value allowed
 * @param max Maximum value allowed
 * @return the number of elements that overcome the specified interval (0 = OK)
 */
int checkLimitsMinMax(short *data, int row, int column, int min, int max)
{
	int i, j;
	int count = 0;

	for (i = 0; i < row; i++) {
		for (j = 0; j < column; j++) {
			if (data[i * column + j] < min
			    || data[i * column + j] > max) {
				logError(1,
					 "%s checkLimitsMinMax: Node[%d,%d] = %d exceed limit [%d, %d] \n",
					 tag, i, j, data[i * column + j], min,
					 max);
				count++;
			}
		}
	}

	return count;
}

/**
 * Check that the difference between the max and min of a matrix of short is less or equal to a threshold.\n
 * The matrix is stored as 1 dimension array one row after the other.
 * @param data pointer to the array of short containing the data to check
 * @param row number of rows of data
 * @param column number of columns of data
 * @param threshold threshold value allowed
 * @return OK if the difference is <= to threshold otherwise ERROR_TEST_CHECK_FAIL
 */
int checkLimitsGap(short *data, int row, int column, int threshold)
{
	int i, j;
	int min_node;
	int max_node;

	if (row == 0 || column == 0) {
		logError(1,
			 "%s checkLimitsGap: invalid number of rows = %d or columns = %d  ERROR %08X\n",
			 tag, row, column, ERROR_OP_NOT_ALLOW);
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
		logError(1, "%s checkLimitsGap: GAP = %d exceed limit  %d \n",
			 tag, max_node - min_node, threshold);
		return ERROR_TEST_CHECK_FAIL;
	} else
		return OK;
}

/**
 * Check that each value of a matrix of i8 doesn't exceed a specific min and Max value  set for each node (these values are included in the interval). \n
 * The matrixes of data, min and max values are stored as 1 dimension arrays one row after the other.
 * @param data pointer to the array of short containing the data to check
 * @param row number of rows of data
 * @param column number of columns of data
 * @param min pointer to a matrix which specify the minimum value allowed for each node
 * @param max pointer to a matrix which specify the Maximum value allowed for each node
 * @return the number of elements that overcome the specified interval (0 = OK)
 */
int checkLimitsMap(i8 *data, int row, int column, int *min, int *max)
{
	int i, j;
	int count = 0;

	for (i = 0; i < row; i++) {
		for (j = 0; j < column; j++) {
			if (data[i * column + j] < min[i * column + j]
			    || data[i * column + j] > max[i * column + j]) {
				logError(1,
					 "%s checkLimitsMap: Node[%d,%d] = %d exceed limit [%d, %d] \n",
					 tag, i, j, data[i * column + j],
					 min[i * column + j],
					 max[i * column + j]);
				count++;
			}
		}
	}

	return count;
}

/**
 * Check that each value of a matrix of short doesn't exceed a specific min and Max value  set for each node (these values are included in the interval).
 * The matrixes of data, min and max values are stored as 1 dimension arrays one row after the other.
 * @param data pointer to the array of short containing the data to check
 * @param row number of rows of data
 * @param column number of columns of data
 * @param min pointer to a matrix which specify the minimum value allowed for each node
 * @param max pointer to a matrix which specify the Maximum value allowed for each node
 * @return the number of elements that overcome the specified interval (0 = OK)
 */
int checkLimitsMapTotal(short *data, int row, int column, int *min, int *max)
{
	int i, j;
	int count = 0;

	for (i = 0; i < row; i++) {
		for (j = 0; j < column; j++) {
			if (data[i * column + j] < min[i * column + j]
			    || data[i * column + j] > max[i * column + j]) {
				logError(1,
					 "%s checkLimitsMapTotal: Node[%d,%d] = %d exceed limit [%d, %d] \n",
					 tag, i, j, data[i * column + j],
					 min[i * column + j],
					 max[i * column + j]);
				count++;
			}
		}
	}

	return count;
}

/**
 * Check that each value of a matrix of u8 doesn't exceed a specific min and Max value  set for each node (these values are included in the interval). \n
 * The matrixes of data, min and max values are stored as 1 dimension arrays one row after the other.
 * @param data pointer to the array of short containing the data to check
 * @param row number of rows of data
 * @param column number of columns of data
 * @param min pointer to a matrix which specify the minimum value allowed for each node
 * @param max pointer to a matrix which specify the Maximum value allowed for each node
 * @return the number of elements that overcome the specified interval (0 = OK)
 */
int checkLimitsMapFromU(u8 *data, int row, int column, int *min, int *max)
{
	int i, j;
	int count = 0;

	for (i = 0; i < row; i++) {
		for (j = 0; j < column; j++) {
			if (data[i * column + j] < min[i * column + j]
			    || data[i * column + j] > max[i * column + j]) {
				logError(1,
					 "%s checkLimitsMap: Node[%d,%d] = %d exceed limit [%d, %d] \n",
					 tag, i, j, data[i * column + j],
					 min[i * column + j],
					 max[i * column + j]);
				count++;
			}
		}
	}

	return count;
}

/**
 * Check that each value of a matrix of u16 doesn't exceed a specific min and Max value  set for each node (these values are included in the interval).
 * The matrixes of data, min and max values are stored as 1 dimension arrays one row after the other.
 * @param data pointer to the array of short containing the data to check
 * @param row number of rows of data
 * @param column number of columns of data
 * @param min pointer to a matrix which specify the minimum value allowed for each node
 * @param max pointer to a matrix which specify the Maximum value allowed for each node
 * @return the number of elements that overcome the specified interval (0 = OK)
 */
int checkLimitsMapTotalFromU(u16 *data, int row, int column, int *min,
			     int *max)
{
	int i, j;
	int count = 0;

	for (i = 0; i < row; i++) {
		for (j = 0; j < column; j++) {
			if (data[i * column + j] < min[i * column + j]
			    || data[i * column + j] > max[i * column + j]) {
				logError(1,
					 "%s checkLimitsMapTotal: Node[%d,%d] = %d exceed limit [%d, %d] \n",
					 tag, i, j, data[i * column + j],
					 min[i * column + j],
					 max[i * column + j]);
				count++;
			}
			logError(1,
				 "%s checkLimitsMapTotal: Node[%d,%d] = %d\n",
				 tag, i, j, data[i * column + j],
				 min[i * column + j],
				 max[i * column + j]);
		}
	}

	return count;
}

/**
 * Check that each value of a matrix of u8 doesn't exceed a specific Max value  set for each node (max value is included in the interval).
 * The matrixes of data and max values are stored as 1 dimension arrays one row after the other.
 * @param data pointer to the array of short containing the data to check
 * @param row number of rows of data
 * @param column number of columns of data
 * @param max pointer to a matrix which specify the Maximum value allowed for each node
 * @return the number of elements that overcome the specified interval (0 = OK)
 */
int checkLimitsMapAdj(u8 *data, int row, int column, int *max)
{
	int i, j;
	int count = 0;

	for (i = 0; i < row; i++) {
		for (j = 0; j < column; j++) {
			if (data[i * column + j] > max[i * column + j]) {
				logError(1,
					 "%s checkLimitsMapAdj: Node[%d,%d] = %d exceed limit > %d \n",
					 tag, i, j, data[i * column + j],
					 max[i * column + j]);
				count++;
			}
		}
	}

	return count;
}

/**
 * Check that each value of a matrix of u16 doesn't exceed a specific Max value set for each node (max value is included in the interval).
 * The matrixes of data and max values are stored as 1 dimension arrays one row after the other.
 * @param data pointer to the array of short containing the data to check
 * @param row number of rows of data
 * @param column number of columns of data
 * @param max pointer to a matrix which specify the Maximum value allowed for each node
 * @return the number of elements that overcome the specified interval (0 = OK)
 */
int checkLimitsMapAdjTotal(u16 *data, int row, int column, int *max)
{
	int i, j;
	int count = 0;

	for (i = 0; i < row; i++) {
		for (j = 0; j < column; j++) {
			if (data[i * column + j] > max[i * column + j]) {
				logError(1,
					 "%s checkLimitsMapAdjTotal: Node[%d,%d] = %d exceed limit > %d \n",
					 tag, i, j, data[i * column + j],
					 max[i * column + j]);
				count++;
			}
		}
	}

	return count;
}

/**
 * Perform an ITO test setting all the possible options (see @link ito_opt ITO Options @endlink)
 * @return the number of elements that overcome the specified interval (0 = OK)
 */
int production_test_ito(const char *path_limits, TestToDo *todo)
{
	int res = OK;
	u8 sett[2] = { 0x00, 0x00 };
	MutualSenseFrame msRawFrame;
	int *thresholds = NULL;
	u16 *adj = NULL;
	int trows, tcolumns;
	logError(1, "%s ITO Production test is starting...\n", tag);
	memset(&msRawFrame, 0x00, sizeof(msRawFrame));
	res = fts_system_reset();

	if (res < 0) {
		logError(1, "%s %s: ERROR %08X \n", tag, __func__,
			 ERROR_PROD_TEST_ITO);
		return (res | ERROR_PROD_TEST_ITO);
	}

	sett[0] = SPECIAL_TUNING_IOFF;
	logError(0, "%s Trimming Ioff... \n", tag);
	res = writeSysCmd(SYS_CMD_SPECIAL_TUNING, sett, 2);

	if (res < OK) {
		logError(1, "%s production_test_ito: Trimm Ioff ERROR %08X \n",
			 tag, (res | ERROR_PROD_TEST_ITO));
		return (res | ERROR_PROD_TEST_ITO);
	}

	sett[0] = 0xFF;
	sett[1] = 0x01;
	logError(0, "%s ITO Check command sent... \n", tag);
	res = writeSysCmd(SYS_CMD_ITO, sett, 2);

	if (res < OK) {
		logError(1, "%s production_test_ito: ERROR %08X \n", tag,
			 (res | ERROR_PROD_TEST_ITO));
		return (res | ERROR_PROD_TEST_ITO);
	}

	logError(0, "%s ITO Command = OK! \n", tag);

	if (todo->MutualRawAdjITO == 1) {
		logError(1, "%s MS RAW ITO ADJ TEST: \n", tag);
		logError(0, "%s Collecting MS Raw data... \n", tag);
		res |= getMSFrame3(MS_RAW, &msRawFrame);

		if (res < OK) {
			logError(1, "%s %s: getMSFrame failed... ERROR %08X \n",
				 tag, __func__, ERROR_PROD_TEST_ITO);
			goto ERROR;
		}

		logError(0, "%s MS RAW ITO ADJ HORIZONTAL TEST: \n", tag);
		res =
			computeAdjHorizTotal(msRawFrame.node_data,
					     msRawFrame.header.force_node,
					     msRawFrame.header.sense_node, &adj);

		if (res < OK) {
			logError(1,
				 "%s %s: computeAdjHoriz failed... ERROR %08X \n",
				 tag, __func__, ERROR_PROD_TEST_ITO);
			goto ERROR;
		}

		res =
			parseProductionTestLimits(path_limits, &limit_file,
						  MS_RAW_ITO_ADJH, &thresholds,
						  &trows, &tcolumns);

		if (res < OK
		    || (trows != msRawFrame.header.force_node
			|| tcolumns != msRawFrame.header.sense_node - 1)) {
			logError(1,
				 "%s %s: parseProductionTestLimits MS_RAW_ITO_ADJH failed... ERROR %08X \n",
				 tag, __func__, ERROR_PROD_TEST_DATA);
			goto ERROR;
		}

		res =
			checkLimitsMapAdjTotal(adj, msRawFrame.header.force_node,
					       msRawFrame.header.sense_node - 1,
					       thresholds);

		if (res != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsAdj MS RAW ITO ADJH failed... ERROR COUNT = %d \n",
				 tag, res);
			logError(0,
				 "%s MS RAW ITO ADJ HORIZONTAL TEST:.................FAIL \n\n",
				 tag);
			print_frame_short("MS Raw ITO frame =",
					  array1dTo2d_short
					  (msRawFrame.node_data,
					   msRawFrame.node_data_size,
					   msRawFrame.header.sense_node),
					  msRawFrame.header.force_node,
					  msRawFrame.header.sense_node);
			res = ERROR_PROD_TEST_ITO;
			goto ERROR;
		} else
			logError(0,
				 "%s MS RAW ITO ADJ HORIZONTAL TEST:.................OK  \n",
				 tag);

		kfree(thresholds);
		thresholds = NULL;
		kfree(adj);
		adj = NULL;
		logError(0, "%s MS RAW ITO ADJ VERTICAL TEST: \n", tag);
		res =
			computeAdjVertTotal(msRawFrame.node_data,
					    msRawFrame.header.force_node,
					    msRawFrame.header.sense_node, &adj);

		if (res < OK) {
			logError(1,
				 "%s %s: computeAdjVert failed... ERROR %08X \n",
				 tag, __func__, ERROR_PROD_TEST_ITO);
			goto ERROR;
		}

		res =
			parseProductionTestLimits(path_limits, &limit_file,
						  MS_RAW_ITO_ADJV, &thresholds,
						  &trows, &tcolumns);

		if (res < OK
		    || (trows != msRawFrame.header.force_node - 1
			|| tcolumns != msRawFrame.header.sense_node)) {
			logError(1,
				 "%s %s: parseProductionTestLimits MS_RAW_ITO_ADJV failed... ERROR %08X \n",
				 tag, __func__, ERROR_PROD_TEST_ITO);
			goto ERROR;
		}

		res =
			checkLimitsMapAdjTotal(adj,
					       msRawFrame.header.force_node - 1,
					       msRawFrame.header.sense_node,
					       thresholds);

		if (res != OK) {
			logError(1,
				 "%s %s: checkLimitsAdj MS RAW ITO ADJV failed... ERROR COUNT = %d \n",
				 tag, __func__, res);
			logError(0,
				 "%s MS RAW ITO ADJ VERTICAL TEST:.................FAIL \n\n",
				 tag);
			print_frame_short("MS Raw ITO frame =",
					  array1dTo2d_short
					  (msRawFrame.node_data,
					   msRawFrame.node_data_size,
					   msRawFrame.header.sense_node),
					  msRawFrame.header.force_node,
					  msRawFrame.header.sense_node);
			res = ERROR_PROD_TEST_ITO;
			goto ERROR;
		} else
			logError(0,
				 "%s MS RAW ITO ADJ VERTICAL TEST:.................OK  \n",
				 tag);

		kfree(thresholds);
		thresholds = NULL;
		kfree(adj);
		adj = NULL;
	} else
		logError(0,
			 "%s MS RAW ITO ADJ TEST:.................SKIPPED  \n",
			 tag);

ERROR:

	if (thresholds != NULL) {
		kfree(thresholds);
		thresholds = NULL;
	}
	if (adj != NULL) {
		kfree(adj);
		adj = NULL;
	}
	if (msRawFrame.node_data != NULL) {
		kfree(msRawFrame.node_data);
		msRawFrame.node_data = NULL;
	}
	freeLimitsFile(&limit_file);
	res |= fts_system_reset();

	if (res < OK) {
		logError(1, "%s production_test_ito: ERROR %08X \n", tag,
			 ERROR_PROD_TEST_ITO);
		res = (res | ERROR_PROD_TEST_ITO);
	}

	return res;
}

/**
 * Perform the Initialization of the IC
 * @param type type of initialization to do (see @link sys_special_opt Initialization Options (Full or Panel) @endlink)
 * @return OK if success or an error code which specify the type of error encountered
 */
int production_test_initialization(u8 type)
{
	int res;
	logError(1, "%s INITIALIZATION Production test is starting,type:%d\n", tag, type);

	if (type != SPECIAL_PANEL_INIT && type != SPECIAL_FULL_PANEL_INIT) {
		logError(1,
			 "%s production_test_initialization: Type incompatible! Type = %02X ERROR %08X \n",
			 tag, type,
			 ERROR_OP_NOT_ALLOW | ERROR_PROD_TEST_INITIALIZATION);
		return (ERROR_OP_NOT_ALLOW | ERROR_PROD_TEST_INITIALIZATION);
	}

	res = fts_system_reset();

	if (res < 0) {
		logError(1, "%s production_test_initialization: ERROR %08X \n",
			 tag, ERROR_PROD_TEST_INITIALIZATION);
		return (res | ERROR_PROD_TEST_INITIALIZATION);
	}

	logError(0, "%s INITIALIZATION command sent... %02X \n", tag, type);
	res = writeSysCmd(SYS_CMD_SPECIAL, &type, 1);

	if (res < OK) {
		logError(1, "%s production_test_initialization: ERROR %08X \n",
			 tag, (res | ERROR_PROD_TEST_INITIALIZATION));
		return (res | ERROR_PROD_TEST_INITIALIZATION);
	}

	logError(0, "%s Refresh Sys Info...\n", tag);
	res |= readSysInfo(1);

	if (res < 0) {
		logError(1,
			 "%s production_test_initialization: read sys info ERROR %08X\n",
			 tag, ERROR_PROD_TEST_INITIALIZATION);
		res = (res | ERROR_PROD_TEST_INITIALIZATION);
	}

	return res;
}

/**
 * Perform a FULL (ITO + INIT + DATA CHECK) Mass Production Test of the IC
 * @param pathThresholds name of Production Limit file to load or "NULL" if the limits data should be loaded by a .h file
 * @param stop_on_fail if 1, the test flow stops at the first data check failure otherwise it keeps going performing all the selected test
 * @param saveInit if >0 (possible values: NO_INIT, SPECIAL_PANEL_INIT or SPECIAL_FULL_PANEL_INIT), the Initialization of the IC is executed otherwise it is skipped
 * @param todo pointer to a TestToDo variable which select the test to do
 * @return OK if success or an error code which specify the type of error encountered
 */
int production_test_main(const char *pathThresholds, int stop_on_fail, int saveInit,
			 TestToDo *todo)
{
	int res, ret;
	logError(0, "%s MAIN Production test is starting...\n", tag);
	logError(0, "%s \n", tag);
	logError(0, "%s ITO TEST: \n", tag);
	res = production_test_ito(pathThresholds, todo);

	if (res < 0) {
		logError(0, "%s Error during ITO TEST! ERROR %08X\n", tag, res);
		goto END;
	} else {
		logError(0, "%s ITO TEST OK!\n", tag);
	}

	logError(0, "%s \n", tag);
	logError(0, "%s INITIALIZATION TEST : \n", tag);

	if (saveInit != NO_INIT) {
		res = production_test_initialization((u8) saveInit);

		if (res < 0) {
			logError(0,
				 "%s Error during  INITIALIZATION TEST! ERROR %08X\n",
				 tag, res);

			if (stop_on_fail)
				goto END;
		} else {
			logError(0, "%s INITIALIZATION TEST OK!\n", tag);
		}
	} else
		logError(0,
			 "%s INITIALIZATION TEST :................. SKIPPED  \n",
			 tag);

	logError(0, "%s \n", tag);

	if (saveInit == 1) {
		logError(0, "%s Cleaning up...\n", tag);
		ret = fts_system_reset();

		if (ret < 0) {
			logError(1,
				 "%s production_test_main: system reset ERROR %08X\n",
				 tag, ret);
			res |= ret;

			if (stop_on_fail)
				goto END;
		}

		logError(0, "%s \n", tag);
	}

	logError(1, "%s PRODUCTION DATA TEST: \n", tag);
	ret = production_test_data(pathThresholds, stop_on_fail, todo);

	if (ret < 0) {
		logError(0,
			 "%s Error during PRODUCTION DATA TEST! ERROR %08X\n",
			 tag, ret);
	} else {
		logError(0, "%s PRODUCTION DATA TEST OK!\n", tag);
	}

	res |= ret;
END:

	if (res < 0) {
		logError(0,
			 "%s MAIN Production test finished.................FAILED \n",
			 tag);
		return res;
	} else {
		logError(0,
			 "%s MAIN Production test finished.................OK\n",
			 tag);
		return OK;
	}
}

/**
 * Perform all the test selected in a TestTodo variable related to MS raw data (touch, keys etc..)
 * @param path_limits name of Production Limit file to load or "NULL" if the limits data should be loaded by a .h file
 * @param stop_on_fail if 1, the test flow stops at the first data check failure otherwise it keeps going performing all the selected test
 * @param todo pointer to a TestToDo variable which select the test to do
 * @return OK if success or an error code which specify the type of error encountered
 */
int production_test_ms_raw(const char *path_limits, int stop_on_fail, TestToDo *todo)
{
	int ret, count_fail = 0;
	MutualSenseFrame msRawFrame;
	int *thresholds = NULL;
	int *thresholds_min = NULL;
	int *thresholds_max = NULL;
	int trows, tcolumns;
	u16 *adj = NULL;
	/******************************* Mutual Sense Test *******************************/
	logError(0, "%s \n", tag);

	if (todo->MutualRaw == 1 || todo->MutualRawGap == 1
	    || todo->MutualRawAdj == 1 || todo->MutualRawEachNode == 1) {
		ret = setScanMode(SCAN_MODE_LOCKED, LOCKED_ACTIVE);
		mdelay(WAIT_FOR_FRESH_FRAMES);
		ret |= setScanMode(SCAN_MODE_ACTIVE, 0x00);
		mdelay(WAIT_AFTER_SENSEOFF);
		ret |= getMSFrame3(MS_RAW, &msRawFrame);

		if (ret < OK) {
			logError(1,
				 "%s production_test_data: getMSFrame failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			return (ret | ERROR_PROD_TEST_DATA);
		}


		if (todo->MutualRaw == 1) {
			logError(1, "%s MS RAW MIN MAX TEST:  \n", tag);
			ret =
				parseProductionTestLimits(path_limits, &limit_file,
							  MS_RAW_MIN_MAX,
							  &thresholds, &trows,
							  &tcolumns);

			if (ret < OK || (trows != 1 || tcolumns != 2)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits MS_RAW_MIN_MAX failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret =
				checkLimitsMinMax(msRawFrame.node_data,
						  msRawFrame.header.force_node,
						  msRawFrame.header.sense_node,
						  thresholds[0], thresholds[1]);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsMinMax MS RAW failed... ERROR COUNT = %d \n",
					 tag, ret);
				logError(0,
					 "%s MS RAW MIN MAX TEST:.................FAIL \n\n",
					 tag);
				count_fail += 1;

				if (stop_on_fail == 1)
					goto ERROR;
			} else
				logError(0,
					 "%s MS RAW MIN MAX TEST:.................OK  \n",
					 tag);

			kfree(thresholds);
			thresholds = NULL;
		} else
			logError(0,
				 "%s MS RAW MIN MAX TEST:.................SKIPPED  \n",
				 tag);
		if (todo->MutualRawEachNode == 1) {
			logError(1, "%s MS RAW EACH NODE MIN MAX TEST:\n", tag);
			ret = parseProductionTestLimits(path_limits, &limit_file, MS_RAW_EACH_NODE_MIN, &thresholds_min, &trows, &tcolumns);
			if (ret < OK || (trows != msRawFrame.header.force_node || tcolumns != msRawFrame.header.sense_node)) {
				logError(1, "%s production_test_data: parseProductionTestLimits MS_RAW_EACH_NODE_MIN failed... ERROR %08X\n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}
			ret = parseProductionTestLimits(path_limits, &limit_file, MS_RAW_EACH_NODE_MAX, &thresholds_max, &trows, &tcolumns);
			if (ret < OK || (trows != msRawFrame.header.force_node ||
					 tcolumns != msRawFrame.header.sense_node)) {
				logError(1, "%s production_test_data: parseProductionTestLimits MS_RAW_EACH_NODE_MAX failed... ERROR %08X\n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}
			ret = checkLimitsMapTotal(msRawFrame.node_data, msRawFrame.header.force_node, msRawFrame.header.sense_node, thresholds_min, thresholds_max);
			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsMinMaxEachNodeData failed... ERROR COUNT = %d\n",
					 tag, ret);
				logError(0,
					 "%s MS_RAW EACH NODE MIN MAX TEST:.................FAIL\n\n", tag);
				count_fail += 1;
				if (stop_on_fail == 1)
					goto ERROR;
			} else {
				logError(0, "%s MS_RAW_EACH_NODE_MAX TEST:.................OK\n", tag);
			}
			if (thresholds_min != NULL) {
				kfree(thresholds_min);
				thresholds_min = NULL;
			}
			if (thresholds_max != NULL) {
				kfree(thresholds_max);
				thresholds_max = NULL;
			}
		} else {
			logError(0, "%s MS RAW EACH NODE MIN MAX TEST:.................SKIPPED\n", tag);
		}

		logError(0, "%s \n", tag);

		if (todo->MutualRawGap == 1) {
			logError(1, "%s MS RAW GAP TEST: \n", tag);
			ret =
				parseProductionTestLimits(path_limits, &limit_file,
							  MS_RAW_GAP, &thresholds,
							  &trows, &tcolumns);

			if (ret < OK || (trows != 1 || tcolumns != 1)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits MS_RAW_GAP failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret =
				checkLimitsGap(msRawFrame.node_data,
					       msRawFrame.header.force_node,
					       msRawFrame.header.sense_node,
					       thresholds[0]);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsGap MS RAW failed... ERROR = %08X \n",
					 tag, ret);
				count_fail += 1;

				if (stop_on_fail == 1)
					goto ERROR;
			} else
				logError(0,
					 "%s MS RAW GAP TEST:.................OK \n\n",
					 tag);

			kfree(thresholds);
			thresholds = NULL;
		} else
			logError(0,
				 "%s MS RAW GAP TEST:.................SKIPPED  \n",
				 tag);

		logError(0, "%s \n", tag);

		if (todo->MutualRawAdj == 1) {
			logError(1, "%s MS RAW ADJ HORIZONTAL TEST: \n", tag);
			ret =
				computeAdjHorizTotal(msRawFrame.node_data,
						     msRawFrame.header.force_node,
						     msRawFrame.header.sense_node,
						     &adj);

			if (ret < OK) {
				logError(1,
					 "%s production_test_data: computeAdjHoriz failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret =
				parseProductionTestLimits(path_limits, &limit_file,
							  MS_RAW_ADJH, &thresholds,
							  &trows, &tcolumns);

			if (ret < OK
			    || (trows != msRawFrame.header.force_node
				|| tcolumns !=
				msRawFrame.header.sense_node - 1)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits MS_RAW_ADJH failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret =
				checkLimitsMapAdjTotal(adj,
						       msRawFrame.header.force_node,
						       msRawFrame.header.
						       sense_node - 1, thresholds);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsAdj MS RAW ADJH failed... ERROR COUNT = %d \n",
					 tag, ret);
				logError(0,
					 "%s MS RAW ADJ HORIZONTAL TEST:.................FAIL \n\n",
					 tag);
				count_fail += 1;

				if (stop_on_fail == 1)
					goto ERROR;
			} else
				logError(0,
					 "%s MS RAW ADJ HORIZONTAL TEST:.................OK  \n",
					 tag);

			kfree(thresholds);
			thresholds = NULL;
			kfree(adj);
			adj = NULL;
			logError(0, "%s MS RAW ADJ VERTICAL TEST: \n", tag);
			ret =
				computeAdjVertTotal(msRawFrame.node_data,
						    msRawFrame.header.force_node,
						    msRawFrame.header.sense_node,
						    &adj);

			if (ret < OK) {
				logError(1,
					 "%s production_test_data: computeAdjVert failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret =
				parseProductionTestLimits(path_limits, &limit_file,
							  MS_RAW_ADJV, &thresholds,
							  &trows, &tcolumns);

			if (ret < OK
			    || (trows != msRawFrame.header.force_node - 1
				|| tcolumns != msRawFrame.header.sense_node)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits MS_RAW_ADJV failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret =
				checkLimitsMapAdjTotal(adj,
						       msRawFrame.header.
						       force_node - 1,
						       msRawFrame.header.sense_node,
						       thresholds);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsAdj MS RAW ADJV failed... ERROR COUNT = %d \n",
					 tag, ret);
				logError(0,
					 "%s MS RAW ADJ VERTICAL TEST:.................FAIL \n\n",
					 tag);
				count_fail += 1;

				if (stop_on_fail == 1)
					goto ERROR;
			} else
				logError(0,
					 "%s MS RAW ADJ VERTICAL TEST:.................OK  \n",
					 tag);

			kfree(thresholds);
			thresholds = NULL;
			kfree(adj);
			adj = NULL;
		} else
			logError(0,
				 "%s MS RAW ADJ TEST:.................SKIPPED  \n",
				 tag);
	} else
		logError(0, "%s MS RAW FRAME TEST:.................SKIPPED  \n",
			 tag);

	logError(0, "%s \n", tag);

	if (todo->MutualKeyRaw == 1) {
		logError(1, "%s MS KEY RAW TEST:\n", tag);
		ret = production_test_ms_key_raw(path_limits);

		if (ret < 0) {
			logError(1,
				 "%s production_test_data: production_test_ms_key_raw failed... ERROR = %08X \n",
				 tag, ret);
			count_fail += 1;

			if (count_fail == 1) {
				logError(0,
					 "%s MS RAW DATA TEST:.................FAIL fails_count = %d\n\n",
					 tag, count_fail);
				goto ERROR_LIMITS;
			}
		}
	} else
		logError(0, "%s MS KEY RAW TEST:.................SKIPPED  \n",
			 tag);

	ret = production_test_ms_raw_lp(path_limits, stop_on_fail, todo);

	if (ret < 0) {
		logError(1,
			 "%s production_test_data: production_test_ms_raw_lp failed... ERROR = %08X \n",
			 tag, ret);
		count_fail += 1;

		if (count_fail == 1) {
			logError(0,
				 "%s MS RAW DATA TEST:.................FAIL fails_count = %d\n\n",
				 tag, count_fail);
			goto ERROR_LIMITS;
		}
	}

ERROR:
	logError(0, "%s \n", tag);

	if (count_fail == 0) {
		if (msRawFrame.node_data != NULL) {
			kfree(msRawFrame.node_data);
			msRawFrame.node_data = NULL;
		}

		logError(0,
			 "%s MS RAW DATA TEST finished!.................OK\n",
			 tag);
		return OK;
	} else {
		print_frame_short("MS Raw frame =",
				  array1dTo2d_short(msRawFrame.node_data,
						    msRawFrame.node_data_size,
						    msRawFrame.header.
						    sense_node),
				  msRawFrame.header.force_node,
				  msRawFrame.header.sense_node);

		if (msRawFrame.node_data != NULL) {
			kfree(msRawFrame.node_data);
			msRawFrame.node_data = NULL;
		}
		if (thresholds != NULL) {
			kfree(thresholds);
			thresholds = NULL;
		}
		if (adj != NULL) {
			kfree(adj);
			adj = NULL;
		}
		logError(0,
			 "%s MS RAW DATA TEST:.................FAIL fails_count = %d\n\n",
			 tag, count_fail);
		return (ERROR_PROD_TEST_DATA | ERROR_TEST_CHECK_FAIL);
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

/**
 * Perform all the test selected in a TestTodo variable related to MS low power raw data
 * @param path_limits name of Production Limit file to load or "NULL" if the limits data should be loaded by a .h file
 * @param stop_on_fail if 1, the test flow stops at the first data check failure otherwise it keeps going performing all the selected test
 * @param todo pointer to a TestToDo variable which select the test to do
 * @return OK if success or an error code which specify the type of error encountered
 */
int production_test_ms_raw_lp(const char *path_limits, int stop_on_fail,
			      TestToDo *todo)
{
	int ret, count_fail = 0;
	MutualSenseFrame msRawFrame;
	int *thresholds = NULL;
	int trows, tcolumns;
	u16 *adj = NULL;
	memset(&msRawFrame, 0x00, sizeof(msRawFrame));
	/******************************* Mutual Sense Test *******************************/
	logError(0, "%s \n", tag);

	if (todo->MutualRawLP == 1 || todo->MutualRawGapLP == 1
	    || todo->MutualRawAdjLP == 1) {
		ret = setScanMode(SCAN_MODE_LOCKED, LOCKED_LP_ACTIVE);
		mdelay(WAIT_FOR_FRESH_FRAMES);
		ret |= setScanMode(SCAN_MODE_ACTIVE, 0x00);
		mdelay(WAIT_AFTER_SENSEOFF);
		ret |= getMSFrame3(MS_RAW, &msRawFrame);

		if (ret < 0) {
			logError(1,
				 "%s production_test_data: getMSFrame failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			return (ret | ERROR_PROD_TEST_DATA);
		}


		if (todo->MutualRawLP == 1) {
			logError(1, "%s MS RAW LP MIN MAX TEST:  \n", tag);
			ret =
				parseProductionTestLimits(path_limits, &limit_file,
							  MS_RAW_LP_MIN_MAX,
							  &thresholds, &trows,
							  &tcolumns);

			if (ret < 0 || (trows != 1 || tcolumns != 2)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits MS_RAW_LP_MIN_MAX failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret =
				checkLimitsMinMax(msRawFrame.node_data,
						  msRawFrame.header.force_node,
						  msRawFrame.header.sense_node,
						  thresholds[0], thresholds[1]);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsMinMax MS RAW LP failed... ERROR COUNT = %d \n",
					 tag, ret);
				logError(0,
					 "%s MS RAW LP MIN MAX TEST:.................FAIL \n\n",
					 tag);
				count_fail += 1;

				if (stop_on_fail == 1)
					goto ERROR;
			} else
				logError(0,
					 "%s MS RAW LP MIN MAX TEST:.................OK  \n",
					 tag);

			kfree(thresholds);
			thresholds = NULL;
		} else
			logError(0,
				 "%s MS RAW LP MIN MAX TEST:.................SKIPPED  \n",
				 tag);

		logError(0, "%s \n", tag);

		if (todo->MutualRawGapLP == 1) {
			logError(1, "%s MS RAW LP GAP TEST: \n", tag);
			ret =
				parseProductionTestLimits(path_limits, &limit_file,
							  MS_RAW_LP_GAP,
							  &thresholds, &trows,
							  &tcolumns);

			if (ret < 0 || (trows != 1 || tcolumns != 1)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits MS_RAW_LP_GAP failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret =
				checkLimitsGap(msRawFrame.node_data,
					       msRawFrame.header.force_node,
					       msRawFrame.header.sense_node,
					       thresholds[0]);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsGap MS RAW LP failed... ERROR = %08X \n",
					 tag, ret);
				count_fail += 1;

				if (stop_on_fail == 1)
					goto ERROR;
			} else
				logError(0,
					 "%s MS RAW LP GAP TEST:.................OK \n\n",
					 tag);

			kfree(thresholds);
			thresholds = NULL;
		} else
			logError(0,
				 "%s MS RAW LP GAP TEST:.................SKIPPED  \n",
				 tag);

		logError(0, "%s \n", tag);

		if (todo->MutualRawAdjLP == 1) {
			logError(1, "%s MS RAW LP ADJ HORIZONTAL TEST: \n",
				 tag);
			ret =
				computeAdjHorizTotal(msRawFrame.node_data,
						     msRawFrame.header.force_node,
						     msRawFrame.header.sense_node,
						     &adj);

			if (ret < 0) {
				logError(1,
					 "%s production_test_data: computeAdjHoriz failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret =
				parseProductionTestLimits(path_limits, &limit_file,
							  MS_RAW_LP_ADJH,
							  &thresholds, &trows,
							  &tcolumns);

			if (ret < 0
			    || (trows != msRawFrame.header.force_node
				|| tcolumns !=
				msRawFrame.header.sense_node - 1)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits MS_RAW_LP_ADJH failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret =
				checkLimitsMapAdjTotal(adj,
						       msRawFrame.header.force_node,
						       msRawFrame.header.
						       sense_node - 1, thresholds);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsAdj MS RAW LP ADJH failed... ERROR COUNT = %d \n",
					 tag, ret);
				logError(0,
					 "%s MS RAW LP ADJ HORIZONTAL TEST:.................FAIL \n\n",
					 tag);
				count_fail += 1;

				if (stop_on_fail == 1)
					goto ERROR;
			} else
				logError(0,
					 "%s MS RAW LP ADJ HORIZONTAL TEST:.................OK  \n",
					 tag);

			kfree(thresholds);
			thresholds = NULL;
			kfree(adj);
			adj = NULL;
			logError(0, "%s MS RAW LP ADJ VERTICAL TEST: \n", tag);
			ret =
				computeAdjVertTotal(msRawFrame.node_data,
						    msRawFrame.header.force_node,
						    msRawFrame.header.sense_node,
						    &adj);

			if (ret < 0) {
				logError(1,
					 "%s production_test_data: computeAdjVert failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret =
				parseProductionTestLimits(path_limits, &limit_file,
							  MS_RAW_LP_ADJV,
							  &thresholds, &trows,
							  &tcolumns);

			if (ret < 0
			    || (trows != msRawFrame.header.force_node - 1
				|| tcolumns != msRawFrame.header.sense_node)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits MS_RAW_ADJV failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret =
				checkLimitsMapAdjTotal(adj,
						       msRawFrame.header.
						       force_node - 1,
						       msRawFrame.header.sense_node,
						       thresholds);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsAdj MS RAW ADJV failed... ERROR COUNT = %d \n",
					 tag, ret);
				logError(0,
					 "%s MS RAW LP ADJ VERTICAL TEST:.................FAIL \n\n",
					 tag);
				count_fail += 1;

				if (stop_on_fail == 1)
					goto ERROR;
			} else
				logError(0,
					 "%s MS RAW LP ADJ VERTICAL TEST:.................OK  \n",
					 tag);

			kfree(thresholds);
			thresholds = NULL;
			kfree(adj);
			adj = NULL;
		} else
			logError(0,
				 "%s MS RAW LP ADJ TEST:.................SKIPPED  \n",
				 tag);
	} else
		logError(0,
			 "%s MS RAW LP FRAME TEST:.................SKIPPED  \n",
			 tag);

ERROR:
	logError(0, "%s \n", tag);

	if (count_fail == 0) {
		if (msRawFrame.node_data != NULL) {
			kfree(msRawFrame.node_data);
			msRawFrame.node_data = NULL;
		}

		logError(0,
			 "%s MS RAW DATA TEST finished!.................OK\n",
			 tag);
		return OK;
	} else {
		if (msRawFrame.node_data != NULL) {
			print_frame_short("MS Raw LP frame =",
					  array1dTo2d_short
					  (msRawFrame.node_data,
					   msRawFrame.node_data_size,
					   msRawFrame.header.sense_node),
					  msRawFrame.header.force_node,
					  msRawFrame.header.sense_node);
			kfree(msRawFrame.node_data);
			msRawFrame.node_data = NULL;
		}

		if (thresholds != NULL) {
			kfree(thresholds);
			thresholds = NULL;
		}
		if (adj != NULL) {
			kfree(adj);
			adj = NULL;
		}
		logError(0,
			 "%s MS RAW LP DATA TEST:.................FAIL fails_count = %d\n\n",
			 tag, count_fail);
		return (ERROR_PROD_TEST_DATA | ERROR_TEST_CHECK_FAIL);
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

/**
 * Perform MS raw test for keys data
 * @param path_limits name of Production Limit file to load or "NULL" if the limits data should be loaded by a .h file
 * @return OK if success or an error code which specify the type of error encountered
 */
int production_test_ms_key_raw(const char *path_limits)
{
	int ret;
	MutualSenseFrame msRawFrame;
	int *thresholds = NULL;
	int trows, tcolumns;
	/******************************* Mutual Sense Test *******************************/
	logError(1, "%s MS KEY RAW DATA TEST is starting...\n", tag);
	ret = setScanMode(SCAN_MODE_ACTIVE, 0x01);
	mdelay(WAIT_FOR_FRESH_FRAMES);
	ret |= setScanMode(SCAN_MODE_ACTIVE, 0x00);
	mdelay(WAIT_AFTER_SENSEOFF);
	ret |= getMSFrame3(MS_KEY_RAW, &msRawFrame);

	if (ret < 0) {
		logError(1,
			 "%s production_test_data: getMSKeyFrame failed... ERROR %08X \n",
			 tag, ERROR_PROD_TEST_DATA);
		return (ret | ERROR_PROD_TEST_DATA);
	}

	ret =
		parseProductionTestLimits(path_limits, &limit_file,
					  MS_KEY_RAW_MIN_MAX, &thresholds, &trows,
					  &tcolumns);

	if (ret < 0 || (trows != 1 || tcolumns != 2)) {
		logError(1,
			 "%s production_test_data: parseProductionTestLimits MS_KEY_RAW_MIN_MAX failed... ERROR %08X \n",
			 tag, ERROR_PROD_TEST_DATA);
		ret |= ERROR_PROD_TEST_DATA;
		goto ERROR_LIMITS;
	}

	ret =
		checkLimitsMinMax(msRawFrame.node_data,
				  msRawFrame.header.force_node,
				  msRawFrame.header.sense_node, thresholds[0],
				  thresholds[1]);

	if (ret != OK) {
		logError(1,
			 "%s production_test_data: checkLimitsMinMax MS KEY RAW failed... ERROR COUNT = %d \n",
			 tag, ret);
		goto ERROR;
	} else
		logError(0, "%s MS KEY RAW TEST:.................OK \n\n", tag);

	kfree(thresholds);
	thresholds = NULL;
	kfree(msRawFrame.node_data);
	msRawFrame.node_data = NULL;
	return OK;
ERROR:
	print_frame_short("MS Key Raw frame =",
			  array1dTo2d_short(msRawFrame.node_data,
					    msRawFrame.node_data_size,
					    msRawFrame.header.sense_node),
			  msRawFrame.header.force_node,
			  msRawFrame.header.sense_node);

	if (msRawFrame.node_data != NULL) {
		kfree(msRawFrame.node_data);
		msRawFrame.node_data = NULL;
	}
	if (thresholds != NULL) {
		kfree(thresholds);
		thresholds = NULL;
	}
	logError(0, "%s MS KEY RAW TEST:.................FAIL \n\n", tag);
	return (ERROR_PROD_TEST_DATA | ERROR_TEST_CHECK_FAIL);
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

/**
 * Perform all the tests selected in a TestTodo variable related to MS Init data (touch, keys etc..)
 * @param path_limits name of Production Limit file to load or "NULL" if the limits data should be loaded by a .h file
 * @param stop_on_fail if 1, the test flow stops at the first data check failure otherwise it keeps going performing all the selected test
 * @param todo pointer to a TestToDo variable which select the test to do
 * @return OK if success or an error code which specify the type of error encountered
 */
int production_test_ms_cx(const char *path_limits, int stop_on_fail, TestToDo *todo)
{
	int ret;
	int count_fail = 0;
	int *thresholds = NULL;
	int *thresholds_min = NULL;
	int *thresholds_max = NULL;
	int trows, tcolumns;
	MutualSenseData msCompData;
	TotMutualSenseData totCompData;
	u8 *adjhor = NULL;
	u8 *adjvert = NULL;
	u16 container;
	u16 *total_adjhor = NULL;
	u16 *total_adjvert = NULL;
	logError(0, "%s \n", tag);
	ret = readMutualSenseCompensationData(LOAD_CX_MS_TOUCH, &msCompData);

	if (ret < 0) {
		logError(1,
			 "%s production_test_data: readMutualSenseCompensationData failed... ERROR %08X \n",
			 tag, ERROR_PROD_TEST_DATA);
		return (ret | ERROR_PROD_TEST_DATA);
	}

	ret =
		readTotMutualSenseCompensationData(LOAD_PANEL_CX_TOT_MS_TOUCH,
				&totCompData);

	if (ret < 0) {
		logError(1,
			 "%s production_test_data: readTotMutualSenseCompensationData failed... ERROR %08X \n",
			 tag, ERROR_PROD_TEST_DATA);
		kfree(msCompData.node_data);
		msCompData.node_data = NULL;
		return (ret | ERROR_PROD_TEST_DATA);
	}


	if (todo->MutualCx1 == 1) {
		logError(1, "%s MS CX1 TEST: \n", tag);
		ret =
			parseProductionTestLimits(path_limits, &limit_file,
						  MS_CX1_MIN_MAX, &thresholds,
						  &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != 2)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits MS_CX1_MIN_MAX failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		container = (u16) msCompData.cx1;
		ret =
			checkLimitsMinMax(&container, 1, 1, thresholds[0],
					  thresholds[1]);

		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMinMax MS CX1 failed... ERROR COUNT = %d \n",
				 tag, ret);
			logError(0, "%s MS CX1 TEST:.................FAIL \n\n",
				 tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			logError(0, "%s MS CX1 TEST:.................OK \n\n",
				 tag);
	} else
		logError(0, "%s MS CX1 TEST:.................SKIPPED \n\n",
			 tag);

	kfree(thresholds);
	thresholds = NULL;

	if (todo->MutualCx2 == 1) {
		logError(1, "%s MS CX2 MIN MAX TEST: \n", tag);
		ret = parseProductionTestLimits(path_limits, &limit_file, MS_CX2_MAP_MIN, &thresholds_min, &trows, &tcolumns);

		if (ret < 0
		    || (trows != msCompData.header.force_node
			|| tcolumns != msCompData.header.sense_node)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits MS_CX2_MAP_MIN failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = parseProductionTestLimits(path_limits, &limit_file, MS_CX2_MAP_MAX, &thresholds_max, &trows, &tcolumns);

		if (ret < 0
		    || (trows != msCompData.header.force_node
			|| tcolumns != msCompData.header.sense_node)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits MS_CX2_MAP_MAX failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMap(msCompData.node_data, msCompData.header.force_node, msCompData.header.sense_node, thresholds_min, thresholds_max);

		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMap MS CX2 MIN MAX failed... ERROR COUNT = %d \n",
				 tag, ret);
			logError(0,
				 "%s MS CX2 MIN MAX TEST:.................FAIL \n\n",
				 tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			logError(0,
				 "%s MS CX2 MIN MAX TEST:.................OK \n\n",
				 tag);

		kfree(thresholds_min);
		thresholds_min = NULL;
		kfree(thresholds_max);
		thresholds_max = NULL;
	} else
		logError(0,
			 "%s MS CX2 MIN MAX TEST:.................SKIPPED \n\n",
			 tag);


	if (todo->MutualCx2Adj == 1) {
		logError(1, "%s MS CX2 ADJ TEST: \n", tag);
		ret =
			computeAdjHoriz(msCompData.node_data,
					msCompData.header.force_node,
					msCompData.header.sense_node, &adjhor);

		if (ret < 0) {
			logError(1,
				 "%s production_test_data: computeAdjHoriz failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		logError(0, "%s MS CX2 ADJ HORIZ computed! \n", tag);
		ret =
			parseProductionTestLimits(path_limits, &limit_file,
						  MS_CX2_ADJH_MAP_MAX,
						  &thresholds_max, &trows,
						  &tcolumns);

		if (ret < 0
		    || (trows != msCompData.header.force_node
			|| tcolumns != msCompData.header.sense_node - 1)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits MS_CX2_ADJH_MAP_MAX failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret =
			checkLimitsMapAdj(adjhor, msCompData.header.force_node,
					  msCompData.header.sense_node - 1,
					  thresholds_max);

		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMapAdj CX2 ADJH failed... ERROR COUNT = %d \n",
				 tag, ret);
			logError(0,
				 "%s MS CX2 ADJ HORIZ TEST:.................FAIL \n\n",
				 tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			logError(0,
				 "%s MS CX2 ADJ HORIZ TEST:.................OK \n\n",
				 tag);

		kfree(thresholds_max);
		thresholds_max = NULL;
		kfree(adjhor);
		adjhor = NULL;
		logError(0, "%s MS CX2 ADJ VERT TEST: \n", tag);
		ret =
			computeAdjVert(msCompData.node_data,
				       msCompData.header.force_node,
				       msCompData.header.sense_node, &adjvert);

		if (ret < 0) {
			logError(1,
				 "%s production_test_data: computeAdjVert failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		logError(0, "%s MS CX2 ADJ VERT computed! \n", tag);
		ret =
			parseProductionTestLimits(path_limits, &limit_file,
						  MS_CX2_ADJV_MAP_MAX,
						  &thresholds_max, &trows,
						  &tcolumns);

		if (ret < 0
		    || (trows != msCompData.header.force_node - 1
			|| tcolumns != msCompData.header.sense_node)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits MS_CX2_ADJV_MAP_MAX failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret =
			checkLimitsMapAdj(adjvert, msCompData.header.force_node - 1,
					  msCompData.header.sense_node - 1,
					  thresholds_max);

		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMapAdj CX2 ADJV failed... ERROR COUNT = %d \n",
				 tag, ret);
			logError(0,
				 "%s MS CX2 ADJ HORIZ TEST:.................FAIL \n\n",
				 tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			logError(0,
				 "%s MS CX2 ADJ VERT TEST:.................OK \n\n",
				 tag);

		kfree(thresholds_max);
		thresholds_max = NULL;
		kfree(adjvert);
		adjvert = NULL;
	} else
		logError(0, "%s MS CX2 ADJ TEST:.................SKIPPED \n\n",
			 tag);


	if (todo->MutualCxTotal == 1 || todo->MutualCxTotalAdj == 1) {

		if (todo->MutualCxTotal == 1) {
			logError(1, "%s MS TOTAL CX MIN MAX TEST: \n", tag);
			ret = parseProductionTestLimits(path_limits, &limit_file, MS_TOTAL_CX_MAP_MIN, &thresholds_min, &trows, &tcolumns);

			if (ret < 0
			    || (trows != totCompData.header.force_node
				|| tcolumns != totCompData.header.sense_node)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits MS_TOTAL_CX_MAP_MIN failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret = parseProductionTestLimits(path_limits, &limit_file, MS_TOTAL_CX_MAP_MAX, &thresholds_max, &trows, &tcolumns);

			if (ret < 0
			    || (trows != totCompData.header.force_node
				|| tcolumns != totCompData.header.sense_node)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits MS_TOTAL_CX_MAP_MAX failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMapTotal(totCompData.node_data, totCompData.header.force_node, totCompData.header.sense_node, thresholds_min, thresholds_max);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsMap  MS TOTAL CX TEST failed... ERROR COUNT = %d \n",
					 tag, ret);
				logError(0,
					 "%s MS TOTAL CX MIN MAX TEST:.................FAIL \n\n",
					 tag);
				count_fail += 1;

				if (stop_on_fail)
					goto ERROR;
			} else
				logError(0,
					 "%s MS TOTAL CX MIN MAX TEST:.................OK \n\n",
					 tag);

			kfree(thresholds_min);
			thresholds_min = NULL;
			kfree(thresholds_max);
			thresholds_max = NULL;
		} else
			logError(0,
				 "%s MS TOTAL CX MIN MAX TEST:.................SKIPPED \n\n",
				 tag);


		if (todo->MutualCxTotalAdj == 1) {
			logError(1, "%s MS TOTAL CX ADJ HORIZ TEST: \n", tag);
			ret =
				computeAdjHorizTotal(totCompData.node_data,
						     totCompData.header.force_node,
						     totCompData.header.sense_node,
						     &total_adjhor);

			if (ret < 0) {
				logError(1,
					 "%s production_test_data: computeAdjHoriz failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			logError(0, "%s MS TOTAL CX ADJ HORIZ computed! \n",
				 tag);
			ret =
				parseProductionTestLimits(path_limits, &limit_file,
							  MS_TOTAL_CX_ADJH_MAP_MAX,
							  &thresholds_max, &trows,
							  &tcolumns);

			if (ret < 0
			    || (trows != totCompData.header.force_node
				|| tcolumns !=
				totCompData.header.sense_node - 1)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits MS_TOTAL_CX_ADJH_MAP_MAX failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret =
				checkLimitsMapAdjTotal(total_adjhor,
						       totCompData.header.
						       force_node,
						       totCompData.header.
						       sense_node - 1,
						       thresholds_max);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsMapAdj MS TOTAL CX ADJH failed... ERROR COUNT = %d \n",
					 tag, ret);
				logError(0,
					 "%s MS TOTAL CX ADJ HORIZ TEST:.................FAIL \n\n",
					 tag);
				count_fail += 1;

				if (stop_on_fail)
					goto ERROR;
			} else
				logError(0,
					 "%s MS TOTAL CX ADJ HORIZ TEST:.................OK \n\n",
					 tag);

			kfree(thresholds_max);
			thresholds_max = NULL;
			kfree(total_adjhor);
			total_adjhor = NULL;
			logError(0, "%s MS TOTAL CX ADJ VERT TEST: \n", tag);
			ret =
				computeAdjVertTotal(totCompData.node_data,
						    totCompData.header.force_node,
						    totCompData.header.sense_node,
						    &total_adjvert);

			if (ret < 0) {
				logError(1,
					 "%s production_test_data: computeAdjVert failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			logError(0, "%s MS TOTAL CX ADJ VERT computed! \n",
				 tag);
			ret =
				parseProductionTestLimits(path_limits, &limit_file,
							  MS_TOTAL_CX_ADJV_MAP_MAX,
							  &thresholds_max, &trows,
							  &tcolumns);

			if (ret < 0
			    || (trows != totCompData.header.force_node - 1
				|| tcolumns != totCompData.header.sense_node)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits MS_TOTAL_CX_ADJV_MAP_MAX failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret =
				checkLimitsMapAdjTotal(total_adjvert,
						       totCompData.header.
						       force_node - 1,
						       totCompData.header.
						       sense_node - 1,
						       thresholds_max);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsMapAdj MS TOTAL CX ADJV failed... ERROR COUNT = %d \n",
					 tag, ret);
				logError(0,
					 "%s MS TOTAL CX ADJ HORIZ TEST:.................FAIL \n",
					 tag);
				count_fail += 1;

				if (stop_on_fail)
					goto ERROR;
			} else
				logError(0,
					 "%s MS TOTAL CX ADJ VERT TEST:.................OK \n",
					 tag);

			kfree(thresholds_max);
			thresholds_max = NULL;
			kfree(total_adjvert);
			total_adjvert = NULL;
		} else
			logError(0,
				 "%s MS TOTAL CX ADJ TEST:.................SKIPPED \n",
				 tag);

		kfree(totCompData.node_data);
		totCompData.node_data = NULL;
	} else
		logError(0, "%s MS TOTAL CX TEST:.................SKIPPED \n",
			 tag);

	if ((todo->MutualCx1LP | todo->MutualCx2LP | todo->MutualCx2AdjLP |
	     todo->MutualCxTotalLP | todo->MutualCxTotalAdjLP) == 1) {
		ret = production_test_ms_cx_lp(path_limits, stop_on_fail, todo);
		if (ret < OK) {
			count_fail += 1;
			logError(1,
				 "%s production_test_data: production_test_cx_lp failed... ERROR = %08X\n",
				 tag, ret);
			logError(0,
				 "%s MS CX testes finished!.................FAILED  fails_count = %d\n\n",
				 tag, count_fail);
			return ret;
		}
	} else
		logError(0, "%s MS KEY CX TEST:.................SKIPPED\n",
			 tag);

	if ((todo->MutualKeyCx1 |
	     todo->MutualKeyCx2 | todo->MutualKeyCxTotal) == 1) {
		ret =
			production_test_ms_key_cx(path_limits, stop_on_fail, todo);

		if (ret < 0) {
			count_fail += 1;
			logError(1,
				 "%s production_test_data: production_test_ms_key_cx failed... ERROR = %08X \n",
				 tag, ret);
			logError(0,
				 "%s MS CX testes finished!.................FAILED  fails_count = %d\n\n",
				 tag, count_fail);
			return ret;
		}
	} else
		logError(0, "%s MS KEY CX TEST:.................SKIPPED \n",
			 tag);

ERROR:
	logError(0, "%s \n", tag);

	if (count_fail == 0) {
		logError(0, "%s MS CX testes finished!.................OK\n",
			 tag);
		kfree(msCompData.node_data);
		msCompData.node_data = NULL;
		return OK;
	} else {
		print_frame_i8("MS Init Data (Cx2) =",
			       array1dTo2d_i8(msCompData.node_data,
					      msCompData.node_data_size,
					      msCompData.header.sense_node),
			       msCompData.header.force_node,
			       msCompData.header.sense_node);
		print_frame_short(" TOT MS Init Data (Cx) =",
				  array1dTo2d_short(totCompData.node_data,
						    totCompData.node_data_size,
						    totCompData.header.
						    sense_node),
				  totCompData.header.force_node,
				  totCompData.header.sense_node);
		logError(0,
			 "%s MS CX testes finished!.................FAILED  fails_count = %d\n\n",
			 tag, count_fail);

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
		if (totCompData.node_data != NULL) {
			kfree(totCompData.node_data);
			totCompData.node_data = NULL;
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
		return (ERROR_TEST_CHECK_FAIL | ERROR_PROD_TEST_DATA);
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
	if (totCompData.node_data != NULL) {
		kfree(totCompData.node_data);
		totCompData.node_data = NULL;
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

/**
 * Perform all the tests selected in a TestTodo variable related to MS Init data of the keys
 * @param path_limits name of Production Limit file to load or "NULL" if the limits data should be loaded by a .h file
 * @param stop_on_fail if 1, the test flow stops at the first data check failure otherwise it keeps going performing all the selected test
 * @param todo pointer to a TestToDo variable which select the test to do
 * @return OK if success or an error code which specify the type of error encountered
 */
int production_test_ms_key_cx(const char *path_limits, int stop_on_fail,
			      TestToDo *todo)
{
	int ret;
	int count_fail = 0;
	int num_keys = 0;
	int *thresholds = NULL;
	int *thresholds_min = NULL;
	int *thresholds_max = NULL;
	int trows, tcolumns;
	MutualSenseData msCompData;
	TotMutualSenseData totCompData;
	short container;
	ret = readMutualSenseCompensationData(LOAD_CX_MS_KEY, &msCompData);

	if (ret < 0) {
		logError(1,
			 "%s production_test_data: readMutualSenseCompensationData failed... ERROR %08X \n",
			 tag, ERROR_PROD_TEST_DATA);
		return (ret | ERROR_PROD_TEST_DATA);
	}

	if (msCompData.header.force_node > msCompData.header.sense_node)
		num_keys = msCompData.header.force_node;
	else
		num_keys = msCompData.header.sense_node;


	if (todo->MutualKeyCx1 == 1) {
		logError(1, "%s MS KEY CX1 TEST: \n", tag);
		ret =
			parseProductionTestLimits(path_limits, &limit_file,
						  MS_KEY_CX1_MIN_MAX, &thresholds,
						  &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != 2)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits MS_KEY_CX1_MIN_MAX failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		container = (short)msCompData.cx1;
		ret =
			checkLimitsMinMax(&container, 1, 1, thresholds[0],
					  thresholds[1]);

		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMinMax MS CX1 failed... ERROR COUNT = %d \n",
				 tag, ret);
			logError(0,
				 "%s MS KEY CX1 TEST:.................FAIL \n\n",
				 tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			logError(0,
				 "%s MS KEY CX1 TEST:.................OK \n\n",
				 tag);
	} else
		logError(0, "%s MS KEY CX1 TEST:.................SKIPPED \n\n",
			 tag);

	kfree(thresholds);
	thresholds = NULL;

	if (todo->MutualKeyCx2 == 1) {
		logError(1, "%s MS KEY CX2 TEST: \n", tag);
		ret = parseProductionTestLimits(path_limits, &limit_file, MS_KEY_CX2_MAP_MIN, &thresholds_min, &trows, &tcolumns);

		if (ret < 0
		    || (trows != msCompData.header.force_node
			|| tcolumns != msCompData.header.sense_node)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits MS_KEY_CX2_MAP_MIN failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = parseProductionTestLimits(path_limits, &limit_file, MS_KEY_CX2_MAP_MAX, &thresholds_max, &trows, &tcolumns);

		if (ret < 0
		    || (trows != msCompData.header.force_node
			|| tcolumns != msCompData.header.sense_node)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits MS_KEY_CX2_MAP_MAX failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMap(msCompData.node_data, msCompData.header.force_node, msCompData.header.sense_node, thresholds_min, thresholds_max);

		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMap MS KEY CX2 failed... ERROR COUNT = %d \n",
				 tag, ret);
			logError(0,
				 "%s MS KEY CX2 TEST:.................FAIL \n\n",
				 tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			logError(0,
				 "%s MS KEY CX2 TEST:.................OK \n\n",
				 tag);

		kfree(thresholds_min);
		thresholds_min = NULL;
		kfree(thresholds_max);
		thresholds_max = NULL;
	} else
		logError(0, "%s MS CX2 TEST:.................SKIPPED \n\n",
			 tag);


	if (todo->MutualKeyCxTotal == 1) {
		logError(1, "%s MS KEY TOTAL CX TEST: \n", tag);
		ret =
			readTotMutualSenseCompensationData(LOAD_PANEL_CX_TOT_MS_KEY,
					&totCompData);

		if (ret < 0) {
			logError(1,
				 "%s production_test_data: computeTotalCx failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = parseProductionTestLimits(path_limits, &limit_file, MS_KEY_TOTAL_CX_MAP_MIN, &thresholds_min, &trows, &tcolumns);

		if (ret < 0
		    || (trows != totCompData.header.force_node
			|| tcolumns != totCompData.header.sense_node)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits MS_KEY_TOTAL_CX_MAP_MIN failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = parseProductionTestLimits(path_limits, &limit_file, MS_KEY_TOTAL_CX_MAP_MAX, &thresholds_max, &trows, &tcolumns);

		if (ret < 0
		    || (trows != totCompData.header.force_node
			|| tcolumns != totCompData.header.sense_node)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits MS_KEY_TOTAL_CX_MAP_MAX failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMapTotal(totCompData.node_data, totCompData.header.force_node, totCompData.header.sense_node, thresholds_min, thresholds_max);

		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMap  MS TOTAL KEY CX TEST failed... ERROR COUNT = %d \n",
				 tag, ret);
			logError(0,
				 "%s MS KEY TOTAL CX TEST:.................FAIL \n\n",
				 tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			logError(0,
				 "%s MS KEY TOTAL CX TEST:.................OK \n\n",
				 tag);

		kfree(thresholds_min);
		thresholds_min = NULL;
		kfree(thresholds_max);
		thresholds_max = NULL;
		kfree(totCompData.node_data);
		totCompData.node_data = NULL;
	} else
		logError(0,
			 "%s MS KEY TOTAL CX TEST:.................SKIPPED \n",
			 tag);

ERROR:
	logError(0, "%s \n", tag);

	if (count_fail == 0) {
		logError(0,
			 "%s MS KEY CX testes finished!.................OK\n",
			 tag);
		kfree(msCompData.node_data);
		msCompData.node_data = NULL;
		return OK;
	} else {
		print_frame_i8("MS Key Init Data (Cx2) =",
			       array1dTo2d_i8(msCompData.node_data,
					      msCompData.node_data_size,
					      msCompData.header.sense_node),
			       msCompData.header.force_node,
			       msCompData.header.sense_node);
		logError(0,
			 "%s MS Key CX testes finished!.................FAILED  fails_count = %d\n\n",
			 tag, count_fail);

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
		if (totCompData.node_data != NULL) {
			kfree(totCompData.node_data);
			totCompData.node_data = NULL;
		}
		return (ERROR_TEST_CHECK_FAIL | ERROR_PROD_TEST_DATA);
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
	if (totCompData.node_data != NULL) {
		kfree(totCompData.node_data);
		totCompData.node_data = NULL;
	}
	return ret;
}

/**
  * Perform all the tests selected in a TestTodo variable related to MS LowPower
  * Init data (touch, keys etc..)
  * @param path_limits name of Production Limit file to load or
  * "NULL" if the limits data should be loaded by a .h file
  * @param stop_on_fail if 1, the test flow stops at the first data check
  * failure otherwise it keeps going performing all the selected test
  * @param todo pointer to a TestToDo variable which select the test to do
  * @return OK if success or an error code which specify the type of error
  */
int production_test_ms_cx_lp(const char *path_limits, int stop_on_fail, TestToDo *todo)
{
	int ret;
	int count_fail = 0;

	int *thresholds = NULL;
	int *thresholds_min = NULL;
	int *thresholds_max = NULL;
	int trows, tcolumns;

	MutualSenseData msCompData;
	TotMutualSenseData totCompData;

	u8 *adjhor = NULL;

	u8 *adjvert = NULL;

	u16 container;
	/* u16 *total_cx = NULL; */
	u16 *total_adjhor = NULL;
	u16 *total_adjvert = NULL;


	/* MS CX TEST */
	logError(0, "%s\n", tag);

	ret = readMutualSenseCompensationData(LOAD_CX_MS_LOW_POWER, &msCompData);
	/* read MS compensation data */
	if (ret < 0) {
		logError(1,
			 "%s production_test_data: readMutualSenseCompensationData failed... ERROR %08X\n",
			 tag, ERROR_PROD_TEST_DATA);
		return ret | ERROR_PROD_TEST_DATA;
	}

	ret = readTotMutualSenseCompensationData(LOAD_PANEL_CX_TOT_MS_LOW_POWER,
						 &totCompData);
	/* read  TOT MS compensation data */
	if (ret < 0) {
		logError(1,
			 "%s production_test_data: readTotMutualSenseCompensationData failed... ERROR %08X\n",
			 tag, ERROR_PROD_TEST_DATA);
		kfree(msCompData.node_data);
		msCompData.node_data = NULL;
		return ret | ERROR_PROD_TEST_DATA;
	}

	if (todo->MutualCx1LP == 1) {
		logError(1, "%s MS LP CX1 TEST:\n", tag);
		ret = parseProductionTestLimits(path_limits, &limit_file,
						MS_CX1_LP_MIN_MAX, &thresholds,
						&trows, &tcolumns);
		if (ret < 0 || (trows != 1 || tcolumns != 2)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits MS_CX1_LP_MIN_MAX failed... ERROR %08X\n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		container = (u16)msCompData.cx1;
		ret = checkLimitsMinMax(&container, 1, 1, thresholds[0],
					thresholds[1]);
		/* check the limits */
		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMinMax MS LP CX1 failed... ERROR COUNT = %d\n",
				 tag, ret);
			logError(0, "%s MS LP CX1 TEST:.................FAIL\n\n",
				 tag);
			count_fail += 1;
			if (stop_on_fail)
				goto ERROR;
		} else
			logError(0, "%s MS LP CX1 TEST:.................OK\n\n",
				 tag);
	} else
		logError(0, "%s MS LP CX1 TEST:.................SKIPPED\n\n",
			 tag);

	kfree(thresholds);
	thresholds = NULL;

	if (todo->MutualCx2LP == 1) {
		logError(1, "%s MS LP CX2 MIN MAX TEST:\n", tag);
		ret = parseProductionTestLimits(path_limits, &limit_file,
						MS_CX2_LP_MAP_MIN, &thresholds_min,
						&trows, &tcolumns);
						/* load min thresholds */
		if (ret < 0 || (trows != msCompData.header.force_node ||
				tcolumns != msCompData.header.sense_node)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits MS_CX2_LP_MAP_MIN failed... ERROR %08X\n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = parseProductionTestLimits(path_limits, &limit_file,
						MS_CX2_LP_MAP_MAX, &thresholds_max,
						&trows, &tcolumns);
						/* load max thresholds */
		if (ret < 0 || (trows != msCompData.header.force_node ||
				tcolumns != msCompData.header.sense_node)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits MS_CX2_LP_MAP_MAX failed... ERROR %08X\n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMap(msCompData.node_data,
				     msCompData.header.force_node,
				     msCompData.header.sense_node,
				     thresholds_min, thresholds_max);
					 /* check the limits */
		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMap MS LP CX2 MIN MAX failed... ERROR COUNT = %d\n",
				 tag, ret);
			logError(0,
				 "%s MS LP CX2 MIN MAX TEST:.................FAIL\n\n",
				 tag);
			count_fail += 1;
			if (stop_on_fail)
				goto ERROR;
		} else
			logError(0,
				 "%s MS LP CX2 MIN MAX TEST:.................OK\n\n",
				 tag);

		kfree(thresholds_min);
		thresholds_min = NULL;
		kfree(thresholds_max);
		thresholds_max = NULL;
	} else
		logError(0,
			 "%s MS LP CX2 MIN MAX TEST:.................SKIPPED\n\n",
			 tag);

	if (todo->MutualCx2AdjLP == 1) {
		logError(1, "%s MS LP CX2 ADJ TEST:\n", tag);
		/* MS CX2 ADJ HORIZ */
		logError(0, "%s MS LP CX2 ADJ HORIZ TEST:\n", tag);

		ret = computeAdjHoriz(msCompData.node_data,
				      msCompData.header.force_node,
				      msCompData.header.sense_node,
				      &adjhor);
		if (ret < 0) {
			logError(1,
				 "%s production_test_data: computeAdjHoriz failed... ERROR %08X\n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}
		logError(0, "%s MS LP CX2 ADJ HORIZ computed!\n", tag);

		ret = parseProductionTestLimits(path_limits, &limit_file,
						MS_CX2_ADJH_LP_MAP_MAX,
						&thresholds_max, &trows,
						&tcolumns);
		if (ret < 0 || (trows != msCompData.header.force_node ||
				tcolumns != msCompData.header.sense_node - 1)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits MS_CX2_ADJH_LP_MAP_MAX failed... ERROR %08X\n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMapAdj(adjhor, msCompData.header.force_node,
					msCompData.header.sense_node - 1,
					thresholds_max);
		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMapAdj CX2 ADJH LP failed... ERROR COUNT = %d\n",
				 tag, ret);
			logError(0,
				 "%s MS LP CX2 ADJ HORIZ TEST:.................FAIL\n\n",
				 tag);
			count_fail += 1;
			if (stop_on_fail)
				goto ERROR;
		} else
			logError(0,
				 "%s MS LP CX2 ADJ HORIZ TEST:.................OK\n\n",
				 tag);

		kfree(thresholds_max);
		thresholds_max = NULL;
		kfree(adjhor);
		adjhor = NULL;

		/* MS CX2 ADJ VERT */
		logError(0, "%s MS LP CX2 ADJ VERT TEST:\n", tag);

		ret = computeAdjVert(msCompData.node_data,
				     msCompData.header.force_node,
				     msCompData.header.sense_node,
				     &adjvert);
		if (ret < 0) {
			logError(1,
				 "%s production_test_data: computeAdjVert failed... ERROR %08X\n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}
		logError(0, "%s MS LP CX2 ADJ VERT computed!\n", tag);

		ret = parseProductionTestLimits(path_limits, &limit_file,
						MS_CX2_ADJV_LP_MAP_MAX,
						&thresholds_max, &trows,
						&tcolumns);
		if (ret < 0 || (trows != msCompData.header.force_node - 1 ||
				tcolumns != msCompData.header.sense_node)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits MS_CX2_ADJV_LP_MAP_MAX failed... ERROR %08X\n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMapAdj(adjvert, msCompData.header.force_node -
					1, msCompData.header.sense_node - 1,
					thresholds_max);
		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMapAdj CX2 ADJV LP failed... ERROR COUNT = %d\n",
				 tag, ret);
			logError(0,
				 "%s MS LP CX2 ADJ HORIZ TEST:.................FAIL\n\n",
				 tag);
			count_fail += 1;
			if (stop_on_fail)
				goto ERROR;
		} else
			logError(0,
				 "%s MS LP CX2 ADJ VERT TEST:.................OK\n\n",
				 tag);

		kfree(thresholds_max);
		thresholds_max = NULL;
		kfree(adjvert);
		adjvert = NULL;
	} else
		logError(0, "%s MS LP CX2 ADJ TEST:.................SKIPPED\n\n",
			 tag);

	/* START OF TOTAL CHECK */

	if (todo->MutualCxTotalLP == 1 || todo->MutualCxTotalAdjLP == 1) {
		if (todo->MutualCxTotalLP == 1) {
			logError(1, "%s MS TOTAL LP CX MIN MAX TEST:\n", tag);
			ret = parseProductionTestLimits(path_limits,
							&limit_file,
							MS_TOTAL_CX_LP_MAP_MIN,
							&thresholds_min,
							&trows, &tcolumns);
			/* load min thresholds */
			if (ret < 0 || (trows !=
					totCompData.header.force_node ||
					tcolumns !=
					totCompData.header.sense_node)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits MS_TOTAL_CX_LP_MAP_MIN failed... ERROR %08X\n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret = parseProductionTestLimits(path_limits,
							&limit_file,
							MS_TOTAL_CX_LP_MAP_MAX,
							&thresholds_max,
							&trows, &tcolumns);
			/* load max thresholds */
			if (ret < 0 || (trows !=
					totCompData.header.force_node ||
					tcolumns !=
					totCompData.header.sense_node)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits MS_TOTAL_CX_LP_MAP_MAX failed... ERROR %08X\n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMapTotal(totCompData.node_data,
						  totCompData.header.force_node,
						  totCompData.header.sense_node,
						  thresholds_min,
						  thresholds_max);
			/* check the limits */
			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsMap  MS TOTAL CX LP TEST failed... ERROR COUNT = %d\n",
					 tag, ret);
				logError(0,
					 "%s MS TOTAL CX LP MIN MAX TEST:.................FAIL\n\n",
					 tag);
				count_fail += 1;
				if (stop_on_fail)
					goto ERROR;
			} else
				logError(0,
					 "%s MS TOTAL CX LP MIN MAX TEST:.................OK\n\n",
					 tag);

			kfree(thresholds_min);
			thresholds_min = NULL;
			kfree(thresholds_max);
			thresholds_max = NULL;
		} else
			logError(0,
				 "%s MS TOTAL CX LP MIN MAX TEST:.................SKIPPED\n\n",
				 tag);


		if (todo->MutualCxTotalAdjLP == 1) {
			/* MS TOTAL CX ADJ HORIZ */
			logError(1, "%s MS TOTAL CX ADJ HORIZ LP TEST:\n", tag);

			ret = computeAdjHorizTotal(totCompData.node_data,
						   totCompData.header.force_node,
						   totCompData.header.sense_node,
						   &total_adjhor);
			if (ret < 0) {
				logError(1,
					 "%s production_test_data: computeAdjHoriz failed... ERROR %08X\n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}
			logError(0, "%s MS TOTAL CX ADJ HORIZ LP computed!\n",
				 tag);

			ret = parseProductionTestLimits(path_limits,
							&limit_file,
							MS_TOTAL_CX_ADJH_LP_MAP_MAX,
							&thresholds_max,
							&trows, &tcolumns);
			if (ret < 0 || (trows !=
					totCompData.header.force_node ||
					tcolumns !=
					totCompData.header.sense_node - 1)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits MS_TOTAL_CX_ADJH_LP_MAP_MAX failed... ERROR %08X\n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMapAdjTotal(total_adjhor,
						     totCompData.header.
						     force_node,
						     totCompData.header.
						     sense_node - 1,
						     thresholds_max);
			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsMapAdj MS TOTAL CX ADJH LP failed... ERROR COUNT = %d\n",
					 tag, ret);
				logError(0,
					 "%s MS TOTAL CX ADJ HORIZ LP TEST:.................FAIL\n\n",
					 tag);
				count_fail += 1;
				if (stop_on_fail)
					goto ERROR;
			} else
				logError(0,
					 "%s MS TOTAL CX ADJ HORIZ LP TEST:.................OK\n\n",
					 tag);

			kfree(thresholds_max);
			thresholds_max = NULL;
			kfree(total_adjhor);
			total_adjhor = NULL;

			/* MS TOTAL CX ADJ VERT */
			logError(0, "%s MS TOTAL CX ADJ VERT LP TEST:\n", tag);

			ret = computeAdjVertTotal(totCompData.node_data,
						  totCompData.header.force_node,
						  totCompData.header.sense_node,
						  &total_adjvert);
			if (ret < 0) {
				logError(1,
					 "%s production_test_data: computeAdjVert failed... ERROR %08X\n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}
			logError(0, "%s MS TOTAL CX ADJ VERT LP computed!\n", tag);

			ret = parseProductionTestLimits(path_limits,
							&limit_file,
							MS_TOTAL_CX_ADJV_LP_MAP_MAX,
							&thresholds_max,
							&trows, &tcolumns);
			if (ret < 0 || (trows != totCompData.header.force_node -
					1 || tcolumns !=
					totCompData.header.sense_node)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits MS_TOTAL_CX_ADJV_LP_MAP_MAX failed... ERROR %08X\n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMapAdjTotal(total_adjvert,
						     totCompData.header.
						     force_node - 1,
						     totCompData.header.
						     sense_node - 1,
						     thresholds_max);
			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsMapAdj MS TOTAL CX ADJV failed... ERROR COUNT = %d\n",
					 tag, ret);
				logError(0,
					 "%s MS TOTAL CX ADJ HORIZ LP TEST:.................FAIL\n",
					 tag);
				count_fail += 1;
				if (stop_on_fail)
					goto ERROR;
			} else
				logError(0,
					 "%s MS TOTAL CX ADJ VERT LP TEST:.................OK\n",
					 tag);

			kfree(thresholds_max);
			thresholds_max = NULL;
			kfree(total_adjvert);
			total_adjvert = NULL;
		} else
			logError(0,
				 "%s MS TOTAL CX ADJ LP TEST:.................SKIPPED\n",
				 tag);

		kfree(totCompData.node_data);
		totCompData.node_data = NULL;
	} else
		logError(0, "%s MS TOTAL CX LP TEST:.................SKIPPED\n",
			 tag);



ERROR:
	logError(0, "%s\n", tag);
	if (count_fail == 0) {
		logError(0, "%s MS LP CX testes finished!.................OK\n",
			 tag);
		kfree(msCompData.node_data);
		msCompData.node_data = NULL;
		return OK;
	} else {
		print_frame_i8("MS LP Init Data (Cx2) =", array1dTo2d_i8(
				       msCompData.node_data,
				       msCompData.node_data_size,
				       msCompData.header.sense_node),
			       msCompData.header.force_node,
			       msCompData.header.sense_node);
		print_frame_short(" TOT MS LP Init Data (Cx) =", array1dTo2d_short(
					  totCompData.node_data,
					  totCompData.node_data_size,
					  totCompData.header.sense_node),
				  totCompData.header.force_node,
				  totCompData.header.sense_node);
		logError(0,
			 "%s MS LP CX testes finished!.................FAILED  fails_count = %d\n\n",
			 tag, count_fail);
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
		if (totCompData.node_data != NULL) {
			kfree(totCompData.node_data);
			totCompData.node_data = NULL;
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
		return ERROR_TEST_CHECK_FAIL | ERROR_PROD_TEST_DATA;
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
	if (totCompData.node_data != NULL) {
		kfree(totCompData.node_data);
		totCompData.node_data = NULL;
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

int production_test_ss_hover_raw(const char *path_limits, int stop_on_fail,
			      TestToDo *todo){
	int ret;
	int rows, columns;
	int *thresholds = NULL;
	int trows, tcolumns;
	u8 hover_cnt[4] = {0xa8, 0x0b, 0x01, 0x00};

	SelfSenseFrame ssHoverRawFrame;
	int count_fail = 0;

	ret = fts_write_dma_safe(hover_cnt, sizeof(hover_cnt));
	if (ret != OK) {
		logError(1,
			 "%s hover clear count ERROR = %d\n",tag, ret);
		goto ERROR_LIMITS;
	}

	logError(0, "%s Getting SS Hover Frame...\n", tag);
	ret = setScanMode(SCAN_MODE_LOCKED, LOCKED_HOVER);
	msleep(WAIT_FOR_FRESH_FRAMES);
	ret |= setScanMode(SCAN_MODE_ACTIVE, 0x00);
	msleep(WAIT_AFTER_SENSEOFF);
	ret |= getSSFrame3(SS_HVR_RAW, &ssHoverRawFrame);
	/* SS Hover RAW FORCE TEST */
	logError(0, "%s SS Hover RAW FORCE TEST:\n", tag);

	logError(0, "%s SS RAW FORCE MIN MAX TEST:\n", tag);
	if (todo->SelfHoverForceRaw == 1) {
		columns = 1;
		rows = ssHoverRawFrame.header.force_node;
		ret = parseProductionTestLimits(path_limits,
						&limit_file,
						SS_HOVER_RAW_FORCE_MIN_MAX,
						&thresholds, &trows,
						&tcolumns);
		if (ret < 0 || (trows != 1 || tcolumns != 2)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits SS_HOVER_RAW_FORCE_MIN_MAX failed... ERROR %08X\n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMinMax(ssHoverRawFrame.force_data, rows,columns, thresholds[0],thresholds[1]);
		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMinMax SS HOVER RAW FORCE failed... ERROR COUNT = %d\n",tag, ret);
			logError(0,
				 "%s SS Hover RAW FORCE MIN MAX TEST:.................FAIL\n\n", tag);
			count_fail += 1;
			print_frame_short("SS Raw force frame =",
					  array1dTo2d_short(
						  ssHoverRawFrame.force_data,
						  rows *
						  columns,
						  columns), rows,
					  columns);
			if (stop_on_fail) {
				ret = ERROR_PROD_TEST_DATA |
				      ERROR_TEST_CHECK_FAIL;
				goto ERROR_LIMITS;
			}
		} else
			logError(0,
				 "%s SS RAW FORCE MIN MAX TEST:.................OK\n\n",
				 tag);

		kfree(thresholds);
		thresholds = NULL;
	} else
			logError(0,
				 "%s SS RAW FORCE MIN MAX TEST:.................SKIPPED\n\n",
				 tag);
	logError(0, "%s\n", tag);
	/* SS Hover RAW SENSE TEST */
	logError(0, "%s SS Hover RAW SENSE TEST:\n", tag);

	logError(0, "%s SS RAW SENSE MIN MAX TEST:\n", tag);
	if (todo->SelfHoverSenceRaw == 1) {
		columns = ssHoverRawFrame.header.sense_node;
		rows = 1;
		ret = parseProductionTestLimits(path_limits,
						&limit_file,
						SS_HOVER_RAW_SENSE_MIN_MAX,
						&thresholds, &trows,
						&tcolumns);
		if (ret < 0 || (trows != 1 || tcolumns != 2)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits SS_RAW_SENSE_MIN_MAX failed... ERROR %08X\n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMinMax(ssHoverRawFrame.sense_data, rows,columns, thresholds[0],thresholds[1]);
		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMinMax SS Hover RAW SENSE failed... ERROR COUNT = %d\n",
				 tag, ret);
			logError(0,
				 "%s SS Hover RAW SENSE MIN MAX TEST:.................FAIL\n",
				 tag);
			count_fail += 1;
			print_frame_short("SS Hover Raw sense frame =",
					  array1dTo2d_short(
						  ssHoverRawFrame.sense_data,
						  rows *
						  columns,
						  columns), rows,
					  columns);
			if (stop_on_fail) {
				ret = ERROR_PROD_TEST_DATA |
					  ERROR_TEST_CHECK_FAIL;
				goto ERROR_LIMITS;
			}
		} else
			logError(0,
				 "%s SS Hover RAW SENSE MIN MAX TEST:.................OK\n",tag);

		kfree(thresholds);
		thresholds = NULL;
	}

ERROR_LIMITS:
	if (ssHoverRawFrame.force_data != NULL)
		kfree(ssHoverRawFrame.force_data);
	if (ssHoverRawFrame.sense_data != NULL)
		kfree(ssHoverRawFrame.sense_data);
	if (thresholds != NULL)
		kfree(thresholds);

	return ret;
}

/**
 * Perform all the test selected in a TestTodo variable related to SS raw data (touch, keys etc..)
 * @param path_limits name of Production Limit file to load or "NULL" if the limits data should be loaded by a .h file
 * @param stop_on_fail if 1, the test flow stops at the first data check failure otherwise it keeps going performing all the selected test
 * @param todo pointer to a TestToDo variable which select the test to do
 * @return OK if success or an error code which specify the type of error encountered
 */
int production_test_ss_raw(const char *path_limits, int stop_on_fail, TestToDo *todo)
{
	int ret;
	int count_fail = 0;
	int rows, columns;
	SelfSenseFrame ssRawFrame;
	int *thresholds = NULL;
	int *thresholds_min = NULL;
	int *thresholds_max = NULL;
	int trows, tcolumns;
	logError(0, "%s \n", tag);
	/******************************* Self Sense Test *******************************/
	logError(0, "%s Getting SS Frame... \n", tag);
	ret = setScanMode(SCAN_MODE_LOCKED, LOCKED_ACTIVE);
	mdelay(WAIT_FOR_FRESH_FRAMES);
	ret |= setScanMode(SCAN_MODE_ACTIVE, 0x00);
	mdelay(WAIT_AFTER_SENSEOFF);
	ret |= getSSFrame3(SS_RAW, &ssRawFrame);

	if (ret < 0) {
		logError(1,
			 "%s production_test_data: getSSFrame failed... ERROR %08X \n",
			 tag, ERROR_PROD_TEST_DATA);
		return (ret | ERROR_PROD_TEST_DATA);
	}


	if (todo->SelfForceRaw == 1 || todo->SelfForceRawGap == 1 || todo->SelfForceRawMap == 1) {
		columns = 1;
		rows = ssRawFrame.header.force_node;
		logError(0, "%s SS RAW FORCE MIN MAX TEST:  \n", tag);

		if (todo->SelfForceRaw == 1) {
			logError(1, "%s SS RAW FORCE MIN MAX TEST:  \n", tag);
			ret =
				parseProductionTestLimits(path_limits, &limit_file,
							  SS_RAW_FORCE_MIN_MAX,
							  &thresholds, &trows,
							  &tcolumns);

			if (ret < 0 || (trows != 1 || tcolumns != 2)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_RAW_FORCE_MIN_MAX failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret =
				checkLimitsMinMax(ssRawFrame.force_data, rows,
						  columns, thresholds[0],
						  thresholds[1]);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsMinMax SS RAW FORCE failed... ERROR COUNT = %d \n",
					 tag, ret);
				logError(0,
					 "%s SS RAW (PROXIMITY) FORCE MIN MAX TEST:.................FAIL \n\n",
					 tag);
				count_fail += 1;
				print_frame_short("SS Raw force frame =",
						  array1dTo2d_short
						  (ssRawFrame.force_data,
						   rows * columns, columns),
						  rows, columns);

				if (stop_on_fail) {
					ret =
						ERROR_PROD_TEST_DATA |
						ERROR_TEST_CHECK_FAIL;
					goto ERROR_LIMITS;
				}
			} else
				logError(0,
					 "%s SS RAW (PROXIMITY) FORCE MIN MAX TEST:.................OK \n\n",
					 tag);

			kfree(thresholds);
			thresholds = NULL;
		} else
			logError(0,
				 "%s SS RAW (PROXIMITY) FORCE MIN MAX TEST:.................SKIPPED \n\n",
				 tag);

		logError(1, "%s SS RAW FORCE MAP MIN MAX TEST:\n", tag);
		if (todo->SelfForceRawMap == 1) {
			ret = parseProductionTestLimits(path_limits,
				&limit_file, SS_RAW_FORCE_EACH_NODE_MIN,
				&thresholds_min, &trows, &tcolumns);
			if (ret < OK || (trows != rows ||
					 tcolumns != columns)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_RAW_FORCE_EACH_NODE_MIN failed... ERROR %08X\n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}
			ret = parseProductionTestLimits(path_limits,
				&limit_file, SS_RAW_FORCE_EACH_NODE_MAX,
				&thresholds_max, &trows, &tcolumns);
			if (ret < OK || (trows != rows ||
					 tcolumns != columns)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_RAW_FORCE_EACH_NODE_MAX failed... ERROR %08X\n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMapTotal(ssRawFrame.force_data, rows,
						columns, thresholds_min,
						thresholds_max);
			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsMinMax SS RAW FORCE MAP failed... ERROR COUNT = %d\n",
					 tag, ret);
				logError(0,
					 "%s SS RAW FORCE MAP MIN MAX TEST:.................FAIL\n\n",
					 tag);
				count_fail += 1;
				print_frame_short("SS Raw force frame =",
						  array1dTo2d_short(
							  ssRawFrame.force_data,
							  rows *
							  columns,
							  columns), rows,
						  columns);
				if (stop_on_fail) {
					ret = ERROR_PROD_TEST_DATA |
					      ERROR_TEST_CHECK_FAIL;
					goto ERROR_LIMITS;
				}
			} else
				logError(0,
					 "%s SS RAW FORCE MAP MIN MAX TEST:.................OK\n\n",
					 tag);

			if (thresholds_min != NULL) {
				kfree(thresholds_min);
				thresholds_min = NULL;
			}
			if (thresholds_max != NULL) {
				kfree(thresholds_max);
				thresholds_max = NULL;
			}
		} else
			logError(0,
				 "%s SS RAW FORCE MAP MIN MAX TEST:.................SKIPPED\n\n",
				 tag);

		if (todo->SelfForceRawGap == 1) {
			logError(1, "%s SS RAW FORCE GAP TEST:  \n", tag);
			ret =
				parseProductionTestLimits(path_limits, &limit_file,
							  SS_RAW_FORCE_GAP,
							  &thresholds, &trows,
							  &tcolumns);

			if (ret < 0 || (trows != 1 || tcolumns != 1)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_RAW_FORCE_GAP failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret =
				checkLimitsGap(ssRawFrame.force_data, rows, columns,
					       thresholds[0]);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsGap SS RAW FORCE GAP failed... ERROR = %08X \n",
					 tag, ret);
				logError(0,
					 "%s SS RAW FORCE GAP TEST:.................FAIL \n\n",
					 tag);
				count_fail += 1;
				print_frame_short("SS Raw force frame =",
						  array1dTo2d_short
						  (ssRawFrame.force_data,
						   rows * columns, columns),
						  rows, columns);

				if (stop_on_fail) {
					ret =
						ERROR_PROD_TEST_DATA |
						ERROR_TEST_CHECK_FAIL;
					goto ERROR_LIMITS;
				}
			} else
				logError(0,
					 "%s SS RAW (PROXIMITY) FORCE GAP TEST:.................OK \n\n",
					 tag);

			kfree(thresholds);
			thresholds = NULL;
		} else
			logError(0,
				 "%s SS RAW (PROXIMITY) FORCE GAP TEST:.................SKIPPED \n\n",
				 tag);

		kfree(ssRawFrame.force_data);
		ssRawFrame.force_data = NULL;
	} else
		logError(0,
			 "%s SS RAW (PROXIMITY) FORCE TEST:.................SKIPPED \n\n",
			 tag);

	logError(0, "%s \n", tag);

	if (todo->SelfSenseRaw == 1 || todo->SelfSenseRawGap == 1 || todo->SelfSenseRawMap == 1) {
		columns = ssRawFrame.header.sense_node;
		rows = 1;

		if (todo->SelfSenseRaw == 1) {
			logError(1, "%s SS RAW SENSE MIN MAX TEST:  \n", tag);
			ret =
				parseProductionTestLimits(path_limits, &limit_file,
							  SS_RAW_SENSE_MIN_MAX,
							  &thresholds, &trows,
							  &tcolumns);

			if (ret < 0 || (trows != 1 || tcolumns != 2)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_RAW_SENSE_MIN_MAX failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret =
				checkLimitsMinMax(ssRawFrame.sense_data, rows,
						  columns, thresholds[0],
						  thresholds[1]);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsMinMax SS RAW SENSE failed... ERROR COUNT = %d \n",
					 tag, ret);
				logError(0,
					 "%s SS RAW SENSE MIN MAX TEST:.................FAIL \n",
					 tag);
				count_fail += 1;
				print_frame_short("SS Raw sense frame =",
						  array1dTo2d_short
						  (ssRawFrame.sense_data,
						   rows * columns, columns),
						  rows, columns);

				if (stop_on_fail) {
					ret =
						ERROR_PROD_TEST_DATA |
						ERROR_TEST_CHECK_FAIL;
					goto ERROR_LIMITS;
				}
			} else
				logError(0,
					 "%s SS RAW (PROXIMITY) SENSE MIN MAX TEST:.................OK \n",
					 tag);

			kfree(thresholds);
			thresholds = NULL;
		} else
			logError(0,
				 "%s SS RAW (PROXIMITY) SENSE MIN MAX TEST:.................SKIPPED \n",
				 tag);
		logError(1, "%s SS RAW SENSE MAP MIN MAX TEST:\n", tag);
		if (todo->SelfSenseRawMap == 1) {
			ret = parseProductionTestLimits(path_limits,
				&limit_file, SS_RAW_SENSE_EACH_NODE_MIN,
				&thresholds_min, &trows, &tcolumns);
			if (ret < OK || (trows != rows ||
					 tcolumns != columns)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_RAW_SENSE_EACH_NODE_MIN failed... ERROR %08X\n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}
			ret = parseProductionTestLimits(path_limits,
				&limit_file, SS_RAW_SENSE_EACH_NODE_MAX,
				&thresholds_max, &trows, &tcolumns);
			if (ret < OK || (trows != rows ||
					 tcolumns != columns)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_RAW_SENSE_EACH_NODE_MAX failed... ERROR %08X\n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMapTotal(ssRawFrame.sense_data, rows,
						columns, thresholds_min,
						thresholds_max);
			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsMinMax SS RAW SENSE MAP failed... ERROR COUNT = %d\n",
					 tag, ret);
				logError(0,
					 "%s SS RAW SENSE MAP MIN MAX TEST:.................FAIL\n\n",
					 tag);
				count_fail += 1;
				print_frame_short("SS Raw sense frame =",
						  array1dTo2d_short(
							  ssRawFrame.sense_data,
							  rows *
							  columns,
							  columns), rows,
						  columns);
				if (stop_on_fail) {
					ret = ERROR_PROD_TEST_DATA |
					      ERROR_TEST_CHECK_FAIL;
					goto ERROR_LIMITS;
				}
			} else
				logError(0,
					 "%s SS RAW SENSE MAP MIN MAX TEST:.................OK\n\n",
					 tag);

			if (thresholds_min != NULL) {
				kfree(thresholds_min);
				thresholds_min = NULL;
			}
			if (thresholds_max != NULL) {
				kfree(thresholds_max);
				thresholds_max = NULL;
			}
		} else
			logError(0,
				 "%s SS RAW SENSE MAP MIN MAX TEST:.................SKIPPED\n\n",
				 tag);

		logError(0, "%s \n", tag);

		if (todo->SelfSenseRawGap == 1) {
			logError(1, "%s SS RAW SENSE GAP TEST:  \n", tag);
			ret =
				parseProductionTestLimits(path_limits, &limit_file,
							  SS_RAW_SENSE_GAP,
							  &thresholds, &trows,
							  &tcolumns);

			if (ret < 0 || (trows != 1 || tcolumns != 1)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_RAW_SENSE_GAP failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret =
				checkLimitsGap(ssRawFrame.sense_data, rows, columns,
					       thresholds[0]);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsGap SS RAW SENSE GAP failed... ERROR = %08X \n",
					 tag, ret);
				logError(0,
					 "%s SS RAW SENSE GAP TEST:.................FAIL \n",
					 tag);
				count_fail += 1;
				print_frame_short("SS Raw sense frame =",
						  array1dTo2d_short
						  (ssRawFrame.sense_data,
						   rows * columns, columns),
						  rows, columns);

				if (stop_on_fail) {
					ret =
						ERROR_PROD_TEST_DATA |
						ERROR_TEST_CHECK_FAIL;
					goto ERROR_LIMITS;
				}
			} else
				logError(0,
					 "%s SS RAW (PROXIMITY) SENSE GAP TEST:.................OK \n",
					 tag);

			kfree(thresholds);
			thresholds = NULL;
		} else
			logError(0,
				 "%s SS RAW (PROXIMITY) SENSE GAP TEST:.................SKIPPED \n",
				 tag);

		kfree(ssRawFrame.sense_data);
		ssRawFrame.sense_data = NULL;
	}

	ret = production_test_ss_raw_lp(path_limits, stop_on_fail, todo);

	if (ret < OK) {
		logError(1,
			 "%s production_test_data: production_test_ss_raw_lp failed... ERROR = %08X \n",
			 tag, ret);
		count_fail += 1;
	}
	if (todo->SelfHoverForceRaw == 1 || todo->SelfHoverSenceRaw) {
		ret = production_test_ss_hover_raw(path_limits, stop_on_fail, todo);
		if (ret < OK) {
			logError(1,
				 "%s production_test_data: production_test_ss_hover_raw failed... ERROR = %08X\n",
				 tag, ret);
			count_fail += 1;
		}
	}

	logError(0, "%s \n", tag);

	if (count_fail == 0) {
		logError(0, "%s SS RAW testes finished!.................OK\n\n",
			 tag);
		return OK;
	} else {
		logError(0,
			 "%s SS RAW testes finished!.................FAILED  fails_count = %d\n\n",
			 tag, count_fail);
		return (ERROR_TEST_CHECK_FAIL | ERROR_PROD_TEST_DATA);
	}

ERROR_LIMITS:

	if (ssRawFrame.force_data != NULL)
		kfree(ssRawFrame.force_data);

	if (ssRawFrame.sense_data != NULL)
		kfree(ssRawFrame.sense_data);

	if (thresholds != NULL)
		kfree(thresholds);

	return ret;
}

/**
 * Perform all the test selected in a TestTodo variable related to SS raw data low power
 * @param path_limits name of Production Limit file to load or "NULL" if the limits data should be loaded by a .h file
 * @param stop_on_fail if 1, the test flow stops at the first data check failure otherwise it keeps going performing all the selected test
 * @param todo pointer to a TestToDo variable which select the test to do
 * @return OK if success or an error code which specify the type of error encountered
 */
int production_test_ss_raw_lp(const char *path_limits, int stop_on_fail,
			      TestToDo *todo)
{
	int ret;
	int count_fail = 0;
	int rows, columns;
	SelfSenseFrame ssRawFrame;
	int *thresholds = NULL;
	int trows, tcolumns;
	logError(0, "%s \n", tag);
	/******************************* Self Sense Test *******************************/
	ret = setScanMode(SCAN_MODE_LOCKED, LOCKED_LP_DETECT);
	mdelay(WAIT_FOR_FRESH_FRAMES);
	ret |= setScanMode(SCAN_MODE_ACTIVE, 0x00);
	mdelay(WAIT_AFTER_SENSEOFF);
	ret |= getSSFrame3(SS_RAW, &ssRawFrame);

	if (ret < 0) {
		logError(1,
			 "%s production_test_data: getSSFrame failed... ERROR %08X \n",
			 tag, ERROR_PROD_TEST_DATA);
		return (ret | ERROR_PROD_TEST_DATA);
	}


	if (todo->SelfForceRawLP == 1 || todo->SelfForceRawGapLP == 1) {
		columns = 1;
		rows = ssRawFrame.header.force_node;

		if (todo->SelfForceRawLP == 1) {
			logError(1, "%s SS RAW LP FORCE MIN MAX TEST:  \n", tag);
			ret =
				parseProductionTestLimits(path_limits, &limit_file,
							  SS_RAW_LP_FORCE_MIN_MAX,
							  &thresholds, &trows,
							  &tcolumns);

			if (ret < 0 || (trows != 1 || tcolumns != 2)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_RAW_FORCE_MIN_MAX failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret =
				checkLimitsMinMax(ssRawFrame.force_data, rows,
						  columns, thresholds[0],
						  thresholds[1]);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsMinMax SS RAW FORCE failed... ERROR COUNT = %d \n",
					 tag, ret);
				logError(0,
					 "%s SS RAW LP FORCE MIN MAX TEST:.................FAIL \n\n",
					 tag);
				count_fail += 1;
				print_frame_short("SS Raw LP force frame =",
						  array1dTo2d_short
						  (ssRawFrame.force_data,
						   rows * columns, columns),
						  rows, columns);

				if (stop_on_fail) {
					ret =
						ERROR_PROD_TEST_DATA |
						ERROR_TEST_CHECK_FAIL;
					goto ERROR_LIMITS;
				}
			} else
				logError(0,
					 "%s SS RAW LP FORCE MIN MAX TEST:.................OK \n\n",
					 tag);

			kfree(thresholds);
			thresholds = NULL;
		} else
			logError(0,
				 "%s SS RAW LP FORCE MIN MAX TEST:.................SKIPPED \n\n",
				 tag);

		logError(0, "%s \n", tag);

		if (todo->SelfForceRawGapLP == 1) {
			logError(1, "%s SS RAW LP FORCE GAP TEST:  \n", tag);
			ret =
				parseProductionTestLimits(path_limits, &limit_file,
							  SS_RAW_LP_FORCE_GAP,
							  &thresholds, &trows,
							  &tcolumns);

			if (ret < OK || (trows != 1 || tcolumns != 1)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_RAW_FORCE_GAP failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret =
				checkLimitsGap(ssRawFrame.force_data, rows, columns,
					       thresholds[0]);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsGap SS RAW FORCE GAP failed... ERROR = %08X \n",
					 tag, ret);
				logError(0,
					 "%s SS RAW LP FORCE GAP TEST:.................FAIL \n\n",
					 tag);
				count_fail += 1;
				print_frame_short("SS Raw LP force frame =",
						  array1dTo2d_short
						  (ssRawFrame.force_data,
						   rows * columns, columns),
						  rows, columns);

				if (stop_on_fail) {
					ret =
						ERROR_PROD_TEST_DATA |
						ERROR_TEST_CHECK_FAIL;
					goto ERROR_LIMITS;
				}
			} else
				logError(0,
					 "%s SS RAW LP FORCE GAP TEST:.................OK \n\n",
					 tag);

			kfree(thresholds);
			thresholds = NULL;
		} else
			logError(0,
				 "%s SS RAW LP FORCE GAP TEST:.................SKIPPED \n\n",
				 tag);

		kfree(ssRawFrame.force_data);
		ssRawFrame.force_data = NULL;
	} else
		logError(0,
			 "%s SS RAW LP FORCE TEST:.................SKIPPED \n\n",
			 tag);

	logError(0, "%s \n", tag);

	if (todo->SelfSenseRawLP == 1 || todo->SelfSenseRawGapLP == 1) {
		columns = ssRawFrame.header.sense_node;
		rows = 1;
		logError(0, "%s SS RAW LP SENSE MIN MAX TEST:  \n", tag);

		if (todo->SelfSenseRawLP == 1) {
			logError(1, "%s SS RAW LP SENSE MIN MAX TEST:  \n", tag);
			ret =
				parseProductionTestLimits(path_limits, &limit_file,
							  SS_RAW_LP_SENSE_MIN_MAX,
							  &thresholds, &trows,
							  &tcolumns);

			if (ret < OK || (trows != 1 || tcolumns != 2)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_RAW_SENSE_MIN_MAX failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret =
				checkLimitsMinMax(ssRawFrame.sense_data, rows,
						  columns, thresholds[0],
						  thresholds[1]);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsMinMax SS RAW SENSE failed... ERROR COUNT = %d \n",
					 tag, ret);
				logError(0,
					 "%s SS RAW LP SENSE MIN MAX TEST:.................FAIL \n",
					 tag);
				count_fail += 1;
				print_frame_short("SS Raw LP sense frame =",
						  array1dTo2d_short
						  (ssRawFrame.sense_data,
						   rows * columns, columns),
						  rows, columns);

				if (stop_on_fail) {
					ret =
						ERROR_PROD_TEST_DATA |
						ERROR_TEST_CHECK_FAIL;
					goto ERROR_LIMITS;
				}
			} else
				logError(0,
					 "%s SS RAW SENSE MIN MAX TEST:.................OK \n",
					 tag);

			kfree(thresholds);
			thresholds = NULL;
		} else
			logError(0,
				 "%s SS RAW LP SENSE MIN MAX TEST:.................SKIPPED \n",
				 tag);

		logError(0, "%s \n", tag);

		if (todo->SelfSenseRawGapLP == 1) {
			logError(1, "%s SS RAW LP SENSE GAP TEST:  \n", tag);
			ret =
				parseProductionTestLimits(path_limits, &limit_file,
							  SS_RAW_LP_SENSE_GAP,
							  &thresholds, &trows,
							  &tcolumns);

			if (ret < OK || (trows != 1 || tcolumns != 1)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_RAW_SENSE_GAP failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret =
				checkLimitsGap(ssRawFrame.sense_data, rows, columns,
					       thresholds[0]);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsGap SS RAW SENSE GAP failed... ERROR = %08X \n",
					 tag, ret);
				logError(0,
					 "%s SS RAW LP SENSE GAP TEST:.................FAIL \n",
					 tag);
				count_fail += 1;
				print_frame_short("SS Raw LP sense frame =",
						  array1dTo2d_short
						  (ssRawFrame.sense_data,
						   rows * columns, columns),
						  rows, columns);

				if (stop_on_fail) {
					ret =
						ERROR_PROD_TEST_DATA |
						ERROR_TEST_CHECK_FAIL;
					goto ERROR_LIMITS;
				}
			} else
				logError(0,
					 "%s SS RAW LP SENSE GAP TEST:.................OK \n",
					 tag);

			kfree(thresholds);
			thresholds = NULL;
		} else
			logError(0,
				 "%s SS RAW LP SENSE GAP TEST:.................SKIPPED \n",
				 tag);

		kfree(ssRawFrame.sense_data);
		ssRawFrame.sense_data = NULL;
	}

	logError(0, "%s \n", tag);

	if (count_fail == 0) {
		logError(0,
			 "%s SS RAW LP testes finished!.................OK\n\n",
			 tag);
		return OK;
	} else {
		logError(0,
			 "%s SS RAW LP testes finished!.................FAILED  fails_count = %d\n\n",
			 tag, count_fail);
		return (ERROR_TEST_CHECK_FAIL | ERROR_PROD_TEST_DATA);
	}

ERROR_LIMITS:

	if (ssRawFrame.force_data != NULL)
		kfree(ssRawFrame.force_data);

	if (ssRawFrame.sense_data != NULL)
		kfree(ssRawFrame.sense_data);

	if (thresholds != NULL)
		kfree(thresholds);

	return ret;
}

int production_test_ss_hover_ix(const char *path_limits, int stop_on_fail,TestToDo *todo){
	TotSelfSenseData ssHoverCompData;
	int ret;
	int trows, tcolumns;
	int *thresholds_min = NULL;
	int *thresholds_max = NULL;
	int count_fail = 0;

	/* ret = readSelfSenseCompensationData(LOAD_CX_SS_HOVER, &ssHoverCompData); */
	ret = readTotSelfSenseCompensationData(STAPI_HOST_DATA_ID_PANEL_CX_SS_HVR, &ssHoverCompData);
	/* read the SS compensation data */
	if (ret < 0) {
		logError(1,
		"%s production_test_data: readSelfSenseCompensationData failed... ERROR %08X\n",tag, ERROR_PROD_TEST_DATA);
		return ret | ERROR_PROD_TEST_DATA;
	}

	logError(0, "%s SS Hover TOTAL IX FORCE TEST:\n", tag);
	logError(0, "%s SS Hover TOTAL IX FORCE MIN MAX TEST:\n", tag);
	if (todo->SelfHoverForceIxTotal == 1) {
		ret = parseProductionTestLimits(path_limits,&limit_file,SS_HOVER_TOTAL_IX_FORCE_MAP_MIN,&thresholds_min,&trows, &tcolumns);
					/* load the min thresholds */
		if (ret < 0 || (trows != ssHoverCompData.header.force_node ||tcolumns != 1)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits SS_TOTAL_IX_FORCE_MAP_MIN failed... ERROR %08X\n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = parseProductionTestLimits(path_limits,&limit_file,SS_TOTAL_IX_FORCE_MAP_MAX,&thresholds_max,&trows, &tcolumns);
					/* load the max thresholds */
		if (ret < 0 || (trows !=
				ssHoverCompData.header.force_node ||
				tcolumns != 1)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits SS_TOTAL_IX_FORCE_MAP_MAX failed... ERROR %08X\n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMapTotalFromU(ssHoverCompData.ix_fm,ssHoverCompData.header.
						   force_node, 1,
						   thresholds_min,
						   thresholds_max);
		/* check the values with thresholds */
		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMap  SS TOTAL IX FORCE failed... ERROR COUNT = %d\n",
				 tag, ret);
			logError(0,
				 "%s SS TOTAL IX FORCE MIN MAX TEST:.................FAIL\n\n",
				 tag);
			count_fail += 1;
			if (stop_on_fail)
				goto ERROR;
		} else
			logError(0,
				 "%s SS TOTAL IX FORCE MIN MAX TEST:.................OK\n\n",
				 tag);

		kfree(thresholds_min);
		thresholds_min = NULL;
		kfree(thresholds_max);
		thresholds_max = NULL;
	} else
	logError(0,
			 "%s SS Hover TOTAL IX FORCE MIN MAX TEST:.................SKIPPED\n",
			 tag);

	logError(0, "%s SS HOVER TOTAL IX SENSE MIN MAX TEST:\n", tag);
		if (todo->SelfHoverSenceIxTotal == 1) {
			ret = parseProductionTestLimits(path_limits,
							&limit_file,
							SS_TOTAL_IX_SENSE_MAP_MIN,
							&thresholds_min,
							&trows, &tcolumns);
			/* load the min thresholds */
			if (ret < 0 || (trows != 1 || tcolumns !=
					ssHoverCompData.header.sense_node)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_TOTAL_IX_SENSE_MAP_MIN failed... ERROR %08X\n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret = parseProductionTestLimits(path_limits,
							&limit_file,
							SS_TOTAL_IX_SENSE_MAP_MAX,
							&thresholds_max,
							&trows, &tcolumns);
			/* load the max thresholds */
			if (ret < 0 || (trows != 1 || tcolumns !=
					ssHoverCompData.header.sense_node)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_TOTAL_IX_SENSE_MAP_MAX failed... ERROR %08X\n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMapTotalFromU(ssHoverCompData.ix_sn, 1,
						       ssHoverCompData.header.
						       sense_node,
						       thresholds_min,
						       thresholds_max);
			/* check the values with thresholds */
			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsMap SS TOTAL IX SENSE failed... ERROR COUNT = %d\n",
					 tag, ret);
				logError(0,
					 "%s SS Hover TOTAL IX SENSE MIN MAX TEST:.................FAIL\n\n",
					 tag);
				count_fail += 1;
				if (stop_on_fail)
					goto ERROR;
			} else
				logError(0,
					 "%s SS TOTAL IX SENSE MIN MAX TEST:.................OK\n\n",
					 tag);

			kfree(thresholds_min);
			thresholds_min = NULL;
			kfree(thresholds_max);
			thresholds_max = NULL;
		} else
			logError(0,
				 "%s SS Hover TOTAL IX SENSE MIN MAX TEST:.................SKIPPED\n",
				 tag);
ERROR:
	logError(0, "%s\n", tag);
	if (count_fail == 0) {
		kfree(ssHoverCompData.ix_fm);
		ssHoverCompData.ix_fm = NULL;
		kfree(ssHoverCompData.ix_sn);
		ssHoverCompData.ix_sn = NULL;
		kfree(ssHoverCompData.cx_fm);
		ssHoverCompData.cx_fm = NULL;
		kfree(ssHoverCompData.cx_sn);
		ssHoverCompData.cx_sn = NULL;
		logError(0,
			 "%s SS Hover IX  testes finished!.................OK\n\n",
			 tag);
		return OK;
	} else {
		print_frame_u16("TOT SS hover Init Data Ix_fm = ", array1dTo2d_u16(
					ssHoverCompData.ix_fm,
					ssHoverCompData.header.force_node, 1),
				ssHoverCompData.header.force_node, 1);
		print_frame_short("TOT SS hover hover Init Data Cx_fm = ",
				  array1dTo2d_short(ssHoverCompData.cx_fm,
							ssHoverCompData.header.
							force_node, 1),
				  ssHoverCompData.header.force_node, 1);
		print_frame_u16("TOT SS hover Init Data Ix_sn = ", array1dTo2d_u16(
					ssHoverCompData.ix_sn,
					ssHoverCompData.header.sense_node,
					ssHoverCompData.header.sense_node), 1,
				ssHoverCompData.header.sense_node);
		print_frame_short("TOT SS hover Init Data Cx_sn = ",
				  array1dTo2d_short(ssHoverCompData.cx_sn,
							ssHoverCompData.header.
							sense_node,
							ssHoverCompData.header.
							sense_node),
				  1, ssHoverCompData.header.sense_node);
		logError(0,
			 "%s SS hover IX CX testes finished!.................FAILED  fails_count = %d\n\n",
			 tag, count_fail);
		if (thresholds_min != NULL)
			kfree(thresholds_min);
		if (thresholds_max != NULL)
			kfree(thresholds_max);
		if (ssHoverCompData.ix_fm != NULL)
			kfree(ssHoverCompData.ix_fm);
		if (ssHoverCompData.ix_sn != NULL)
			kfree(ssHoverCompData.ix_sn);
		if (ssHoverCompData.cx_fm != NULL)
			kfree(ssHoverCompData.cx_fm);
		if (ssHoverCompData.cx_sn != NULL)
			kfree(ssHoverCompData.cx_sn);
		return ERROR_TEST_CHECK_FAIL | ERROR_PROD_TEST_DATA;
	}
ERROR_LIMITS:
	if (thresholds_min != NULL)
		kfree(thresholds_min);
	if (thresholds_max != NULL)
		kfree(thresholds_max);
	if (ssHoverCompData.ix_fm != NULL)
		kfree(ssHoverCompData.ix_fm);
	if (ssHoverCompData.ix_sn != NULL)
		kfree(ssHoverCompData.ix_sn);
	if (ssHoverCompData.cx_fm != NULL)
		kfree(ssHoverCompData.cx_fm);
	if (ssHoverCompData.cx_sn != NULL)
		kfree(ssHoverCompData.cx_sn);
	return ret;
}

/**
 * Perform all the tests selected in a TestTodo variable related to SS Init data (touch, keys etc..)
 * @param path_limits name of Production Limit file to load or "NULL" if the limits data should be loaded by a .h file
 * @param stop_on_fail if 1, the test flow stops at the first data check failure otherwise it keeps going performing all the selected test
 * @param todo pointer to a TestToDo variable which select the test to do
 * @return OK if success or an error code which specify the type of error encountered
 */
int production_test_ss_ix_cx(const char *path_limits, int stop_on_fail,
			     TestToDo *todo)
{
	int ret;
	int count_fail = 0;
	int *thresholds = NULL;
	int trows, tcolumns;
	int *thresholds_min = NULL;
	int *thresholds_max = NULL;
	SelfSenseData ssCompData;
	TotSelfSenseData totCompData;
	u8 *adjhor = NULL;
	u8 *adjvert = NULL;
	short container;
	u16 *total_adjhor = NULL;
	u16 *total_adjvert = NULL;
	logError(0, "%s \n", tag);
	ret = readSelfSenseCompensationData(LOAD_CX_SS_TOUCH, &ssCompData);

	if (ret < 0) {
		logError(1,
			 "%s production_test_data: readSelfSenseCompensationData failed... ERROR %08X \n",
			 tag, ERROR_PROD_TEST_DATA);
		return (ret | ERROR_PROD_TEST_DATA);
	}

	ret =
		readTotSelfSenseCompensationData(LOAD_PANEL_CX_TOT_SS_TOUCH,
				&totCompData);

	if (ret < 0) {
		logError(1,
			 "%s production_test_data: readTotSelfSenseCompensationData failed... ERROR %08X \n",
			 tag, ERROR_PROD_TEST_DATA);
		kfree(ssCompData.ix2_fm);
		kfree(ssCompData.ix2_sn);
		kfree(ssCompData.cx2_fm);
		kfree(ssCompData.cx2_sn);
		return (ret | ERROR_PROD_TEST_DATA);
	}

	/********************************************************* SS FORCE IX ****************************************************************/

	if (todo->SelfForceIx1 == 1) {
		logError(1, "%s SS IX1 FORCE TEST:  \n", tag);
		ret =
			parseProductionTestLimits(path_limits, &limit_file,
						  SS_IX1_FORCE_MIN_MAX, &thresholds,
						  &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != 2)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits SS_IX1_FORCE_MIN_MAX failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		container = (short)ssCompData.f_ix1;
		ret =
			checkLimitsMinMax(&container, 1, 1, thresholds[0],
					  thresholds[1]);

		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMinMax SS IX1 FORCE TEST failed... ERROR COUNT = %d \n",
				 tag, ret);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			logError(0,
				 "%s SS IX1 FORCE TEST:.................OK \n\n",
				 tag);
	} else
		logError(0,
			 "%s SS IX1 FORCE TEST:.................SKIPPED \n\n",
			 tag);

	kfree(thresholds);
	thresholds = NULL;

	if (todo->SelfForceIx2 == 1) {
		logError(1, "%s SS IX2 FORCE MIN MAX TEST:  \n", tag);
		ret = parseProductionTestLimits(path_limits, &limit_file, SS_IX2_FORCE_MAP_MIN, &thresholds_min, &trows, &tcolumns);

		if (ret < 0
		    || (trows != ssCompData.header.force_node
			|| tcolumns != 1)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits SS_IX2_FORCE_MAP_MIN failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = parseProductionTestLimits(path_limits, &limit_file, SS_IX2_FORCE_MAP_MAX, &thresholds_max, &trows, &tcolumns);

		if (ret < 0
		    || (trows != ssCompData.header.force_node
			|| tcolumns != 1)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits SS_IX2_FORCE_MAP_MAX failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMapFromU(ssCompData.ix2_fm, ssCompData.header.force_node, 1, thresholds_min, thresholds_max);

		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMap SS IX2 FORCE failed... ERROR COUNT = %d \n",
				 tag, ret);
			logError(0,
				 "%s SS IX2 FORCE MIN MAX TEST:.................FAIL \n\n",
				 tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			logError(0,
				 "%s SS IX2 FORCE MIN MAX TEST:.................OK \n\n",
				 tag);

		kfree(thresholds_min);
		thresholds_min = NULL;
		kfree(thresholds_max);
		thresholds_max = NULL;
	} else
		logError(0,
			 "%s SS IX2 FORCE MIN MAX TEST:.................SKIPPED \n\n",
			 tag);


	if (todo->SelfForceIx2Adj == 1) {
		logError(1, "%s SS IX2 FORCE ADJ TEST:  \n", tag);
		ret =
			computeAdjVertFromU(ssCompData.ix2_fm,
					    ssCompData.header.force_node, 1,
					    &adjvert);

		if (ret < 0) {
			logError(1,
				 "%s production_test_data: computeAdjVert SS IX2 FORCE ADJV failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		logError(0, "%s SS IX2 FORCE ADJV computed! \n", tag);
		ret = parseProductionTestLimits(path_limits, &limit_file, SS_IX2_FORCE_ADJV_MAP_MAX, &thresholds_max, &trows, &tcolumns);

		if (ret < 0
		    || (trows != ssCompData.header.force_node - 1
			|| tcolumns != 1)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits SS_IX2_FORCE_ADJV_MAP_MAX failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMapAdj(adjvert, ssCompData.header.force_node - 1, 1, thresholds_max);

		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMap SS IX2 FORCE failed... ERROR COUNT = %d \n",
				 tag, ret);
			logError(0,
				 "%s SS IX2 FORCE ADJV TEST:.................FAIL \n\n",
				 tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			logError(0,
				 "%s SS IX2 FORCE ADJV TEST:.................OK \n\n",
				 tag);

		kfree(thresholds_max);
		thresholds_max = NULL;
		kfree(adjvert);
		adjvert = NULL;
	} else
		logError(0,
			 "%s SS IX2 FORCE ADJ TEST:.................SKIPPED \n\n",
			 tag);


	if (todo->SelfForceIxTotal == 1 || todo->SelfForceIxTotalAdj == 1) {

		if (todo->SelfForceIxTotal == 1) {
			logError(1, "%s SS TOTAL IX FORCE MIN MAX TEST:  \n", tag);
			ret = parseProductionTestLimits(path_limits, &limit_file, SS_TOTAL_IX_FORCE_MAP_MIN, &thresholds_min, &trows, &tcolumns);

			if (ret < 0
			    || (trows != totCompData.header.force_node
				|| tcolumns != 1)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_TOTAL_IX_FORCE_MAP_MIN failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret = parseProductionTestLimits(path_limits, &limit_file, SS_TOTAL_IX_FORCE_MAP_MAX, &thresholds_max, &trows, &tcolumns);

			if (ret < 0
			    || (trows != totCompData.header.force_node
				|| tcolumns != 1)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_TOTAL_IX_FORCE_MAP_MAX failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMapTotalFromU(totCompData.ix_fm, totCompData.header.force_node, 1, thresholds_min, thresholds_max);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsMap  SS TOTAL IX FORCE failed... ERROR COUNT = %d \n",
					 tag, ret);
				logError(0,
					 "%s SS TOTAL IX FORCE MIN MAX TEST:.................FAIL \n\n",
					 tag);
				count_fail += 1;

				if (stop_on_fail)
					goto ERROR;
			} else
				logError(0,
					 "%s SS TOTAL IX FORCE MIN MAX TEST:.................OK \n\n",
					 tag);

			kfree(thresholds_min);
			thresholds_min = NULL;
			kfree(thresholds_max);
			thresholds_max = NULL;
		} else
			logError(0,
				 "%s SS TOTAL IX FORCE MIN MAX TEST:.................SKIPPED \n",
				 tag);


		if (todo->SelfForceIxTotalAdj == 1) {
			logError(1, "%s SS TOTAL IX FORCE ADJVERT TEST:  \n",
				 tag);
			ret =
				computeAdjVertTotalFromU(totCompData.ix_fm,
							 totCompData.header.
							 force_node, 1,
							 &total_adjvert);

			if (ret < 0) {
				logError(1,
					 "%s production_test_data: computeAdjVert SS TOTAL IX FORCE ADJV failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			logError(0, "%s SS TOTAL IX FORCE ADJV computed! \n",
				 tag);
			ret = parseProductionTestLimits(path_limits, &limit_file, SS_TOTAL_IX_FORCE_ADJV_MAP_MAX, &thresholds_max, &trows, &tcolumns);

			if (ret < 0
			    || (trows != totCompData.header.force_node - 1
				|| tcolumns != 1)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_TOTAL_IX_FORCE_ADJV_MAP_MAX... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMapAdjTotal(total_adjvert, totCompData.header.force_node - 1, 1, thresholds_max);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsMap SS TOTAL IX FORCE failed... ERROR COUNT = %d \n",
					 tag, ret);
				logError(0,
					 "%s SS TOTAL IX FORCE ADJV TEST:.................FAIL \n\n",
					 tag);
				count_fail += 1;

				if (stop_on_fail)
					goto ERROR;
			} else
				logError(0,
					 "%s SS TOTAL IX FORCE ADJV TEST:.................OK \n\n",
					 tag);

			kfree(thresholds_max);
			thresholds_max = NULL;
			kfree(total_adjvert);
			total_adjvert = NULL;
		} else
			logError(0,
				 "%s SS TOTAL IX FORCE ADJ TEST:.................SKIPPED  \n",
				 tag);
	} else
		logError(0,
			 "%s SS TOTAL IX FORCE TEST:.................SKIPPED \n\n",
			 tag);

	/********************************************************* SS SENSE IX ****************************************************************/

	if (todo->SelfSenseIx1 == 1) {
		logError(1, "%s SS IX1 SENSE TEST:  \n", tag);
		ret =
			parseProductionTestLimits(path_limits, &limit_file,
						  SS_IX1_SENSE_MIN_MAX, &thresholds,
						  &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != 2)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits SS_IX1_SENSE_MIN_MAX failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		container = (short)ssCompData.s_ix1;
		ret = checkLimitsMinMax(&container, 1, 1, thresholds[0], thresholds[1]);

		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMinMax SS IX1 SENSE TEST failed... ERROR COUNT = %d \n",
				 tag, ret);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			logError(0,
				 "%s SS IX1 SENSE TEST:.................OK \n\n",
				 tag);
	} else
		logError(0,
			 "%s SS IX1 SENSE TEST:.................SKIPPED \n\n",
			 tag);

	kfree(thresholds);
	thresholds = NULL;

	if (todo->SelfSenseIx2 == 1) {
		logError(1, "%s SS IX2 SENSE MIN MAX TEST:  \n", tag);
		ret = parseProductionTestLimits(path_limits, &limit_file, SS_IX2_SENSE_MAP_MIN, &thresholds_min, &trows, &tcolumns);

		if (ret < 0
		    || (trows != 1
			|| tcolumns != ssCompData.header.sense_node)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits SS_IX2_SENSE_MAP_MIN failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = parseProductionTestLimits(path_limits, &limit_file, SS_IX2_SENSE_MAP_MAX, &thresholds_max, &trows, &tcolumns);

		if (ret < 0
		    || (trows != 1
			|| tcolumns != ssCompData.header.sense_node)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits SS_IX2_SENSE_MAP_MAX failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMapFromU(ssCompData.ix2_sn, 1, ssCompData.header.sense_node, thresholds_min, thresholds_max);

		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMap SS IX2 SENSE failed... ERROR COUNT = %d \n",
				 tag, ret);
			logError(0,
				 "%s SS IX2 SENSE MIN MAX TEST:.................FAIL \n\n",
				 tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			logError(0,
				 "%s SS IX2 SENSE MIN MAX TEST:.................OK \n\n",
				 tag);

		kfree(thresholds_min);
		thresholds_min = NULL;
		kfree(thresholds_max);
		thresholds_max = NULL;
	} else
		logError(0,
			 "%s SS IX2 SENSE MIN MAX TEST:.................SKIPPED \n\n",
			 tag);


	if (todo->SelfSenseIx2Adj == 1) {
		logError(1, "%s SS IX2 SENSE ADJHORIZ TEST:  \n", tag);
		ret =
			computeAdjHorizFromU(ssCompData.ix2_sn, 1,
					     ssCompData.header.sense_node, &adjhor);

		if (ret < 0) {
			logError(1,
				 "%s production_test_data: computeAdjHoriz SS IX2 SENSE ADJH failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		logError(0, "%s SS IX2 SENSE ADJ HORIZ computed! \n", tag);
		ret = parseProductionTestLimits(path_limits, &limit_file, SS_IX2_SENSE_ADJH_MAP_MAX, &thresholds_max, &trows, &tcolumns);

		if (ret < 0
		    || (trows != 1
			|| tcolumns != ssCompData.header.sense_node - 1)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits SS_IX2_SENSE_ADJH_MAP_MAX failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMapAdj(adjhor, 1, ssCompData.header.sense_node - 1, thresholds_max);

		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMapAdj SS IX2 SENSE ADJH failed... ERROR COUNT = %d \n",
				 tag, ret);
			logError(0,
				 "%s SS IX2 SENSE ADJH TEST:.................FAIL \n\n",
				 tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			logError(0,
				 "%s SS IX2 SENSE ADJH TEST:.................OK \n\n",
				 tag);

		kfree(thresholds_max);
		thresholds_max = NULL;
		kfree(adjhor);
		adjhor = NULL;
	} else
		logError(0,
			 "%s SS IX2 SENSE ADJ TEST:.................SKIPPED  \n",
			 tag);


	if (todo->SelfSenseIxTotal == 1 || todo->SelfSenseIxTotalAdj == 1) {

		if (todo->SelfSenseIxTotal == 1) {
			logError(1, "%s SS TOTAL IX SENSE MIN MAX TEST:  \n", tag);
			ret = parseProductionTestLimits(path_limits, &limit_file, SS_TOTAL_IX_SENSE_MAP_MIN, &thresholds_min, &trows, &tcolumns);

			if (ret < 0
			    || (trows != 1
				|| tcolumns != totCompData.header.sense_node)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_TOTAL_IX_SENSE_MAP_MIN failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret = parseProductionTestLimits(path_limits, &limit_file, SS_TOTAL_IX_SENSE_MAP_MAX, &thresholds_max, &trows, &tcolumns);

			if (ret < 0
			    || (trows != 1
				|| tcolumns != totCompData.header.sense_node)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_TOTAL_IX_SENSE_MAP_MAX failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMapTotalFromU(totCompData.ix_sn, 1, totCompData.header.sense_node, thresholds_min, thresholds_max);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsMap SS TOTAL IX SENSE failed... ERROR COUNT = %d \n",
					 tag, ret);
				logError(0,
					 "%s SS TOTAL IX SENSE MIN MAX TEST:.................FAIL \n\n",
					 tag);
				count_fail += 1;

				if (stop_on_fail)
					goto ERROR;
			} else
				logError(0,
					 "%s SS TOTAL IX SENSE MIN MAX TEST:.................OK \n\n",
					 tag);

			kfree(thresholds_min);
			thresholds_min = NULL;
			kfree(thresholds_max);
			thresholds_max = NULL;
		} else
			logError(0,
				 "%s SS TOTAL IX SENSE MIN MAX TEST:.................SKIPPED  \n",
				 tag);


		if (todo->SelfSenseIxTotalAdj == 1) {
			logError(1, "%s SS TOTAL IX SENSE ADJHORIZ TEST:  \n",
				 tag);
			ret =
				computeAdjHorizTotalFromU(totCompData.ix_sn, 1,
							  totCompData.header.
							  sense_node,
							  &total_adjhor);

			if (ret < 0) {
				logError(1,
					 "%s production_test_data: computeAdjHoriz SS TOTAL IX SENSE ADJH failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			logError(0,
				 "%s SS TOTAL IX SENSE ADJ HORIZ computed! \n",
				 tag);
			ret = parseProductionTestLimits(path_limits, &limit_file, SS_TOTAL_IX_SENSE_ADJH_MAP_MAX, &thresholds_max, &trows, &tcolumns);

			if (ret < 0
			    || (trows != 1
				|| tcolumns !=
				totCompData.header.sense_node - 1)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_TOTAL_IX_SENSE_ADJH_MAP_MAX failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMapAdjTotal(total_adjhor, 1, totCompData.header.sense_node - 1, thresholds_max);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsMapAdj SS TOTAL IX SENSE ADJH failed... ERROR COUNT = %d \n",
					 tag, ret);
				logError(0,
					 "%s SS TOTAL IX SENSE ADJH TEST:.................FAIL \n\n",
					 tag);
				count_fail += 1;

				if (stop_on_fail)
					goto ERROR;
			} else
				logError(0,
					 "%s SS TOTAL IX SENSE ADJH TEST:.................OK \n\n",
					 tag);

			kfree(thresholds_max);
			thresholds_max = NULL;
			kfree(total_adjhor);
			total_adjhor = NULL;
		} else
			logError(0,
				 "%s SS TOTAL IX SENSE ADJ TEST:.................SKIPPED  \n",
				 tag);
	} else
		logError(0,
			 "%s SS TOTAL IX SENSE TEST:.................SKIPPED  \n",
			 tag);

	/********************************************************* SS SENSE CX ****************************************************************/

	if (todo->SelfForceCx1 == 1) {
		logError(1, "%s SS CX1 FORCE TEST:  \n", tag);
		ret =
			parseProductionTestLimits(path_limits, &limit_file,
						  SS_CX1_FORCE_MIN_MAX, &thresholds,
						  &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != 2)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits SS_CX1_FORCE_MIN_MAX failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		container = (short)ssCompData.f_cx1;
		ret =
			checkLimitsMinMax(&container, 1, 1, thresholds[0],
					  thresholds[1]);

		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMinMax SS CX1 FORCE TEST failed... ERROR COUNT = %d \n",
				 tag, ret);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			logError(0,
				 "%s SS CX1 FORCE TEST:.................OK \n\n",
				 tag);

		kfree(thresholds);
		thresholds = NULL;
	} else
		logError(0,
			 "%s SS CX1 FORCE TEST:.................SKIPPED \n\n",
			 tag);


	if (todo->SelfForceCx2 == 1) {
		logError(1, "%s SS CX2 FORCE MIN MAX TEST:  \n", tag);
		ret = parseProductionTestLimits(path_limits, &limit_file, SS_CX2_FORCE_MAP_MIN, &thresholds_min, &trows, &tcolumns);

		if (ret < 0
		    || (trows != ssCompData.header.force_node
			|| tcolumns != 1)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits SS_CX2_FORCE_MAP_MIN failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = parseProductionTestLimits(path_limits, &limit_file, SS_CX2_FORCE_MAP_MAX, &thresholds_max, &trows, &tcolumns);

		if (ret < 0
		    || (trows != ssCompData.header.force_node
			|| tcolumns != 1)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits SS_CX2_FORCE_MAP_MAX failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMap(ssCompData.cx2_fm, ssCompData.header.force_node, 1, thresholds_min, thresholds_max);

		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMap SS CX2 FORCE failed... ERROR COUNT = %d \n",
				 tag, ret);
			logError(0,
				 "%s SS CX2 FORCE MIN MAX TEST:.................FAIL \n\n",
				 tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			logError(0,
				 "%s SS CX2 FORCE MIN MAX TEST:.................OK \n\n",
				 tag);

		kfree(thresholds_min);
		thresholds_min = NULL;
		kfree(thresholds_max);
		thresholds_max = NULL;
	} else
		logError(0,
			 "%s SS CX2 FORCE MIN MAX TEST:.................SKIPPED  \n",
			 tag);


	if (todo->SelfForceCx2Adj == 1) {
		logError(1, "%s SS CX2 FORCE ADJVERT TEST:  \n", tag);
		ret = computeAdjVert(ssCompData.cx2_fm, ssCompData.header.force_node, 1, &adjvert);

		if (ret < 0) {
			logError(1,
				 "%s production_test_data: computeAdjVert SS CX2 FORCE ADJV failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		logError(0, "%s SS CX2 FORCE ADJV computed! \n", tag);
		ret = parseProductionTestLimits(path_limits, &limit_file, SS_CX2_FORCE_ADJV_MAP_MAX, &thresholds_max, &trows, &tcolumns);

		if (ret < 0
		    || (trows != ssCompData.header.force_node - 1
			|| tcolumns != 1)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits SS_CX2_FORCE_ADJV_MAP_MAX failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMapAdj(adjvert, ssCompData.header.force_node - 1, 1, thresholds_max);

		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMap SS IX2 FORCE failed... ERROR COUNT = %d \n",
				 tag, ret);
			logError(0,
				 "%s SS CX2 FORCE ADJV TEST:.................FAIL \n\n",
				 tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			logError(0,
				 "%s SS CX2 FORCE ADJV TEST:.................OK \n\n",
				 tag);

		kfree(thresholds_max);
		thresholds_max = NULL;
		kfree(adjvert);
		adjvert = NULL;
	} else
		logError(0,
			 "%s SS CX2 FORCE ADJ TEST:.................SKIPPED \n\n",
			 tag);


	if (todo->SelfForceCxTotal == 1 || todo->SelfForceCxTotalAdj == 1) {

		if (todo->SelfForceCxTotal == 1) {
			logError(1, "%s SS TOTAL CX FORCE MIN MAX TEST:  \n", tag);
			ret = parseProductionTestLimits(path_limits, &limit_file, SS_TOTAL_CX_FORCE_MAP_MIN, &thresholds_min, &trows, &tcolumns);

			if (ret < 0
			    || (trows != totCompData.header.force_node
				|| tcolumns != 1)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_TOTAL_CX_FORCE_MAP_MIN failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret = parseProductionTestLimits(path_limits, &limit_file, SS_TOTAL_CX_FORCE_MAP_MAX, &thresholds_max, &trows, &tcolumns);

			if (ret < 0
			    || (trows != totCompData.header.force_node
				|| tcolumns != 1)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_TOTAL_CX_FORCE_MAP_MAX failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMapTotal(totCompData.cx_fm, totCompData.header.force_node, 1, thresholds_min, thresholds_max);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsMap SS TOTAL FORCE failed... ERROR COUNT = %d \n",
					 tag, ret);
				logError(0,
					 "%s SS TOTAL FORCE MIN MAX TEST:.................FAIL \n\n",
					 tag);
				count_fail += 1;

				if (stop_on_fail)
					goto ERROR;
			} else
				logError(0,
					 "%s SS TOTAL FORCE MIN MAX TEST:.................OK \n\n",
					 tag);

			kfree(thresholds_min);
			thresholds_min = NULL;
			kfree(thresholds_max);
			thresholds_max = NULL;
		} else
			logError(0,
				 "%s SS TOTAL CX FORCE MIN MAX TEST:.................SKIPPED  \n",
				 tag);


		if (todo->SelfForceCxTotalAdj == 1) {
			logError(1, "%s SS TOTAL CX FORCE ADJVERT TEST:  \n",
				 tag);
			ret = computeAdjVertTotal(totCompData.cx_fm, totCompData.header.force_node, 1, &total_adjvert);

			if (ret < 0) {
				logError(1,
					 "%s production_test_data: computeAdjVert SS TOTAL CX FORCE ADJV failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			logError(0, "%s SS TOTAL CX FORCE ADJV computed! \n",
				 tag);
			ret =
				parseProductionTestLimits(path_limits, &limit_file,
							  SS_TOTAL_CX_FORCE_ADJV_MAP_MAX,
							  &thresholds_max, &trows,
							  &tcolumns);

			if (ret < 0
			    || (trows != totCompData.header.force_node - 1
				|| tcolumns != 1)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_TOTAL_CX_FORCE_ADJV_MAP_MAX failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret =
				checkLimitsMapAdjTotal(total_adjvert,
						       totCompData.
						       header.force_node - 1, 1,
						       thresholds_max);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsMap SS TOTAL CX FORCE failed... ERROR COUNT = %d \n",
					 tag, ret);
				logError(0,
					 "%s SS TOTAL CX FORCE ADJV TEST:.................FAIL \n\n",
					 tag);
				count_fail += 1;

				if (stop_on_fail)
					goto ERROR;
			} else
				logError(0,
					 "%s SS TOTAL CX FORCE ADJV TEST:.................OK \n\n",
					 tag);

			kfree(thresholds_max);
			thresholds_max = NULL;
			kfree(total_adjvert);
			total_adjvert = NULL;
		} else
			logError(0,
				 "%s SS TOTAL CX FORCE ADJ TEST:.................SKIPPED  \n",
				 tag);
	} else
		logError(0,
			 "%s SS TOTAL CX FORCE TEST:.................SKIPPED \n\n",
			 tag);

	/********************************************************* SS SENSE CX ****************************************************************/

	if (todo->SelfSenseCx1 == 1) {
		logError(1, "%s SS CX1 SENSE TEST:  \n", tag);
		ret =
			parseProductionTestLimits(path_limits, &limit_file,
						  SS_CX1_SENSE_MIN_MAX, &thresholds,
						  &trows, &tcolumns);

		if (ret < 0 || (trows != 1 || tcolumns != 2)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits SS_CX1_SENSE_MIN_MAX failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		container = (short)ssCompData.s_cx1;
		ret =
			checkLimitsMinMax(&container, 1, 1, thresholds[0],
					  thresholds[1]);

		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMinMax SS CX1 SENSE TEST failed... ERROR COUNT = %d \n",
				 tag, ret);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			logError(0,
				 "%s SS CX1 SENSE TEST:.................OK \n\n",
				 tag);

		kfree(thresholds);
		thresholds = NULL;
	} else
		logError(0,
			 "%s SS CX1 SENSE TEST:.................SKIPPED \n\n",
			 tag);


	if (todo->SelfSenseCx2 == 1) {
		logError(1, "%s SS CX2 SENSE MIN MAX TEST:  \n", tag);
		ret = parseProductionTestLimits(path_limits, &limit_file, SS_CX2_SENSE_MAP_MIN, &thresholds_min, &trows, &tcolumns);

		if (ret < 0
		    || (trows != 1
			|| tcolumns != ssCompData.header.sense_node)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits SS_CX2_SENSE_MAP_MIN failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = parseProductionTestLimits(path_limits, &limit_file, SS_CX2_SENSE_MAP_MAX, &thresholds_max, &trows, &tcolumns);

		if (ret < 0
		    || (trows != 1
			|| tcolumns != ssCompData.header.sense_node)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits SS_CX2_SENSE_MAP_MAX failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMap(ssCompData.cx2_sn, 1, ssCompData.header.sense_node, thresholds_min, thresholds_max);

		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMap SS CX2 SENSE failed... ERROR COUNT = %d \n",
				 tag, ret);
			logError(0,
				 "%s SS CX2 SENSE MIN MAX TEST:.................FAIL \n\n",
				 tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			logError(0,
				 "%s SS CX2 SENSE MIN MAX TEST:.................OK \n\n",
				 tag);

		kfree(thresholds_min);
		thresholds_min = NULL;
		kfree(thresholds_max);
		thresholds_max = NULL;
	} else
		logError(0,
			 "%s SS CX2 SENSE MIN MAX TEST:.................SKIPPED  \n",
			 tag);


	if (todo->SelfSenseCx2Adj == 1) {
		logError(1, "%s SS CX2 SENSE ADJHORIZ TEST:  \n", tag);
		ret =
			computeAdjHoriz(ssCompData.cx2_sn, 1,
					ssCompData.header.sense_node, &adjhor);

		if (ret < 0) {
			logError(1,
				 "%s production_test_data: computeAdjHoriz SS CX2 SENSE ADJH failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		logError(0, "%s SS CX2 SENSE ADJH computed! \n", tag);
		ret = parseProductionTestLimits(path_limits, &limit_file, SS_CX2_SENSE_ADJH_MAP_MAX, &thresholds_max, &trows, &tcolumns);

		if (ret < 0
		    || (trows != 1
			|| tcolumns != ssCompData.header.sense_node - 1)) {
			logError(1,
				 "%s production_test_data: parseProductionTestLimits SS_IX2_SENSE_MAP_MAX failed... ERROR %08X \n",
				 tag, ERROR_PROD_TEST_DATA);
			ret |= ERROR_PROD_TEST_DATA;
			goto ERROR_LIMITS;
		}

		ret = checkLimitsMapAdj(adjhor, 1, ssCompData.header.sense_node - 1, thresholds_max);

		if (ret != OK) {
			logError(1,
				 "%s production_test_data: checkLimitsMapAdj SS CX2 SENSE ADJH failed... ERROR COUNT = %d \n",
				 tag, ret);
			logError(0,
				 "%s SS CX2 SENSE ADJH TEST:.................FAIL \n\n",
				 tag);
			count_fail += 1;

			if (stop_on_fail)
				goto ERROR;
		} else
			logError(0,
				 "%s SS CX2 SENSE ADJH TEST:.................OK \n",
				 tag);

		kfree(thresholds_max);
		thresholds_max = NULL;
		kfree(adjhor);
		adjhor = NULL;
	} else
		logError(0,
			 "%s SS CX2 SENSE ADJ TEST:.................SKIPPED \n\n",
			 tag);


	if (todo->SelfSenseCxTotal == 1 || todo->SelfSenseCxTotalAdj == 1) {

		if (todo->SelfSenseCxTotal == 1) {
			logError(1, "%s SS TOTAL CX SENSE MIN MAX TEST:  \n", tag);
			ret = parseProductionTestLimits(path_limits, &limit_file, SS_TOTAL_CX_SENSE_MAP_MIN, &thresholds_min, &trows, &tcolumns);

			if (ret < 0
			    || (trows != 1
				|| tcolumns != totCompData.header.sense_node)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_TOTAL_CX_SENSE_MAP_MIN failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret = parseProductionTestLimits(path_limits, &limit_file, SS_TOTAL_CX_SENSE_MAP_MAX, &thresholds_max, &trows, &tcolumns);

			if (ret < 0
			    || (trows != 1
				|| tcolumns != totCompData.header.sense_node)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_TOTAL_CX_SENSE_MAP_MAX failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMapTotal(totCompData.cx_sn, 1, totCompData.header.sense_node, thresholds_min, thresholds_max);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsMap SS TOTAL CX SENSE failed... ERROR COUNT = %d \n",
					 tag, ret);
				logError(0,
					 "%s SS TOTAL CX SENSE MIN MAX TEST:.................FAIL \n\n",
					 tag);
				count_fail += 1;

				if (stop_on_fail)
					goto ERROR;
			} else
				logError(0,
					 "%s SS TOTAL CX SENSE MIN MAX TEST:.................OK \n\n",
					 tag);

			kfree(thresholds_min);
			thresholds_min = NULL;
			kfree(thresholds_max);
			thresholds_max = NULL;
		} else
			logError(0,
				 "%s SS TOTAL CX SENSE MIN MAX TEST:.................SKIPPED  \n",
				 tag);


		if (todo->SelfSenseCxTotalAdj == 1) {
			logError(1, "%s SS TOTAL CX SENSE ADJHORIZ TEST:  \n",
				 tag);
			ret =
				computeAdjHorizTotal(totCompData.cx_sn, 1,
						     totCompData.header.sense_node,
						     &total_adjhor);

			if (ret < 0) {
				logError(1,
					 "%s production_test_data: computeAdjHoriz SS TOTAL CX SENSE ADJH failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			logError(0,
				 "%s SS TOTAL CX SENSE ADJ HORIZ computed! \n",
				 tag);
			ret = parseProductionTestLimits(path_limits, &limit_file, SS_TOTAL_CX_SENSE_ADJH_MAP_MAX, &thresholds_max, &trows, &tcolumns);

			if (ret < 0
			    || (trows != 1
				|| tcolumns !=
				totCompData.header.sense_node - 1)) {
				logError(1,
					 "%s production_test_data: parseProductionTestLimits SS_TOTAL_CX_SENSE_ADJH_MAP_MAX failed... ERROR %08X \n",
					 tag, ERROR_PROD_TEST_DATA);
				ret |= ERROR_PROD_TEST_DATA;
				goto ERROR_LIMITS;
			}

			ret = checkLimitsMapAdjTotal(total_adjhor, 1, totCompData.header.sense_node - 1, thresholds_max);

			if (ret != OK) {
				logError(1,
					 "%s production_test_data: checkLimitsMapAdj SS TOTAL CX SENSE ADJH failed... ERROR COUNT = %d \n",
					 tag, ret);
				logError(0,
					 "%s SS TOTAL CX SENSE ADJH TEST:.................FAIL \n\n",
					 tag);
				count_fail += 1;

				if (stop_on_fail)
					goto ERROR;
			} else
				logError(0,
					 "%s SS TOTAL CX SENSE ADJH TEST:.................OK \n\n",
					 tag);

			kfree(thresholds_max);
			thresholds_max = NULL;
			kfree(total_adjhor);
			total_adjhor = NULL;
		} else
			logError(0,
				 "%s SS TOTAL CX SENSE ADJ TEST:.................SKIPPED  \n",
				 tag);
	} else
		logError(0,
			 "%s SS TOTAL CX SENSE TEST:.................SKIPPED  \n",
			 tag);
	if (todo->SelfHoverForceIxTotal == 1 || todo->SelfHoverSenceIxTotal == 1) {
		ret = production_test_ss_hover_ix(path_limits, stop_on_fail, todo);
		if (ret < OK) {
			logError(1,
				 "%s production_test_data: production_test_ss_hover_ix_cx failed... ERROR = %08X\n",
				 tag, ret);
			count_fail += 1;
		}
	}

ERROR:
	logError(0, "%s \n", tag);

	if (count_fail == 0) {
		kfree(ssCompData.ix2_fm);
		ssCompData.ix2_fm = NULL;
		kfree(ssCompData.ix2_sn);
		ssCompData.ix2_sn = NULL;
		kfree(ssCompData.cx2_fm);
		ssCompData.cx2_fm = NULL;
		kfree(ssCompData.cx2_sn);
		ssCompData.cx2_sn = NULL;
		kfree(totCompData.ix_fm);
		totCompData.ix_fm = NULL;
		kfree(totCompData.ix_sn);
		totCompData.ix_sn = NULL;
		kfree(totCompData.cx_fm);
		totCompData.cx_fm = NULL;
		kfree(totCompData.cx_sn);
		totCompData.cx_sn = NULL;
		logError(0,
			 "%s SS IX CX testes finished!.................OK\n\n",
			 tag);
		return OK;
	} else {
		print_frame_u8("SS Init Data Ix2_fm = ",
			       array1dTo2d_u8(ssCompData.ix2_fm,
					      ssCompData.header.force_node, 1),
			       ssCompData.header.force_node, 1);
		print_frame_i8("SS Init Data Cx2_fm = ",
			       array1dTo2d_i8(ssCompData.cx2_fm,
					      ssCompData.header.force_node, 1),
			       ssCompData.header.force_node, 1);
		print_frame_u8("SS Init Data Ix2_sn = ",
			       array1dTo2d_u8(ssCompData.ix2_sn,
					      ssCompData.header.sense_node,
					      ssCompData.header.sense_node), 1,
			       ssCompData.header.sense_node);
		print_frame_i8("SS Init Data Cx2_sn = ",
			       array1dTo2d_i8(ssCompData.cx2_sn,
					      ssCompData.header.sense_node,
					      ssCompData.header.sense_node), 1,
			       ssCompData.header.sense_node);
		print_frame_u16("TOT SS Init Data Ix_fm = ",
				array1dTo2d_u16(totCompData.ix_fm,
						totCompData.header.force_node,
						1),
				totCompData.header.force_node, 1);
		print_frame_short("TOT SS Init Data Cx_fm = ",
				  array1dTo2d_short(totCompData.cx_fm,
						    totCompData.header.
						    force_node, 1),
				  totCompData.header.force_node, 1);
		print_frame_u16("TOT SS Init Data Ix_sn = ",
				array1dTo2d_u16(totCompData.ix_sn,
						totCompData.header.sense_node,
						totCompData.header.sense_node),
				1, totCompData.header.sense_node);
		print_frame_short("TOT SS Init Data Cx_sn = ",
				  array1dTo2d_short(totCompData.cx_sn,
						    totCompData.header.
						    sense_node,
						    totCompData.header.
						    sense_node), 1,
				  totCompData.header.sense_node);
		logError(0,
			 "%s SS IX CX testes finished!.................FAILED  fails_count = %d\n\n",
			 tag, count_fail);

		if (thresholds != NULL)
			kfree(thresholds);

		if (thresholds_min != NULL)
			kfree(thresholds_min);

		if (thresholds_max != NULL)
			kfree(thresholds_max);

		if (adjhor != NULL)
			kfree(adjhor);

		if (adjvert != NULL)
			kfree(adjvert);

		if (total_adjhor != NULL)
			kfree(total_adjhor);

		if (total_adjvert != NULL)
			kfree(total_adjvert);

		if (ssCompData.ix2_fm != NULL)
			kfree(ssCompData.ix2_fm);

		if (ssCompData.ix2_sn != NULL)
			kfree(ssCompData.ix2_sn);

		if (ssCompData.cx2_fm != NULL)
			kfree(ssCompData.cx2_fm);

		if (ssCompData.cx2_sn != NULL)
			kfree(ssCompData.cx2_sn);

		if (totCompData.ix_fm != NULL)
			kfree(totCompData.ix_fm);

		if (totCompData.ix_sn != NULL)
			kfree(totCompData.ix_sn);

		if (totCompData.cx_fm != NULL)
			kfree(totCompData.cx_fm);

		if (totCompData.cx_sn != NULL)
			kfree(totCompData.cx_sn);

		return (ERROR_TEST_CHECK_FAIL | ERROR_PROD_TEST_DATA);
	}

ERROR_LIMITS:

	if (thresholds != NULL)
		kfree(thresholds);

	if (thresholds_min != NULL)
		kfree(thresholds_min);

	if (thresholds_max != NULL)
		kfree(thresholds_max);

	if (adjhor != NULL)
		kfree(adjhor);

	if (adjvert != NULL)
		kfree(adjvert);

	if (total_adjhor != NULL)
		kfree(total_adjhor);

	if (total_adjvert != NULL)
		kfree(total_adjvert);

	if (ssCompData.ix2_fm != NULL)
		kfree(ssCompData.ix2_fm);

	if (ssCompData.ix2_sn != NULL)
		kfree(ssCompData.ix2_sn);

	if (ssCompData.cx2_fm != NULL)
		kfree(ssCompData.cx2_fm);

	if (ssCompData.cx2_sn != NULL)
		kfree(ssCompData.cx2_sn);

	if (totCompData.ix_fm != NULL)
		kfree(totCompData.ix_fm);

	if (totCompData.ix_sn != NULL)
		kfree(totCompData.ix_sn);

	if (totCompData.cx_fm != NULL)
		kfree(totCompData.cx_fm);

	if (totCompData.cx_sn != NULL)
		kfree(totCompData.cx_sn);

	return ret;
}

/**
 * Perform a complete Data Test check of the IC
 * @param path_limits name of Production Limit file to load or "NULL" if the limits data should be loaded by a .h file
 * @param stop_on_fail if 1, the test flow stops at the first data check failure otherwise it keeps going performing all the selected test
 * @param todo pointer to a TestToDo variable which select the test to do
 * @return OK if success or an error code which specify the type of error encountered
 */
int production_test_data(const char *path_limits, int stop_on_fail, TestToDo *todo)
{
	int res = OK, ret;

	if (todo == NULL) {
		logError(1,
			 "%s production_test_data: No TestToDo specified!! ERROR = %08X \n",
			 tag, (ERROR_OP_NOT_ALLOW | ERROR_PROD_TEST_DATA));
		return (ERROR_OP_NOT_ALLOW | ERROR_PROD_TEST_DATA);
	}

	logError(1, "%s DATA Production test is starting...,path_limits:%s\n", tag, path_limits);
	ret = production_test_ms_raw(path_limits, stop_on_fail, todo);
	res |= ret;

	if (ret < 0) {
		logError(1,
			 "%s production_test_data: production_test_ms_raw failed... ERROR = %08X \n",
			 tag, ret);

		if (stop_on_fail == 1)
			goto END;
	}

	ret = production_test_ms_cx(path_limits, stop_on_fail, todo);
	res |= ret;

	if (ret < 0) {
		logError(1,
			 "%s production_test_data: production_test_ms_cx failed... ERROR = %08X \n",
			 tag, ret);

		if (stop_on_fail == 1)
			goto END;
	}

	ret = production_test_ss_raw(path_limits, stop_on_fail, todo);
	res |= ret;

	if (ret < 0) {
		logError(1,
			 "%s production_test_data: production_test_ss_raw failed... ERROR = %08X \n",
			 tag, ret);

		if (stop_on_fail == 1)
			goto END;
	}

	ret = production_test_ss_ix_cx(path_limits, stop_on_fail, todo);
	res |= ret;

	if (ret < 0) {
		logError(1,
			 "%s production_test_data: production_test_ss_ix_cx failed... ERROR = %08X \n",
			 tag, ret);

		if (stop_on_fail == 1)
			goto END;
	}

END:
	freeLimitsFile(&limit_file);

	if (res < OK)
		logError(0, "%s DATA Production test failed!\n", tag);
	else
		logError(0, "%s DATA Production test finished!\n", tag);

	return res;
}

/**
 * Retrieve the actual Test Limit data from the system (bin file or header file)
 * @param path name of Production Test Limit file to load or "NULL" if the limits data should be loaded by a .h file
 * @param file pointer to the LimitFile struct which will contains the limits data
 * @return OK if success or an error code which specify the type of error encountered
 */
int getLimitsFile(const char *path, LimitFile *file)
{
	const struct firmware *fw = NULL;
	struct device *dev = NULL;
	int fd = -1;
	logError(0, "%s Get Limits File starting... %s\n", tag, path);

	if (file->data != NULL) {
		logError(0,
			 "%s Pointer to Limits Data already contains something... freeing its content!\n",
			 tag);
		kfree(file->data);
		file->data = NULL;
		file->size = 0;
	}

	strlcpy(file->name, path, MAX_LIMIT_FILE_NAME);

	if (strncmp(path, "NULL", 4) == 0) {
#ifdef LIMITS_H_FILE
		logError(0, "%s Loading Limits File from .h!\n", tag);
		file->size = LIMITS_SIZE_NAME;
		file->data =
			(char *)kmalloc((file->size) * sizeof(char), GFP_KERNEL);

		if (file->data != NULL) {
			memcpy(file->data, (char *)(LIMITS_ARRAY_NAME),
			       file->size);
			return OK;
		} else {
			logError(1,
				 "%s Error while allocating data... ERROR %08X \n",
				 tag, path, ERROR_ALLOC);
			return ERROR_ALLOC;
		}

#else
		logError(1, "%s limit file path NULL... ERROR %08X \n", tag,
			 ERROR_FILE_NOT_FOUND);
		return ERROR_FILE_NOT_FOUND;
#endif
	} else {
		dev = getDev();

		if (dev != NULL) {
			logError(0, "%s Loading Limits File from .csv!\n", tag);
			fd = request_firmware(&fw, path, dev);

			if (fd == 0) {
				logError(0, "%s Start to copy %s...\n", tag,
					 path);
				file->size = fw->size;
				file->data =
					(char *)kmalloc((file->size) * sizeof(char),
							GFP_KERNEL);

				if (file->data != NULL) {
					memcpy(file->data, (char *)fw->data,
					       file->size);
					logError(0,
						 "%s Limit file Size = %d \n",
						 tag, file->size);
					release_firmware(fw);
					return OK;
				} else {
					logError(1,
						 "%s Error while allocating data... ERROR %08X \n",
						 tag, path, ERROR_ALLOC);
					release_firmware(fw);
					return ERROR_ALLOC;
				}
			} else {
				logError(1,
					 "%s Request the file %s failed... ERROR %08X \n",
					 tag, path, ERROR_FILE_NOT_FOUND);
				return ERROR_FILE_NOT_FOUND;
			}
		} else {
			logError(1,
				 "%s Error while getting the device ERROR %08X \n",
				 tag, ERROR_FILE_READ);
			return ERROR_FILE_READ;
		}
	}
}

/**
 * Reset and release the memory which store a Production Limit File previously loaded
 * @param file pointer to the LimitFile struct to free
 * @return OK if success or an error code which specify the type of error encountered
 */

int freeLimitsFile(LimitFile *file)
{
	logError(0, "%s Freeing Limit File ...\n", tag);

	if (file != NULL) {
		if (file->data != NULL) {
			kfree(file->data);
			file->data = NULL;
		} else
			logError(0, "%s Limit File was already freed!\n", tag);

		file->size = 0;
		strlcpy(file->name, " ", MAX_LIMIT_FILE_NAME);
		return OK;
	} else {
		logError(1, "%s Passed a NULL argument! ERROR %08X \n", tag,
			 ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}
}

/**
 * Reset and release the memory which store the current Limit File previously loaded
 * @return OK if success or an error code which specify the type of error encountered
 */

int freeCurrentLimitsFile(void)
{
	return freeLimitsFile(&limit_file);
}

/**
 * Parse the raw data read from a Production test limit file in order to find the specified information
 * If no limits file data are passed, the function loads and stores the limit file from the system
 * @param path name of Production Test Limit file to load or "NULL" if the limits data should be loaded by a .h file
 * @param file pointer to LimitFile struct that should be parsed or NULL if the limit file in the system should be loaded and then parsed
 * @param label string which identify a particular set of data in the file that want to be loaded
 * @param data pointer to the pointer which will contains the specified limits data as 1 dimension matrix with data arranged row after row
 * @param row pointer to a int variable which will contain the number of row of data
 * @param column pointer to a int variable which will contain the number of column of data
 * @return OK if success or an error code which specify the type of error encountered
 */
int parseProductionTestLimits(const char *path, LimitFile *file, char *label,
			      int **data, int *row, int *column)
{
	int find = 0;
	char *token = NULL;
	int i = 0;
	int j = 0;
	int z = 0;
	char *line2 = NULL;
	char line[800];
	char *buf = NULL;
	int n, size, pointer = 0, ret = OK;
	char *data_file = NULL;

	if (file == NULL || strcmp(path, file->name) != 0 || file->size == 0) {
		logError(0,
			 "%s No limit File data passed... try to get them from the system!\n",
			 tag);
		ret = getLimitsFile(path, &limit_file);

		if (ret < OK) {
			logError(1,
				 "%s parseProductionTestLimits: ERROR %08X\n",
				 tag, ERROR_FILE_NOT_FOUND);
			return ERROR_FILE_NOT_FOUND;
		}

		size = limit_file.size;
		data_file = limit_file.data;
	} else {
		logError(0, "%s Limit File data passed as arguments!\n", tag);
		size = file->size;
		data_file = file->data;
	}

	logError(0, "%s The size of the limits file is %d bytes...\n", tag,
		 size);

	while (find == 0) {
		if (readLine(&data_file[pointer], line, size - pointer, &n) < 0) {
			find = -1;
			break;
		}

		pointer += n;

		if (line[0] == '*') {
			line2 = kstrdup(line, GFP_KERNEL);

			if (line2 == NULL) {
				logError(1,
					 "%s parseProductionTestLimits: kstrdup ERROR %08X\n",
					 tag, ERROR_ALLOC);
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
					logError(0, "%s Row = %d\n", tag, *row);
				} else {
					logError(1,
						 "%s parseProductionTestLimits 1: ERROR %08X\n",
						 tag, ERROR_FILE_PARSE);
					ret = ERROR_FILE_PARSE;
					goto END;
				}

				token = strsep(&line2, ",");

				if (token != NULL) {
					sscanf(token, "%d", column);
					logError(0, "%s Column = %d\n", tag,
						 *column);
				} else {
					logError(1,
						 "%s parseProductionTestLimits 2: ERROR %08X\n",
						 tag, ERROR_FILE_PARSE);
					ret = ERROR_FILE_PARSE;
					goto END;
				}

				kfree(buf);
				buf = NULL;
				*data = (int *)kmalloc(((*row) * (*column)) * sizeof(int), GFP_KERNEL);
				j = 0;

				if (*data == NULL) {
					logError(1,
						 "%s parseProductionTestLimits: ERROR %08X\n",
						 tag, ERROR_ALLOC);
					ret = ERROR_ALLOC;
					goto END;
				}

				for (i = 0; i < *row; i++) {
					if (readLine
					    (&data_file[pointer], line,
					     size - pointer, &n) < 0) {
						logError(1,
							 "%s parseProductionTestLimits : ERROR %08X\n",
							 tag, ERROR_FILE_READ);
						ret = ERROR_FILE_READ;
						goto END;
					}

					pointer += n;
					line2 = kstrdup(line, GFP_KERNEL);

					if (line2 == NULL) {
						logError(1,
							 "%s parseProductionTestLimits: kstrdup ERROR %08X\n",
							 tag, ERROR_ALLOC);
						ret = ERROR_ALLOC;
						goto END;
					}

					buf = line2;
					token = strsep(&line2, ",");

					for (z = 0;
					     (z < *column) && (token != NULL);
					     z++) {
						sscanf(token, "%d",
						       ((*data) + j));
						j++;
						token = strsep(&line2, ",");
					}

					kfree(buf);
					buf = NULL;
				}

				if (j == ((*row) * (*column))) {
					logError(0, "%s READ DONE!\n", tag);
					ret = OK;
					goto END;
				}

				logError(1,
					 "%s parseProductionTestLimits 3: ERROR %08X\n",
					 tag, ERROR_FILE_PARSE);
				ret = ERROR_FILE_PARSE;
				goto END;
			}

			kfree(buf);
			buf = NULL;
		}
	}

	logError(1, "%s parseProductionTestLimits: ERROR %08X\n", tag,
		 ERROR_LABEL_NOT_FOUND);
	ret = ERROR_LABEL_NOT_FOUND;
END:

	if (buf != NULL)
		kfree(buf);

	return ret;
}

/**
 * Read one line of a text file passed as array of byte and terminate it with a termination character '\0'
 * @param data text file as array of bytes
 * @param line pointer to an array of char that will contain the line read
 * @param size size of data
 * @param n pointer to a int variable which will contain the number of characters of the line
 * @return OK if success or an error code which specify the type of error encountered
 */
int readLine(char *data, char *line, int size, int *n)
{
	int i = 0;

	if (size < 1)
		return -EINVAL;

	while (data[i] != '\n' && i < size) {
		line[i] = data[i];
		i++;
	}

	*n = i + 1;
	line[i] = '\0';
	return OK;
}
