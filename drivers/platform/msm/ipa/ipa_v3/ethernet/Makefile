
obj-$(CONFIG_IPA_ETH) += ipa-eth.o

ipa-eth-y := \
	ipa_eth_bus.o \
	ipa_eth.o \
	ipa_eth_debugfs.o \
	ipa_eth_ep.o \
	ipa_eth_gsi.o \
	ipa_eth_net.o \
	ipa_eth_offload.o \
	ipa_eth_pci.o \
	ipa_eth_pm.o \
	ipa_eth_uc.o \
	ipa_eth_utils.o

ifneq ($(wildcard $(srctree)/$(src)/aquantia/Makefile),)
obj-$(CONFIG_AQC_IPA) += aquantia/
endif
