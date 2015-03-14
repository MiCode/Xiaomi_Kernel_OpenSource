ifneq ($(CONFIG_FIPS_ENABLE), y)
obj-$(CONFIG_CRYPTO_DEV_QCEDEV) += qcedev.o
ifeq ($(CONFIG_CRYPTO_DEV_QCE50), y)
	obj-$(CONFIG_CRYPTO_DEV_QCE) += qce50.o
else
	ifeq ($(CONFIG_CRYPTO_DEV_QCE40), y)
	else
		obj-$(CONFIG_CRYPTO_DEV_QCE) += qce.o
	endif
endif
ifdef CONFIG_COMPAT
obj-$(CONFIG_CRYPTO_DEV_QCE) += compat_qcedev.o
endif
obj-$(CONFIG_CRYPTO_DEV_QCRYPTO) += qcrypto.o
obj-$(CONFIG_CRYPTO_DEV_OTA_CRYPTO) += ota_crypto.o
endif
obj-$(CONFIG_CRYPTO_DEV_QCOM_ICE) += ice.o
