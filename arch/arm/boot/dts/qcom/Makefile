dtb-$(CONFIG_ARCH_MSM8996) += gemini-msm8996-v2-pmi8994-mtp.dtb \
	gemini-msm8996-v3-pmi8994-mtp.dtb \
	gemini-msm8996-v3.0-pmi8994-mtp.dtb \
	gemini-msm8996pro-pmi8994-mtp.dtb \
	gemini2-msm8996-v3-pmi8994-mtp.dtb \
	gemini2-msm8996-v3.0-pmi8994-mtp.dtb \
	gemini2-msm8996pro-pmi8994-mtp.dtb \
	scorpio-msm8996-v3-pmi8996-mtp.dtb \
	scorpio-msm8996-v3.0-pmi8996-mtp.dtb \
	scorpio-msm8996pro-pmi8996-mtp.dtb \
	scorpio-p1-msm8996-v3-pmi8996-mtp.dtb \
	scorpio-p1-msm8996-v3.0-pmi8996-mtp.dtb \
	scorpio-p1-msm8996pro-pmi8996-mtp.dtb \
	capricorn-msm8996-v3-pmi8996-mtp.dtb \
	capricorn-msm8996-v3.0-pmi8996-mtp.dtb \
	capricorn-msm8996pro-pmi8996-mtp.dtb  \
	lithium-msm8996-v3.0-pmi8996-mtp.dtb \
	lithium-msm8996pro-pmi8996-mtp.dtb \
	natrium-msm8996-v3.0-pmi8996-mtp.dtb \
	natrium-msm8996pro-pmi8996-mtp.dtb

dtb-$(CONFIG_ARCH_MSMCOBALT) += msmcobalt-sim.dtb \
	msmcobalt-rumi.dtb

dtb-$(CONFIG_ARCH_MDM9640) += mdm9640-sim.dtb \
	mdm9640-rumi.dtb \
	mdm9640-emmc-cdp.dtb \
	mdm9640-nand-cdp.dtb \
	mdm9640-mtp.dtb

dtb-$(CONFIG_ARCH_MDMCALIFORNIUM) += mdmcalifornium-sim.dtb \
	mdmcalifornium-rumi.dtb \
	mdmcalifornium-emmc-cdp.dtb \
	mdmcalifornium-nand-cdp.dtb \
	mdmcalifornium-emmc-mtp.dtb \
	mdmcalifornium-nand-mtp.dtb

dtb-$(CONFIG_ARCH_MSM8937) += msm8937-rumi.dtb \
	msm8937-pmi8950-cdp.dtb \
	msm8937-pmi8937-cdp.dtb \
	msm8937-pmi8950-ext-codec-cdp.dtb \
	msm8937-pmi8950-mtp.dtb \
	msm8937-pmi8937-mtp.dtb \
	msm8937-pmi8950-rcm.dtb \
	msm8937-pmi8937-rcm.dtb \
	msm8937-pmi8950-qrd-sku1.dtb \
	msm8937-pmi8937-qrd-sku2.dtb \
	msm8937-pmi8950-qrd-sku1-dvt1.dtb \
	msm8937-pmi8937-qrd-sku2-dvt1.dtb \
	apq8037-pmi8950-cdp.dtb \
	apq8037-pmi8937-cdp.dtb \
	apq8037-pmi8950-mtp.dtb \
	apq8037-pmi8937-mtp.dtb

dtb-$(CONFIG_ARCH_MSMGOLD) += msmgold-rumi.dtb \
	apqgold-pmi8937-cdp.dtb \
	apqgold-pmi8937-mtp.dtb \
	apqgold-pmi8950-cdp.dtb \
	apqgold-pmi8950-mtp.dtb \
	msmgold-pmi8937-cdp.dtb \
	msmgold-pmi8937-mtp.dtb \
	msmgold-pmi8950-cdp.dtb \
	msmgold-pmi8950-ext-codec-cdp.dtb \
	msmgold-pmi8950-mtp.dtb \
	msmgold-pmi8937-rcm.dtb \
	msmgold-pmi8950-rcm.dtb

dtb-$(CONFIG_ARCH_MSMTITANIUM) += msmtitanium-sim.dtb \
	msmtitanium-rumi.dtb \
	msmtitanium-cdp.dtb \
	msmtitanium-mtp.dtb \
	msmtitanium-ext-codec-mtp.dtb \
	msmtitanium-qrd-sku3.dtb \
	msmtitanium-rcm.dtb \
	msmtitanium-ext-codec-rcm.dtb \
	apqtitanium-cdp.dtb \
	apqtitanium-mtp.dtb

dtb-$(CONFIG_ARCH_MDM9607) += mdm9607-rumi.dtb \
	mdm9607-cdp.dtb \
	mdm9607-mtp.dtb \
	mdm9607-rcm.dtb \
	mdm9607-mtp-sdcard.dtb

dtb-$(CONFIG_ARCH_MSM8916) += msm8952-qrd-skum.dtb \
       msm8952-cdp.dtb \
       msm8952-ext-codec-cdp.dtb \
       msm8952-mtp.dtb

dtb-$(CONFIG_ARCH_MSM8909) += msm8909-pm8916-mtp.dtb \
	msm8909w-wtp.dtb \
	apq8009w-wtp.dtb

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
