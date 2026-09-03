# SPDX-License-Identifier: GPL-2.0
#
# Makefile for sensor chip drivers.
#
obj-$(CONFIG_UNISOC_SOC_THERMAL)	+= sprd_soc_thm.o

obj-$(CONFIG_UNISOC_THERMAL_CTL)	+= sprd_thermal_ctl.o
sprd_thermal_ctl-y	:= sprd_thermal_core.o
sprd_thermal_ctl-$(CONFIG_UNISOC_GPU_COOLING_DEVICE)	+= sprd_gpu_cooling.o
sprd_thermal_ctl-$(CONFIG_UNISOC_NPU_COOLING_DEVICE)	+= sprd_npu_cooling.o

obj-$(CONFIG_UNISOC_CPU_COOLING_DEVICE)	+= sprd_cpu_cooling.o

