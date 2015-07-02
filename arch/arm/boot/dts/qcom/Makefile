ifeq ($(CONFIG_OF),y)

dtb-$(CONFIG_ARCH_MSM8996) += msm8996-v2-sim.dtb \
	msm8996-v3-sim.dtb \
	msm8996-v2-rumi.dtb \
	msm8996-v3-rumi.dtb \
	msm8996-v1-pmi8994-cdp.dtb \
	msm8996-v1-pmi8994-mtp.dtb \
	msm8996-v1-pmi8994-pm8004-cdp.dtb \
	msm8996-v1-pmi8994-pm8004-mtp.dtb \
	msm8996-v2-pmi8994-cdp.dtb \
	msm8996-v2-pmi8994-mtp.dtb \
	msm8996-v2-pmi8994-pmk8001-cdp.dtb \
	msm8996-v2-pmi8994-pmk8001-mtp.dtb \
	msm8996-v2-pmi8994-pm8004-cdp.dtb \
	msm8996-v2-pmi8994-pm8004-mtp.dtb \
	msm8996-v2-pmi8994-pm8004-pmk8001-cdp.dtb \
	msm8996-v2-pmi8994-pm8004-pmk8001-mtp.dtb \
	msm8996-v3-pmi8994-cdp.dtb \
	msm8996-v3-pmi8994-mtp.dtb \
	msm8996-v3-pmi8994-pmk8001-cdp.dtb \
	msm8996-v3-pmi8994-pmk8001-mtp.dtb \
	msm8996-v3-pmi8994-pm8004-cdp.dtb \
	msm8996-v3-pmi8994-pm8004-mtp.dtb \
	msm8996-v3-pmi8994-pm8004-pmk8001-cdp.dtb \
	msm8996-v3-pmi8994-pm8004-pmk8001-mtp.dtb \
	msm8996-v1-liquid.dtb \
	msm8996-v2-fluid.dtb \
	msm8996-v3-fluid.dtb \
	msm8996-v2-liquid.dtb \
	msm8996-v3-liquid.dtb \
	msm8996-v2.0-pmi8994-cdp.dtb \
	msm8996-v2.0-pmi8994-mtp.dtb \
	msm8996-v2.0-pmi8994-pmk8001-cdp.dtb \
	msm8996-v2.0-pmi8994-pmk8001-mtp.dtb \
	msm8996-v2.0-pmi8994-pm8004-cdp.dtb \
	msm8996-v2.0-pmi8994-pm8004-mtp.dtb \
	msm8996-v2.0-pmi8994-pm8004-pmk8001-cdp.dtb \
	msm8996-v2.0-pmi8994-pm8004-pmk8001-mtp.dtb \
	msm8996-v2.0-fluid.dtb \
	msm8996-v2.0-liquid.dtb \
	apq8096-v2-liquid.dtb \
	apq8096-v3-liquid.dtb \
	apq8096-v2-dragonboard.dtb \
	apq8096-v3-dragonboard.dtb

dtb-$(CONFIG_ARCH_MSMCOBALT) += msmcobalt-sim.dtb

dtb-$(CONFIG_ARCH_MDM9640) += mdm9640-sim.dtb \
	mdm9640-rumi.dtb \
	mdm9640-emmc-cdp.dtb \
	mdm9640-nand-cdp.dtb \
	mdm9640-mtp.dtb

dtb-$(CONFIG_ARCH_MSMTITANIUM) += msmtitanium-sim.dtb \
	msmtitanium-rumi.dtb

dtb-$(CONFIG_ARCH_MDMFERMIUM) += mdmfermium-rumi.dtb

targets += dtbs
targets += $(addprefix ../, $(dtb-y))
endif

$(obj)/../%.dtb: $(src)/%.dts FORCE
	$(call if_changed_dep,dtc)

dtbs: $(addprefix $(obj)/../,$(dtb-y))

clean-files := *.dtb
