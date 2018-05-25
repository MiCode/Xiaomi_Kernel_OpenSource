obj-$(CONFIG_IPA3) += ipahal/

ifdef CONFIG_X86
ccflags-y += -DIPA_EMULATION_COMPILE=1
else
ccflags-y += -DIPA_EMULATION_COMPILE=0
endif

obj-$(CONFIG_IPA3) += ipat.o
ipat-y := ipa.o ipa_debugfs.o ipa_hdr.o ipa_flt.o ipa_rt.o ipa_dp.o ipa_client.o \
	ipa_utils.o ipa_nat.o ipa_intf.o teth_bridge.o ipa_interrupts.o \
	ipa_uc.o ipa_uc_wdi.o ipa_dma.o ipa_uc_mhi.o ipa_mhi.o ipa_uc_ntn.o \
	ipa_hw_stats.o ipa_pm.o ipa_wdi3_i.o

ifdef CONFIG_X86
ipat-y += ipa_dtsi_replacement.o
endif

obj-$(CONFIG_RMNET_IPA3) += rmnet_ipa.o ipa_qmi_service_v01.o ipa_qmi_service.o rmnet_ipa_fd_ioctl.o
