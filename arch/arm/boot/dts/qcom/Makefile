
dtb-$(CONFIG_ARCH_SDXPOORWILLS) += sdxpoorwills-rumi.dtb \
	sdxpoorwills-cdp.dtb \
	sdxpoorwills-mtp.dtb \
	sdxpoorwills-atp.dtb \
	sdxpoorwills-cdp-256.dtb \
	sdxpoorwills-mtp-256.dtb \
	sdxpoorwills-dualwifi-cdp.dtb \
	sdxpoorwills-dualwifi-mtp.dtb \
	sdxpoorwills-pcie-ep-cdp-256.dtb \
	sdxpoorwills-pcie-ep-mtp-256.dtb \
	sdxpoorwills-pcie-ep-cdp.dtb \
	sdxpoorwills-pcie-ep-mtp.dtb \
	sdxpoorwills-v2-mtp.dtb \
	sdxpoorwills-v2-cdp.dtb \
	sdxpoorwills-v2-dualwifi-mtp.dtb \
	sdxpoorwills-v2-dualwifi-cdp.dtb \
	sdxpoorwills-v2-pcie-ep-mtp-256.dtb \
	sdxpoorwills-v2-pcie-ep-mtp.dtb \
	sa415m-ttp.dtb \
	sa415m-ccard.dtb \
	sa415m-ccard-pcie-ep.dtb \
	sa415m-ccard-usb-ep.dtb \
	sa415m-ttp-pcie-ep.dtb \
	sa415m-ttp-usb-ep.dtb \
	sa415m-mtp-256.dtb \
	sa415m-cdp.dtb \
	sa415m-v2-cdp.dtb \
	sa415m-v2-mtp.dtb

dtb-$(CONFIG_ARCH_MDM9650) += mdm9650-nand-mtp.dtb \
	mdm9650-ttp.dtb \
	mdm9650-v1.1-nand-mtp.dtb \
	mdm9650-v1.1-nand-cv2x.dtb

dtb-$(CONFIG_ARCH_MDM9607) += mdm9607-mtp.dtb \
	mdm9607-ttp.dtb

targets += dtbs

include $(srctree)/arch/arm64/boot/dts/qcom/Makefile
$(obj)/%.dtb: $(src)/../../../../arm64/boot/dts/qcom/%.dts FORCE
	$(call if_changed_dep,dtc)

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
$(obj)/%.dtbo:$(src)/../../../../arm64/boot/dts/qcom/%.dts FORCE
	$(call if_changed_dep,dtc)
	$(call if_changed,dtbo_verify)

dtbs: $(addprefix $(obj)/,$(dtb-y)) $(addprefix $(obj)/,$(dtbo-y))
else
dtbs: $(addprefix $(obj)/,$(dtb-y))
endif
clean-files := *.dtb
