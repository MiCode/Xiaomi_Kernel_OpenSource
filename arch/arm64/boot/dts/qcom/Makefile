dtb-$(CONFIG_ARCH_QCOM)	+= apq8016-sbc.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= apq8096-db820c.dtb
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

sdm855-cdp-overlay.dtbo-base := sdm855.dtb
sdm855-mtp-overlay.dtbo-base := sdm855.dtb
sdm855-rumi-overlay.dtbo-base := sdm855.dtb
sdm855-qrd-overlay.dtbo-base := sdm855.dtb
else
dtb-$(CONFIG_ARCH_SDM855)	+= sdm855-rumi.dtb \
	sdm855-mtp.dtb \
	sdm855-cdp.dtb \
	sdm855-qrd.dtb
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
