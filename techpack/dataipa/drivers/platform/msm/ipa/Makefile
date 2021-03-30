# SPDX-License-Identifier: GPL-2.0-only

ipam-y += \
	ipa_rm.o ipa_rm_dependency_graph.o ipa_rm_peers_list.o ipa_rm_resource.o ipa_rm_inactivity_timer.o \
	ipa_v3/ipa.o \
	ipa_v3/ipa_debugfs.o \
	ipa_v3/ipa_hdr.o \
	ipa_v3/ipa_flt.o \
	ipa_v3/ipa_rt.o \
	ipa_v3/ipa_dp.o \
	ipa_v3/ipa_client.o \
	ipa_v3/ipa_utils.o \
	ipa_v3/ipa_nat.o \
	ipa_v3/ipa_intf.o \
	ipa_v3/teth_bridge.o \
	ipa_v3/ipa_interrupts.o \
	ipa_v3/ipa_uc.o \
	ipa_v3/ipa_uc_wdi.o \
	ipa_v3/ipa_dma.o \
	ipa_v3/ipa_uc_mhi.o \
	ipa_v3/ipa_mhi.o \
	ipa_v3/ipa_uc_ntn.o \
	ipa_v3/ipa_hw_stats.o \
	ipa_v3/ipa_pm.o \
	ipa_v3/ipa_wdi3_i.o \
	ipa_v3/ipa_odl.o \
	ipa_v3/ipa_wigig_i.o \
	ipa_v3/ipa_uc_holb_monitor.o \
	ipa_v3/ipahal/ipahal.o \
	ipa_v3/ipahal/ipahal_reg.o \
	ipa_v3/ipahal/ipahal_fltrt.o \
	ipa_v3/ipahal/ipahal_hw_stats.o \
	ipa_v3/ipahal/ipahal_nat.o \

ipam-$(CONFIG_RMNET_IPA3) += ipa_v3/rmnet_ipa.o ipa_v3/ipa_qmi_service_v01.o \
	ipa_v3/ipa_qmi_service.o ipa_v3/rmnet_ctl_ipa.o \
	ipa_v3/rmnet_ipa_fd_ioctl.o

ipam-$(CONFIG_IPA3_MHI_PROXY) += ipa_v3/ipa_mhi_proxy.o
ipam-$(CONFIG_IPA_EMULATION) += ipa_v3/ipa_dt_replacement.o
ipam-$(CONFIG_IPA3_REGDUMP) += ipa_v3/dump/ipa_reg_dump.o

ipam-$(CONFIG_IPA_UT) += test/ipa_ut_framework.o test/ipa_test_example.o \
	test/ipa_test_mhi.o test/ipa_test_dma.o \
	test/ipa_test_hw_stats.o test/ipa_pm_ut.o \
	test/ipa_test_wdi3.o

obj-$(CONFIG_IPA3) += ipam.o

obj-y += ipa_v3/ ipa_clients/

