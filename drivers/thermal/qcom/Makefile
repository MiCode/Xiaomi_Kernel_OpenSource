# SPDX-License-Identifier: GPL-2.0-only
obj-$(CONFIG_QCOM_TSENS)	+= qcom_tsens.o

qcom_tsens-y			+= tsens.o tsens-common.o tsens-v0_1.o \
				   tsens-8960.o tsens-v2.o tsens-v1.o
obj-$(CONFIG_QCOM_SPMI_TEMP_ALARM)	+= qcom-spmi-temp-alarm.o
obj-$(CONFIG_QTI_ADC_TM) += adc-tm.o adc-tm-common.o adc-tm5.o adc-tm7.o
obj-$(CONFIG_QTI_QMI_SENSOR) += qti_qmi_sensor.o
qti_qmi_sensor-y += thermal_sensor_service_v01.o qmi_sensors.o

obj-$(CONFIG_QTI_QMI_COOLING_DEVICE) += qti_qmi_cdev.o
qti_qmi_cdev-y += thermal_mitigation_device_service_v01.o qmi_cooling.o

obj-$(CONFIG_QTI_CPU_ISOLATE_COOLING_DEVICE) += cpu_isolate.o
obj-$(CONFIG_QTI_BCL_PMIC5) += bcl_pmic5.o
obj-$(CONFIG_QTI_BCL_SOC_DRIVER) += bcl_soc.o
