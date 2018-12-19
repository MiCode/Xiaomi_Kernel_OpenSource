# SPDX-License-Identifier: GPL-2.0
dtb-$(CONFIG_ARCH_QCOM)	+= apq8016-sbc.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= apq8096-db820c.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= ipq8074-hk01.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= msm8916-mtp.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= msm8992-bullhead-rev-101.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= msm8994-angler-rev-101.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= msm8996-mtp.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= sdm845-mtp.dtb
ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
	dtbo-$(CONFIG_ARCH_KONA) += \
		kona-cdp-overlay.dtbo \
		kona-mtp-overlay.dtbo \
		kona-rumi-overlay.dtbo \
		kona-qrd-overlay.dtbo

kona-cdp-overlay.dtbo-base := kona.dtb
kona-mtp-overlay.dtbo-base := kona.dtb
kona-rumi-overlay.dtbo-base := kona.dtb
kona-qrd-overlay.dtbo-base := kona.dtb
else
dtb-$(CONFIG_ARCH_KONA)	+= kona-rumi.dtb \
	kona-mtp.dtb \
	kona-cdp.dtb \
	kona-qrd.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_LITO) += lito-rumi-overlay.dtbo

lito-rumi-overlay.dtbo-base := lito.dtb
else
dtb-$(CONFIG_ARCH_LITO) += lito-rumi.dtb
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files    := *.dtb
