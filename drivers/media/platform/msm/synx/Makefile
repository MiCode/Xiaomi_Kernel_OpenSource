# SPDX-License-Identifier: GPL-2.0-only

obj-$(CONFIG_MSM_GLOBAL_SYNX) += qcom_ipc_lite.o
obj-$(CONFIG_MSM_GLOBAL_SYNX) += synx-driver.o
obj-$(CONFIG_MSM_GLOBAL_IPCLITE) += ipclite.o

synx-driver-y := synx.o synx_util.o synx_debugfs.o

obj-$(CONFIG_MSM_GLOBAL_SYNX_V2) += synx-driver-v2.o

synx-driver-v2-y := synx_v2.o synx_global_v2.o synx_util_v2.o synx_debugfs_v2.o
