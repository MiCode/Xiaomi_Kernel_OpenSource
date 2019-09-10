# SPDX-License-Identifier: GPL-2.0-only

obj-$(CONFIG_IPA3) += ipahal/

obj-$(CONFIG_IPA3) += ipat.o
ipat-y := ipa.o ipa_debugfs.o ipa_hdr.o ipa_flt.o ipa_rt.o ipa_dp.o ipa_client.o \
	ipa_utils.o ipa_nat.o ipa_intf.o teth_bridge.o ipa_interrupts.o \
	ipa_uc.o ipa_uc_wdi.o ipa_dma.o ipa_uc_mhi.o ipa_mhi.o ipa_uc_ntn.o \
	ipa_hw_stats.o ipa_pm.o ipa_wdi3_i.o ipa_odl.o ipa_wigig_i.o

ipat-$(CONFIG_IPA_EMULATION) += ipa_dt_replacement.o

obj-$(CONFIG_RMNET_IPA3) += rmnet_ipa.o ipa_qmi_service_v01.o ipa_qmi_service.o rmnet_ipa_fd_ioctl.o

obj-$(CONFIG_IPA3_MHI_PROXY) += ipa_mhi_proxy.o

obj-$(CONFIG_IPA3_MHI_PRIME_MANAGER) += ipa_mpm.o

ipat-$(CONFIG_IPA3_REGDUMP) += dump/ipa_reg_dump.o

ccflags-$(CONFIG_IPA3_REGDUMP) += -Idrivers/platform/msm/ipa/ipa_v3/dump

ccflags-$(CONFIG_IPA3_REGDUMP_IPA_4_5) += -Idrivers/platform/msm/ipa/ipa_v3/dump/ipa4.5
