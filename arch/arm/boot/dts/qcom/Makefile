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
	msm8996-v2-pmi8994-pm8004-cdp.dtb \
	msm8996-v2-pmi8994-pm8004-mtp.dtb \
	msm8996-v3-pmi8994-cdp.dtb \
	msm8996-v3-pmi8994-mtp.dtb \
	msm8996-v3-pmi8994-pm8004-cdp.dtb \
	msm8996-v3-pmi8994-pm8004-mtp.dtb \
	msm8996-v2-fluid.dtb \
	msm8996-v3-fluid.dtb \
	msm8996-v2-liquid.dtb \
	msm8996-v3-liquid.dtb

targets += dtbs
targets += $(addprefix ../, $(dtb-y))
endif

$(obj)/../%.dtb: $(src)/%.dts FORCE
	$(call if_changed_dep,dtc)

dtbs: $(addprefix $(obj)/../,$(dtb-y))

clean-files := *.dtb
