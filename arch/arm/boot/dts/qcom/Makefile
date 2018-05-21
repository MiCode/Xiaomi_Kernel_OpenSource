
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
	sdxpoorwills-pcie-ep-mtp.dtb

dtb-$(CONFIG_ARCH_MDM9650) += mdm9650-nand-mtp.dtb \
	mdm9650-ttp.dtb \
	mdm9650-v1.1-nand-mtp.dtb \
	mdm9650-v1.1-nand-cv2x.dtb

dtb-$(CONFIG_ARCH_MDM9607) += mdm9607-mtp.dtb \
	mdm9607-ttp.dtb

targets += dtbs
targets += $(addprefix ../, $(dtb-y))

$(obj)/../%.dtb: $(src)/%.dts FORCE
	$(call if_changed_dep,dtc)

include $(srctree)/arch/arm64/boot/dts/qcom/Makefile
$(obj)/../%.dtb: $(src)/../../../../arm64/boot/dts/qcom/%.dts FORCE
	$(call if_changed_dep,dtc)

dtbs: $(addprefix $(obj)/../,$(dtb-y))
clean-files := *.dtb
