# SPDX-License-Identifier: GPL-2.0
dtb-$(CONFIG_ARCH_QCOM)	+= apq8016-sbc.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= apq8096-db820c.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= ipq8074-hk01.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= msm8916-mtp.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= msm8992-bullhead-rev-101.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= msm8994-angler-rev-101.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= msm8996-mtp.dtb
dtb-$(CONFIG_ARCH_QCS405) += qcs405-rumi.dtb \
		qcs405-mtp.dtb \
		qcs405-cdp.dtb

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
	dtbo-$(CONFIG_ARCH_SM8150) += \
		sm8150-cdp-overlay.dtbo \
		sm8150-mtp-overlay.dtbo \
		sm8150-rumi-overlay.dtbo \
		sm8150-qrd-overlay.dtbo \
		sm8150-auto-adp-star-overlay.dtbo \
		sm8150p-cdp-overlay.dtbo \
		sm8150p-mtp-overlay.dtbo \
		sm8150p-qrd-overlay.dtbo \
		sm8150-sdx50m-cdp-overlay.dtbo \
		sm8150-sdx50m-mtp-overlay.dtbo

sm8150-cdp-overlay.dtbo-base := sm8150.dtb sm8150-v2.dtb
sm8150-mtp-overlay.dtbo-base := sm8150.dtb sm8150-v2.dtb
sm8150-rumi-overlay.dtbo-base := sm8150.dtb sm8150-v2.dtb
sm8150-qrd-overlay.dtbo-base := sm8150.dtb sm8150-v2.dtb
sm8150-auto-adp-star-overlay.dtbo-base := sm8150-auto.dtb
sm8150-sdx50m-cdp-overlay.dtbo-base := sm8150.dtb sm8150-v2.dtb
sm8150-sdx50m-mtp-overlay.dtbo-base := sm8150.dtb sm8150-v2.dtb
sm8150p-mtp-overlay.dtbo-base := sm8150p.dtb sm8150p-v2.dtb
sm8150p-qrd-overlay.dtbo-base := sm8150p.dtb sm8150p-v2.dtb
else
dtb-$(CONFIG_ARCH_SM8150)	+= sm8150-rumi.dtb \
	sm8150-mtp.dtb \
	sm8150-cdp.dtb \
	sm8150-qrd.dtb \
	sm8150-auto-adp-star.dtb \
	sm8150-v2-rumi.dtb \
	sm8150-v2-mtp.dtb \
	sm8150-v2-cdp.dtb \
	sm8150-v2-qrd.dtb \
	sm8150p-mtp.dtb \
	sm8150p-cdp.dtb \
	sm8150p-qrd.dtb \
	sm8150p-v2-mtp.dtb \
	sm8150p-v2-cdp.dtb \
	sm8150p-v2-qrd.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
	dtbo-$(CONFIG_ARCH_SDMSHRIKE) += \
		sdmshrike-cdp-overlay.dtbo \
		sdmshrike-mtp-overlay.dtbo

sdmshrike-cdp-overlay.dtbo-base := sdmshrike.dtb
sdmshrike-mtp-overlay.dtbo-base := sdmshrike.dtb
else
dtb-$(CONFIG_ARCH_SDMSHRIKE)	+= sdmshrike-rumi.dtb \
	sdmshrike-mtp.dtb \
	sdmshrike-cdp.dtb
endif

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

ifeq ($(CONFIG_ARM64),y)
always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
else
targets += dtbs
targets += $(addprefix ../, $(dtb-y))

$(obj)/../%.dtb: $(src)/%.dts FORCE
	$(call if_changed_dep,dtc)

dtbs: $(addprefix $(obj)/../,$(dtb-y))
endif
clean-files	:= *.dtb *.dtbo
