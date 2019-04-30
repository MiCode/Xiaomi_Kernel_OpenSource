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

kona-cdp-overlay.dtbo-base := kona.dtb kona-v2.dtb
kona-mtp-overlay.dtbo-base := kona.dtb kona-v2.dtb
kona-rumi-overlay.dtbo-base := kona.dtb kona-v2.dtb
kona-qrd-overlay.dtbo-base := kona.dtb kona-v2.dtb
else
dtb-$(CONFIG_ARCH_KONA)	+= kona-rumi.dtb \
	kona-mtp.dtb \
	kona-cdp.dtb \
	kona-qrd.dtb \
	kona-v2-rumi.dtb \
	kona-v2-mtp.dtb \
	kona-v2-cdp.dtb \
	kona-v2-qrd.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_LITO) += lito-rumi-overlay.dtbo \
		lito-mtp-overlay.dtbo \
		lito-cdp-overlay.dtbo \
		lito-atp-overlay.dtbo \
		lito-qrd-overlay.dtbo

lito-rumi-overlay.dtbo-base := lito.dtb
lito-mtp-overlay.dtbo-base := lito.dtb
lito-cdp-overlay.dtbo-base := lito.dtb
lito-atp-overlay.dtbo-base := lito.dtb
lito-qrd-overlay.dtbo-base := lito.dtb
else
dtb-$(CONFIG_ARCH_LITO) += lito-rumi.dtb \
		lito-mtp.dtb \
		lito-cdp.dtb \
		lito-atp.dtb \
		lito-qrd.dtb
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files    := *.dtb *.dtbo
