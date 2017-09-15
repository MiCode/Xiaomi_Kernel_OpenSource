dtb-$(CONFIG_ARCH_QCOM)	+= apq8016-sbc.dtb msm8916-mtp.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= msm8996-mtp.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= apq8096-db820c.dtb

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
	dtbo-$(CONFIG_ARCH_SDM845) += \
		sdm845-cdp-overlay.dtbo \
		sdm845-mtp-overlay.dtbo \
		sdm845-qrd-overlay.dtbo \
		sdm845-qvr-overlay.dtbo \
		sdm845-4k-panel-mtp-overlay.dtbo \
		sdm845-4k-panel-cdp-overlay.dtbo \
		sdm845-4k-panel-qrd-overlay.dtbo \
		sdm845-v2-cdp-overlay.dtbo \
		sdm845-v2-mtp-overlay.dtbo \
		sdm845-v2-qrd-overlay.dtbo \
		sdm845-v2-4k-panel-mtp-overlay.dtbo \
		sdm845-v2-4k-panel-cdp-overlay.dtbo \
		sdm845-v2-4k-panel-qrd-overlay.dtbo \
		sda845-cdp-overlay.dtbo \
		sda845-mtp-overlay.dtbo \
		sda845-qrd-overlay.dtbo \
		sda845-4k-panel-mtp-overlay.dtbo \
		sda845-4k-panel-cdp-overlay.dtbo \
		sda845-4k-panel-qrd-overlay.dtbo \
		sda845-v2-cdp-overlay.dtbo \
		sda845-v2-mtp-overlay.dtbo \
		sda845-v2-qrd-overlay.dtbo \
		sda845-v2-4k-panel-mtp-overlay.dtbo \
		sda845-v2-4k-panel-cdp-overlay.dtbo \
		sda845-v2-4k-panel-qrd-overlay.dtbo

sdm845-cdp-overlay.dtbo-base := sdm845.dtb
sdm845-mtp-overlay.dtbo-base := sdm845.dtb
sdm845-qrd-overlay.dtbo-base := sdm845.dtb
sdm845-qvr-overlay.dtbo-base := sdm845-v2.dtb
sdm845-qvr-overlay.dtbo-base := sdm845.dtb
sdm845-4k-panel-mtp-overlay.dtbo-base := sdm845.dtb
sdm845-4k-panel-cdp-overlay.dtbo-base := sdm845.dtb
sdm845-4k-panel-qrd-overlay.dtbo-base := sdm845.dtb
sdm845-v2-cdp-overlay.dtbo-base := sdm845-v2.dtb
sdm845-v2-mtp-overlay.dtbo-base := sdm845-v2.dtb
sdm845-v2-qrd-overlay.dtbo-base := sdm845-v2.dtb
sdm845-v2-4k-panel-mtp-overlay.dtbo-base := sdm845-v2.dtb
sdm845-v2-4k-panel-cdp-overlay.dtbo-base := sdm845-v2.dtb
sdm845-v2-4k-panel-qrd-overlay.dtbo-base := sdm845-v2.dtb
sda845-cdp-overlay.dtbo-base := sda845.dtb
sda845-mtp-overlay.dtbo-base := sda845.dtb
sda845-qrd-overlay.dtbo-base := sda845.dtb
sda845-4k-panel-mtp-overlay.dtbo-base := sda845.dtb
sda845-4k-panel-cdp-overlay.dtbo-base := sda845.dtb
sda845-4k-panel-qrd-overlay.dtbo-base := sda845.dtb
sda845-v2-cdp-overlay.dtbo-base := sda845-v2.dtb
sda845-v2-mtp-overlay.dtbo-base := sda845-v2.dtb
sda845-v2-qrd-overlay.dtbo-base := sda845-v2.dtb
sda845-v2-4k-panel-mtp-overlay.dtbo-base := sda845-v2.dtb
sda845-v2-4k-panel-cdp-overlay.dtbo-base := sda845-v2.dtb
sda845-v2-4k-panel-qrd-overlay.dtbo-base := sda845-v2.dtb
else
dtb-$(CONFIG_ARCH_SDM845) += sdm845-sim.dtb \
	sdm845-rumi.dtb \
	sdm845-mtp.dtb \
	sdm845-cdp.dtb \
	sdm845-v2-rumi.dtb \
	sdm845-v2-mtp.dtb \
	sdm845-v2-cdp.dtb \
	sdm845-qrd.dtb \
	sdm845-v2-qrd.dtb \
	sdm845-qvr.dtb \
	sdm845-4k-panel-mtp.dtb \
	sdm845-4k-panel-cdp.dtb \
	sdm845-4k-panel-qrd.dtb \
	sdm845-interposer-sdm670-mtp.dtb \
	sdm845-interposer-sdm670-cdp.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
	dtbo-$(CONFIG_ARCH_SDM670) += \
		sdm670-cdp-overlay.dtbo \
		sdm670-mtp-overlay.dtbo \
		sdm670-rumi-overlay.dtbo \
		sdm670-pm660a-cdp-overlay.dtbo \
		sdm670-pm660a-mtp-overlay.dtbo \
		sdm670-external-codec-cdp-overlay.dtbo \
		sdm670-external-codec-mtp-overlay.dtbo \
		sdm670-external-codec-pm660a-cdp-overlay.dtbo \
		sdm670-external-codec-pm660a-mtp-overlay.dtbo

sdm670-cdp-overlay.dtbo-base := sdm670.dtb
sdm670-mtp-overlay.dtbo-base := sdm670.dtb
sdm670-rumi-overlay.dtbo-base := sdm670.dtb
sdm670-pm660a-cdp-overlay.dtbo-base := sdm670.dtb
sdm670-pm660a-mtp-overlay.dtbo-base := sdm670.dtb
sdm670-external-codec-cdp-overlay.dtbo-base := sdm670.dtb
sdm670-external-codec-mtp-overlay.dtbo-base := sdm670.dtb
sdm670-external-codec-pm660a-cdp-overlay.dtbo-base := sdm670.dtb
sdm670-external-codec-pm660a-mtp-overlay.dtbo-base := sdm670.dtb
else
dtb-$(CONFIG_ARCH_SDM670) += sdm670-rumi.dtb \
	sdm670-mtp.dtb \
	sdm670-cdp.dtb \
	sdm670-pm660a-mtp.dtb \
	sdm670-pm660a-cdp.dtb \
	sdm670-external-codec-cdp.dtb \
	sdm670-external-codec-mtp.dtb \
	sdm670-external-codec-pm660a-cdp.dtb \
	sdm670-external-codec-pm660a-mtp.dtb
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb
