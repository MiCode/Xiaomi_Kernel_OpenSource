/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _DT_BINDINGS_QCOM_SPMI_VADC_PM8550VX_H
#define _DT_BINDINGS_QCOM_SPMI_VADC_PM8550VX_H

#include <dt-bindings/iio/qcom,spmi-vadc.h>

/* ADC channels for PM8550VX_ADC for PMIC5 Gen3 */
#define PM8550VX_ADC5_GEN3_OFFSET_REF(sid)		((sid) << 8 | ADC5_GEN3_OFFSET_REF)
#define PM8550VX_ADC5_GEN3_1P25VREF(sid)		((sid) << 8 | ADC5_GEN3_1P25VREF)
#define PM8550VX_ADC5_GEN3_VREF_VADC(sid)		((sid) << 8 | ADC5_GEN3_VREF_VADC)
#define PM8550VX_ADC5_GEN3_DIE_TEMP(sid)		((sid) << 8 | ADC5_GEN3_DIE_TEMP)

#endif /* _DT_BINDINGS_QCOM_SPMI_VADC_PM8550VX_H */
