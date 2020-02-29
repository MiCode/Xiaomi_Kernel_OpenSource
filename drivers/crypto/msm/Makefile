# SPDX-License-Identifier: GPL-2.0-only
obj-$(CONFIG_CRYPTO_DEV_QCOM_MSM_QCE) += qce50.o

obj-$(CONFIG_CRYPTO_DEV_QCEDEV) += qcedev-mod.o
qcedev-mod-y = qcedev.o qcedev_smmu.o
qcedev-mod-$(CONFIG_COMPAT) += compat_qcedev.o

obj-$(CONFIG_CRYPTO_DEV_QCRYPTO) += qcrypto-msm.o
qcrypto-msm-y = qcrypto.o
