
dtb-$(CONFIG_MSM_GVM_QUIN) += vplatform-lfv-msm8996.dtb

dtb-$(CONFIG_ARCH_MSMCOBALT) += msmcobalt-sim.dtb \
	msmcobalt-rumi.dtb \
	msmcobalt-cdp.dtb \
	msmcobalt-mtp.dtb \
	msmcobalt-v2-sim.dtb \
	msmcobalt-v2-rumi.dtb \
	msmcobalt-v2-mtp.dtb \
	msmcobalt-v2-cdp.dtb

dtb-$(CONFIG_ARCH_MSM8937) += msm8937-rumi.dtb \
	msm8937-pmi8950-cdp.dtb \
	msm8937-pmi8937-cdp.dtb \
	msm8937-pmi8940-cdp.dtb \
	msm8937-pmi8950-ext-codec-cdp.dtb \
	msm8937-pmi8950-mtp.dtb \
	msm8937-pmi8937-mtp.dtb \
	msm8937-pmi8940-mtp.dtb \
	msm8937-pmi8950-rcm.dtb \
	msm8937-pmi8937-rcm.dtb \
	msm8937-pmi8940-rcm.dtb \
	msm8937-pmi8950-qrd-sku1.dtb \
	msm8937-pmi8937-qrd-sku2.dtb \
	msm8937-pmi8950-qrd-sku1-dvt1.dtb \
	msm8937-pmi8937-qrd-sku2-dvt1.dtb \
	apq8037-pmi8950-cdp.dtb \
	apq8037-pmi8937-cdp.dtb \
	apq8037-pmi8950-mtp.dtb \
	apq8037-pmi8937-mtp.dtb

dtb-$(CONFIG_ARCH_MSM8917) += msm8917-rumi.dtb \
	apq8017-pmi8937-cdp.dtb \
	apq8017-pmi8937-mtp.dtb \
	apq8017-pmi8950-cdp.dtb \
	apq8017-pmi8950-mtp.dtb \
	msm8917-pmi8937-cdp.dtb \
	msm8917-pmi8937-mtp.dtb \
	msm8917-pmi8940-cdp.dtb \
	msm8917-pmi8940-mtp.dtb \
	msm8917-pmi8950-cdp.dtb \
	msm8917-pmi8950-cdp-mirror-lake-touch.dtb \
	msm8917-pmi8950-ext-codec-cdp.dtb \
	msm8917-pmi8950-mtp.dtb \
	msm8917-pmi8937-rcm.dtb \
	msm8917-pmi8940-rcm.dtb \
	msm8917-pmi8950-rcm.dtb \
	msm8917-qgp-tmo.dtb \
	msm8917-pmi8937-qrd-sku5.dtb \
	apq8017-pmi8937-cdp-wcd-rome.dtb \
	apq8017-pmi8950-cdp-wcd-rome.dtb \
	apq8017-no-pmi-wcd-rome-cdp.dtb


dtb-$(CONFIG_ARCH_MSM8953) += msm8953-sim.dtb \
	msm8953-rumi.dtb \
	msm8953-cdp.dtb \
	msm8953-mtp.dtb \
	msm8953-ext-codec-mtp.dtb \
	msm8953-qrd-sku3.dtb \
	msm8953-qrd-sku3-e7.dtb \
	msm8953-qrd-sku3-daisy.dtb \
	msm8953-rcm.dtb \
	msm8953-ext-codec-rcm.dtb \
	msm8953-ipc.dtb \
	msm8953-cdp-1200p.dtb \
	msm8953-iot-mtp.dtb \
	msm8953-pmi8940-cdp.dtb \
	msm8953-pmi8940-mtp.dtb \
	msm8953-pmi8937-cdp.dtb \
	msm8953-pmi8937-mtp.dtb \
	msm8953-pmi8940-ext-codec-mtp.dtb \
	msm8953-pmi8937-ext-codec-mtp.dtb


ifeq ($(CONFIG_ARM64),y)
always          := $(dtb-y)
subdir-y        := $(dts-dirs)
else
targets += dtbs
targets += $(addprefix ../, $(dtb-y))

$(obj)/../%.dtb: $(src)/%.dts FORCE
        $(call if_changed_dep,dtc)

dtbs: $(addprefix $(obj)/,$(dtb-y))
endif
clean-files := *.dtb
