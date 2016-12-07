dtb-$(CONFIG_ARCH_QCOM)	+= apq8016-sbc.dtb msm8916-mtp.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= msm8996-mtp.dtb
dtb-$(CONFIG_ARCH_QCOM)	+= apq8096-db820c.dtb

dtb-$(CONFIG_ARCH_MSMSKUNK) += msmskunk-sim.dtb \
	msmskunk-rumi.dtb \
	msmskunk-mtp.dtb \
	msmskunk-cdp.dtb

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb
