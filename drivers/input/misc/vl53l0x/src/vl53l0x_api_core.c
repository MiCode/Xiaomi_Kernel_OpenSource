/*
 *  vl53l0x_api_core.c - Linux kernel modules for STM VL53L0 FlightSense TOF
 *						 sensor
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

int8_t VL_reverse_bytes(uint8_t *data, uint32_t size)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t tempData;
	uint32_t mirrorIndex;
	uint32_t middle = size/2;
	uint32_t index;

	for (index = 0; index < middle; index++) {
		mirrorIndex		 = size - index - 1;
		tempData		 = data[index];
		data[index]		 = data[mirrorIndex];
		data[mirrorIndex] = tempData;
	}
	return Status;
}

int8_t VL_measurement_poll_for_completion(struct vl_data *Dev)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t NewDataReady = 0;
	uint32_t LoopNb;

	LOG_FUNCTION_START("");

	LoopNb = 0;

	do {
		Status = VL_GetMeasurementDataReady(Dev, &NewDataReady);
		if (Status != 0)
			break; /* the error is set */

		if (NewDataReady == 1)
			break; /* done note that status == 0 */

		LoopNb++;
		if (LoopNb >= VL_DEFAULT_MAX_LOOP) {
			Status = VL_ERROR_TIME_OUT;
			break;
		}

		VL_PollingDelay(Dev);
	} while (1);

	LOG_FUNCTION_END(Status);

	return Status;
}


uint8_t VL_decode_vcsel_period(uint8_t vcsel_period_reg)
{
	/*!
	 * Converts the encoded VCSEL period register value into the real
	 * period in PLL clocks
	 */

	uint8_t vcsel_period_pclks = 0;

	vcsel_period_pclks = (vcsel_period_reg + 1) << 1;

	return vcsel_period_pclks;
}

uint8_t VL_encode_vcsel_period(uint8_t vcsel_period_pclks)
{
	/*!
	 * Converts the encoded VCSEL period register value into the real period
	 * in PLL clocks
	 */

	uint8_t vcsel_period_reg = 0;

	vcsel_period_reg = (vcsel_period_pclks >> 1) - 1;

	return vcsel_period_reg;
}


uint32_t VL_isqrt(uint32_t num)
{
	/*
	 * Implements an integer square root
	 *
	 * From: http://en.wikipedia.org/wiki/Methods_of_computing_square_roots
	 */

	uint32_t  res = 0;
	uint32_t  bit = 1 << 30;
	/* The second-to-top bit is set:
	 *	1 << 14 for 16-bits, 1 << 30 for 32 bits */

	 /* "bit" starts at the highest power of four <= the argument. */
	while (bit > num)
		bit >>= 2;


	while (bit != 0) {
		if (num >= res + bit) {
			num -= res + bit;
			res = (res >> 1) + bit;
		} else
			res >>= 1;

		bit >>= 2;
	}

	return res;
}


uint32_t VL_quadrature_sum(uint32_t a, uint32_t b)
{
	/*
	 * Implements a quadrature sum
	 *
	 * rea = sqrt(a^2 + b^2)
	 *
	 * Trap overflow case max input value is 65535 (16-bit value)
	 * as internal calc are 32-bit wide
	 *
	 * If overflow then seta output to maximum
	 */
	uint32_t  res = 0;

	if (a > 65535 || b > 65535)
		res = 65535;
	else
		res = VL_isqrt(a * a + b * b);

	return res;
}


int8_t VL_device_read_strobe(struct vl_data *Dev)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t strobe;
	uint32_t LoopNb;

	LOG_FUNCTION_START("");

	Status |= VL_WrByte(Dev, 0x83, 0x00);

	/* polling
	 * use timeout to avoid deadlock*/
	if (Status == VL_ERROR_NONE) {
		LoopNb = 0;
		do {
			Status = VL_RdByte(Dev, 0x83, &strobe);
			if ((strobe != 0x00) || Status != VL_ERROR_NONE)
					break;

			LoopNb = LoopNb + 1;
		} while (LoopNb < VL_DEFAULT_MAX_LOOP);

		if (LoopNb >= VL_DEFAULT_MAX_LOOP)
			Status = VL_ERROR_TIME_OUT;

	}

	Status |= VL_WrByte(Dev, 0x83, 0x01);

	LOG_FUNCTION_END(Status);
	return Status;

}

int8_t VL_get_info_from_device(struct vl_data *Dev, uint8_t option)
{

	int8_t Status = VL_ERROR_NONE;
	uint8_t byte;
	uint32_t TmpDWord;
	uint8_t ModuleId;
	uint8_t Revision;
	uint8_t ReferenceSpadCount = 0;
	uint8_t ReferenceSpadType = 0;
	uint32_t PartUIDUpper = 0;
	uint32_t PartUIDLower = 0;
	uint32_t OffsetFixed1104_mm = 0;
	int16_t OffsetMicroMeters = 0;
	uint32_t DistMeasTgtFixed1104_mm = 400 << 4;
	uint32_t DistMeasFixed1104_400_mm = 0;
	uint32_t SignalRateMeasFixed1104_400_mm = 0;
	char ProductId[19];
	char *ProductId_tmp;
	uint8_t ReadDataFromDeviceDone;
	unsigned int SignalRateMeasFixed400mmFix = 0;
	uint8_t NvmRefGoodSpadMap[VL_REF_SPAD_BUFFER_SIZE];
	int i;


	LOG_FUNCTION_START("");

	ReadDataFromDeviceDone = VL_GETDEVICESPECIFICPARAMETER(Dev,
			ReadDataFromDeviceDone);

	/* This access is done only once after that a GetDeviceInfo or
	 * datainit is done*/
	if (ReadDataFromDeviceDone != 7) {

		Status |= VL_WrByte(Dev, 0x80, 0x01);
		Status |= VL_WrByte(Dev, 0xFF, 0x01);
		Status |= VL_WrByte(Dev, 0x00, 0x00);

		Status |= VL_WrByte(Dev, 0xFF, 0x06);
		Status |= VL_RdByte(Dev, 0x83, &byte);
		Status |= VL_WrByte(Dev, 0x83, byte|4);
		Status |= VL_WrByte(Dev, 0xFF, 0x07);
		Status |= VL_WrByte(Dev, 0x81, 0x01);

		Status |= VL_PollingDelay(Dev);

		Status |= VL_WrByte(Dev, 0x80, 0x01);

		if (((option & 1) == 1) &&
			((ReadDataFromDeviceDone & 1) == 0)) {
			Status |= VL_WrByte(Dev, 0x94, 0x6b);
			Status |= VL_device_read_strobe(Dev);
			Status |= VL_RdDWord(Dev, 0x90, &TmpDWord);

			ReferenceSpadCount = (uint8_t)((TmpDWord >> 8) & 0x07f);
			ReferenceSpadType  = (uint8_t)((TmpDWord >> 15) & 0x01);

			Status |= VL_WrByte(Dev, 0x94, 0x24);
			Status |= VL_device_read_strobe(Dev);
			Status |= VL_RdDWord(Dev, 0x90, &TmpDWord);


			NvmRefGoodSpadMap[0] = (uint8_t)((TmpDWord >> 24)
				& 0xff);
			NvmRefGoodSpadMap[1] = (uint8_t)((TmpDWord >> 16)
				& 0xff);
			NvmRefGoodSpadMap[2] = (uint8_t)((TmpDWord >> 8)
				& 0xff);
			NvmRefGoodSpadMap[3] = (uint8_t)(TmpDWord & 0xff);

			Status |= VL_WrByte(Dev, 0x94, 0x25);
			Status |= VL_device_read_strobe(Dev);
			Status |= VL_RdDWord(Dev, 0x90, &TmpDWord);

			NvmRefGoodSpadMap[4] = (uint8_t)((TmpDWord >> 24)
				& 0xff);
			NvmRefGoodSpadMap[5] = (uint8_t)((TmpDWord >> 16)
				& 0xff);
		}

		if (((option & 2) == 2) &&
			((ReadDataFromDeviceDone & 2) == 0)) {

			Status |= VL_WrByte(Dev, 0x94, 0x02);
			Status |= VL_device_read_strobe(Dev);
			Status |= VL_RdByte(Dev, 0x90, &ModuleId);

			Status |= VL_WrByte(Dev, 0x94, 0x7B);
			Status |= VL_device_read_strobe(Dev);
			Status |= VL_RdByte(Dev, 0x90, &Revision);

			Status |= VL_WrByte(Dev, 0x94, 0x77);
			Status |= VL_device_read_strobe(Dev);
			Status |= VL_RdDWord(Dev, 0x90, &TmpDWord);

			ProductId[0] = (char)((TmpDWord >> 25) & 0x07f);
			ProductId[1] = (char)((TmpDWord >> 18) & 0x07f);
			ProductId[2] = (char)((TmpDWord >> 11) & 0x07f);
			ProductId[3] = (char)((TmpDWord >> 4) & 0x07f);

			byte = (uint8_t)((TmpDWord & 0x00f) << 3);

			Status |= VL_WrByte(Dev, 0x94, 0x78);
			Status |= VL_device_read_strobe(Dev);
			Status |= VL_RdDWord(Dev, 0x90, &TmpDWord);

			ProductId[4] = (char)(byte +
					((TmpDWord >> 29) & 0x07f));
			ProductId[5] = (char)((TmpDWord >> 22) & 0x07f);
			ProductId[6] = (char)((TmpDWord >> 15) & 0x07f);
			ProductId[7] = (char)((TmpDWord >> 8) & 0x07f);
			ProductId[8] = (char)((TmpDWord >> 1) & 0x07f);

			byte = (uint8_t)((TmpDWord & 0x001) << 6);

			Status |= VL_WrByte(Dev, 0x94, 0x79);

			Status |= VL_device_read_strobe(Dev);

			Status |= VL_RdDWord(Dev, 0x90, &TmpDWord);

			ProductId[9] = (char)(byte +
					((TmpDWord >> 26) & 0x07f));
			ProductId[10] = (char)((TmpDWord >> 19) & 0x07f);
			ProductId[11] = (char)((TmpDWord >> 12) & 0x07f);
			ProductId[12] = (char)((TmpDWord >> 5) & 0x07f);

			byte = (uint8_t)((TmpDWord & 0x01f) << 2);

			Status |= VL_WrByte(Dev, 0x94, 0x7A);

			Status |= VL_device_read_strobe(Dev);

			Status |= VL_RdDWord(Dev, 0x90, &TmpDWord);

			ProductId[13] = (char)(byte +
					((TmpDWord >> 30) & 0x07f));
			ProductId[14] = (char)((TmpDWord >> 23) & 0x07f);
			ProductId[15] = (char)((TmpDWord >> 16) & 0x07f);
			ProductId[16] = (char)((TmpDWord >> 9) & 0x07f);
			ProductId[17] = (char)((TmpDWord >> 2) & 0x07f);
			ProductId[18] = '\0';

		}

		if (((option & 4) == 4) &&
			((ReadDataFromDeviceDone & 4) == 0)) {

			Status |= VL_WrByte(Dev, 0x94, 0x7B);
			Status |= VL_device_read_strobe(Dev);
			Status |= VL_RdDWord(Dev, 0x90, &PartUIDUpper);

			Status |= VL_WrByte(Dev, 0x94, 0x7C);
			Status |= VL_device_read_strobe(Dev);
			Status |= VL_RdDWord(Dev, 0x90, &PartUIDLower);

			Status |= VL_WrByte(Dev, 0x94, 0x73);
			Status |= VL_device_read_strobe(Dev);
			Status |= VL_RdDWord(Dev, 0x90, &TmpDWord);

			SignalRateMeasFixed1104_400_mm = (TmpDWord &
				0x0000000ff) << 8;

			Status |= VL_WrByte(Dev, 0x94, 0x74);
			Status |= VL_device_read_strobe(Dev);
			Status |= VL_RdDWord(Dev, 0x90, &TmpDWord);

			SignalRateMeasFixed1104_400_mm |= ((TmpDWord &
				0xff000000) >> 24);

			Status |= VL_WrByte(Dev, 0x94, 0x75);
			Status |= VL_device_read_strobe(Dev);
			Status |= VL_RdDWord(Dev, 0x90, &TmpDWord);

			DistMeasFixed1104_400_mm = (TmpDWord & 0x0000000ff)
							<< 8;

			Status |= VL_WrByte(Dev, 0x94, 0x76);
			Status |= VL_device_read_strobe(Dev);
			Status |= VL_RdDWord(Dev, 0x90, &TmpDWord);

			DistMeasFixed1104_400_mm |= ((TmpDWord & 0xff000000)
							>> 24);
		}

		Status |= VL_WrByte(Dev, 0x81, 0x00);
		Status |= VL_WrByte(Dev, 0xFF, 0x06);
		Status |= VL_RdByte(Dev, 0x83, &byte);
		Status |= VL_WrByte(Dev, 0x83, byte&0xfb);
		Status |= VL_WrByte(Dev, 0xFF, 0x01);
		Status |= VL_WrByte(Dev, 0x00, 0x01);

		Status |= VL_WrByte(Dev, 0xFF, 0x00);
		Status |= VL_WrByte(Dev, 0x80, 0x00);
	}

	if ((Status == VL_ERROR_NONE) &&
		(ReadDataFromDeviceDone != 7)) {
		/* Assign to variable if status is ok */
		if (((option & 1) == 1) &&
			((ReadDataFromDeviceDone & 1) == 0)) {
			VL_SETDEVICESPECIFICPARAMETER(Dev,
				ReferenceSpadCount, ReferenceSpadCount);

			VL_SETDEVICESPECIFICPARAMETER(Dev,
				ReferenceSpadType, ReferenceSpadType);

			for (i = 0; i < VL_REF_SPAD_BUFFER_SIZE; i++) {
				Dev->Data.SpadData.RefGoodSpadMap[i] =
					NvmRefGoodSpadMap[i];
			}
		}

		if (((option & 2) == 2) &&
			((ReadDataFromDeviceDone & 2) == 0)) {
			VL_SETDEVICESPECIFICPARAMETER(Dev,
					ModuleId, ModuleId);

			VL_SETDEVICESPECIFICPARAMETER(Dev,
					Revision, Revision);

			ProductId_tmp = VL_GETDEVICESPECIFICPARAMETER(Dev,
					ProductId);
			VL_COPYSTRING(ProductId_tmp, ProductId);

		}

		if (((option & 4) == 4) &&
			((ReadDataFromDeviceDone & 4) == 0)) {
			VL_SETDEVICESPECIFICPARAMETER(Dev,
						PartUIDUpper, PartUIDUpper);

			VL_SETDEVICESPECIFICPARAMETER(Dev,
						PartUIDLower, PartUIDLower);

			SignalRateMeasFixed400mmFix =
				VL_FIXPOINT97TOFIXPOINT1616(
					SignalRateMeasFixed1104_400_mm);

			VL_SETDEVICESPECIFICPARAMETER(Dev,
				SignalRateMeasFixed400mm,
				SignalRateMeasFixed400mmFix);

			OffsetMicroMeters = 0;
			if (DistMeasFixed1104_400_mm != 0) {
					OffsetFixed1104_mm =
						DistMeasFixed1104_400_mm -
						DistMeasTgtFixed1104_mm;
					OffsetMicroMeters = (OffsetFixed1104_mm
						* 1000) >> 4;
					OffsetMicroMeters *= -1;
			}

			PALDevDataSet(Dev,
				Part2PartOffsetAdjustmentNVMMicroMeter,
				OffsetMicroMeters);
		}
		byte = (uint8_t)(ReadDataFromDeviceDone|option);
		VL_SETDEVICESPECIFICPARAMETER(Dev, ReadDataFromDeviceDone,
				byte);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}


uint32_t VL_calc_macro_period_ps(struct vl_data *Dev,
	uint8_t vcsel_period_pclks)
{
	uint64_t PLL_period_ps;
	uint32_t macro_period_vclks;
	uint32_t macro_period_ps;

	LOG_FUNCTION_START("");

	/* The above calculation will produce rounding errors,
	   therefore set fixed value
	*/
	PLL_period_ps = 1655;

	macro_period_vclks = 2304;
	macro_period_ps = (uint32_t)(macro_period_vclks
			* vcsel_period_pclks * PLL_period_ps);

	LOG_FUNCTION_END("");
	return macro_period_ps;
}

uint16_t VL_encode_timeout(uint32_t timeout_macro_clks)
{
	/*!
	 * Encode timeout in macro periods in (LSByte * 2^MSByte) + 1 format
	 */

	uint16_t encoded_timeout = 0;
	uint32_t ls_byte = 0;
	uint16_t ms_byte = 0;

	if (timeout_macro_clks > 0) {
		ls_byte = timeout_macro_clks - 1;

		while ((ls_byte & 0xFFFFFF00) > 0) {
			ls_byte = ls_byte >> 1;
			ms_byte++;
		}

		encoded_timeout = (ms_byte << 8)
				+ (uint16_t) (ls_byte & 0x000000FF);
	}

	return encoded_timeout;

}

uint32_t VL_decode_timeout(uint16_t encoded_timeout)
{
	/*!
	 * Decode 16-bit timeout register value - format (LSByte * 2^MSByte) + 1
	 */

	uint32_t timeout_macro_clks = 0;

	timeout_macro_clks = ((uint32_t) (encoded_timeout & 0x00FF)
			<< (uint32_t) ((encoded_timeout & 0xFF00) >> 8)) + 1;

	return timeout_macro_clks;
}


/* To convert ms into register value */
uint32_t VL_calc_timeout_mclks(struct vl_data *Dev,
		uint32_t timeout_period_us,
		uint8_t vcsel_period_pclks)
{
	uint32_t macro_period_ps;
	uint32_t macro_period_ns;
	uint32_t timeout_period_mclks = 0;

	macro_period_ps = VL_calc_macro_period_ps(Dev, vcsel_period_pclks);
	macro_period_ns = (macro_period_ps + 500) / 1000;

	timeout_period_mclks =
		(uint32_t) (((timeout_period_us * 1000)
		+ (macro_period_ns / 2)) / macro_period_ns);

	return timeout_period_mclks;
}

/* To convert register value into us */
uint32_t VL_calc_timeout_us(struct vl_data *Dev,
		uint16_t timeout_period_mclks,
		uint8_t vcsel_period_pclks)
{
	uint32_t macro_period_ps;
	uint32_t macro_period_ns;
	uint32_t actual_timeout_period_us = 0;

	macro_period_ps = VL_calc_macro_period_ps(Dev, vcsel_period_pclks);
	macro_period_ns = (macro_period_ps + 500) / 1000;

	actual_timeout_period_us =
		((timeout_period_mclks * macro_period_ns) + 500) / 1000;

	return actual_timeout_period_us;
}


int8_t get_sequence_step_timeout(struct vl_data *Dev,
				uint8_t SequenceStepId,
				uint32_t *pTimeOutMicroSecs)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t CurrentVCSELPulsePeriodPClk;
	uint8_t EncodedTimeOutByte = 0;
	uint32_t TimeoutMicroSeconds = 0;
	uint16_t PreRangeEncodedTimeOut = 0;
	uint16_t MsrcTimeOutMClks;
	uint16_t PreRangeTimeOutMClks;
	uint16_t FinalRangeTimeOutMClks = 0;
	uint16_t FinalRangeEncodedTimeOut;
	struct VL_SchedulerSequenceSteps_t SchedulerSequenceSteps;

	if ((SequenceStepId == VL_SEQUENCESTEP_TCC)	 ||
		(SequenceStepId == VL_SEQUENCESTEP_DSS)	 ||
		(SequenceStepId == VL_SEQUENCESTEP_MSRC)) {

		Status = VL_GetVcselPulsePeriod(Dev,
					VL_VCSEL_PERIOD_PRE_RANGE,
					&CurrentVCSELPulsePeriodPClk);
		if (Status == VL_ERROR_NONE) {
			Status = VL_RdByte(Dev,
					VL_REG_MSRC_CONFIG_TIMEOUT_MACROP,
					&EncodedTimeOutByte);
		}
		MsrcTimeOutMClks = VL_decode_timeout(EncodedTimeOutByte);

		TimeoutMicroSeconds = VL_calc_timeout_us(Dev,
						MsrcTimeOutMClks,
						CurrentVCSELPulsePeriodPClk);
	} else if (SequenceStepId == VL_SEQUENCESTEP_PRE_RANGE) {
		/* Retrieve PRE-RANGE VCSEL Period */
		Status = VL_GetVcselPulsePeriod(Dev,
						VL_VCSEL_PERIOD_PRE_RANGE,
						&CurrentVCSELPulsePeriodPClk);

		/* Retrieve PRE-RANGE Timeout in Macro periods (MCLKS) */
		if (Status == VL_ERROR_NONE) {

			/* Retrieve PRE-RANGE VCSEL Period */
			Status = VL_GetVcselPulsePeriod(Dev,
					VL_VCSEL_PERIOD_PRE_RANGE,
					&CurrentVCSELPulsePeriodPClk);

			if (Status == VL_ERROR_NONE) {
				Status = VL_RdWord(Dev,
				VL_REG_PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI,
				&PreRangeEncodedTimeOut);
			}

			PreRangeTimeOutMClks = VL_decode_timeout(
					PreRangeEncodedTimeOut);

			TimeoutMicroSeconds = VL_calc_timeout_us(Dev,
					PreRangeTimeOutMClks,
					CurrentVCSELPulsePeriodPClk);
		}
	} else if (SequenceStepId == VL_SEQUENCESTEP_FINAL_RANGE) {

		VL_GetSequenceStepEnables(Dev, &SchedulerSequenceSteps);
		PreRangeTimeOutMClks = 0;

		if (SchedulerSequenceSteps.PreRangeOn) {
			/* Retrieve PRE-RANGE VCSEL Period */
			Status = VL_GetVcselPulsePeriod(Dev,
				VL_VCSEL_PERIOD_PRE_RANGE,
				&CurrentVCSELPulsePeriodPClk);

			/* Retrieve PRE-RANGE Timeout in Macro periods
			 * (MCLKS) */
			if (Status == VL_ERROR_NONE) {
				Status = VL_RdWord(Dev,
				VL_REG_PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI,
				&PreRangeEncodedTimeOut);
				PreRangeTimeOutMClks = VL_decode_timeout(
						PreRangeEncodedTimeOut);
			}
		}

		if (Status == VL_ERROR_NONE) {
			/* Retrieve FINAL-RANGE VCSEL Period */
			Status = VL_GetVcselPulsePeriod(Dev,
					VL_VCSEL_PERIOD_FINAL_RANGE,
					&CurrentVCSELPulsePeriodPClk);
		}

		/* Retrieve FINAL-RANGE Timeout in Macro periods (MCLKS) */
		if (Status == VL_ERROR_NONE) {
			Status = VL_RdWord(Dev,
			    VL_REG_FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI,
				&FinalRangeEncodedTimeOut);
			FinalRangeTimeOutMClks = VL_decode_timeout(
					FinalRangeEncodedTimeOut);
		}

		FinalRangeTimeOutMClks -= PreRangeTimeOutMClks;
		TimeoutMicroSeconds = VL_calc_timeout_us(Dev,
						FinalRangeTimeOutMClks,
						CurrentVCSELPulsePeriodPClk);
	}

	*pTimeOutMicroSecs = TimeoutMicroSeconds;

	return Status;
}


int8_t set_sequence_step_timeout(struct vl_data *Dev,
					uint8_t SequenceStepId,
					uint32_t TimeOutMicroSecs)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t CurrentVCSELPulsePeriodPClk;
	uint8_t MsrcEncodedTimeOut;
	uint16_t PreRangeEncodedTimeOut;
	uint16_t PreRangeTimeOutMClks;
	uint16_t MsrcRangeTimeOutMClks;
	uint32_t FinalRangeTimeOutMClks;
	uint16_t FinalRangeEncodedTimeOut;
	struct VL_SchedulerSequenceSteps_t SchedulerSequenceSteps;

	if ((SequenceStepId == VL_SEQUENCESTEP_TCC)	 ||
		(SequenceStepId == VL_SEQUENCESTEP_DSS)	 ||
		(SequenceStepId == VL_SEQUENCESTEP_MSRC)) {

		Status = VL_GetVcselPulsePeriod(Dev,
					VL_VCSEL_PERIOD_PRE_RANGE,
					&CurrentVCSELPulsePeriodPClk);

		if (Status == VL_ERROR_NONE) {
			MsrcRangeTimeOutMClks = VL_calc_timeout_mclks(Dev,
					TimeOutMicroSecs,
					(uint8_t)CurrentVCSELPulsePeriodPClk);

			if (MsrcRangeTimeOutMClks > 256)
				MsrcEncodedTimeOut = 255;
			else
				MsrcEncodedTimeOut =
					(uint8_t)MsrcRangeTimeOutMClks - 1;

			VL_SETDEVICESPECIFICPARAMETER(Dev,
				LastEncodedTimeout,
				MsrcEncodedTimeOut);
		}

		if (Status == VL_ERROR_NONE) {
			Status = VL_WrByte(Dev,
				VL_REG_MSRC_CONFIG_TIMEOUT_MACROP,
				MsrcEncodedTimeOut);
		}
	} else {

		if (SequenceStepId == VL_SEQUENCESTEP_PRE_RANGE) {

			if (Status == VL_ERROR_NONE) {
				Status = VL_GetVcselPulsePeriod(Dev,
						VL_VCSEL_PERIOD_PRE_RANGE,
						&CurrentVCSELPulsePeriodPClk);
				PreRangeTimeOutMClks =
					VL_calc_timeout_mclks(Dev,
					TimeOutMicroSecs,
					(uint8_t)CurrentVCSELPulsePeriodPClk);
				PreRangeEncodedTimeOut = VL_encode_timeout(
					PreRangeTimeOutMClks);

				VL_SETDEVICESPECIFICPARAMETER(Dev,
					LastEncodedTimeout,
					PreRangeEncodedTimeOut);
			}

			if (Status == VL_ERROR_NONE) {
				Status = VL_WrWord(Dev,
				VL_REG_PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI,
				PreRangeEncodedTimeOut);
			}

			if (Status == VL_ERROR_NONE) {
				VL_SETDEVICESPECIFICPARAMETER(
					Dev,
					PreRangeTimeoutMicroSecs,
					TimeOutMicroSecs);
			}
		} else if (SequenceStepId == VL_SEQUENCESTEP_FINAL_RANGE) {

			/* For the final range timeout, the pre-range timeout
			 * must be added. To do this both final and pre-range
			 * timeouts must be expressed in macro periods MClks
			 * because they have different vcsel periods.
			 */

			VL_GetSequenceStepEnables(Dev,
					&SchedulerSequenceSteps);
			PreRangeTimeOutMClks = 0;
			if (SchedulerSequenceSteps.PreRangeOn) {

				/* Retrieve PRE-RANGE VCSEL Period */
				Status = VL_GetVcselPulsePeriod(Dev,
					VL_VCSEL_PERIOD_PRE_RANGE,
					&CurrentVCSELPulsePeriodPClk);

				/* Retrieve PRE-RANGE Timeout in Macro periods
				 * (MCLKS) */
				if (Status == VL_ERROR_NONE) {
					Status = VL_RdWord(Dev, 0x51,
						&PreRangeEncodedTimeOut);
					PreRangeTimeOutMClks =
						VL_decode_timeout(
							PreRangeEncodedTimeOut);
				}
			}

			/* Calculate FINAL RANGE Timeout in Macro Periods
			 * (MCLKS) and add PRE-RANGE value
			 */
			if (Status == VL_ERROR_NONE) {

				Status = VL_GetVcselPulsePeriod(Dev,
					VL_VCSEL_PERIOD_FINAL_RANGE,
					&CurrentVCSELPulsePeriodPClk);
			}
			if (Status == VL_ERROR_NONE) {

				FinalRangeTimeOutMClks =
					VL_calc_timeout_mclks(Dev,
					TimeOutMicroSecs,
					(uint8_t) CurrentVCSELPulsePeriodPClk);

				FinalRangeTimeOutMClks += PreRangeTimeOutMClks;

				FinalRangeEncodedTimeOut =
				VL_encode_timeout(FinalRangeTimeOutMClks);

				if (Status == VL_ERROR_NONE) {
					Status = VL_WrWord(Dev, 0x71,
					FinalRangeEncodedTimeOut);
				}

				if (Status == VL_ERROR_NONE) {
					VL_SETDEVICESPECIFICPARAMETER(
						Dev,
						FinalRangeTimeoutMicroSecs,
						TimeOutMicroSecs);
				}
			}
		} else
			Status = VL_ERROR_INVALID_PARAMS;

	}
	return Status;
}

int8_t VL_set_vcsel_pulse_period(struct vl_data *Dev,
	uint8_t VcselPeriodType, uint8_t VCSELPulsePeriodPCLK)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t vcsel_period_reg;
	uint8_t MinPreVcselPeriodPCLK = 12;
	uint8_t MaxPreVcselPeriodPCLK = 18;
	uint8_t MinFinalVcselPeriodPCLK = 8;
	uint8_t MaxFinalVcselPeriodPCLK = 14;
	uint32_t MeasurementTimingBudgetMicroSeconds;
	uint32_t FinalRangeTimeoutMicroSeconds;
	uint32_t PreRangeTimeoutMicroSeconds;
	uint32_t MsrcTimeoutMicroSeconds;
	uint8_t PhaseCalInt = 0;

	/* Check if valid clock period requested */

	if ((VCSELPulsePeriodPCLK % 2) != 0) {
		/* Value must be an even number */
		Status = VL_ERROR_INVALID_PARAMS;
	} else if (VcselPeriodType == VL_VCSEL_PERIOD_PRE_RANGE &&
		(VCSELPulsePeriodPCLK < MinPreVcselPeriodPCLK ||
		VCSELPulsePeriodPCLK > MaxPreVcselPeriodPCLK)) {
		Status = VL_ERROR_INVALID_PARAMS;
	} else if (VcselPeriodType == VL_VCSEL_PERIOD_FINAL_RANGE &&
		(VCSELPulsePeriodPCLK < MinFinalVcselPeriodPCLK ||
		 VCSELPulsePeriodPCLK > MaxFinalVcselPeriodPCLK)) {

		Status = VL_ERROR_INVALID_PARAMS;
	}

	/* Apply specific settings for the requested clock period */

	if (Status != VL_ERROR_NONE)
		return Status;


	if (VcselPeriodType == VL_VCSEL_PERIOD_PRE_RANGE) {

		/* Set phase check limits */
		if (VCSELPulsePeriodPCLK == 12) {

			Status = VL_WrByte(Dev,
				VL_REG_PRE_RANGE_CONFIG_VALID_PHASE_HIGH,
				0x18);
			Status = VL_WrByte(Dev,
				VL_REG_PRE_RANGE_CONFIG_VALID_PHASE_LOW,
				0x08);
		} else if (VCSELPulsePeriodPCLK == 14) {

			Status = VL_WrByte(Dev,
				VL_REG_PRE_RANGE_CONFIG_VALID_PHASE_HIGH,
				0x30);
			Status = VL_WrByte(Dev,
				VL_REG_PRE_RANGE_CONFIG_VALID_PHASE_LOW,
				0x08);
		} else if (VCSELPulsePeriodPCLK == 16) {

			Status = VL_WrByte(Dev,
				VL_REG_PRE_RANGE_CONFIG_VALID_PHASE_HIGH,
				0x40);
			Status = VL_WrByte(Dev,
				VL_REG_PRE_RANGE_CONFIG_VALID_PHASE_LOW,
				0x08);
		} else if (VCSELPulsePeriodPCLK == 18) {

			Status = VL_WrByte(Dev,
				VL_REG_PRE_RANGE_CONFIG_VALID_PHASE_HIGH,
				0x50);
			Status = VL_WrByte(Dev,
				VL_REG_PRE_RANGE_CONFIG_VALID_PHASE_LOW,
				0x08);
		}
	} else if (VcselPeriodType == VL_VCSEL_PERIOD_FINAL_RANGE) {

		if (VCSELPulsePeriodPCLK == 8) {

			Status = VL_WrByte(Dev,
				VL_REG_FINAL_RANGE_CONFIG_VALID_PHASE_HIGH,
				0x10);
			Status = VL_WrByte(Dev,
				VL_REG_FINAL_RANGE_CONFIG_VALID_PHASE_LOW,
				0x08);

			Status |= VL_WrByte(Dev,
				VL_REG_GLOBAL_CONFIG_VCSEL_WIDTH, 0x02);
			Status |= VL_WrByte(Dev,
				VL_REG_ALGO_PHASECAL_CONFIG_TIMEOUT, 0x0C);

			Status |= VL_WrByte(Dev, 0xff, 0x01);
			Status |= VL_WrByte(Dev,
				VL_REG_ALGO_PHASECAL_LIM,
				0x30);
			Status |= VL_WrByte(Dev, 0xff, 0x00);
		} else if (VCSELPulsePeriodPCLK == 10) {

			Status = VL_WrByte(Dev,
				VL_REG_FINAL_RANGE_CONFIG_VALID_PHASE_HIGH,
				0x28);
			Status = VL_WrByte(Dev,
				VL_REG_FINAL_RANGE_CONFIG_VALID_PHASE_LOW,
				0x08);

			Status |= VL_WrByte(Dev,
				VL_REG_GLOBAL_CONFIG_VCSEL_WIDTH, 0x03);
			Status |= VL_WrByte(Dev,
				VL_REG_ALGO_PHASECAL_CONFIG_TIMEOUT, 0x09);

			Status |= VL_WrByte(Dev, 0xff, 0x01);
			Status |= VL_WrByte(Dev,
				VL_REG_ALGO_PHASECAL_LIM,
				0x20);
			Status |= VL_WrByte(Dev, 0xff, 0x00);
		} else if (VCSELPulsePeriodPCLK == 12) {

			Status = VL_WrByte(Dev,
				VL_REG_FINAL_RANGE_CONFIG_VALID_PHASE_HIGH,
				0x38);
			Status = VL_WrByte(Dev,
				VL_REG_FINAL_RANGE_CONFIG_VALID_PHASE_LOW,
				0x08);

			Status |= VL_WrByte(Dev,
				VL_REG_GLOBAL_CONFIG_VCSEL_WIDTH, 0x03);
			Status |= VL_WrByte(Dev,
				VL_REG_ALGO_PHASECAL_CONFIG_TIMEOUT, 0x08);

			Status |= VL_WrByte(Dev, 0xff, 0x01);
			Status |= VL_WrByte(Dev,
				VL_REG_ALGO_PHASECAL_LIM,
				0x20);
			Status |= VL_WrByte(Dev, 0xff, 0x00);
		} else if (VCSELPulsePeriodPCLK == 14) {

			Status = VL_WrByte(Dev,
				VL_REG_FINAL_RANGE_CONFIG_VALID_PHASE_HIGH,
				0x048);
			Status = VL_WrByte(Dev,
				VL_REG_FINAL_RANGE_CONFIG_VALID_PHASE_LOW,
				0x08);

			Status |= VL_WrByte(Dev,
				VL_REG_GLOBAL_CONFIG_VCSEL_WIDTH, 0x03);
			Status |= VL_WrByte(Dev,
				VL_REG_ALGO_PHASECAL_CONFIG_TIMEOUT, 0x07);

			Status |= VL_WrByte(Dev, 0xff, 0x01);
			Status |= VL_WrByte(Dev,
				VL_REG_ALGO_PHASECAL_LIM,
				0x20);
			Status |= VL_WrByte(Dev, 0xff, 0x00);
		}
	}


	/* Re-calculate and apply timeouts, in macro periods */

	if (Status == VL_ERROR_NONE) {
		vcsel_period_reg = VL_encode_vcsel_period((uint8_t)
			VCSELPulsePeriodPCLK);

		/* When the VCSEL period for the pre or final range is changed,
		* the corresponding timeout must be read from the device using
		* the current VCSEL period, then the new VCSEL period can be
		* applied. The timeout then must be written back to the device
		* using the new VCSEL period.
		*
		* For the MSRC timeout, the same applies - this timeout being
		* dependent on the pre-range vcsel period.
		*/
		switch (VcselPeriodType) {
		case VL_VCSEL_PERIOD_PRE_RANGE:
			Status = get_sequence_step_timeout(Dev,
				VL_SEQUENCESTEP_PRE_RANGE,
				&PreRangeTimeoutMicroSeconds);

			if (Status == VL_ERROR_NONE)
				Status = get_sequence_step_timeout(Dev,
					VL_SEQUENCESTEP_MSRC,
					&MsrcTimeoutMicroSeconds);

			if (Status == VL_ERROR_NONE)
				Status = VL_WrByte(Dev,
				VL_REG_PRE_RANGE_CONFIG_VCSEL_PERIOD,
					vcsel_period_reg);


			if (Status == VL_ERROR_NONE)
				Status = set_sequence_step_timeout(Dev,
					VL_SEQUENCESTEP_PRE_RANGE,
					PreRangeTimeoutMicroSeconds);


			if (Status == VL_ERROR_NONE)
				Status = set_sequence_step_timeout(Dev,
					VL_SEQUENCESTEP_MSRC,
					MsrcTimeoutMicroSeconds);

			VL_SETDEVICESPECIFICPARAMETER(
				Dev,
				PreRangeVcselPulsePeriod,
				VCSELPulsePeriodPCLK);
			break;
		case VL_VCSEL_PERIOD_FINAL_RANGE:
			Status = get_sequence_step_timeout(Dev,
				VL_SEQUENCESTEP_FINAL_RANGE,
				&FinalRangeTimeoutMicroSeconds);

			if (Status == VL_ERROR_NONE)
				Status = VL_WrByte(Dev,
				VL_REG_FINAL_RANGE_CONFIG_VCSEL_PERIOD,
					vcsel_period_reg);


			if (Status == VL_ERROR_NONE)
				Status = set_sequence_step_timeout(Dev,
					VL_SEQUENCESTEP_FINAL_RANGE,
					FinalRangeTimeoutMicroSeconds);

			VL_SETDEVICESPECIFICPARAMETER(
				Dev,
				FinalRangeVcselPulsePeriod,
				VCSELPulsePeriodPCLK);
			break;
		default:
			Status = VL_ERROR_INVALID_PARAMS;
		}
	}

	/* Finally, the timing budget must be re-applied */
	if (Status == VL_ERROR_NONE) {
		VL_GETPARAMETERFIELD(Dev,
			MeasurementTimingBudgetMicroSeconds,
			MeasurementTimingBudgetMicroSeconds);

		Status = VL_SetMeasurementTimingBudgetMicroSeconds(Dev,
				MeasurementTimingBudgetMicroSeconds);
	}

	/* Perform the phase calibration. This is needed after changing on
	 * vcsel period.
	 * get_data_enable = 0, restore_config = 1 */
	if (Status == VL_ERROR_NONE)
		Status = VL_perform_phase_calibration(
			Dev, &PhaseCalInt, 0, 1);

	return Status;
}

int8_t VL_get_vcsel_pulse_period(struct vl_data *Dev,
	uint8_t VcselPeriodType, uint8_t *pVCSELPulsePeriodPCLK)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t vcsel_period_reg;

	switch (VcselPeriodType) {
	case VL_VCSEL_PERIOD_PRE_RANGE:
		Status = VL_RdByte(Dev,
			VL_REG_PRE_RANGE_CONFIG_VCSEL_PERIOD,
			&vcsel_period_reg);
	break;
	case VL_VCSEL_PERIOD_FINAL_RANGE:
		Status = VL_RdByte(Dev,
			VL_REG_FINAL_RANGE_CONFIG_VCSEL_PERIOD,
			&vcsel_period_reg);
	break;
	default:
		Status = VL_ERROR_INVALID_PARAMS;
	}

	if (Status == VL_ERROR_NONE)
		*pVCSELPulsePeriodPCLK =
			VL_decode_vcsel_period(vcsel_period_reg);

	return Status;
}



int8_t VL_set_measurement_timing_budget_micro_seconds(
	struct vl_data *Dev, uint32_t MeasurementTimingBudgetMicroSeconds)
{
	int8_t Status = VL_ERROR_NONE;
	uint32_t FinalRangeTimingBudgetMicroSeconds;
	struct VL_SchedulerSequenceSteps_t SchedulerSequenceSteps;
	uint32_t MsrcDccTccTimeoutMicroSeconds	= 2000;
	uint32_t StartOverheadMicroSeconds		= 1910;
	uint32_t EndOverheadMicroSeconds		= 960;
	uint32_t MsrcOverheadMicroSeconds		= 660;
	uint32_t TccOverheadMicroSeconds		= 590;
	uint32_t DssOverheadMicroSeconds		= 690;
	uint32_t PreRangeOverheadMicroSeconds	= 660;
	uint32_t FinalRangeOverheadMicroSeconds = 550;
	uint32_t PreRangeTimeoutMicroSeconds	= 0;
	uint32_t cMinTimingBudgetMicroSeconds	= 20000;
	uint32_t SubTimeout = 0;

	LOG_FUNCTION_START("");

	if (MeasurementTimingBudgetMicroSeconds
			< cMinTimingBudgetMicroSeconds) {
		Status = VL_ERROR_INVALID_PARAMS;
		return Status;
	}

	FinalRangeTimingBudgetMicroSeconds =
		MeasurementTimingBudgetMicroSeconds -
		(StartOverheadMicroSeconds + EndOverheadMicroSeconds);

	Status = VL_GetSequenceStepEnables(Dev, &SchedulerSequenceSteps);

	if (Status == VL_ERROR_NONE &&
		(SchedulerSequenceSteps.TccOn  ||
		SchedulerSequenceSteps.MsrcOn ||
		SchedulerSequenceSteps.DssOn)) {

		/* TCC, MSRC and DSS all share the same timeout */
		Status = get_sequence_step_timeout(Dev,
					VL_SEQUENCESTEP_MSRC,
					&MsrcDccTccTimeoutMicroSeconds);

		/* Subtract the TCC, MSRC and DSS timeouts if they are
		 * enabled. */

		if (Status != VL_ERROR_NONE)
			return Status;

		/* TCC */
		if (SchedulerSequenceSteps.TccOn) {

			SubTimeout = MsrcDccTccTimeoutMicroSeconds
				+ TccOverheadMicroSeconds;

			if (SubTimeout <
				FinalRangeTimingBudgetMicroSeconds) {
				FinalRangeTimingBudgetMicroSeconds -=
							SubTimeout;
			} else {
				/* Requested timeout too big. */
				Status = VL_ERROR_INVALID_PARAMS;
			}
		}

		if (Status != VL_ERROR_NONE) {
			LOG_FUNCTION_END(Status);
			return Status;
		}

		/* DSS */
		if (SchedulerSequenceSteps.DssOn) {

			SubTimeout = 2 * (MsrcDccTccTimeoutMicroSeconds +
				DssOverheadMicroSeconds);

			if (SubTimeout < FinalRangeTimingBudgetMicroSeconds) {
				FinalRangeTimingBudgetMicroSeconds
							-= SubTimeout;
			} else {
				/* Requested timeout too big. */
				Status = VL_ERROR_INVALID_PARAMS;
			}
		} else if (SchedulerSequenceSteps.MsrcOn) {
			/* MSRC */
			SubTimeout = MsrcDccTccTimeoutMicroSeconds +
						MsrcOverheadMicroSeconds;

			if (SubTimeout < FinalRangeTimingBudgetMicroSeconds) {
				FinalRangeTimingBudgetMicroSeconds
							-= SubTimeout;
			} else {
				/* Requested timeout too big. */
				Status = VL_ERROR_INVALID_PARAMS;
			}
		}

	}

	if (Status != VL_ERROR_NONE) {
		LOG_FUNCTION_END(Status);
		return Status;
	}

	if (SchedulerSequenceSteps.PreRangeOn) {

		/* Subtract the Pre-range timeout if enabled. */

		Status = get_sequence_step_timeout(Dev,
				VL_SEQUENCESTEP_PRE_RANGE,
				&PreRangeTimeoutMicroSeconds);

		SubTimeout = PreRangeTimeoutMicroSeconds +
				PreRangeOverheadMicroSeconds;

		if (SubTimeout < FinalRangeTimingBudgetMicroSeconds) {
			FinalRangeTimingBudgetMicroSeconds -= SubTimeout;
		} else {
			/* Requested timeout too big. */
			Status = VL_ERROR_INVALID_PARAMS;
		}
	}


	if (Status == VL_ERROR_NONE &&
		SchedulerSequenceSteps.FinalRangeOn) {

		FinalRangeTimingBudgetMicroSeconds -=
				FinalRangeOverheadMicroSeconds;

		/* Final Range Timeout
		 * Note that the final range timeout is determined by the timing
		 * budget and the sum of all other timeouts within the sequence.
		 * If there is no room for the final range timeout,then an error
		 * will be set. Otherwise the remaining time will be applied to
		 * the final range.
		 */
		Status = set_sequence_step_timeout(Dev,
			VL_SEQUENCESTEP_FINAL_RANGE,
			FinalRangeTimingBudgetMicroSeconds);

		VL_SETPARAMETERFIELD(Dev,
			MeasurementTimingBudgetMicroSeconds,
			MeasurementTimingBudgetMicroSeconds);
	}

	LOG_FUNCTION_END(Status);

	return Status;
}

int8_t VL_get_measurement_timing_budget_micro_seconds(
	struct vl_data *Dev, uint32_t *pMeasurementTimingBudgetMicroSeconds)
{
	int8_t Status = VL_ERROR_NONE;
	struct VL_SchedulerSequenceSteps_t SchedulerSequenceSteps;
	uint32_t FinalRangeTimeoutMicroSeconds;
	uint32_t MsrcDccTccTimeoutMicroSeconds	= 2000;
	uint32_t StartOverheadMicroSeconds		= 1910;
	uint32_t EndOverheadMicroSeconds		= 960;
	uint32_t MsrcOverheadMicroSeconds		= 660;
	uint32_t TccOverheadMicroSeconds		= 590;
	uint32_t DssOverheadMicroSeconds		= 690;
	uint32_t PreRangeOverheadMicroSeconds	= 660;
	uint32_t FinalRangeOverheadMicroSeconds = 550;
	uint32_t PreRangeTimeoutMicroSeconds	= 0;

	LOG_FUNCTION_START("");

	/* Start and end overhead times always present */
	*pMeasurementTimingBudgetMicroSeconds
		= StartOverheadMicroSeconds + EndOverheadMicroSeconds;

	Status = VL_GetSequenceStepEnables(Dev, &SchedulerSequenceSteps);

	if (Status != VL_ERROR_NONE) {
		LOG_FUNCTION_END(Status);
		return Status;
	}


	if (SchedulerSequenceSteps.TccOn  ||
		SchedulerSequenceSteps.MsrcOn ||
		SchedulerSequenceSteps.DssOn) {

		Status = get_sequence_step_timeout(Dev,
				VL_SEQUENCESTEP_MSRC,
				&MsrcDccTccTimeoutMicroSeconds);

		if (Status == VL_ERROR_NONE) {
			if (SchedulerSequenceSteps.TccOn) {
				*pMeasurementTimingBudgetMicroSeconds +=
					MsrcDccTccTimeoutMicroSeconds +
					TccOverheadMicroSeconds;
			}

			if (SchedulerSequenceSteps.DssOn) {
				*pMeasurementTimingBudgetMicroSeconds +=
				2 * (MsrcDccTccTimeoutMicroSeconds +
					DssOverheadMicroSeconds);
			} else if (SchedulerSequenceSteps.MsrcOn) {
				*pMeasurementTimingBudgetMicroSeconds +=
					MsrcDccTccTimeoutMicroSeconds +
					MsrcOverheadMicroSeconds;
			}
		}
	}

	if (Status == VL_ERROR_NONE) {
		if (SchedulerSequenceSteps.PreRangeOn) {
			Status = get_sequence_step_timeout(Dev,
				VL_SEQUENCESTEP_PRE_RANGE,
				&PreRangeTimeoutMicroSeconds);
			*pMeasurementTimingBudgetMicroSeconds +=
				PreRangeTimeoutMicroSeconds +
				PreRangeOverheadMicroSeconds;
		}
	}

	if (Status == VL_ERROR_NONE) {
		if (SchedulerSequenceSteps.FinalRangeOn) {
			Status = get_sequence_step_timeout(Dev,
					VL_SEQUENCESTEP_FINAL_RANGE,
					&FinalRangeTimeoutMicroSeconds);
			*pMeasurementTimingBudgetMicroSeconds +=
				(FinalRangeTimeoutMicroSeconds +
				FinalRangeOverheadMicroSeconds);
		}
	}

	if (Status == VL_ERROR_NONE) {
		VL_SETPARAMETERFIELD(Dev,
			MeasurementTimingBudgetMicroSeconds,
			*pMeasurementTimingBudgetMicroSeconds);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}



int8_t VL_load_tuning_settings(struct vl_data *Dev,
		uint8_t *pTuningSettingBuffer)
{
	int8_t Status = VL_ERROR_NONE;
	int i;
	int Index;
	uint8_t msb;
	uint8_t lsb;
	uint8_t SelectParam;
	uint8_t NumberOfWrites;
	uint8_t Address;
	uint8_t localBuffer[4]; /* max */
	uint16_t Temp16;

	LOG_FUNCTION_START("");

	Index = 0;

	while ((*(pTuningSettingBuffer + Index) != 0) &&
			(Status == VL_ERROR_NONE)) {
		NumberOfWrites = *(pTuningSettingBuffer + Index);
		Index++;
		if (NumberOfWrites == 0xFF) {
			/* internal parameters */
			SelectParam = *(pTuningSettingBuffer + Index);
			Index++;
			switch (SelectParam) {
			case 0: /* uint16_t SigmaEstRefArray -> 2 bytes */
				msb = *(pTuningSettingBuffer + Index);
				Index++;
				lsb = *(pTuningSettingBuffer + Index);
				Index++;
				Temp16 = VL_MAKEUINT16(lsb, msb);
				PALDevDataSet(Dev, SigmaEstRefArray, Temp16);
				break;
			case 1: /* uint16_t SigmaEstEffPulseWidth -> 2 bytes */
				msb = *(pTuningSettingBuffer + Index);
				Index++;
				lsb = *(pTuningSettingBuffer + Index);
				Index++;
				Temp16 = VL_MAKEUINT16(lsb, msb);
				PALDevDataSet(Dev, SigmaEstEffPulseWidth,
					Temp16);
				break;
			case 2: /* uint16_t SigmaEstEffAmbWidth -> 2 bytes */
				msb = *(pTuningSettingBuffer + Index);
				Index++;
				lsb = *(pTuningSettingBuffer + Index);
				Index++;
				Temp16 = VL_MAKEUINT16(lsb, msb);
				PALDevDataSet(Dev, SigmaEstEffAmbWidth, Temp16);
				break;
			case 3: /* uint16_t targetRefRate -> 2 bytes */
				msb = *(pTuningSettingBuffer + Index);
				Index++;
				lsb = *(pTuningSettingBuffer + Index);
				Index++;
				Temp16 = VL_MAKEUINT16(lsb, msb);
				PALDevDataSet(Dev, targetRefRate, Temp16);
				break;
			default: /* invalid parameter */
				Status = VL_ERROR_INVALID_PARAMS;
			}

		} else if (NumberOfWrites <= 4) {
			Address = *(pTuningSettingBuffer + Index);
			Index++;

			for (i = 0; i < NumberOfWrites; i++) {
				localBuffer[i] = *(pTuningSettingBuffer +
							Index);
				Index++;
			}

			Status = VL_WriteMulti(Dev, Address, localBuffer,
					NumberOfWrites);

		} else {
			Status = VL_ERROR_INVALID_PARAMS;
		}
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_get_total_xtalk_rate(struct vl_data *Dev,
	struct VL_RangingMeasurementData_t *pRangingMeasurementData,
	unsigned int *ptotal_xtalk_rate_mcps)
{
	int8_t Status = VL_ERROR_NONE;

	uint8_t xtalkCompEnable;
	unsigned int totalXtalkMegaCps;
	unsigned int xtalkPerSpadMegaCps;

	*ptotal_xtalk_rate_mcps = 0;

	Status = VL_GetXTalkCompensationEnable(Dev, &xtalkCompEnable);
	if (Status == VL_ERROR_NONE) {

		if (xtalkCompEnable) {

			VL_GETPARAMETERFIELD(
				Dev,
				XTalkCompensationRateMegaCps,
				xtalkPerSpadMegaCps);

			/* FixPoint1616 * FixPoint 8:8 = FixPoint0824 */
			totalXtalkMegaCps =
				pRangingMeasurementData->EffectiveSpadRtnCount *
				xtalkPerSpadMegaCps;

			/* FixPoint0824 >> 8 = FixPoint1616 */
			*ptotal_xtalk_rate_mcps =
				(totalXtalkMegaCps + 0x80) >> 8;
		}
	}

	return Status;
}

int8_t VL_get_total_signal_rate(struct vl_data *Dev,
	struct VL_RangingMeasurementData_t *pRangingMeasurementData,
	unsigned int *ptotal_signal_rate_mcps)
{
	int8_t Status = VL_ERROR_NONE;
	unsigned int totalXtalkMegaCps;

	LOG_FUNCTION_START("");

	*ptotal_signal_rate_mcps =
		pRangingMeasurementData->SignalRateRtnMegaCps;

	Status = VL_get_total_xtalk_rate(
		Dev, pRangingMeasurementData, &totalXtalkMegaCps);

	if (Status == VL_ERROR_NONE)
		*ptotal_signal_rate_mcps += totalXtalkMegaCps;

	return Status;
}

int8_t VL_calc_dmax(
	struct vl_data *Dev,
	unsigned int totalSignalRate_mcps,
	unsigned int totalCorrSignalRate_mcps,
	unsigned int pwMult,
	uint32_t sigmaEstimateP1,
	unsigned int sigmaEstimateP2,
	uint32_t peakVcselDuration_us,
	uint32_t *pdmax_mm)
{
	const uint32_t cSigmaLimit		= 18;
	const unsigned int cSignalLimit	= 0x4000; /* 0.25 */
	const unsigned int cSigmaEstRef	= 0x00000042; /* 0.001 */
	const uint32_t cAmbEffWidthSigmaEst_ns = 6;
	const uint32_t cAmbEffWidthDMax_ns	   = 7;
	uint32_t dmaxCalRange_mm;
	unsigned int dmaxCalSignalRateRtn_mcps;
	unsigned int minSignalNeeded;
	unsigned int minSignalNeeded_p1;
	unsigned int minSignalNeeded_p2;
	unsigned int minSignalNeeded_p3;
	unsigned int minSignalNeeded_p4;
	unsigned int sigmaLimitTmp;
	unsigned int sigmaEstSqTmp;
	unsigned int signalLimitTmp;
	unsigned int SignalAt0mm;
	unsigned int dmaxDark;
	unsigned int dmaxAmbient;
	unsigned int dmaxDarkTmp;
	unsigned int sigmaEstP2Tmp;
	uint32_t signalRateTemp_mcps;

	int8_t Status = VL_ERROR_NONE;

	LOG_FUNCTION_START("");

	dmaxCalRange_mm =
		PALDevDataGet(Dev, DmaxCalRangeMilliMeter);

	dmaxCalSignalRateRtn_mcps =
		PALDevDataGet(Dev, DmaxCalSignalRateRtnMegaCps);

	/* uint32 * FixPoint1616 = FixPoint1616 */
	SignalAt0mm = dmaxCalRange_mm * dmaxCalSignalRateRtn_mcps;

	/* FixPoint1616 >> 8 = FixPoint2408 */
	SignalAt0mm = (SignalAt0mm + 0x80) >> 8;
	SignalAt0mm *= dmaxCalRange_mm;

	minSignalNeeded_p1 = 0;
	if (totalCorrSignalRate_mcps > 0) {

		/* Shift by 10 bits to increase resolution prior to the
		 * division */
		signalRateTemp_mcps = totalSignalRate_mcps << 10;

		/* Add rounding value prior to division */
		minSignalNeeded_p1 = signalRateTemp_mcps +
			(totalCorrSignalRate_mcps/2);

		/* FixPoint0626/FixPoint1616 = FixPoint2210 */
		minSignalNeeded_p1 /= totalCorrSignalRate_mcps;

		/* Apply a factored version of the speed of light.
		 Correction to be applied at the end */
		minSignalNeeded_p1 *= 3;

		/* FixPoint2210 * FixPoint2210 = FixPoint1220 */
		minSignalNeeded_p1 *= minSignalNeeded_p1;

		/* FixPoint1220 >> 16 = FixPoint2804 */
		minSignalNeeded_p1 = (minSignalNeeded_p1 + 0x8000) >> 16;
	}

	minSignalNeeded_p2 = pwMult * sigmaEstimateP1;

	/* FixPoint1616 >> 16 =	 uint32 */
	minSignalNeeded_p2 = (minSignalNeeded_p2 + 0x8000) >> 16;

	/* uint32 * uint32	=  uint32 */
	minSignalNeeded_p2 *= minSignalNeeded_p2;

	/* Check sigmaEstimateP2
	 * If this value is too high there is not enough signal rate
	 * to calculate dmax value so set a suitable value to ensure
	 * a very small dmax.
	 */
	sigmaEstP2Tmp = (sigmaEstimateP2 + 0x8000) >> 16;
	sigmaEstP2Tmp = (sigmaEstP2Tmp + cAmbEffWidthSigmaEst_ns/2)/
		cAmbEffWidthSigmaEst_ns;
	sigmaEstP2Tmp *= cAmbEffWidthDMax_ns;

	if (sigmaEstP2Tmp > 0xffff) {
		minSignalNeeded_p3 = 0xfff00000;
	} else {

		/* DMAX uses a different ambient width from sigma, so apply
		 * correction.
		 * Perform division before multiplication to prevent overflow.
		 */
		sigmaEstimateP2 = (sigmaEstimateP2 + cAmbEffWidthSigmaEst_ns/2)/
			cAmbEffWidthSigmaEst_ns;
		sigmaEstimateP2 *= cAmbEffWidthDMax_ns;

		/* FixPoint1616 >> 16 = uint32 */
		minSignalNeeded_p3 = (sigmaEstimateP2 + 0x8000) >> 16;

		minSignalNeeded_p3 *= minSignalNeeded_p3;

	}

	/* FixPoint1814 / uint32 = FixPoint1814 */
	sigmaLimitTmp = ((cSigmaLimit << 14) + 500) / 1000;

	/* FixPoint1814 * FixPoint1814 = FixPoint3628 := FixPoint0428 */
	sigmaLimitTmp *= sigmaLimitTmp;

	/* FixPoint1616 * FixPoint1616 = FixPoint3232 */
	sigmaEstSqTmp = cSigmaEstRef * cSigmaEstRef;

	/* FixPoint3232 >> 4 = FixPoint0428 */
	sigmaEstSqTmp = (sigmaEstSqTmp + 0x08) >> 4;

	/* FixPoint0428 - FixPoint0428	= FixPoint0428 */
	sigmaLimitTmp -=  sigmaEstSqTmp;

	/* uint32_t * FixPoint0428 = FixPoint0428 */
	minSignalNeeded_p4 = 4 * 12 * sigmaLimitTmp;

	/* FixPoint0428 >> 14 = FixPoint1814 */
	minSignalNeeded_p4 = (minSignalNeeded_p4 + 0x2000) >> 14;

	/* uint32 + uint32 = uint32 */
	minSignalNeeded = (minSignalNeeded_p2 + minSignalNeeded_p3);

	/* uint32 / uint32 = uint32 */
	minSignalNeeded += (peakVcselDuration_us/2);
	minSignalNeeded /= peakVcselDuration_us;

	/* uint32 << 14 = FixPoint1814 */
	minSignalNeeded <<= 14;

	/* FixPoint1814 / FixPoint1814 = uint32 */
	minSignalNeeded += (minSignalNeeded_p4/2);
	minSignalNeeded /= minSignalNeeded_p4;

	/* FixPoint3200 * FixPoint2804 := FixPoint2804*/
	minSignalNeeded *= minSignalNeeded_p1;

	/* Apply correction by dividing by 1000000.
	 * This assumes 10E16 on the numerator of the equation
	 * and 10E-22 on the denominator.
	 * We do this because 32bit fix point calculation can't
	 * handle the larger and smaller elements of this equation,
	 * i.e. speed of light and pulse widths.
	 */
	minSignalNeeded = (minSignalNeeded + 500) / 1000;
	minSignalNeeded <<= 4;

	minSignalNeeded = (minSignalNeeded + 500) / 1000;

	/* FixPoint1616 >> 8 = FixPoint2408 */
	signalLimitTmp = (cSignalLimit + 0x80) >> 8;

	/* FixPoint2408/FixPoint2408 = uint32 */
	if (signalLimitTmp != 0)
		dmaxDarkTmp = (SignalAt0mm + (signalLimitTmp / 2))
			/ signalLimitTmp;
	else
		dmaxDarkTmp = 0;

	dmaxDark = VL_isqrt(dmaxDarkTmp);

	/* FixPoint2408/FixPoint2408 = uint32 */
	if (minSignalNeeded != 0)
		dmaxAmbient = (SignalAt0mm + minSignalNeeded/2)
			/ minSignalNeeded;
	else
		dmaxAmbient = 0;

	dmaxAmbient = VL_isqrt(dmaxAmbient);

	*pdmax_mm = dmaxDark;
	if (dmaxDark > dmaxAmbient)
		*pdmax_mm = dmaxAmbient;

	LOG_FUNCTION_END(Status);

	return Status;
}


int8_t VL_calc_sigma_estimate(struct vl_data *Dev,
	struct VL_RangingMeasurementData_t *pRangingMeasurementData,
	unsigned int *pSigmaEstimate,
	uint32_t *pDmax_mm)
{
	/* Expressed in 100ths of a ns, i.e. centi-ns */
	const uint32_t cPulseEffectiveWidth_centi_ns   = 800;
	/* Expressed in 100ths of a ns, i.e. centi-ns */
	const uint32_t cAmbientEffectiveWidth_centi_ns = 600;
	/* 25ms */
	const unsigned int cDfltFinalRangeIntegrationTimeMilliSecs =
		0x00190000;
	const uint32_t cVcselPulseWidth_ps	= 4700; /* pico secs */
	const unsigned int cSigmaEstMax	= 0x028F87AE;
	const unsigned int cSigmaEstRtnMax	= 0xF000;
	const unsigned int cAmbToSignalRatioMax = 0xF0000000/
		cAmbientEffectiveWidth_centi_ns;
	/* Time Of Flight per mm (6.6 pico secs) */
	const unsigned int cTOF_per_mm_ps		= 0x0006999A;
	const uint32_t c16BitRoundingParam		= 0x00008000;
	const unsigned int cMaxXTalk_kcps		= 0x00320000;
	const uint32_t cPllPeriod_ps			= 1655;

	uint32_t vcselTotalEventsRtn;
	uint32_t finalRangeTimeoutMicroSecs;
	uint32_t preRangeTimeoutMicroSecs;
	uint32_t finalRangeIntegrationTimeMilliSecs;
	unsigned int sigmaEstimateP1;
	unsigned int sigmaEstimateP2;
	unsigned int sigmaEstimateP3;
	unsigned int deltaT_ps;
	unsigned int pwMult;
	unsigned int sigmaEstRtn;
	unsigned int sigmaEstimate;
	unsigned int xTalkCorrection;
	unsigned int ambientRate_kcps;
	unsigned int peakSignalRate_kcps;
	unsigned int xTalkCompRate_mcps;
	uint32_t xTalkCompRate_kcps;
	int8_t Status = VL_ERROR_NONE;
	unsigned int diff1_mcps;
	unsigned int diff2_mcps;
	unsigned int sqr1;
	unsigned int sqr2;
	unsigned int sqrSum;
	unsigned int sqrtResult_centi_ns;
	unsigned int sqrtResult;
	unsigned int totalSignalRate_mcps;
	unsigned int correctedSignalRate_mcps;
	unsigned int sigmaEstRef;
	uint32_t vcselWidth;
	uint32_t finalRangeMacroPCLKS;
	uint32_t preRangeMacroPCLKS;
	uint32_t peakVcselDuration_us;
	uint8_t finalRangeVcselPCLKS;
	uint8_t preRangeVcselPCLKS;
	/*! \addtogroup calc_sigma_estimate
	 * @{
	 *
	 * Estimates the range sigma
	 */

	LOG_FUNCTION_START("");

	VL_GETPARAMETERFIELD(Dev, XTalkCompensationRateMegaCps,
			xTalkCompRate_mcps);

	/*
	 * We work in kcps rather than mcps as this helps keep within the
	 * confines of the 32 Fix1616 type.
	 */

	ambientRate_kcps =
		(pRangingMeasurementData->AmbientRateRtnMegaCps * 1000) >> 16;

	correctedSignalRate_mcps =
		pRangingMeasurementData->SignalRateRtnMegaCps;


	Status = VL_get_total_signal_rate(
		Dev, pRangingMeasurementData, &totalSignalRate_mcps);
	Status = VL_get_total_xtalk_rate(
		Dev, pRangingMeasurementData, &xTalkCompRate_mcps);


	/* Signal rate measurement provided by device is the
	 * peak signal rate, not average.
	 */
	peakSignalRate_kcps = (totalSignalRate_mcps * 1000);
	peakSignalRate_kcps = (peakSignalRate_kcps + 0x8000) >> 16;

	xTalkCompRate_kcps = xTalkCompRate_mcps * 1000;

	if (xTalkCompRate_kcps > cMaxXTalk_kcps)
		xTalkCompRate_kcps = cMaxXTalk_kcps;

	if (Status == VL_ERROR_NONE) {

		/* Calculate final range macro periods */
		finalRangeTimeoutMicroSecs = VL_GETDEVICESPECIFICPARAMETER(
			Dev, FinalRangeTimeoutMicroSecs);

		finalRangeVcselPCLKS = VL_GETDEVICESPECIFICPARAMETER(
			Dev, FinalRangeVcselPulsePeriod);

		finalRangeMacroPCLKS = VL_calc_timeout_mclks(
			Dev, finalRangeTimeoutMicroSecs, finalRangeVcselPCLKS);

		/* Calculate pre-range macro periods */
		preRangeTimeoutMicroSecs = VL_GETDEVICESPECIFICPARAMETER(
			Dev, PreRangeTimeoutMicroSecs);

		preRangeVcselPCLKS = VL_GETDEVICESPECIFICPARAMETER(
			Dev, PreRangeVcselPulsePeriod);

		preRangeMacroPCLKS = VL_calc_timeout_mclks(
			Dev, preRangeTimeoutMicroSecs, preRangeVcselPCLKS);

		vcselWidth = 3;
		if (finalRangeVcselPCLKS == 8)
			vcselWidth = 2;


		peakVcselDuration_us = vcselWidth * 2048 *
			(preRangeMacroPCLKS + finalRangeMacroPCLKS);
		peakVcselDuration_us = (peakVcselDuration_us + 500)/1000;
		peakVcselDuration_us *= cPllPeriod_ps;
		peakVcselDuration_us = (peakVcselDuration_us + 500)/1000;

		/* Fix1616 >> 8 = Fix2408 */
		totalSignalRate_mcps = (totalSignalRate_mcps + 0x80) >> 8;

		/* Fix2408 * uint32 = Fix2408 */
		vcselTotalEventsRtn = totalSignalRate_mcps *
			peakVcselDuration_us;

		/* Fix2408 >> 8 = uint32 */
		vcselTotalEventsRtn = (vcselTotalEventsRtn + 0x80) >> 8;

		/* Fix2408 << 8 = Fix1616 = */
		totalSignalRate_mcps <<= 8;
	}

	if (Status != VL_ERROR_NONE) {
		LOG_FUNCTION_END(Status);
		return Status;
	}

	if (peakSignalRate_kcps == 0) {
		*pSigmaEstimate = cSigmaEstMax;
		PALDevDataSet(Dev, SigmaEstimate, cSigmaEstMax);
		*pDmax_mm = 0;
	} else {
		if (vcselTotalEventsRtn < 1)
			vcselTotalEventsRtn = 1;

		sigmaEstimateP1 = cPulseEffectiveWidth_centi_ns;

		/* ((FixPoint1616 << 16)* uint32)/uint32 = FixPoint1616 */
		sigmaEstimateP2 = (ambientRate_kcps << 16)/peakSignalRate_kcps;
		if (sigmaEstimateP2 > cAmbToSignalRatioMax) {
			/* Clip to prevent overflow. Will ensure safe
			 * max result. */
			sigmaEstimateP2 = cAmbToSignalRatioMax;
		}
		sigmaEstimateP2 *= cAmbientEffectiveWidth_centi_ns;

		sigmaEstimateP3 = 2 * VL_isqrt(vcselTotalEventsRtn * 12);

		/* uint32 * FixPoint1616 = FixPoint1616 */
		deltaT_ps = pRangingMeasurementData->RangeMilliMeter *
					cTOF_per_mm_ps;

		/*
		 * vcselRate - xtalkCompRate
		 * (uint32 << 16) - FixPoint1616 = FixPoint1616.
		 * Divide result by 1000 to convert to mcps.
		 * 500 is added to ensure rounding when integer division
		 * truncates.
		 */
		diff1_mcps = (((peakSignalRate_kcps << 16) -
			2 * xTalkCompRate_kcps) + 500)/1000;

		/* vcselRate + xtalkCompRate */
		diff2_mcps = ((peakSignalRate_kcps << 16) + 500)/1000;

		/* Shift by 8 bits to increase resolution prior to the
		 * division */
		diff1_mcps <<= 8;

		/* FixPoint0824/FixPoint1616 = FixPoint2408 */
		xTalkCorrection	 = abs(diff1_mcps/diff2_mcps);

		/* FixPoint2408 << 8 = FixPoint1616 */
		xTalkCorrection <<= 8;

		if (pRangingMeasurementData->RangeStatus != 0) {
			pwMult = 1 << 16;
		} else {
			/* FixPoint1616/uint32 = FixPoint1616 *i/
			/* smaller than 1.0f */
			pwMult = deltaT_ps/cVcselPulseWidth_ps;

		/*
		 * FixPoint1616 * FixPoint1616 = FixPoint3232, however both
		 * values are small enough such that32 bits will not be
		 * exceeded.
		 */
			pwMult *= ((1 << 16) - xTalkCorrection);

			/* (FixPoint3232 >> 16) = FixPoint1616 */
			pwMult =  (pwMult + c16BitRoundingParam) >> 16;

			/* FixPoint1616 + FixPoint1616 = FixPoint1616 */
			pwMult += (1 << 16);

		/*
		 * At this point the value will be 1.xx, therefore if we square
		 * the value this will exceed 32 bits. To address this perform
		 * a single shift to the right before the multiplication.
		 */
			pwMult >>= 1;
			/* FixPoint1715 * FixPoint1715 = FixPoint3430 */
			pwMult = pwMult * pwMult;

			/* (FixPoint3430 >> 14) = Fix1616 */
			pwMult >>= 14;
		}

		/* FixPoint1616 * uint32 = FixPoint1616 */
		sqr1 = pwMult * sigmaEstimateP1;

		/* (FixPoint1616 >> 16) = FixPoint3200 */
		sqr1 = (sqr1 + 0x8000) >> 16;

		/* FixPoint3200 * FixPoint3200 = FixPoint6400 */
		sqr1 *= sqr1;

		sqr2 = sigmaEstimateP2;

		/* (FixPoint1616 >> 16) = FixPoint3200 */
		sqr2 = (sqr2 + 0x8000) >> 16;

		/* FixPoint3200 * FixPoint3200 = FixPoint6400 */
		sqr2 *= sqr2;

		/* FixPoint64000 + FixPoint6400 = FixPoint6400 */
		sqrSum = sqr1 + sqr2;

		/* SQRT(FixPoin6400) = FixPoint3200 */
		sqrtResult_centi_ns = VL_isqrt(sqrSum);

		/* (FixPoint3200 << 16) = FixPoint1616 */
		sqrtResult_centi_ns <<= 16;

		/*
		 * Note that the Speed Of Light is expressed in um per 1E-10
		 * seconds (2997) Therefore to get mm/ns we have to divide by
		 * 10000
		 */
		sigmaEstRtn = (((sqrtResult_centi_ns+50)/100) /
				sigmaEstimateP3);
		sigmaEstRtn		 *= VL_SPEED_OF_LIGHT_IN_AIR;

		/* Add 5000 before dividing by 10000 to ensure rounding. */
		sigmaEstRtn		 += 5000;
		sigmaEstRtn		 /= 10000;

		if (sigmaEstRtn > cSigmaEstRtnMax) {
			/* Clip to prevent overflow. Will ensure safe
			 * max result. */
			sigmaEstRtn = cSigmaEstRtnMax;
		}
		finalRangeIntegrationTimeMilliSecs =
		    (finalRangeTimeoutMicroSecs +
			preRangeTimeoutMicroSecs + 500)/1000;

		/* sigmaEstRef = 1mm * 25ms/final range integration time
		 * (inc pre-range) sqrt(FixPoint1616/int) = FixPoint2408)
		 */
		sigmaEstRef =
			VL_isqrt((cDfltFinalRangeIntegrationTimeMilliSecs +
				finalRangeIntegrationTimeMilliSecs/2)/
				finalRangeIntegrationTimeMilliSecs);

		/* FixPoint2408 << 8 = FixPoint1616 */
		sigmaEstRef <<= 8;
		sigmaEstRef = (sigmaEstRef + 500)/1000;

		/* FixPoint1616 * FixPoint1616 = FixPoint3232 */
		sqr1 = sigmaEstRtn * sigmaEstRtn;
		/* FixPoint1616 * FixPoint1616 = FixPoint3232 */
		sqr2 = sigmaEstRef * sigmaEstRef;

		/* sqrt(FixPoint3232) = FixPoint1616 */
		sqrtResult = VL_isqrt((sqr1 + sqr2));
		/*
		 * Note that the Shift by 4 bits increases resolution prior to
		 * the sqrt, therefore the result must be shifted by 2 bits to
		 * the right to revert back to the FixPoint1616 format.
		 */

		sigmaEstimate	 = 1000 * sqrtResult;

		if ((peakSignalRate_kcps < 1) || (vcselTotalEventsRtn < 1) ||
				(sigmaEstimate > cSigmaEstMax)) {
				sigmaEstimate = cSigmaEstMax;
		}

		*pSigmaEstimate = (uint32_t)(sigmaEstimate);
		PALDevDataSet(Dev, SigmaEstimate, *pSigmaEstimate);
		Status = VL_calc_dmax(
			Dev,
			totalSignalRate_mcps,
			correctedSignalRate_mcps,
			pwMult,
			sigmaEstimateP1,
			sigmaEstimateP2,
			peakVcselDuration_us,
			pDmax_mm);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

int8_t VL_get_pal_range_status(struct vl_data *Dev,
		uint8_t DeviceRangeStatus,
		unsigned int SignalRate,
		uint16_t EffectiveSpadRtnCount,
		struct VL_RangingMeasurementData_t *pRangingMeasurementData,
		uint8_t *pPalRangeStatus)
{
	int8_t Status = VL_ERROR_NONE;
	uint8_t NoneFlag;
	uint8_t SigmaLimitflag = 0;
	uint8_t SignalRefClipflag = 0;
	uint8_t RangeIgnoreThresholdflag = 0;
	uint8_t SigmaLimitCheckEnable = 0;
	uint8_t SignalRateFinalRangeLimitCheckEnable = 0;
	uint8_t SignalRefClipLimitCheckEnable = 0;
	uint8_t RangeIgnoreThresholdLimitCheckEnable = 0;
	unsigned int SigmaEstimate;
	unsigned int SigmaLimitValue;
	unsigned int SignalRefClipValue;
	unsigned int RangeIgnoreThresholdValue;
	unsigned int SignalRatePerSpad;
	uint8_t DeviceRangeStatusInternal = 0;
	uint16_t tmpWord = 0;
	uint8_t Temp8;
	uint32_t Dmax_mm = 0;
	unsigned int LastSignalRefMcps;

	LOG_FUNCTION_START("");


	/*
	 * VL53L0X has a good ranging when the value of the
	 * DeviceRangeStatus = 11. This function will replace the value 0 with
	 * the value 11 in the DeviceRangeStatus.
	 * In addition, the SigmaEstimator is not included in the VL53L0X
	 * DeviceRangeStatus, this will be added in the PalRangeStatus.
	 */

	DeviceRangeStatusInternal = ((DeviceRangeStatus & 0x78) >> 3);

	if (DeviceRangeStatusInternal == 0 ||
		DeviceRangeStatusInternal == 5 ||
		DeviceRangeStatusInternal == 7 ||
		DeviceRangeStatusInternal == 12 ||
		DeviceRangeStatusInternal == 13 ||
		DeviceRangeStatusInternal == 14 ||
		DeviceRangeStatusInternal == 15
			) {
		NoneFlag = 1;
	} else {
		NoneFlag = 0;
	}

	/*
	 * Check if Sigma limit is enabled, if yes then do comparison with limit
	 * value and put the result back into pPalRangeStatus.
	 */
	if (Status == VL_ERROR_NONE)
		Status =  VL_GetLimitCheckEnable(Dev,
			VL_CHECKENABLE_SIGMA_FINAL_RANGE,
			&SigmaLimitCheckEnable);

	if ((SigmaLimitCheckEnable != 0) && (Status == VL_ERROR_NONE)) {
		/*
		* compute the Sigma and check with limit
		*/
		Status = VL_calc_sigma_estimate(
			Dev,
			pRangingMeasurementData,
			&SigmaEstimate,
			&Dmax_mm);
		if (Status == VL_ERROR_NONE)
			pRangingMeasurementData->RangeDMaxMilliMeter = Dmax_mm;

		if (Status == VL_ERROR_NONE) {
			Status = VL_GetLimitCheckValue(Dev,
				VL_CHECKENABLE_SIGMA_FINAL_RANGE,
				&SigmaLimitValue);

			if ((SigmaLimitValue > 0) &&
				(SigmaEstimate > SigmaLimitValue))
					/* Limit Fail */
					SigmaLimitflag = 1;
		}
	}

	/*
	 * Check if Signal ref clip limit is enabled, if yes then do comparison
	 * with limit value and put the result back into pPalRangeStatus.
	 */
	if (Status == VL_ERROR_NONE)
		Status =  VL_GetLimitCheckEnable(Dev,
				VL_CHECKENABLE_SIGNAL_REF_CLIP,
				&SignalRefClipLimitCheckEnable);

	if ((SignalRefClipLimitCheckEnable != 0) &&
			(Status == VL_ERROR_NONE)) {

		Status = VL_GetLimitCheckValue(Dev,
				VL_CHECKENABLE_SIGNAL_REF_CLIP,
				&SignalRefClipValue);

		/* Read LastSignalRefMcps from device */
		if (Status == VL_ERROR_NONE)
			Status = VL_WrByte(Dev, 0xFF, 0x01);

		if (Status == VL_ERROR_NONE)
			Status = VL_RdWord(Dev,
				VL_REG_RESULT_PEAK_SIGNAL_RATE_REF,
				&tmpWord);

		if (Status == VL_ERROR_NONE)
			Status = VL_WrByte(Dev, 0xFF, 0x00);

		LastSignalRefMcps = VL_FIXPOINT97TOFIXPOINT1616(tmpWord);
		PALDevDataSet(Dev, LastSignalRefMcps, LastSignalRefMcps);

		if ((SignalRefClipValue > 0) &&
				(LastSignalRefMcps > SignalRefClipValue)) {
			/* Limit Fail */
			SignalRefClipflag = 1;
		}
	}

	/*
	 * Check if Signal ref clip limit is enabled, if yes then do comparison
	 * with limit value and put the result back into pPalRangeStatus.
	 * EffectiveSpadRtnCount has a format 8.8
	 * If (Return signal rate < (1.5 x Xtalk x number of Spads)) : FAIL
	 */
	if (Status == VL_ERROR_NONE)
		Status =  VL_GetLimitCheckEnable(Dev,
				VL_CHECKENABLE_RANGE_IGNORE_THRESHOLD,
				&RangeIgnoreThresholdLimitCheckEnable);

	if ((RangeIgnoreThresholdLimitCheckEnable != 0) &&
			(Status == VL_ERROR_NONE)) {

		/* Compute the signal rate per spad */
		if (EffectiveSpadRtnCount == 0) {
			SignalRatePerSpad = 0;
		} else {
			SignalRatePerSpad = (unsigned int)((256 * SignalRate)
				/ EffectiveSpadRtnCount);
		}

		Status = VL_GetLimitCheckValue(Dev,
				VL_CHECKENABLE_RANGE_IGNORE_THRESHOLD,
				&RangeIgnoreThresholdValue);

		if ((RangeIgnoreThresholdValue > 0) &&
			(SignalRatePerSpad < RangeIgnoreThresholdValue)) {
			/* Limit Fail add 2^6 to range status */
			RangeIgnoreThresholdflag = 1;
		}
	}

	if (Status == VL_ERROR_NONE) {
		if (NoneFlag == 1) {
			*pPalRangeStatus = 255;	 /* NONE */
		} else if (DeviceRangeStatusInternal == 1 ||
					DeviceRangeStatusInternal == 2 ||
					DeviceRangeStatusInternal == 3) {
			*pPalRangeStatus = 5; /* HW fail */
		} else if (DeviceRangeStatusInternal == 6 ||
					DeviceRangeStatusInternal == 9) {
			*pPalRangeStatus = 4;  /* Phase fail */
		} else if (DeviceRangeStatusInternal == 8 ||
					DeviceRangeStatusInternal == 10 ||
					SignalRefClipflag == 1) {
			*pPalRangeStatus = 3;  /* Min range */
		} else if (DeviceRangeStatusInternal == 4 ||
					RangeIgnoreThresholdflag == 1) {
			*pPalRangeStatus = 2;  /* Signal Fail */
		} else if (SigmaLimitflag == 1) {
			*pPalRangeStatus = 1;  /* Sigma	 Fail */
		} else {
			*pPalRangeStatus = 0; /* Range Valid */
		}
	}

	/* DMAX only relevant during range error */
	if (*pPalRangeStatus == 0)
		pRangingMeasurementData->RangeDMaxMilliMeter = 0;

	/* fill the Limit Check Status */

	Status =  VL_GetLimitCheckEnable(Dev,
			VL_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
			&SignalRateFinalRangeLimitCheckEnable);

	if (Status == VL_ERROR_NONE) {
		if ((SigmaLimitCheckEnable == 0) || (SigmaLimitflag == 1))
			Temp8 = 1;
		else
			Temp8 = 0;
		VL_SETARRAYPARAMETERFIELD(Dev, LimitChecksStatus,
				VL_CHECKENABLE_SIGMA_FINAL_RANGE, Temp8);

		if ((DeviceRangeStatusInternal == 4) ||
				(SignalRateFinalRangeLimitCheckEnable == 0))
			Temp8 = 1;
		else
			Temp8 = 0;
		VL_SETARRAYPARAMETERFIELD(Dev, LimitChecksStatus,
				VL_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
				Temp8);

		if ((SignalRefClipLimitCheckEnable == 0) ||
					(SignalRefClipflag == 1))
			Temp8 = 1;
		else
			Temp8 = 0;

		VL_SETARRAYPARAMETERFIELD(Dev, LimitChecksStatus,
				VL_CHECKENABLE_SIGNAL_REF_CLIP, Temp8);

		if ((RangeIgnoreThresholdLimitCheckEnable == 0) ||
				(RangeIgnoreThresholdflag == 1))
			Temp8 = 1;
		else
			Temp8 = 0;

		VL_SETARRAYPARAMETERFIELD(Dev, LimitChecksStatus,
				VL_CHECKENABLE_RANGE_IGNORE_THRESHOLD,
				Temp8);
	}

	LOG_FUNCTION_END(Status);
	return Status;

}
