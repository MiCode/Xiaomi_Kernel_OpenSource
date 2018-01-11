dtb-$(CONFIG_ARCH_QCOM)	+= apq8016-sbc.dtb msm8916-mtp.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= msm8996-mtp.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= apq8096-db820c.dtb

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
	dtbo-$(CONFIG_ARCH_SDM845) += \
		sdm845-cdp-overlay.dtbo \
		sdm845-mtp-overlay.dtbo \
		sdm845-qrd-overlay.dtbo \
		sdm845-4k-panel-mtp-overlay.dtbo \
		sdm845-4k-panel-cdp-overlay.dtbo \
		sdm845-4k-panel-qrd-overlay.dtbo \
		sdm845-v2-qvr-overlay.dtbo \
		sdm845-v2-cdp-overlay.dtbo \
		sdm845-v2-mtp-overlay.dtbo \
		sdm845-v2-qrd-overlay.dtbo \
		sdm845-v2-4k-panel-mtp-overlay.dtbo \
		sdm845-v2-4k-panel-cdp-overlay.dtbo \
		sdm845-v2-4k-panel-qrd-overlay.dtbo \
		sdm845-v2.1-cdp-overlay.dtbo \
		sdm845-v2.1-mtp-overlay.dtbo \
		sdm845-v2.1-qrd-overlay.dtbo \
		sdm845-v2.1-4k-panel-mtp-overlay.dtbo \
		sdm845-v2.1-4k-panel-cdp-overlay.dtbo \
		sdm845-v2.1-4k-panel-qrd-overlay.dtbo \
		sda845-cdp-overlay.dtbo \
		sda845-mtp-overlay.dtbo \
		sda845-qrd-overlay.dtbo \
		sda845-4k-panel-mtp-overlay.dtbo \
		sda845-4k-panel-cdp-overlay.dtbo \
		sda845-4k-panel-qrd-overlay.dtbo \
		sda845-v2-cdp-overlay.dtbo \
		sda845-v2-mtp-overlay.dtbo \
		sda845-v2-qrd-overlay.dtbo \
		sda845-v2-hdk-overlay.dtbo \
		sda845-v2-4k-panel-mtp-overlay.dtbo \
		sda845-v2-4k-panel-cdp-overlay.dtbo \
		sda845-v2-4k-panel-qrd-overlay.dtbo \
		sda845-v2.1-cdp-overlay.dtbo \
		sda845-v2.1-mtp-overlay.dtbo \
		sda845-v2.1-qrd-overlay.dtbo \
		sda845-v2.1-4k-panel-cdp-overlay.dtbo \
		sda845-v2.1-4k-panel-mtp-overlay.dtbo \
		sda845-v2.1-4k-panel-qrd-overlay.dtbo

sdm845-cdp-overlay.dtbo-base := sdm845.dtb
sdm845-mtp-overlay.dtbo-base := sdm845.dtb
sdm845-qrd-overlay.dtbo-base := sdm845.dtb
sdm845-4k-panel-mtp-overlay.dtbo-base := sdm845.dtb
sdm845-4k-panel-cdp-overlay.dtbo-base := sdm845.dtb
sdm845-4k-panel-qrd-overlay.dtbo-base := sdm845.dtb
sdm845-v2-qvr-overlay.dtbo-base := sdm845-v2.dtb
sdm845-v2-cdp-overlay.dtbo-base := sdm845-v2.dtb
sdm845-v2-mtp-overlay.dtbo-base := sdm845-v2.dtb
sdm845-v2-qrd-overlay.dtbo-base := sdm845-v2.dtb
sdm845-v2-4k-panel-mtp-overlay.dtbo-base := sdm845-v2.dtb
sdm845-v2-4k-panel-cdp-overlay.dtbo-base := sdm845-v2.dtb
sdm845-v2-4k-panel-qrd-overlay.dtbo-base := sdm845-v2.dtb
sdm845-v2.1-cdp-overlay.dtbo-base := sdm845-v2.1.dtb
sdm845-v2.1-mtp-overlay.dtbo-base := sdm845-v2.1.dtb
sdm845-v2.1-qrd-overlay.dtbo-base := sdm845-v2.1.dtb
sdm845-v2.1-4k-panel-mtp-overlay.dtbo-base := sdm845-v2.1.dtb
sdm845-v2.1-4k-panel-cdp-overlay.dtbo-base := sdm845-v2.1.dtb
sdm845-v2.1-4k-panel-qrd-overlay.dtbo-base := sdm845-v2.1.dtb
sda845-cdp-overlay.dtbo-base := sda845.dtb
sda845-mtp-overlay.dtbo-base := sda845.dtb
sda845-qrd-overlay.dtbo-base := sda845.dtb
sda845-4k-panel-mtp-overlay.dtbo-base := sda845.dtb
sda845-4k-panel-cdp-overlay.dtbo-base := sda845.dtb
sda845-4k-panel-qrd-overlay.dtbo-base := sda845.dtb
sda845-v2-cdp-overlay.dtbo-base := sda845-v2.dtb
sda845-v2-mtp-overlay.dtbo-base := sda845-v2.dtb
sda845-v2-qrd-overlay.dtbo-base := sda845-v2.dtb
sda845-v2-hdk-overlay.dtbo-base := sda845-v2.dtb
sda845-v2-4k-panel-mtp-overlay.dtbo-base := sda845-v2.dtb
sda845-v2-4k-panel-cdp-overlay.dtbo-base := sda845-v2.dtb
sda845-v2-4k-panel-qrd-overlay.dtbo-base := sda845-v2.dtb
sda845-v2.1-cdp-overlay.dtbo-base := sda845-v2.1.dtb
sda845-v2.1-mtp-overlay.dtbo-base := sda845-v2.1.dtb
sda845-v2.1-qrd-overlay.dtbo-base := sda845-v2.1.dtb
sda845-v2.1-4k-panel-cdp-overlay.dtbo-base := sda845-v2.1.dtb
sda845-v2.1-4k-panel-mtp-overlay.dtbo-base := sda845-v2.1.dtb
sda845-v2.1-4k-panel-qrd-overlay.dtbo-base := sda845-v2.1.dtb
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
	sdm845-v2-qvr.dtb \
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
		sdm670-qrd-overlay.dtbo \
		sdm670-qrd-sku2-overlay.dtbo \
		sdm670-pm660a-cdp-overlay.dtbo \
		sdm670-pm660a-mtp-overlay.dtbo \
		sdm670-external-codec-cdp-overlay.dtbo \
		sdm670-external-codec-mtp-overlay.dtbo \
		sdm670-external-codec-pm660a-cdp-overlay.dtbo \
		sdm670-external-codec-pm660a-mtp-overlay.dtbo \
		sdm670-usbc-cdp-overlay.dtbo \
		sdm670-usbc-mtp-overlay.dtbo \
		sdm670-usbc-pm660a-cdp-overlay.dtbo \
		sdm670-usbc-pm660a-mtp-overlay.dtbo \
		sdm670-usbc-external-codec-cdp-overlay.dtbo \
		sdm670-usbc-external-codec-mtp-overlay.dtbo \
		sdm670-usbc-external-codec-pm660a-cdp-overlay.dtbo \
		sdm670-usbc-external-codec-pm660a-mtp-overlay.dtbo \
		sda670-cdp-overlay.dtbo \
		sda670-mtp-overlay.dtbo \
		sda670-pm660a-cdp-overlay.dtbo \
		sda670-pm660a-mtp-overlay.dtbo \
		qcs605-cdp-overlay.dtbo \
		qcs605-mtp-overlay.dtbo \
		qcs605-360camera-overlay.dtbo \
		qcs605-external-codec-mtp-overlay.dtbo \
		qcs605-lc-mtp-overlay.dtbo

sdm670-cdp-overlay.dtbo-base := sdm670.dtb
sdm670-mtp-overlay.dtbo-base := sdm670.dtb
sdm670-rumi-overlay.dtbo-base := sdm670.dtb
sdm670-qrd-overlay.dtbo-base := sdm670.dtb
sdm670-qrd-sku2-overlay.dtbo-base := sdm670.dtb
sdm670-pm660a-cdp-overlay.dtbo-base := sdm670.dtb
sdm670-pm660a-mtp-overlay.dtbo-base := sdm670.dtb
sdm670-external-codec-cdp-overlay.dtbo-base := sdm670.dtb
sdm670-external-codec-mtp-overlay.dtbo-base := sdm670.dtb
sdm670-external-codec-pm660a-cdp-overlay.dtbo-base := sdm670.dtb
sdm670-external-codec-pm660a-mtp-overlay.dtbo-base := sdm670.dtb
sdm670-usbc-cdp-overlay.dtbo-base := sdm670.dtb
sdm670-usbc-mtp-overlay.dtbo-base := sdm670.dtb
sdm670-usbc-pm660a-cdp-overlay.dtbo-base := sdm670.dtb
sdm670-usbc-pm660a-mtp-overlay.dtbo-base := sdm670.dtb
sdm670-usbc-external-codec-cdp-overlay.dtbo-base := sdm670.dtb
sdm670-usbc-external-codec-mtp-overlay.dtbo-base := sdm670.dtb
sdm670-usbc-external-codec-pm660a-cdp-overlay.dtbo-base := sdm670.dtb
sdm670-usbc-external-codec-pm660a-mtp-overlay.dtbo-base := sdm670.dtb
sda670-cdp-overlay.dtbo-base := sda670.dtb
sda670-mtp-overlay.dtbo-base := sda670.dtb
sda670-pm660a-cdp-overlay.dtbo-base := sda670.dtb
sda670-pm660a-mtp-overlay.dtbo-base := sda670.dtb
qcs605-cdp-overlay.dtbo-base := qcs605.dtb
qcs605-mtp-overlay.dtbo-base := qcs605.dtb
qcs605-external-codec-mtp-overlay.dtbo-base := qcs605.dtb
qcs605-lc-mtp-overlay.dtbo-base := qcs605.dtb
qcs605-360camera-overlay.dtbo-base := qcs605.dtb

else
dtb-$(CONFIG_ARCH_SDM670) += sdm670-rumi.dtb \
	sdm670-mtp.dtb \
	sdm670-cdp.dtb \
	sdm670-qrd.dtb \
	sdm670-qrd-sku2.dtb \
	sdm670-pm660a-mtp.dtb \
	sdm670-pm660a-cdp.dtb \
	sdm670-external-codec-cdp.dtb \
	sdm670-external-codec-mtp.dtb \
	sdm670-external-codec-pm660a-cdp.dtb \
	sdm670-external-codec-pm660a-mtp.dtb \
	sdm670-usbc-cdp.dtb \
	sdm670-usbc-external-codec-cdp.dtb \
	sdm670-usbc-external-codec-mtp.dtb \
	sdm670-usbc-external-codec-pm660a-cdp.dtb \
	sdm670-usbc-external-codec-pm660a-mtp.dtb \
	sdm670-usbc-mtp.dtb \
	sdm670-usbc-pm660a-cdp.dtb \
	sdm670-usbc-pm660a-mtp.dtb \
	sda670-mtp.dtb \
	sda670-cdp.dtb \
	sda670-pm660a-mtp.dtb \
	sda670-pm660a-cdp.dtb \
	qcs605-360camera.dtb \
	qcs605-mtp.dtb \
	qcs605-cdp.dtb \
	qcs605-external-codec-mtp.dtb \
	qcs605-lc-mtp.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
else
dtb-$(CONFIG_ARCH_MSM8953) += msm8953-cdp.dtb \
	msm8953-mtp.dtb \
	msm8953-ext-codec-mtp.dtb \
	msm8953-qrd-sku3.dtb \
	msm8953-rcm.dtb \
	apq8053-rcm.dtb \
	msm8953-ext-codec-rcm.dtb \
	apq8053-cdp.dtb \
	apq8053-ipc.dtb \
	msm8953-ipc.dtb \
	apq8053-mtp.dtb \
	apq8053-ext-audio-mtp.dtb \
	apq8053-ext-codec-rcm.dtb \
	msm8953-cdp-1200p.dtb \
	msm8953-iot-mtp.dtb \
	apq8053-iot-mtp.dtb \
	msm8953-pmi8940-cdp.dtb \
	msm8953-pmi8940-mtp.dtb \
	msm8953-pmi8937-cdp.dtb \
	msm8953-pmi8937-mtp.dtb \
	msm8953-pmi8940-ext-codec-mtp.dtb \
	msm8953-pmi8937-ext-codec-mtp.dtb

dtb-$(CONFIG_ARCH_SDM450) += sdm450-rcm.dtb \
	sdm450-cdp.dtb \
	sdm450-mtp.dtb \
	sdm450-qrd.dtb \
	sdm450-pmi8940-mtp.dtb \
	sdm450-pmi8937-mtp.dtb \
	sdm450-iot-mtp.dtb \
	sdm450-qrd-sku4.dtb \
	sdm450-pmi632-cdp-s2.dtb \
	sdm450-pmi632-mtp-s3.dtb
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb
