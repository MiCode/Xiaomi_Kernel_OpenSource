# SPDX-License-Identifier: GPL-2.0
dtb-$(CONFIG_ARCH_QCOM)	+= apq8016-sbc.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= apq8096-db820c.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= ipq8074-hk01.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= msm8916-mtp.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= msm8992-bullhead-rev-101.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= msm8994-angler-rev-101.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= msm8996-mtp.dtb

dtb-$(CONFIG_ARCH_QCS403) += qcs403-iot-sku1.dtb \
			qcs403-iot-sku3.dtb \
			qcs403-iot-sku5.dtb \
			qcs401-iot-sku5.dtb \
			qcs404-iot-sku3.dtb \
			qcs404-iot-sku5.dtb \
			qcs404-iot-sku6.dtb

dtb-$(CONFIG_ARCH_QCS405) += qcs405-iot-sku1.dtb \
		qcs407-iot-sku1.dtb \
		qcs405-iot-sku3.dtb \
		qcs407-iot-sku3.dtb \
		qcs405-iot-sku4.dtb \
		qcs407-iot-sku4.dtb \
		qcs405-iot-sku6.dtb \
		qcs407-iot-sku6.dtb \
		qcs407-iot-sku9.dtb \
		qcs405-iot-sku12.dtb \
		qcs407-iot-sku12.dtb \
		sa2150p-ccard-emmc.dtb \
		sa2150p-ccard-nand.dtb \
		qcs405-iot-sku13.dtb \
		qcs407-iot-sku13.dtb \
		qcs405-iot-robot-reference-board.dtb \
		qcs404-iot-robot-reference-board.dtb

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
	dtbo-$(CONFIG_ARCH_SM8150) += \
		sm8150-cdp-overlay.dtbo \
		sm8150-mtp-overlay.dtbo \
		sm8150-rumi-overlay.dtbo \
		sm8150-qrd-overlay.dtbo \
		sm8150-qrd-dvt-overlay.dtbo \
		sa8155-adp-star-overlay.dtbo \
		sa8155p-adp-star-overlay.dtbo \
		sa8155-v2-adp-air-overlay.dtbo \
                sa8155p-v2-adp-air-overlay.dtbo \
		sa8155-adp-alcor-overlay.dtbo \
		sa8155p-adp-alcor-overlay.dtbo \
		sm8150-sdx50m-cdp-overlay.dtbo \
		sm8150-sdx50m-mtp-overlay.dtbo \
		sm8150-sdx50m-mtp-2.5k-panel-overlay.dtbo \
		sm8150-sdx50m-qrd-overlay.dtbo \
		sm8150-sdxprairie-cdp-overlay.dtbo \
		sm8150-sdxprairie-mtp-overlay.dtbo \
		sm8150-sdxprairie-v2-cdp-overlay.dtbo \
		sm8150-sdxprairie-v2-mtp-overlay.dtbo \
		sm8150-hdk-overlay.dtbo

sm8150-cdp-overlay.dtbo-base := sm8150.dtb sm8150-v2.dtb sm8150p.dtb sm8150p-v2.dtb
sm8150-mtp-overlay.dtbo-base := sm8150.dtb sm8150-v2.dtb sm8150p.dtb sm8150p-v2.dtb
sm8150-rumi-overlay.dtbo-base := sm8150.dtb sm8150-v2.dtb sm8150p.dtb sm8150p-v2.dtb
sm8150-qrd-overlay.dtbo-base := sm8150.dtb sm8150-v2.dtb sm8150p.dtb sm8150p-v2.dtb
sm8150-hdk-overlay.dtbo-base := sm8150.dtb sm8150-v2.dtb sm8150p.dtb sm8150p-v2.dtb
sm8150-qrd-dvt-overlay.dtbo-base := sm8150-v2.dtb sm8150p-v2.dtb
sa8155-adp-star-overlay.dtbo-base := sa8155.dtb sa8155-v2.dtb
sa8155p-adp-star-overlay.dtbo-base := sa8155p.dtb sa8155p-v2.dtb
sa8155-v2-adp-air-overlay.dtbo-base := sa8155.dtb sa8155-v2.dtb
sa8155p-v2-adp-air-overlay.dtbo-base := sa8155p.dtb sa8155p-v2.dtb
sa8155-adp-alcor-overlay.dtbo-base := sa8155.dtb sa8155-v2.dtb
sa8155p-adp-alcor-overlay.dtbo-base := sa8155p.dtb sa8155p-v2.dtb
sm8150-sdx50m-cdp-overlay.dtbo-base := sm8150.dtb sm8150-v2.dtb sm8150p.dtb sm8150p-v2.dtb
sm8150-sdx50m-mtp-overlay.dtbo-base := sm8150.dtb sm8150-v2.dtb sm8150p.dtb sm8150p-v2.dtb
sm8150-sdx50m-mtp-2.5k-panel-overlay.dtbo-base := sm8150.dtb sm8150-v2.dtb sm8150p.dtb sm8150p-v2.dtb
sm8150-sdx50m-qrd-overlay.dtbo-base := sm8150.dtb sm8150-v2.dtb sm8150p.dtb sm8150p-v2.dtb
sm8150-sdxprairie-cdp-overlay.dtbo-base := sm8150.dtb sm8150-v2.dtb sm8150p.dtb sm8150p-v2.dtb
sm8150-sdxprairie-mtp-overlay.dtbo-base := sm8150.dtb sm8150-v2.dtb sm8150p.dtb sm8150p-v2.dtb
sm8150-sdxprairie-v2-cdp-overlay.dtbo-base := sm8150.dtb sm8150-v2.dtb sm8150p.dtb sm8150p-v2.dtb
sm8150-sdxprairie-v2-mtp-overlay.dtbo-base := sm8150.dtb sm8150-v2.dtb sm8150p.dtb sm8150p-v2.dtb
else
dtb-$(CONFIG_ARCH_SM8150)	+= sm8150-rumi.dtb \
	sm8150-mtp.dtb \
	sm8150-cdp.dtb \
	sm8150-qrd.dtb \
	sa8155-adp-star.dtb \
	sa8155p-adp-star.dtb \
	sa8155-v2-adp-star.dtb \
	sa8155p-v2-adp-star.dtb \
	sa8155-v2-adp-air.dtb \
	sa8155p-v2-adp-air.dtb \
	sa8155-adp-alcor.dtb \
	sa8155p-adp-alcor.dtb \
	sm8150-v2-rumi.dtb \
	sm8150-v2-mtp.dtb \
	sm8150-v2-cdp.dtb \
	sm8150-v2-qrd.dtb \
	sm8150-v2-qrd-dvt.dtb \
	sm8150p-mtp.dtb \
	sm8150p-cdp.dtb \
	sm8150p-qrd.dtb \
	sm8150p-v2-mtp.dtb \
	sm8150p-v2-cdp.dtb \
	sm8150p-v2-qrd.dtb \
	sm8150-hdk.dtb \
	sm8150p-hdk.dtb
endif

dtb-$(CONFIG_QTI_GVM) += sa8155-vm.dtb \
	sa8155-vm-lv.dtb \
	sa8155-vm-lv-mt.dtb \
	sa8155-vm-la-mt.dtb \
	sa6155p-vm.dtb \
	sa8195-vm.dtb \
	sa8195-vm-lv.dtb \
	sa8195-vm-lv-mt.dtb

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
	dtbo-$(CONFIG_ARCH_SDMSHRIKE) += \
		sdmshrike-cdp-overlay.dtbo \
		sdmshrike-mtp-overlay.dtbo \
		sa8195p-adp-star-overlay.dtbo

sdmshrike-cdp-overlay.dtbo-base := sdmshrike.dtb sdmshrike-v2.dtb
sdmshrike-mtp-overlay.dtbo-base := sdmshrike.dtb sdmshrike-v2.dtb
sa8195p-adp-star-overlay.dtbo-base := sa8195p.dtb
else
dtb-$(CONFIG_ARCH_SDMSHRIKE)	+= sdmshrike-rumi.dtb \
	sdmshrike-mtp.dtb \
	sdmshrike-cdp.dtb \
	sdmshrike-v2-mtp.dtb \
	sa8195p-adp-star.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
	dtbo-$(CONFIG_ARCH_SM6150) += \
		sm6150-rumi-overlay.dtbo \
		sm6150-qrd-overlay.dtbo \
		sm6150-idp-overlay.dtbo \
		sm6150p-qrd-overlay.dtbo \
		sm6150p-idp-overlay.dtbo \
		sm6150-external-codec-idp-overlay.dtbo \
		sm6150-usbc-idp-overlay.dtbo \
		sm6150-usbc-minidp-idp-overlay.dtbo \
		sm6150-cmd-mode-display-idp-overlay.dtbo \
		sa6155-adp-star-overlay.dtbo \
		sa6155p-adp-star-overlay.dtbo \
		sa6155-adp-air-overlay.dtbo \
		sa6155p-adp-air-overlay.dtbo \
		sa6155p-v2-adp-star-overlay.dtbo \
		sa6155p-v2-adp-air-overlay.dtbo \
		qcs610-iot-overlay.dtbo \
		qcs610-ipc-overlay.dtbo \
		qcs410-iot-overlay.dtbo

sm6150-rumi-overlay.dtbo-base := sm6150.dtb
sm6150-qrd-overlay.dtbo-base := sm6150.dtb
sm6150-idp-overlay.dtbo-base := sm6150.dtb
sm6150p-qrd-overlay.dtbo-base := sm6150p.dtb
sm6150p-idp-overlay.dtbo-base := sm6150p.dtb
sm6150-external-codec-idp-overlay.dtbo-base := sm6150.dtb
sm6150-usbc-idp-overlay.dtbo-base := sm6150.dtb
sm6150-usbc-minidp-idp-overlay.dtbo-base := sm6150.dtb
sm6150-cmd-mode-display-idp-overlay.dtbo-base := sm6150.dtb
sa6155-adp-star-overlay.dtbo-base := sa6155.dtb
sa6155p-adp-star-overlay.dtbo-base := sa6155p.dtb
sa6155p-v2-adp-star-overlay.dtbo-base := sa6155p.dtb
sa6155-adp-air-overlay.dtbo-base := sa6155.dtb
sa6155p-adp-air-overlay.dtbo-base := sa6155p.dtb
sa6155p-v2-adp-air-overlay.dtbo-base := sa6155p.dtb
qcs610-iot-overlay.dtbo-base := sm6150.dtb
qcs610-ipc-overlay.dtbo-base := sm6150.dtb
qcs410-iot-overlay.dtbo-base := sm6150.dtb
else
dtb-$(CONFIG_ARCH_SM6150)	+= sm6150-rumi.dtb \
	sm6150-qrd.dtb \
	sm6150-idp.dtb \
	sm6150p-qrd.dtb \
	sm6150p-idp.dtb \
	sm6150-external-codec-idp.dtb \
	sm6150-usbc-idp.dtb \
	sm6150-usbc-minidp-idp.dtb \
	sm6150-cmd-mode-display-idp.dtb \
	sa6155-adp-star.dtb \
	sa6155p-adp-star.dtb \
	sa6155-adp-air.dtb \
	sa6155p-adp-air.dtb \
	sa6155p-v2-adp-star.dtb \
	sa6155p-v2-adp-air.dtb \
	qcs610-iot.dtb \
	qcs610-ipc.dtb \
	qcs410-iot.dtb

endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
	dtbo-$(CONFIG_ARCH_SDMMAGPIE) += \
		sdmmagpie-idp-overlay.dtbo \
		sdmmagpie-atp-overlay.dtbo \
		sdmmagpie-rumi-overlay.dtbo \
		sdmmagpie-qrd-overlay.dtbo \
		sdmmagpiep-idp-overlay.dtbo \
		sdmmagpiep-atp-overlay.dtbo \
		sdmmagpiep-qrd-overlay.dtbo \
		sdmmagpie-external-codec-idp-overlay.dtbo \
		sdmmagpie-usbc-idp-overlay.dtbo \
		sdmmagpie-dual-display-idp-overlay.dtbo

sdmmagpie-idp-overlay.dtbo-base := sdmmagpie.dtb
sdmmagpie-atp-overlay.dtbo-base := sdmmagpie.dtb
sdmmagpie-rumi-overlay.dtbo-base := sdmmagpie.dtb
sdmmagpie-qrd-overlay.dtbo-base := sdmmagpie.dtb
sdmmagpiep-idp-overlay.dtbo-base := sdmmagpiep.dtb
sdmmagpiep-atp-overlay.dtbo-base := sdmmagpiep.dtb
sdmmagpiep-qrd-overlay.dtbo-base := sdmmagpiep.dtb
sdmmagpie-external-codec-idp-overlay.dtbo-base := sdmmagpie.dtb
sdmmagpie-usbc-idp-overlay.dtbo-base := sdmmagpie.dtb
sdmmagpie-dual-display-idp-overlay.dtbo-base := sdmmagpie.dtb
else
dtb-$(CONFIG_ARCH_SDMMAGPIE)	+= sdmmagpie-rumi.dtb \
	sdmmagpie-idp.dtb \
	sdmmagpie-atp.dtb \
	sdmmagpie-qrd.dtb \
	sdmmagpiep-idp.dtb \
	sdmmagpiep-atp.dtb \
	sdmmagpiep-qrd.dtb \
	sdmmagpie-external-codec-idp.dtb \
	sdmmagpie-usbc-idp.dtb \
	sdmmagpie-dual-display-idp.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
	dtbo-$(CONFIG_ARCH_TRINKET) += \
		trinket-rumi-overlay.dtbo \
		trinket-idp-overlay.dtbo \
		trinket-qrd-overlay.dtbo \
		trinket-external-codec-idp-overlay.dtbo \
		trinket-usbc-external-codec-idp-overlay.dtbo \
		trinket-usbc-idp-overlay.dtbo \
		trinket-dp-idp-overlay.dtbo

trinket-rumi-overlay.dtbo-base := trinket.dtb
trinket-idp-overlay.dtbo-base := trinket.dtb
trinket-qrd-overlay.dtbo-base := trinket.dtb
trinket-external-codec-idp-overlay.dtbo-base := trinket.dtb
trinket-usbc-external-codec-idp-overlay.dtbo-base := trinket.dtb
trinket-usbc-idp-overlay.dtbo-base := trinket.dtb
trinket-dp-idp-overlay.dtbo-base := trinket.dtb
else
dtb-$(CONFIG_ARCH_TRINKET)	+= trinket-rumi.dtb \
	trinket-idp.dtb \
	trinket-qrd.dtb \
	trinket-external-codec-idp.dtb \
	trinket-usbc-external-codec-idp.dtb \
	trinket-usbc-idp.dtb \
	trinket-dp-idp.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
	dtbo-$(CONFIG_ARCH_ATOLL) += \
		atoll-idp-overlay.dtbo\
		atoll-atp-overlay.dtbo\
		atoll-qrd-overlay.dtbo\
		atoll-wcd937x-idp-overlay.dtbo\
		atoll-usbc-idp-overlay.dtbo\
		atoll-rumi-overlay.dtbo\
		atollp-idp-overlay.dtbo\
		atollp-atp-overlay.dtbo\
		atollp-qrd-overlay.dtbo\
		atoll-ab-idp-overlay.dtbo\
		atoll-ab-atp-overlay.dtbo\
		atoll-ab-qrd-overlay.dtbo

atoll-idp-overlay.dtbo-base := atoll.dtb
atoll-atp-overlay.dtbo-base := atoll.dtb
atoll-qrd-overlay.dtbo-base := atoll.dtb
atoll-rumi-overlay.dtbo-base := atoll.dtb
atoll-wcd937x-idp-overlay.dtbo-base := atoll.dtb
atoll-usbc-idp-overlay.dtbo-base := atoll.dtb
atollp-idp-overlay.dtbo-base := atollp.dtb
atollp-atp-overlay.dtbo-base := atollp.dtb
atollp-qrd-overlay.dtbo-base := atollp.dtb
atoll-ab-idp-overlay.dtbo-base := atoll-ab.dtb
atoll-ab-atp-overlay.dtbo-base := atoll-ab.dtb
atoll-ab-qrd-overlay.dtbo-base := atoll-ab.dtb
else
dtb-$(CONFIG_ARCH_ATOLL)	+= atoll-idp.dtb\
	atoll-atp.dtb\
	atoll-qrd.dtb\
	atoll-wcd937x-idp.dtb\
	atoll-usbc-idp.dtb\
	atoll-rumi.dtb\
	atollp-idp.dtb\
	atollp-atp.dtb\
	atollp-qrd.dtb\
	atoll-ab-idp.dtb\
	atoll-ab-atp.dtb\
	atoll-ab-qrd.dtb
endif

dtb-$(CONFIG_ARCH_SDXPRAIRIE) += sdxprairie-rumi.dtb \
	sdxprairie-cdp.dtb \
	sdxprairie-dsda-cdp.dtb \
	sdxprairie-mtp.dtb \
	sdxprairie-pcie-ep-mtp.dtb \
	sdxprairie-v2-pcie-ep-mtp.dtb \
	sdxprairie-mtp-cpe.dtb \
	sdxprairie-cdp-cpe.dtb \
	sdxprairie-mtp-v1.1-cpe.dtb \
	sdxprairie-cdp-v1.1-cpe.dtb \
	sdxprairie-v2-mtp-v1.1-cpe.dtb \
	sdxprairie-mtp-v1.1.dtb \
	sdxprairie-v2-mtp-v1.1.dtb \
	sdxprairie-mtp-le-cpe.dtb \
	sdxprairie-v2-mtp-le-cpe.dtb \
	sdxprairie-cdp-256.dtb \
	sdxprairie-mtp-256.dtb \
	sdxprairie-mtp-aqc.dtb \
	sdxprairie-dsda-mtp.dtb \
	sdxprairie-v2-cdp.dtb \
	sdxprairie-v2-mtp.dtb \
        sa515m-v2-ttp.dtb \
        sa515m-v2-ttp-usb-ep.dtb \
        sa515m-v2-ttp-pcie-ep.dtb \
        sa515m-v2-ttp-flashless-usb-ep.dtb \
        sa515m-v2-ttp-flashless-pcie-ep.dtb \
	sa515m-ccard.dtb \
	sa515m-ccard-pcie-ep.dtb \
	sa515m-ccard-usb-ep.dtb \
        sa515m-v2-ccard-flashless-pcie-ep.dtb \
        sa515m-v2-ccard-flashless-usb-ep.dtb \
	sa515m-v2-ccard.dtb \
	sa515m-v2-ccard-pcie-ep.dtb \
	sa515m-v2-ccard-usb-ep.dtb

dtb-$(CONFIG_ARCH_MDM9607) += mdm9607-mtp.dtb

dtb-$(CONFIG_ARCH_SDM660) += sdm660-sim.dtb \
	sdm660-internal-codec-cdp.dtb \
	sdm660-internal-codec-mtp.dtb \
	sdm660-internal-codec-rcm.dtb \
	sdm660-cdp.dtb \
	sdm660-mtp.dtb \
	sdm660-qrd.dtb \
	sdm660-rcm.dtb \
	sdm660-pm660a-cdp.dtb \
	sdm660-pm660a-mtp.dtb \
	sdm660-pm660a-qrd.dtb \
	sdm660-pm660a-rcm.dtb \
	sdm660-internal-codec-pm660a-cdp.dtb \
	sdm660-internal-codec-pm660a-mtp.dtb \
	sdm660-internal-codec-pm660a-rcm.dtb \
	sdm660-pm660a-sim.dtb \
	sdm660-headset-jacktype-no-cdp.dtb \
	sdm660-headset-jacktype-no-rcm.dtb \
	sdm660-pm660a-headset-jacktype-no-cdp.dtb \
	sdm660-pm660a-headset-jacktype-no-rcm.dtb \
	sdm660-usbc-audio-mtp.dtb \
	sdm660-usbc-audio-rcm.dtb \

ifeq ($(CONFIG_ARM64),y)
always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
else
targets += dtbs
targets += $(addprefix ../, $(dtb-y))

$(obj)/../%.dtb: $(src)/%.dts FORCE
	$(call if_changed_dep,dtc)

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
$(obj)/%.dtbo:$(src)/../../../../arm64/boot/dts/qcom/%.dts FORCE
	$(call if_changed_dep,dtc)
	$(call if_changed,dtbo_verify)

dtbs: $(addprefix $(obj)/,$(dtb-y)) $(addprefix $(obj)/,$(dtbo-y))
else
dtbs: $(addprefix $(obj)/../,$(dtb-y))
endif
endif
clean-files	:= *.dtb *.dtbo
