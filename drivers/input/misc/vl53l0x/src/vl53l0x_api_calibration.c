/*
 *  vl53l0x_api_calibration.c - Linux kernel modules for
 *  STM VL53L0 FlightSense TOF sensor
 *
 *  Copyright (C) 2016 STMicroelectronics Imaging Division.
 *  Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */


#include "vl53l0x_api.h"
#include "vl53l0x_api_core.h"
#include "vl53l0x_api_calibration.h"

#ifndef __KERNEL__
#include <stdlib.h>
#endif

#define LOG_FUNCTION_START(fmt, ...) \
	_LOG_FUNCTION_START(TRACE_MODULE_API, fmt, ##__VA_ARGS__)
#define LOG_FUNCTION_END(status, ...) \
	_LOG_FUNCTION_END(TRACE_MODULE_API, status, ##__VA_ARGS__)
#define LOG_FUNCTION_END_FMT(status, fmt, ...) \
	_LOG_FUNCTION_END_FMT(TRACE_MODULE_API, status, fmt, ##__VA_ARGS__)

#define REF_ARRAY_SPAD_0  0
#define REF_ARRAY_SPAD_5  5
#define REF_ARRAY_SPAD_10 10

uint32_t refArrayQuadrants[4] = {REF_ARRAY_SPAD_10, REF_ARRAY_SPAD_5,
		REF_ARRAY_SPAD_0, REF_ARRAY_SPAD_5 };

int8_t VL_perform_xtalk_calibration(struct vl_data *Dev,
			unsigned int XTalkCalDistance,
			unsigned int *pXTalkCompensationRateMegaCps)
{
	int8_t Status = VL_ERROR_NONE;
	uint16_t sum_ranging = 0;
	uint16_t sum_spads = 0;
	unsigned int sum_signalRate = 0;
	unsigned int total_count = 0;
	uint8_t xtalk_meas = 0;
	struct VL_RangingMeasurementData_t RangingMeasurementData;
	unsigned int xTalkStoredMeanSignalRate;
	unsigned int xTalkStoredMeanRange;
	unsigned int xTalkStoredMeanRtnSpads;
	uint32_t signalXTalkTotalPerSpad;
	uint32_t xTalkStoredMeanRtnSpadsAsInt;
	uint32_t xTalkCalDistanceAsInt;
	unsigned int XTalkCompensationRateMegaCps;

	if (XTalkCalDistance <= 0)
		Status = VL_ERROR_INVALID_PARAMS;

	/* Disable the XTalk compensation */
	if (Status == VL_ERROR_NONE)
		Status = VL_SetXTalkCompensationEnable(Dev, 0);

	/* Disable the RIT */
	if (Status == VL_ERROR_NONE) {
		Status = VL_SetLimitCheckEnable(Dev,
				VL_CHECKENABLE_RANGE_IGNORE_THRESHOLD, 0);
	}

	/* Perform 50 measurements and compute the averages */
	if (Status == VL_ERROR_NONE) {
		sum_ranging = 0;
		sum_spads = 0;
		sum_signalRate = 0;
		total_count = 0;
		for (xtalk_meas = 0; xtalk_meas < 50; xtalk_meas++) {
			Status = VL_PerformSingleRangingMeasurement(Dev,
				&RangingMeasurementData);

			if (Status != VL_ERROR_NONE)
				break;

			/* The range is valid when RangeStatus = 0 */
			if (RangingMeasurementData.RangeStatus == 0) {
				sum_ranging = sum_ranging +
					RangingMeasurementData.RangeMilliMeter;
				sum_signalRate = sum_signalRate +
				RangingMeasurementData.SignalRateRtnMegaCps;
				sum_spads = sum_spads +
				RangingMeasurementData.EffectiveSpadRtnCount
					/ 256;
				total_count = total_count + 1;
			}
		}

		/* no valid values found */
		if (total_count == 0)
			Status = VL_ERROR_RANGE_ERROR;

	}


	if (Status == VL_ERROR_NONE) {
		/* unsigned int / uint16_t = unsigned int */
		xTalkStoredMeanSignalRate = sum_signalRate / total_count;
		xTalkStoredMeanRange = (unsigned int)((uint32_t)(
			sum_ranging << 16) / total_count);
		xTalkStoredMeanRtnSpads = (unsigned int)((uint32_t)(
			sum_spads << 16) / total_count);

		/* Round Mean Spads to Whole Number.
		 * Typically the calculated mean SPAD count is a whole number
		 * or very close to a whole
		 * number, therefore any truncation will not result in a
		 * significant loss in accuracy.
		 * Also, for a grey target at a typical distance of around
		 * 400mm, around 220 SPADs will
		 * be enabled, therefore, any truncation will result in a loss
		 * of accuracy of less than
		 * 0.5%.
		 */
		xTalkStoredMeanRtnSpadsAsInt = (xTalkStoredMeanRtnSpads +
			0x8000) >> 16;

		/* Round Cal Distance to Whole Number.
		 * Note that the cal distance is in mm, therefore no resolution
		 * is lost.*/
		 xTalkCalDistanceAsInt = (XTalkCalDistance + 0x8000) >> 16;

		if (xTalkStoredMeanRtnSpadsAsInt == 0 ||
		   xTalkCalDistanceAsInt == 0 ||
		   xTalkStoredMeanRange >= XTalkCalDistance) {
			XTalkCompensationRateMegaCps = 0;
		} else {
			/* Round Cal Distance to Whole Number.
			   Note that the cal distance is in mm, therefore no
			   resolution is lost.*/
			xTalkCalDistanceAsInt = (XTalkCalDistance +
				0x8000) >> 16;

			/* Apply division by mean spad count early in the
			 * calculation to keep the numbers small.
			 * This ensures we can maintain a 32bit calculation.
			 * Fixed1616 / int := Fixed1616 */
			signalXTalkTotalPerSpad = (xTalkStoredMeanSignalRate) /
				xTalkStoredMeanRtnSpadsAsInt;

			/* Complete the calculation for total Signal XTalk per
			 * SPAD
			 * Fixed1616 * (Fixed1616 - Fixed1616/int) :=
			 * (2^16 * Fixed1616)
			 */
			signalXTalkTotalPerSpad *= ((1 << 16) -
				(xTalkStoredMeanRange / xTalkCalDistanceAsInt));

			/* Round from 2^16 * Fixed1616, to Fixed1616. */
			XTalkCompensationRateMegaCps = (signalXTalkTotalPerSpad
				+ 0x8000) >> 16;
		}

		*pXTalkCompensationRateMegaCps = XTalkCompensationRateMegaCps;

		/* Enable the XTalk compensation */
		if (Status == VL_ERROR_NONE)
			Status = VL_SetXTalkCompensationEnable(Dev, 1);

		/* Enable the XTalk compensation */
		if (Status == VL_ERROR_NONE)
			Status = VL_SetXTalkCompensationRateMegaCps(Dev,
					XTalkCompensationRateMegaCps);

	}

	return Status;
}

int8_t VL_perform_offset_calibration(struct vl_data *Dev,
			unsigned int CalDistanceMilliMeter,
			int32_t *pOffsetMicroMeter)
{
	int8_t Status = VL_ERROR_NONE;
	uint16_t sum_ranging = 0;
	unsigned int total_count = 0;
	struct VL_RangingMeasurementData_t RangingMeasurementData;
	unsigned int StoredMeanRange;
	uint32_t StoredMeanRangeAsInt;
	uint32_t CalDistanceAsInt_mm;
	uint8_t SequenceStepEnabled;
	int meas = 0;

	if (CalDistanceMilliMeter <= 0)
		Status = VL_ERROR_INVALID_PARAMS;

	if (Status == VL_ERROR_NONE)
		Status = VL_SetOffsetCalibrationDataMicroMeter(Dev, 0);


	/* Get the value of the TCC */
	if (Status == VL_ERROR_NONE)
		Status = VL_GetSequenceStepEnable(Dev,
				VL_SEQUENCESTEP_TCC, &SequenceStepEnabled);


	/* Disable the TCC */
	if (Status == VL_ERROR_NONE)
		Status = VL_SetSequenceStepEnable(Dev,
				VL_SEQUENCESTEP_TCC, 0);


	/* Disable the RIT */
	if (Status == VL_ERROR_NONE)
		Status = VL_SetLimitCheckEnable(Dev,
				VL_CHECKENABLE_RANGE_IGNORE_THRESHOLD, 0);

	/* Perform 50 measurements and compute the averages */
	if (Status == VL_ERROR_NONE) {
		sum_ranging = 0;
		total_count = 0;
		for (meas = 0; meas < 50; meas++) {
			Status = VL_PerformSingleRangingMeasurement(Dev,
					&RangingMeasurementData);

			if (Status != VL_ERROR_NONE)
				break;

			/* The range is valid when RangeStatus = 0 */
			if (RangingMeasurementData.RangeStatus == 0) {
				sum_ranging = sum_ranging +
					RangingMeasurementData.RangeMilliMeter;
				total_count = total_count + 1;
			}
		}

		/* no valid values found */
		if (total_count == 0)
			Status = VL_ERROR_RANGE_ERROR;
	}


	if (Status == VL_ERROR_NONE) {
		/* unsigned int / uint16_t = unsigned int */
		StoredMeanRange = (unsigned int)((uint32_t)(sum_ranging << 16)
			/ total_count);

		StoredMeanRangeAsInt = (StoredMeanRange + 0x8000) >> 16;

		/* Round Cal Distance to Whole Number.
		 * Note that the cal distance is in mm, therefore no resolution
		 * is lost.*/
		 CalDistanceAsInt_mm = (CalDistanceMilliMeter + 0x8000) >> 16;

		 *pOffsetMicroMeter = (CalDistanceAsInt_mm -
				 StoredMeanRangeAsInt) * 1000;

		/* Apply the calculated offset */
		if (Status == VL_ERROR_NONE) {
			VL_SETPARAMETERFIELD(Dev, RangeOffsetMicroMeters,
					*pOffsetMicroMeter);
			Status = VL_SetOffsetCalibrationDataMicroMeter(Dev,
					*pOffsetMicroMeter);
		}

	}

	/* Restore the TCC */
	if (Status == VL_ERROR_NONE) {
		if (SequenceStepEnabled != 0)
			Status = VL_SetSequenceStepEnable(Dev,
					VL_SEQUENCESTEP_TCC, 1);
	}

	return Status;
}


int8_t VL_set_offset_calibration_data_micro_meter(struct vl_data *Dev,
		int32_t OffsetCalibrationDataMicroMeter)
{
	int8_t Status = VL_ERROR_NONE;
	int32_t cMaxOffsetMicroMeter = 511000;
	int32_t cMinOffsetMicroMeter = -512000;
	int16_t cOffsetRange = 4096;
	uint32_t encodedOffsetVal;

	LOG_FUNCTION_START("");

	if (OffsetCalibrationDataMicroMeter > cMaxOffsetMicroMeter)
		OffsetCalibrationDataMicroMeter = cMaxOffsetMicroMeter;
	else if (OffsetCalibrationDataMicroMeter < cMinOffsetMicroMeter)
		OffsetCalibrationDataMicroMeter = cMinOffsetMicroMeter;

	/* The offset register is 10.2 format and units are mm
	 * therefore conversion is applied by a division of
	 * 250.
	 */
	if (OffsetCalibrationDataMicroMeter >= 0) {
		encodedOffsetVal =
			OffsetCalibrationDataMicroMeter/250;
	} else {
		encodedOffsetVal =
			cOffsetRange +
			OffsetCalibrationDataMicroMeter/250;
	}

	Status = VL_WrWord(Dev,
		VL_REG_ALGO_PART_TO_PART_RANGE_OFFSET_MM,
		encodedOffsetVal);

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_get_offset_calibration_data_micro_meter(struct vl_data *Dev,
		int32_t *pOffsetCalibrationDataMicroMeter)
{
	int8_t Status = VL_ERROR_NONE;
	uint16_t RangeOffsetRegister;
	int16_t cMaxOffset = 2047;
	int16_t cOffsetRange = 4096;

	/* Note that offset has 10.2 format */

	Status = VL_RdWord(Dev,
				VL_REG_ALGO_PART_TO_PART_RANGE_OFFSET_MM,
				&RangeOffsetRegister);

	if (Status == VL_ERROR_NONE) {
		RangeOffsetRegister = (RangeOffsetRegister & 0x0fff);

		/* Apply 12 bit 2's compliment conversion */
		if (RangeOffsetRegister > cMaxOffset)
			*pOffsetCalibrationDataMicroMeter =
				(int16_t)(RangeOffsetRegister - cOffsetRange)
					* 250;
		else
			*pOffsetCalibrationDataMicroMeter =
				(int16_t)RangeOffsetRegister * 250;

	}

	return Status;
}


int8_t VL_apply_offset_adjustment(struct vl_data *Dev)
{
	int8_t Status = VL_ERROR_NONE;
	int32_t CorrectedOffsetMicroMeters;
	int32_t CurrentOffsetMicroMeters;

	/* if we run on this function we can read all the NVM info
	 * used by the API */
	Status = VL_get_info_from_device(Dev, 7);

	/* Read back current device offset */
	if (Status == VL_ERROR_NONE) {
		Status = VL_GetOffsetCalibrationDataMicroMeter(Dev,
					&CurrentOffsetMicroMeters);
	}

	/* Apply Offset Adjustment derived from 400mm measurements */
	if (Status == VL_ERROR_NONE) {

		/* Store initial device offset */
		PALDevDataSet(Dev, Part2PartOffsetNVMMicroMeter,
			CurrentOffsetMicroMeters);

		CorrectedOffsetMicroMeters = CurrentOffsetMicroMeters +
			(int32_t)PALDevDataGet(Dev,
				Part2PartOffsetAdjustmentNVMMicroMeter);

		Status = VL_SetOffsetCalibrationDataMicroMeter(Dev,
					CorrectedOffsetMicroMeters);

		/* store current, adjusted offset */
		if (Status == VL_ERROR_NONE) {
			VL_SETPARAMETERFIELD(Dev, RangeOffsetMicroMeters,
					CorrectedOffsetMicroMeters);
		}
	}

	return Status;
}

void get_next_good_spad(uint8_t goodSpadArray[], uint32_t size,
			uint32_t curr, int32_t *next)
{
	uint32_t startIndex;
	uint32_t fineOffset;
	uint32_t cSpadsPerByte = 8;
	uint32_t coarseIndex;
	uint32_t fineIndex;
	uint8_t dataByte;
	uint8_t success = 0;

	/*
	 * Starting with the current good spad, loop through the array to find
	 * the next. i.e. the next bit set in the sequence.
	 *
	 * The coarse index is the byte index of the array and the fine index is
	 * the index of the bit within each byte.
	 */

	*next = -1;

	startIndex = curr / cSpadsPerByte;
	fineOffset = curr % cSpadsPerByte;

	for (coarseIndex = startIndex; ((coarseIndex < size) && !success);
				coarseIndex++) {
		fineIndex = 0;
		dataByte = goodSpadArray[coarseIndex];

		if (coarseIndex == startIndex) {
			/* locate the bit position of the provided current
			 * spad bit before iterating */
			dataByte >>= fineOffset;
			fineIndex = fineOffset;
		}

		while (fineIndex < cSpadsPerByte) {
			if ((dataByte & 0x1) == 1) {
				success = 1;
				*next = coarseIndex * cSpadsPerByte + fineIndex;
				break;
			}
			dataByte >>= 1;
			fineIndex++;
		}
	}
}


uint8_t is_aperture(uint32_t spadIndex)
{
	/*
	 * This function reports if a given spad index is an aperture SPAD by
	 * deriving the quadrant.
	 */
	uint32_t quadrant;
	uint8_t isAperture = 1;

	quadrant = spadIndex >> 6;
	if (refArrayQuadrants[quadrant] == REF_ARRAY_SPAD_0)
		isAperture = 0;

	return isAperture;
}


int8_t enable_spad_bit(uint8_t spadArray[], uint32_t size,
	uint32_t spadIndex)
{
	int8_t status = VL_ERROR_NONE;
	uint32_t cSpadsPerByte = 8;
	uint32_t coarseIndex;
	uint32_t fineIndex;

	coarseIndex = spadIndex / cSpadsPerByte;
	fineIndex = spadIndex % cSpadsPerByte;
	if (coarseIndex >= size)
		status = VL_ERROR_REF_SPAD_INIT;
	else
		spadArray[coarseIndex] |= (1 << fineIndex);

	return status;
}

int8_t count_enabled_spads(uint8_t spadArray[],
		uint32_t byteCount, uint32_t maxSpads,
		uint32_t *pTotalSpadsEnabled, uint8_t *pIsAperture)
{
	int8_t status = VL_ERROR_NONE;
	uint32_t cSpadsPerByte = 8;
	uint32_t lastByte;
	uint32_t lastBit;
	uint32_t byteIndex = 0;
	uint32_t bitIndex = 0;
	uint8_t tempByte;
	uint8_t spadTypeIdentified = 0;

	/* The entire array will not be used for spads, therefore the last
	 * byte and last bit is determined from the max spads value.
	 */

	lastByte = maxSpads / cSpadsPerByte;
	lastBit = maxSpads % cSpadsPerByte;

	/* Check that the max spads value does not exceed the array bounds. */
	if (lastByte >= byteCount)
		status = VL_ERROR_REF_SPAD_INIT;

	*pTotalSpadsEnabled = 0;

	/* Count the bits enabled in the whole bytes */
	for (byteIndex = 0; byteIndex <= (lastByte - 1); byteIndex++) {
		tempByte = spadArray[byteIndex];

		for (bitIndex = 0; bitIndex <= cSpadsPerByte; bitIndex++) {
			if ((tempByte & 0x01) == 1) {
				(*pTotalSpadsEnabled)++;

				if (!spadTypeIdentified) {
					*pIsAperture = 1;
					if ((byteIndex < 2) && (bitIndex < 4))
							*pIsAperture = 0;
					spadTypeIdentified = 1;
				}
			}
			tempByte >>= 1;
		}
	}

	/* Count the number of bits enabled in the last byte accounting
	 * for the fact that not all bits in the byte may be used.
	 */
	tempByte = spadArray[lastByte];

	for (bitIndex = 0; bitIndex <= lastBit; bitIndex++) {
		if ((tempByte & 0x01) == 1)
			(*pTotalSpadsEnabled)++;
	}

	return status;
}

int8_t set_ref_spad_map(struct vl_data *Dev, uint8_t *refSpadArray)
{
	int8_t status = VL_WriteMulti(Dev,
				VL_REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0,
				refSpadArray, 6);
	return status;
}

int8_t get_ref_spad_map(struct vl_data *Dev, uint8_t *refSpadArray)
{
	int8_t status = VL_ReadMulti(Dev,
				VL_REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0,
				refSpadArray,
				6);
	return status;
}

int8_t enable_ref_spads(struct vl_data *Dev,
				uint8_t apertureSpads,
				uint8_t goodSpadArray[],
				uint8_t spadArray[],
				uint32_t size,
				uint32_t start,
				uint32_t offset,
				uint32_t spadCount,
				uint32_t *lastSpad)
{
	int8_t status = VL_ERROR_NONE;
	uint32_t index;
	uint32_t i;
	int32_t nextGoodSpad = offset;
	uint32_t currentSpad;
	uint8_t checkSpadArray[6];

	/*
	 * This function takes in a spad array which may or may not have SPADS
	 * already enabled and appends from a given offset a requested number
	 * of new SPAD enables. The 'good spad map' is applied to
	 * determine the next SPADs to enable.
	 *
	 * This function applies to only aperture or only non-aperture spads.
	 * Checks are performed to ensure this.
	 */

	currentSpad = offset;
	for (index = 0; index < spadCount; index++) {
		get_next_good_spad(goodSpadArray, size, currentSpad,
			&nextGoodSpad);

		if (nextGoodSpad == -1) {
			status = VL_ERROR_REF_SPAD_INIT;
			break;
		}

		/* Confirm that the next good SPAD is non-aperture */
		if (is_aperture(start + nextGoodSpad) != apertureSpads) {
			/* if we can't get the required number of good aperture
			 * spads from the current quadrant then this is an error
			 */
			status = VL_ERROR_REF_SPAD_INIT;
			break;
		}
		currentSpad = (uint32_t)nextGoodSpad;
		enable_spad_bit(spadArray, size, currentSpad);
		currentSpad++;
	}
	*lastSpad = currentSpad;

	if (status == VL_ERROR_NONE)
		status = set_ref_spad_map(Dev, spadArray);


	if (status == VL_ERROR_NONE) {
		status = get_ref_spad_map(Dev, checkSpadArray);

		i = 0;

		/* Compare spad maps. If not equal report error. */
		while (i < size) {
			if (spadArray[i] != checkSpadArray[i]) {
				status = VL_ERROR_REF_SPAD_INIT;
				break;
			}
			i++;
		}
	}
	return status;
}


int8_t perform_ref_signal_measurement(struct vl_data *Dev,
		uint16_t *refSignalRate)
{
	int8_t status = VL_ERROR_NONE;
	struct VL_RangingMeasurementData_t rangingMeasurementData;

	uint8_t SequenceConfig = 0;

	/* store the value of the sequence config,
	 * this will be reset before the end of the function
	 */

	SequenceConfig = PALDevDataGet(Dev, SequenceConfig);

	/*
	 * This function performs a reference signal rate measurement.
	 */
	if (status == VL_ERROR_NONE)
		status = VL_WrByte(Dev,
			VL_REG_SYSTEM_SEQUENCE_CONFIG, 0xC0);

	if (status == VL_ERROR_NONE)
		status = VL_PerformSingleRangingMeasurement(Dev,
				&rangingMeasurementData);

	if (status == VL_ERROR_NONE)
		status = VL_WrByte(Dev, 0xFF, 0x01);

	if (status == VL_ERROR_NONE)
		status = VL_RdWord(Dev,
			VL_REG_RESULT_PEAK_SIGNAL_RATE_REF,
			refSignalRate);

	if (status == VL_ERROR_NONE)
		status = VL_WrByte(Dev, 0xFF, 0x00);

	if (status == VL_ERROR_NONE) {
		/* restore the previous Sequence Config */
		status = VL_WrByte(Dev, VL_REG_SYSTEM_SEQUENCE_CONFIG,
				SequenceConfig);
		if (status == VL_ERROR_NONE)
			PALDevDataSet(Dev, SequenceConfig, SequenceConfig);
	}

	return status;
}

int8_t VL_perform_ref_spad_management(struct vl_data *Dev,
				uint32_t *refSpadCount,
				uint8_t *isApertureSpads)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t lastSpadArray[6];
	uint8_t startSelect = 0xB4;
	uint32_t minimumSpadCount = 3;
	uint32_t maxSpadCount = 44;
	uint32_t currentSpadIndex = 0;
	uint32_t lastSpadIndex = 0;
	int32_t nextGoodSpad = 0;
	uint16_t targetRefRate = 0x0A00; /* 20 MCPS in 9:7 format */
	uint16_t peakSignalRateRef;
	uint32_t needAptSpads = 0;
	uint32_t index = 0;
	uint32_t spadArraySize = 6;
	uint32_t signalRateDiff = 0;
	uint32_t lastSignalRateDiff = 0;
	uint8_t complete = 0;
	uint8_t VhvSettings = 0;
	uint8_t PhaseCal = 0;
	uint32_t refSpadCount_int = 0;
	uint8_t	 isApertureSpads_int = 0;

	/*
	 * The reference SPAD initialization procedure determines the minimum
	 * amount of reference spads to be enables to achieve a target reference
	 * signal rate and should be performed once during initialization.
	 *
	 * Either aperture or non-aperture spads are applied but never both.
	 * Firstly non-aperture spads are set, beginning with 5 spads, and
	 * increased one spad at a time until the closest measurement to the
	 * target rate is achieved.
	 *
	 * If the target rate is exceeded when 5 non-aperture spads are enabled,
	 * initialization is performed instead with aperture spads.
	 *
	 * When setting spads, a 'Good Spad Map' is applied.
	 *
	 * This procedure operates within a SPAD window of interest of a maximum
	 * 44 spads.
	 * The start point is currently fixed to 180, which lies towards the end
	 * of the non-aperture quadrant and runs in to the adjacent aperture
	 * quadrant.
	 */


	targetRefRate = PALDevDataGet(Dev, targetRefRate);

	/*
	 * Initialize Spad arrays.
	 * Currently the good spad map is initialised to 'All good'.
	 * This is a short term implementation. The good spad map will be
	 * provided as an input.
	 * Note that there are 6 bytes. Only the first 44 bits will be used to
	 * represent spads.
	 */
	for (index = 0; index < spadArraySize; index++)
		Dev->Data.SpadData.RefSpadEnables[index] = 0;


	Status = VL_WrByte(Dev, 0xFF, 0x01);

	if (Status == VL_ERROR_NONE)
		Status = VL_WrByte(Dev,
			VL_REG_DYNAMIC_SPAD_REF_EN_START_OFFSET, 0x00);

	if (Status == VL_ERROR_NONE)
		Status = VL_WrByte(Dev,
			VL_REG_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD, 0x2C);

	if (Status == VL_ERROR_NONE)
		Status = VL_WrByte(Dev, 0xFF, 0x00);

	if (Status == VL_ERROR_NONE)
		Status = VL_WrByte(Dev,
			VL_REG_GLOBAL_CONFIG_REF_EN_START_SELECT,
			startSelect);


	if (Status == VL_ERROR_NONE)
		Status = VL_WrByte(Dev,
			VL_REG_POWER_MANAGEMENT_GO1_POWER_FORCE, 0);

	/* Perform ref calibration */
	if (Status == VL_ERROR_NONE)
		Status = VL_perform_ref_calibration(Dev, &VhvSettings,
			&PhaseCal, 0);

	if (Status == VL_ERROR_NONE) {
		/* Enable Minimum NON-APERTURE Spads */
		currentSpadIndex = 0;
		lastSpadIndex = currentSpadIndex;
		needAptSpads = 0;
		Status = enable_ref_spads(Dev,
					needAptSpads,
					Dev->Data.SpadData.RefGoodSpadMap,
					Dev->Data.SpadData.RefSpadEnables,
					spadArraySize,
					startSelect,
					currentSpadIndex,
					minimumSpadCount,
					&lastSpadIndex);
	}

	if (Status == VL_ERROR_NONE) {
		currentSpadIndex = lastSpadIndex;

		Status = perform_ref_signal_measurement(Dev,
			&peakSignalRateRef);
		if ((Status == VL_ERROR_NONE) &&
			(peakSignalRateRef > targetRefRate)) {
			/* Signal rate measurement too high,
			 * switch to APERTURE SPADs */

			for (index = 0; index < spadArraySize; index++)
				Dev->Data.SpadData.RefSpadEnables[index] = 0;


			/* Increment to the first APERTURE spad */
			while ((is_aperture(startSelect + currentSpadIndex)
				== 0) && (currentSpadIndex < maxSpadCount)) {
				currentSpadIndex++;
			}

			needAptSpads = 1;

			Status = enable_ref_spads(Dev,
					needAptSpads,
					Dev->Data.SpadData.RefGoodSpadMap,
					Dev->Data.SpadData.RefSpadEnables,
					spadArraySize,
					startSelect,
					currentSpadIndex,
					minimumSpadCount,
					&lastSpadIndex);

			if (Status == VL_ERROR_NONE) {
				currentSpadIndex = lastSpadIndex;
				Status = perform_ref_signal_measurement(Dev,
						&peakSignalRateRef);

				if ((Status == VL_ERROR_NONE) &&
					(peakSignalRateRef > targetRefRate)) {
					/* Signal rate still too high after
					 * setting the minimum number of
					 * APERTURE spads. Can do no more
					 * therefore set the min number of
					 * aperture spads as the result.
					 */
					isApertureSpads_int = 1;
					refSpadCount_int = minimumSpadCount;
				}
			}
		} else {
			needAptSpads = 0;
		}
	}

	if ((Status == VL_ERROR_NONE) &&
		(peakSignalRateRef < targetRefRate)) {
		/* At this point, the minimum number of either aperture
		 * or non-aperture spads have been set. Proceed to add
		 * spads and perform measurements until the target
		 * reference is reached.
		 */
		isApertureSpads_int = needAptSpads;
		refSpadCount_int	= minimumSpadCount;

		memcpy(lastSpadArray, Dev->Data.SpadData.RefSpadEnables,
				spadArraySize);
		lastSignalRateDiff = abs(peakSignalRateRef -
			targetRefRate);
		complete = 0;

		while (!complete) {
			get_next_good_spad(
				Dev->Data.SpadData.RefGoodSpadMap,
				spadArraySize, currentSpadIndex,
				&nextGoodSpad);

			if (nextGoodSpad == -1) {
				Status = VL_ERROR_REF_SPAD_INIT;
				break;
			}

			/* Cannot combine Aperture and Non-Aperture spads, so
			 * ensure the current spad is of the correct type.
			 */
			if (is_aperture((uint32_t)startSelect + nextGoodSpad) !=
					needAptSpads) {
				/* At this point we have enabled the maximum
				 * number of Aperture spads.
				 */
				complete = 1;
				break;
			}

			(refSpadCount_int)++;

			currentSpadIndex = nextGoodSpad;
			Status = enable_spad_bit(
					Dev->Data.SpadData.RefSpadEnables,
					spadArraySize, currentSpadIndex);

			if (Status == VL_ERROR_NONE) {
				currentSpadIndex++;
				/* Proceed to apply the additional spad and
				 * perform measurement. */
				Status = set_ref_spad_map(Dev,
					Dev->Data.SpadData.RefSpadEnables);
			}

			if (Status != VL_ERROR_NONE)
				break;

			Status = perform_ref_signal_measurement(Dev,
					&peakSignalRateRef);

			if (Status != VL_ERROR_NONE)
				break;

			signalRateDiff = abs(peakSignalRateRef - targetRefRate);

			if (peakSignalRateRef > targetRefRate) {
				/* Select the spad map that provides the
				 * measurement closest to the target rate,
				 * either above or below it.
				 */
				if (signalRateDiff > lastSignalRateDiff) {
					/* Previous spad map produced a closer
					 * measurement, so choose this. */
					Status = set_ref_spad_map(Dev,
							lastSpadArray);
					memcpy(
					Dev->Data.SpadData.RefSpadEnables,
					lastSpadArray, spadArraySize);

					(refSpadCount_int)--;
				}
				complete = 1;
			} else {
				/* Continue to add spads */
				lastSignalRateDiff = signalRateDiff;
				memcpy(lastSpadArray,
					Dev->Data.SpadData.RefSpadEnables,
					spadArraySize);
			}

		} /* while */
	}

	if (Status == VL_ERROR_NONE) {
		*refSpadCount = refSpadCount_int;
		*isApertureSpads = isApertureSpads_int;

		VL_SETDEVICESPECIFICPARAMETER(Dev, RefSpadsInitialised, 1);
		VL_SETDEVICESPECIFICPARAMETER(Dev,
			ReferenceSpadCount, (uint8_t)(*refSpadCount));
		VL_SETDEVICESPECIFICPARAMETER(Dev,
			ReferenceSpadType, *isApertureSpads);
	}

	return Status;
}

int8_t VL_set_reference_spads(struct vl_data *Dev,
				 uint32_t count, uint8_t isApertureSpads)
{
	int8_t Status = VL_ERROR_NONE;
	uint32_t currentSpadIndex = 0;
	uint8_t startSelect = 0xB4;
	uint32_t spadArraySize = 6;
	uint32_t maxSpadCount = 44;
	uint32_t lastSpadIndex;
	uint32_t index;

	/*
	 * This function applies a requested number of reference spads, either
	 * aperture or
	 * non-aperture, as requested.
	 * The good spad map will be applied.
	 */

	Status = VL_WrByte(Dev, 0xFF, 0x01);

	if (Status == VL_ERROR_NONE)
		Status = VL_WrByte(Dev,
			VL_REG_DYNAMIC_SPAD_REF_EN_START_OFFSET, 0x00);

	if (Status == VL_ERROR_NONE)
		Status = VL_WrByte(Dev,
			VL_REG_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD, 0x2C);

	if (Status == VL_ERROR_NONE)
		Status = VL_WrByte(Dev, 0xFF, 0x00);

	if (Status == VL_ERROR_NONE)
		Status = VL_WrByte(Dev,
			VL_REG_GLOBAL_CONFIG_REF_EN_START_SELECT,
			startSelect);

	for (index = 0; index < spadArraySize; index++)
		Dev->Data.SpadData.RefSpadEnables[index] = 0;

	if (isApertureSpads) {
		/* Increment to the first APERTURE spad */
		while ((is_aperture(startSelect + currentSpadIndex) == 0) &&
			  (currentSpadIndex < maxSpadCount)) {
			currentSpadIndex++;
		}
	}
	Status = enable_ref_spads(Dev,
				isApertureSpads,
				Dev->Data.SpadData.RefGoodSpadMap,
				Dev->Data.SpadData.RefSpadEnables,
				spadArraySize,
				startSelect,
				currentSpadIndex,
				count,
				&lastSpadIndex);

	if (Status == VL_ERROR_NONE) {
		VL_SETDEVICESPECIFICPARAMETER(Dev, RefSpadsInitialised, 1);
		VL_SETDEVICESPECIFICPARAMETER(Dev,
			ReferenceSpadCount, (uint8_t)(count));
		VL_SETDEVICESPECIFICPARAMETER(Dev,
			ReferenceSpadType, isApertureSpads);
	}

	return Status;
}

int8_t VL_get_reference_spads(struct vl_data *Dev,
			uint32_t *pSpadCount, uint8_t *pIsApertureSpads)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t refSpadsInitialised;
	uint8_t refSpadArray[6];
	uint32_t cMaxSpadCount = 44;
	uint32_t cSpadArraySize = 6;
	uint32_t spadsEnabled;
	uint8_t isApertureSpads = 0;

	refSpadsInitialised = VL_GETDEVICESPECIFICPARAMETER(Dev,
					RefSpadsInitialised);

	if (refSpadsInitialised == 1) {

		*pSpadCount = (uint32_t)VL_GETDEVICESPECIFICPARAMETER(Dev,
			ReferenceSpadCount);
		*pIsApertureSpads = VL_GETDEVICESPECIFICPARAMETER(Dev,
			ReferenceSpadType);
	} else {

		/* obtain spad info from device.*/
		Status = get_ref_spad_map(Dev, refSpadArray);

		if (Status == VL_ERROR_NONE) {
			/* count enabled spads within spad map array and
			 * determine if Aperture or Non-Aperture.
			 */
			Status = count_enabled_spads(refSpadArray,
							cSpadArraySize,
							cMaxSpadCount,
							&spadsEnabled,
							&isApertureSpads);

			if (Status == VL_ERROR_NONE) {

				*pSpadCount = spadsEnabled;
				*pIsApertureSpads = isApertureSpads;

				VL_SETDEVICESPECIFICPARAMETER(Dev,
					RefSpadsInitialised, 1);
				VL_SETDEVICESPECIFICPARAMETER(Dev,
					ReferenceSpadCount,
					(uint8_t)spadsEnabled);
				VL_SETDEVICESPECIFICPARAMETER(Dev,
					ReferenceSpadType, isApertureSpads);
			}
		}
	}

	return Status;
}


int8_t VL_perform_single_ref_calibration(struct vl_data *Dev,
		uint8_t vhv_init_byte)
{
	int8_t Status = VL_ERROR_NONE;

	if (Status == VL_ERROR_NONE)
		Status = VL_WrByte(Dev, VL_REG_SYSRANGE_START,
				VL_REG_SYSRANGE_MODE_START_STOP |
				vhv_init_byte);

	if (Status == VL_ERROR_NONE)
		Status = VL_measurement_poll_for_completion(Dev);

	if (Status == VL_ERROR_NONE)
		Status = VL_ClearInterruptMask(Dev, 0);

	if (Status == VL_ERROR_NONE)
		Status = VL_WrByte(Dev, VL_REG_SYSRANGE_START, 0x00);

	return Status;
}


int8_t VL_ref_calibration_io(struct vl_data *Dev,
	uint8_t read_not_write, uint8_t VhvSettings, uint8_t PhaseCal,
	uint8_t *pVhvSettings, uint8_t *pPhaseCal,
	const uint8_t vhv_enable, const uint8_t phase_enable)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t PhaseCalint = 0;

	/* Read VHV from device */
	Status |= VL_WrByte(Dev, 0xFF, 0x01);
	Status |= VL_WrByte(Dev, 0x00, 0x00);
	Status |= VL_WrByte(Dev, 0xFF, 0x00);

	if (read_not_write) {
		if (vhv_enable)
			Status |= VL_RdByte(Dev, 0xCB, pVhvSettings);
		if (phase_enable)
			Status |= VL_RdByte(Dev, 0xEE, &PhaseCalint);
	} else {
		if (vhv_enable)
			Status |= VL_WrByte(Dev, 0xCB, VhvSettings);
		if (phase_enable)
			Status |= VL_UpdateByte(Dev, 0xEE, 0x80, PhaseCal);
	}

	Status |= VL_WrByte(Dev, 0xFF, 0x01);
	Status |= VL_WrByte(Dev, 0x00, 0x01);
	Status |= VL_WrByte(Dev, 0xFF, 0x00);

	*pPhaseCal = (uint8_t)(PhaseCalint&0xEF);

	return Status;
}


int8_t VL_perform_vhv_calibration(struct vl_data *Dev,
	uint8_t *pVhvSettings, const uint8_t get_data_enable,
	const uint8_t restore_config)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t SequenceConfig = 0;
	uint8_t VhvSettings = 0;
	uint8_t PhaseCal = 0;
	uint8_t PhaseCalInt = 0;

	/* store the value of the sequence config,
	 * this will be reset before the end of the function
	 */

	if (restore_config)
		SequenceConfig = PALDevDataGet(Dev, SequenceConfig);

	/* Run VHV */
	Status = VL_WrByte(Dev, VL_REG_SYSTEM_SEQUENCE_CONFIG, 0x01);

	if (Status == VL_ERROR_NONE)
		Status = VL_perform_single_ref_calibration(Dev, 0x40);

	/* Read VHV from device */
	if ((Status == VL_ERROR_NONE) && (get_data_enable == 1)) {
		Status = VL_ref_calibration_io(Dev, 1,
			VhvSettings, PhaseCal, /* Not used here */
			pVhvSettings, &PhaseCalInt,
			1, 0);
	} else
		*pVhvSettings = 0;


	if ((Status == VL_ERROR_NONE) && restore_config) {
		/* restore the previous Sequence Config */
		Status = VL_WrByte(Dev, VL_REG_SYSTEM_SEQUENCE_CONFIG,
				SequenceConfig);
		if (Status == VL_ERROR_NONE)
			PALDevDataSet(Dev, SequenceConfig, SequenceConfig);

	}

	return Status;
}

int8_t VL_perform_phase_calibration(struct vl_data *Dev,
	uint8_t *pPhaseCal, const uint8_t get_data_enable,
	const uint8_t restore_config)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t SequenceConfig = 0;
	uint8_t VhvSettings = 0;
	uint8_t PhaseCal = 0;
	uint8_t VhvSettingsint;

	/* store the value of the sequence config,
	 * this will be reset before the end of the function
	 */

	if (restore_config)
		SequenceConfig = PALDevDataGet(Dev, SequenceConfig);

	/* Run PhaseCal */
	Status = VL_WrByte(Dev, VL_REG_SYSTEM_SEQUENCE_CONFIG, 0x02);

	if (Status == VL_ERROR_NONE)
		Status = VL_perform_single_ref_calibration(Dev, 0x0);

	/* Read PhaseCal from device */
	if ((Status == VL_ERROR_NONE) && (get_data_enable == 1)) {
		Status = VL_ref_calibration_io(Dev, 1,
			VhvSettings, PhaseCal, /* Not used here */
			&VhvSettingsint, pPhaseCal,
			0, 1);
	} else
		*pPhaseCal = 0;


	if ((Status == VL_ERROR_NONE) && restore_config) {
		/* restore the previous Sequence Config */
		Status = VL_WrByte(Dev, VL_REG_SYSTEM_SEQUENCE_CONFIG,
				SequenceConfig);
		if (Status == VL_ERROR_NONE)
			PALDevDataSet(Dev, SequenceConfig, SequenceConfig);

	}

	return Status;
}

int8_t VL_perform_ref_calibration(struct vl_data *Dev,
	uint8_t *pVhvSettings, uint8_t *pPhaseCal, uint8_t get_data_enable)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t SequenceConfig = 0;

	/* store the value of the sequence config,
	 * this will be reset before the end of the function
	 */

	SequenceConfig = PALDevDataGet(Dev, SequenceConfig);

	/* In the following function we don't save the config to optimize
	 * writes on device. Config is saved and restored only once. */
	Status = VL_perform_vhv_calibration(
			Dev, pVhvSettings, get_data_enable, 0);


	if (Status == VL_ERROR_NONE)
		Status = VL_perform_phase_calibration(
			Dev, pPhaseCal, get_data_enable, 0);


	if (Status == VL_ERROR_NONE) {
		/* restore the previous Sequence Config */
		Status = VL_WrByte(Dev, VL_REG_SYSTEM_SEQUENCE_CONFIG,
				SequenceConfig);
		if (Status == VL_ERROR_NONE)
			PALDevDataSet(Dev, SequenceConfig, SequenceConfig);

	}

	return Status;
}

int8_t VL_set_ref_calibration(struct vl_data *Dev,
		uint8_t VhvSettings, uint8_t PhaseCal)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t pVhvSettings;
	uint8_t pPhaseCal;

	Status = VL_ref_calibration_io(Dev, 0,
		VhvSettings, PhaseCal,
		&pVhvSettings, &pPhaseCal,
		1, 1);

	return Status;
}

int8_t VL_get_ref_calibration(struct vl_data *Dev,
		uint8_t *pVhvSettings, uint8_t *pPhaseCal)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t VhvSettings = 0;
	uint8_t PhaseCal = 0;

	Status = VL_ref_calibration_io(Dev, 1,
		VhvSettings, PhaseCal,
		pVhvSettings, pPhaseCal,
		1, 1);

	return Status;
}
