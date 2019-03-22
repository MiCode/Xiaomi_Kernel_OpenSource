ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_MSM8996) += \
	msm8996-mtp-overlay.dtbo \
	msm8996-cdp-overlay.dtbo \
	apq8096pro-v1.1-auto-adp-overlay.dtbo \
	apq8096pro-v1.1-auto-adp-lite-overlay.dtbo \
	msm8996pro-auto-adp-overlay.dtbo \
	msm8996pro-auto-adp-lite-overlay.dtbo \
	apq8096pro-auto-cdp-overlay.dtbo \
	msm8996pro-v1.1-auto-cdp-overlay.dtbo

msm8996-mtp-overlay.dtbo-base :=  msm8996pro-v1.1-pmi8996.dtb \
				msm8996-v3-pmi8996.dtb \
				msm8996pro-v1.1-pmi8994.dtb \
				msm8996-v313-pmi8994.dtb \
				msm8996-v30-pm8994-pmi8994-pm8004.dtb

msm8996-cdp-overlay.dtbo-base :=  msm8996-v3-pmi8994.dtb \
				 msm8996pro-pmi8994-pm8004.dtb

apq8096pro-v1.1-auto-adp-overlay.dtbo-base := apq8096pro-v1.1-auto.dtb
apq8096pro-v1.1-auto-adp-lite-overlay.dtbo-base := apq8096pro-v1.1-auto.dtb
msm8996pro-auto-adp-overlay.dtbo-base := msm8996pro-auto.dtb
msm8996pro-auto-adp-lite-overlay.dtbo-base := msm8996pro-auto.dtb
apq8096pro-auto-cdp-overlay.dtbo-base := apq8096pro-auto.dtb
msm8996pro-v1.1-auto-cdp-overlay.dtbo-base := msm8996pro-v1.1-auto.dtb
else
dtb-$(CONFIG_ARCH_MSM8996) += msm8996-v2-pmi8994-cdp.dtb \
	msm8996-v2-pmi8994-mtp.dtb \
	msm8996-v2-pmi8994-pmk8001-cdp.dtb \
	msm8996-v2-pmi8994-pmk8001-mtp.dtb \
	msm8996-v2-pmi8994-pm8004-cdp.dtb \
	msm8996-v2-pmi8994-pm8004-mtp.dtb \
	msm8996-v2-pmi8994-pm8004-pmk8001-cdp.dtb \
	msm8996-v2-pmi8994-pm8004-pmk8001-mtp.dtb \
	msm8996-v2-fluid.dtb \
	msm8996-v2-liquid.dtb \
	msm8996-v2-dtp.dtb \
	msm8996-v3-auto-cdp.dtb \
	msm8996-v3-auto-adp.dtb \
	msm8996-v3-pmi8994-cdp.dtb \
	msm8996-v3-pmi8994-mtp.dtb \
	msm8996-v3-pmi8994-pmk8001-cdp.dtb \
	msm8996-v3-pmi8994-pmk8001-mtp.dtb \
	msm8996-v3-pmi8994-pm8004-cdp.dtb \
	msm8996-v3-pmi8994-pm8004-mtp.dtb \
	msm8996-v3-pmi8994-pm8004-pmk8001-cdp.dtb \
	msm8996-v3-pmi8994-pm8004-pmk8001-mtp.dtb \
	msm8996-v3-pmi8996-cdp.dtb \
	msm8996-v3-pmi8996-mtp.dtb \
	msm8996-v3-pmi8996-pmk8001-cdp.dtb \
	msm8996-v3-pmi8996-pmk8001-mtp.dtb \
	msm8996-v3-fluid.dtb \
	msm8996-v3-liquid.dtb \
	msm8996-v3-dtp.dtb \
	msm8996-v3-pm8004-mmxf-adp.dtb \
	msm8996-v3-pm8004-agave-adp.dtb \
	msm8996-v3-pm8004-agave-adp-lite.dtb \
	msm8996pro-auto-adp.dtb \
	msm8996pro-auto-adp-lite.dtb \
	msm8996pro-auto-cdp.dtb \
	msm8996pro-auto-cv2x.dtb \
	msm8996pro-pmi8994-cdp.dtb \
	msm8996pro-pmi8994-mtp.dtb \
	msm8996pro-pmi8994-pmk8001-cdp.dtb \
	msm8996pro-pmi8994-pmk8001-mtp.dtb \
	msm8996pro-pmi8994-pm8004-cdp.dtb \
	msm8996pro-pmi8994-pm8004-mtp.dtb \
	msm8996pro-pmi8994-pm8004-pmk8001-cdp.dtb \
	msm8996pro-pmi8994-pm8004-pmk8001-mtp.dtb \
	msm8996pro-pmi8996-cdp.dtb \
	msm8996pro-pmi8996-mtp.dtb \
	msm8996pro-pmi8996-pmk8001-cdp.dtb \
	msm8996pro-pmi8996-pmk8001-mtp.dtb \
	msm8996pro-v1.1-auto-cdp.dtb \
	msm8996pro-v1.1-pmi8994-cdp.dtb \
	msm8996pro-v1.1-pmi8994-mtp.dtb \
	msm8996pro-v1.1-pmi8994-pmk8001-cdp.dtb \
	msm8996pro-v1.1-pmi8994-pmk8001-mtp.dtb \
	msm8996pro-v1.1-pmi8994-pm8004-cdp.dtb \
	msm8996pro-v1.1-pmi8994-pm8004-mtp.dtb \
	msm8996pro-v1.1-pmi8994-pm8004-pmk8001-cdp.dtb \
	msm8996pro-v1.1-pmi8994-pm8004-pmk8001-mtp.dtb \
	msm8996pro-v1.1-pmi8996-cdp.dtb \
	msm8996pro-v1.1-pmi8996-mtp.dtb \
	msm8996pro-v1.1-pmi8996-pmk8001-cdp.dtb \
	msm8996pro-v1.1-pmi8996-pmk8001-mtp.dtb \
	apq8096pro-auto-cdp.dtb \
	apq8096pro-v1.1-auto-adp.dtb \
	apq8096pro-v1.1-auto-adp-lite.dtb \
	apq8096pro-liquid.dtb \
	apq8096pro-v1.1-auto-cdp.dtb \
	msm8996-v3.0-pmi8994-cdp.dtb \
	msm8996-v3.0-pmi8994-mtp.dtb \
	msm8996-v3.0-pmi8994-pm8004-cdp.dtb \
	msm8996-v3.0-pmi8994-pm8004-mtp.dtb \
	msm8996-v3.0-pmi8994-pm8004-pmk8001-cdp.dtb \
	msm8996-v3.0-pmi8994-pmk8001-cdp.dtb \
	msm8996-v3.0-pmi8996-cdp.dtb \
	msm8996-v3.0-pmi8996-mtp.dtb \
	msm8996-v3.0-fluid.dtb \
	msm8996-v3.0-liquid.dtb \
	msm8996-v3.0-dtp.dtb \
	apq8096-v2-pmi8994-cdp.dtb \
	apq8096-v2-pmi8994-mtp.dtb \
	apq8096-v2-pmi8994-pmk8001-cdp.dtb \
	apq8096-v2-pmi8994-pm8004-cdp.dtb \
	apq8096-v2-pmi8994-pm8004-pmk8001-cdp.dtb \
	apq8096-v2-liquid.dtb \
	apq8096-v2-dragonboard.dtb \
	apq8096-v2-auto-dragonboard.dtb \
	apq8096-v3-pmi8994-cdp.dtb \
	apq8096-v3-pmi8994-mtp.dtb \
	apq8096-v3-pmi8994-pmk8001-cdp.dtb \
	apq8096-v3-pmi8994-pm8004-cdp.dtb \
	apq8096-v3-pmi8994-pm8004-pmk8001-cdp.dtb \
	apq8096-v3-pmi8996-cdp.dtb \
	apq8096-v3-pmi8996-mtp.dtb \
	apq8096-v3-liquid.dtb \
	apq8096-v3-dragonboard.dtb \
	apq8096-v3-sbc.dtb \
	apq8096-v3-auto-dragonboard.dtb \
	apq8096-v3-auto-adp.dtb \
	apq8096-v3-auto-cdp.dtb \
	apq8096-v3.0-pmi8994-cdp.dtb \
	apq8096-v3.0-pmi8994-mtp.dtb \
	apq8096-v3.0-pmi8994-pm8004-cdp.dtb \
	apq8096-v3.0-pmi8994-pm8004-pmk8001-cdp.dtb \
	apq8096-v3.0-pmi8994-pmk8001-cdp.dtb \
	apq8096-v3.0-pmi8996-cdp.dtb \
	apq8096-v3.0-pmi8996-mtp.dtb \
	apq8096-v3.0-liquid.dtb \
	apq8096-v3.0-dragonboard.dtb \
	apq8096-v3-pmi8994-mdm9x55-i2s-cdp.dtb \
	apq8096-v3-pmi8994-pm8004-mdm9x55-i2s-cdp.dtb \
	apq8096-v3-pmi8994-pm8004-pmk8001-mdm9x55-i2s-cdp.dtb \
	apq8096-v3-pmi8994-pmk8001-mdm9x55-i2s-cdp.dtb \
	apq8096-v3-pmi8996-mdm9x55-i2s-cdp.dtb \
	apq8096-v3-pmi8994-mdm9x55-i2s-mtp.dtb \
	apq8096-v3-pmi8994-mdm9x55-slimbus-mtp.dtb \
	apq8096-v3-pmi8996-mdm9x55-i2s-mtp.dtb \
	apq8096-v3-pmi8996-mdm9x55-slimbus-mtp.dtb \
	apq8096-v3-pmi8996-dragonboard.dtb \
	msm8996-auto-mizar.dtb
endif

dtb-$(CONFIG_MSM_GVM_QUIN) += vplatform-lfv-msm8996-telematics.dtb \
	vplatform-lfv-msm8996-ivi.dtb \
	vplatform-lfv-msm8996-ivi_8GB.dtb \
	vplatform-lfv-msm8996-baseline.dtb \
	vplatform-lfv-msm8996-ivi-la.dtb \
	vplatform-lfv-msm8996-ivi-la-recovery.dtb \
	vplatform-lfv-msm8996-ivi-lv-mt.dtb

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_MSM8998) += \
	msm8998-cdp-overlay.dtbo \
	msm8998-mtp-overlay.dtbo \
	msm8998-v2-cdp-overlay.dtbo \
	msm8998-v2-mtp-overlay.dtbo \
	msm8998-v2.1-cdp-overlay.dtbo \
	msm8998-v2.1-mtp-overlay.dtbo \
	msm8998-qrd-overlay.dtbo \
	msm8998-qrd-vr1-overlay.dtbo \
	msm8998-qrd-skuk-overlay.dtbo

msm8998-cdp-overlay.dtbo-base :=  msm8998.dtb
msm8998-mtp-overlay.dtbo-base :=  msm8998.dtb
msm8998-v2-cdp-overlay.dtbo-base :=  msm8998-v2.dtb
msm8998-v2-mtp-overlay.dtbo-base :=  msm8998-v2.dtb
msm8998-v2.1-cdp-overlay.dtbo-base :=  msm8998-v2.1.dtb
msm8998-v2.1-mtp-overlay.dtbo-base :=  msm8998-v2.1.dtb
msm8998-qrd-overlay.dtbo-base :=  msm8998-qrd.dtb
msm8998-qrd-vr1-overlay.dtbo-base :=  msm8998-qrd-vr1.dtb
msm8998-qrd-skuk-overlay.dtbo-base :=  msm8998-qrd-skuk.dtb
else
dtb-$(CONFIG_ARCH_MSM8998) += msm8998-sim.dtb \
	msm8998-rumi.dtb \
	msm8998-cdp.dtb \
	msm8998-mtp.dtb \
	msm8998-qrd.dtb \
	msm8998-v2-sim.dtb \
	msm8998-v2-rumi.dtb \
	msm8998-v2-mtp.dtb \
	msm8998-v2-cdp.dtb \
	msm8998-v2-qrd.dtb \
	msm8998-qrd-skuk.dtb \
	msm8998-v2-qrd-skuk.dtb \
	msm8998-qrd-vr1.dtb \
	msm8998-v2-qrd-vr1.dtb \
	msm8998-v2-qrd-skuk-evt3.dtb \
	msm8998-v2-qrd-skuk-hdk.dtb \
	apq8098-mtp.dtb \
	apq8098-cdp.dtb \
	apq8098-v2-mtp.dtb \
	apq8098-v2-cdp.dtb \
	apq8098-v2-qrd.dtb \
	apq8098-v2-qrd-skuk-hdk.dtb \
	msm8998-v2.1-mtp.dtb \
	msm8998-v2.1-mtp-4k-display.dtb \
	msm8998-v2.1-cdp.dtb \
	msm8998-v2.1-qrd.dtb \
	apq8098-v2.1-mtp.dtb \
	apq8098-v2.1-cdp.dtb \
	apq8098-v2.1-qrd.dtb \
	apq8098-v2.1-mediabox.dtb \
	apq8098-v2.1-svr20.dtb \
	msm8998-v2.1-interposer-sdm660-cdp.dtb \
	msm8998-v2.1-interposer-sdm660-mtp.dtb \
	msm8998-v2.1-interposer-sdm660-qrd.dtb
endif

dtb-$(CONFIG_ARCH_MSMHAMSTER) += msmhamster-rumi.dtb

dtb-$(CONFIG_ARCH_SDM660) += sdm660-sim.dtb \
	sdm660-internal-codec-cdp.dtb \
	sdm660-internal-codec-mtp.dtb \
	sdm660-internal-codec-rcm.dtb \
	sdm660-cdp.dtb \
	sdm660-mtp.dtb \
	sdm660-qrd.dtb \
	sdm660-rcm.dtb \
	sdm660-rumi.dtb \
	sdm660-pm660a-cdp.dtb \
	sdm660-pm660a-mtp.dtb \
	sdm660-pm660a-qrd.dtb \
	sdm660-pm660a-rcm.dtb \
	sdm660-pm660a-rumi.dtb \
	sdm660-internal-codec-pm660a-cdp.dtb \
	sdm660-internal-codec-pm660a-mtp.dtb \
	sdm660-internal-codec-pm660a-rcm.dtb \
	sdm660-pm660a-sim.dtb \
	sda660-cdp.dtb \
	sda660-mtp.dtb \
	sda660-rcm.dtb \
	sda660-pm660a-cdp.dtb \
	sda660-pm660a-mtp.dtb \
	sda660-pm660a-rcm.dtb \
	sda660-pm660a-qrd-hdk.dtb \
	sdm660-headset-jacktype-no-cdp.dtb \
	sdm660-headset-jacktype-no-rcm.dtb \
	sdm660-pm660a-headset-jacktype-no-cdp.dtb \
	sdm660-pm660a-headset-jacktype-no-rcm.dtb \
	sdm660-usbc-audio-mtp.dtb \
	sdm660-usbc-audio-rcm.dtb \
	sdm658-mtp.dtb \
	sdm658-cdp.dtb \
	sdm658-rcm.dtb \
	sdm658-qrd.dtb \
	sdm658-pm660a-mtp.dtb \
	sdm658-pm660a-cdp.dtb \
	sdm658-pm660a-rcm.dtb \
	sdm658-pm660a-qrd.dtb \
	sdm658-internal-codec-mtp.dtb \
	sdm658-internal-codec-cdp.dtb \
	sdm658-internal-codec-rcm.dtb \
	sdm658-internal-codec-pm660a-mtp.dtb \
	sdm658-internal-codec-pm660a-cdp.dtb \
	sdm658-internal-codec-pm660a-rcm.dtb \
	sda658-cdp.dtb \
	sda658-mtp.dtb \
	sda658-rcm.dtb \
	sda658-pm660a-mtp.dtb \
	sda658-pm660a-cdp.dtb \
	sda658-pm660a-rcm.dtb \
	sdm636-cdp.dtb \
	sdm636-mtp.dtb \
	sdm636-qrd.dtb \
	sdm636-rcm.dtb \
	sdm636-headset-jacktype-no-cdp.dtb \
	sdm636-headset-jacktype-no-rcm.dtb \
	sdm636-internal-codec-cdp.dtb \
	sdm636-internal-codec-mtp.dtb \
	sdm636-internal-codec-pm660a-cdp.dtb \
	sdm636-internal-codec-pm660a-mtp.dtb \
	sdm636-internal-codec-pm660a-rcm.dtb \
	sdm636-internal-codec-rcm.dtb \
	sdm636-pm660a-headset-jacktype-no-cdp.dtb \
	sdm636-pm660a-headset-jacktype-no-rcm.dtb \
	sdm636-pm660a-cdp.dtb \
	sdm636-pm660a-mtp.dtb \
	sdm636-pm660a-qrd.dtb \
	sdm636-pm660a-rcm.dtb \
	sdm636-usbc-audio-mtp.dtb \
	sdm636-usbc-audio-rcm.dtb \
	sda636-cdp.dtb \
	sda636-mtp.dtb \
	sda636-rcm.dtb \
	sda636-pm660a-cdp.dtb \
	sda636-pm660a-mtp.dtb \
	sda636-pm660a-qrd-hdk.dtb \
	sda636-pm660a-rcm.dtb

dtb-$(CONFIG_ARCH_SDM630) += sdm630-rumi.dtb \
	sdm630-pm660a-rumi.dtb \
	sdm630-mtp.dtb \
	sdm630-usbc-audio-mtp.dtb \
	sdm630-usbc-audio-rcm.dtb \
	sdm630-cdp.dtb \
	sdm630-rcm.dtb \
	sdm630-internal-codec-mtp.dtb \
	sdm630-internal-codec-cdp.dtb \
	sdm630-internal-codec-rcm.dtb \
	sdm630-pm660a-cdp.dtb \
	sdm630-pm660a-mtp.dtb \
	sdm630-pm660a-rcm.dtb \
	sdm630-pm660a-qrd.dtb \
	sdm630-internal-codec-pm660a-cdp.dtb \
	sdm630-internal-codec-pm660a-mtp.dtb \
	sdm630-internal-codec-pm660a-rcm.dtb \
	sda630-mtp.dtb \
	sda630-cdp.dtb \
	sda630-rcm.dtb \
	sda630-pm660a-mtp.dtb \
	sda630-pm660a-cdp.dtb \
	sda630-pm660a-rcm.dtb \
	sda630-pm660a-qrd-hdk.dtb \
	sdm630-headset-jacktype-no-cdp.dtb \
	sdm630-headset-jacktype-no-rcm.dtb \
	sdm630-pm660a-headset-jacktype-no-cdp.dtb \
	sdm630-pm660a-headset-jacktype-no-rcm.dtb \
	sdm455-mtp.dtb \
	sdm455-qrd.dtb \
	sdm455-cdp.dtb \
	sdm455-rcm.dtb \
	sdm455-internal-codec-rcm.dtb

ifeq ($(CONFIG_ARM64),y)
always          := $(dtb-y)
always          += $(dtbo-y)
subdir-y        := $(dts-dirs)
else
targets += dtbs
targets += $(addprefix ../, $(dtb-y))

$(obj)/../%.dtb: $(src)/%.dts FORCE
	$(call if_changed_dep,dtc)

dtbs: $(addprefix $(obj)/../,$(dtb-y))
endif
clean-files := *.dtbo *.dtb
