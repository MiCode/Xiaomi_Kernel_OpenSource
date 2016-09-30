/*******************************************************************************
 * Copyright © 2016, STMicroelectronics International N.V.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of STMicroelectronics nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
NON-INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS ARE DISCLAIMED.
IN NO EVENT SHALL STMICROELECTRONICS INTERNATIONAL N.V. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

#ifndef _VL53L0_API_HISTOGRAM_H_
#define _VL53L0_API_HISTOGRAM_H_

#include "vl53l0_def.h"
#include "vl53l0_platform.h"


#ifdef __cplusplus
extern "C" {
#endif


VL53L0_Error VL53L0_confirm_measurement_start(VL53L0_DEV Dev);

VL53L0_Error VL53L0_set_histogram_mode(VL53L0_DEV Dev,
	VL53L0_HistogramModes HistogramMode);

VL53L0_Error VL53L0_get_histogram_mode(VL53L0_DEV Dev,
	VL53L0_HistogramModes *pHistogramMode);

VL53L0_Error VL53L0_start_histogram_measurement(VL53L0_DEV Dev,
	VL53L0_HistogramModes histoMode,
	uint32_t count);

VL53L0_Error VL53L0_perform_single_histogram_measurement(VL53L0_DEV Dev,
	VL53L0_HistogramMeasurementData_t *pHistogramMeasurementData);

VL53L0_Error VL53L0_get_histogram_measurement_data(VL53L0_DEV Dev,
	VL53L0_HistogramMeasurementData_t *pHistogramMeasurementData);

VL53L0_Error VL53L0_read_histo_measurement(VL53L0_DEV Dev,
	uint32_t *histoData, uint32_t offset, VL53L0_HistogramModes histoMode);

VL53L0_Error VL53L0_perform_xtalk_measurement(VL53L0_DEV dev,
	uint32_t timeout_ms, FixPoint1616_t *pxtalk_per_spad,
	uint8_t *pambient_too_high);

#ifdef __cplusplus
}
#endif

#endif /* _VL53L0_API_HISTOGRAM_H_ */
