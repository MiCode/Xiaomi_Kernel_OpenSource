obj-$(CONFIG_CRYPTO_DEV_QCEDEV) += qcedev.o
ifeq ($(CONFIG_CRYPTO_DEV_QCE50), y)
        obj-$(CONFIG_CRYPTO_DEV_QCOM_MSM_QCE) += qce50.o
else
        obj-$(CONFIG_CRYPTO_DEV_QCOM_MSM_QCE) += qce.o
endif
ifdef CONFIG_COMPAT
obj-$(CONFIG_CRYPTO_DEV_QCOM_MSM_QCE) += compat_qcedev.o
endif
ifeq ($(CONFIG_FIPS_ENABLE), y)
	obj-$(CONFIG_CRYPTO_DEV_QCEDEV) += qcedev_fips.o
	obj-$(CONFIG_CRYPTO_DEV_QCRYPTO) += qcrypto_fips.o
endif
obj-$(CONFIG_CRYPTO_DEV_QCRYPTO) += qcrypto.o
obj-$(CONFIG_CRYPTO_DEV_OTA_CRYPTO) += ota_crypto.o
obj-$(CONFIG_CRYPTO_DEV_QCOM_ICE) += ice.o
