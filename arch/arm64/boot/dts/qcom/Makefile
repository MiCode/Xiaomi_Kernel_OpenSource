# SPDX-License-Identifier: GPL-2.0
dtb-$(CONFIG_ARCH_QCOM)	+= apq8016-sbc.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= apq8096-db820c.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= ipq8074-hk01.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= msm8916-mtp.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= msm8992-bullhead-rev-101.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= msm8994-angler-rev-101.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= msm8996-mtp.dtb

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
	dtbo-$(CONFIG_ARCH_SDM855) += \
		sdm855-cdp-overlay.dtbo \
		sdm855-mtp-overlay.dtbo \
		sdm855-rumi-overlay.dtbo \
		sdm855-qrd-overlay.dtbo \
		sdm855-v2-cdp-overlay.dtbo \
		sdm855-v2-mtp-overlay.dtbo \
		sdm855-v2-rumi-overlay.dtbo \
		sdm855-v2-qrd-overlay.dtbo \
		sdm855-sdx50m-cdp-overlay.dtbo \
		sdm855-sdx50m-mtp-overlay.dtbo

sdm855-cdp-overlay.dtbo-base := sdm855.dtb
sdm855-mtp-overlay.dtbo-base := sdm855.dtb
sdm855-rumi-overlay.dtbo-base := sdm855.dtb
sdm855-qrd-overlay.dtbo-base := sdm855.dtb
sdm855-v2-cdp-overlay.dtbo-base := sdm855-v2.dtb
sdm855-v2-mtp-overlay.dtbo-base := sdm855-v2.dtb
sdm855-v2-rumi-overlay.dtbo-base := sdm855-v2.dtb
sdm855-v2-qrd-overlay.dtbo-base := sdm855-v2.dtb
sdm855-sdx50m-cdp-overlay.dtbo-base := sdm855.dtb
sdm855-sdx50m-mtp-overlay.dtbo-base := sdm855.dtb
else
dtb-$(CONFIG_ARCH_SDM855)	+= sdm855-rumi.dtb \
	sdm855-mtp.dtb \
	sdm855-cdp.dtb \
	sdm855-qrd.dtb \
	sdm855-v2-rumi.dtb \
	sdm855-v2-mtp.dtb \
	sdm855-v2-cdp.dtb \
	sdm855-v2-qrd.dtb
endif

dtb-$(CONFIG_ARCH_SDMSHRIKE)	+= sdmshrike-rumi.dtb \
	sdmshrike-mtp.dtb \
	sdmshrike-cdp.dtb

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
	dtbo-$(CONFIG_ARCH_SDM640) += \
		sdm640-cdp-overlay.dtbo \
		sdm640-mtp-overlay.dtbo \
		sdm640-rumi-overlay.dtbo \
		sdm640-qrd-overlay.dtbo \

sdm640-cdp-overlay.dtbo-base := sdm640.dtb
sdm640-mtp-overlay.dtbo-base := sdm640.dtb
sdm640-rumi-overlay.dtbo-base := sdm640.dtb
sdm640-qrd-overlay.dtbo-base := sdm640.dtb
else
dtb-$(CONFIG_ARCH_SDM640)	+= sdm640-rumi.dtb \
	sdm640-mtp.dtb \
	sdm640-cdp.dtb \
	sdm640-qrd.dtb
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
