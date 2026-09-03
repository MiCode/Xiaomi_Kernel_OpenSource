GCOV_PROFILE 			= ${CONFIG_UNISOC_SIPA_V3_GCOV}

obj-$(CONFIG_UNISOC_SIPA_V3) += sipa-core.o

sipa-core-y += sipa_core.o sipa_hal.o sipa_nic.o sipa_skb_recv.o \
			   sipa_skb_send.o sipa_rm_res.o sipa_rm_dep_graph.o \
			   sipa_rm.o sipa_rm_peers_list.o sipa_usb_cons.o \
			   sipa_debugfs.o sipa_phy_r3p0/sipa_cmn_fifo_phy.o \
			   sipa_phy_r3p0/sipa_glb_phy.o sipa_dummy.o \
			   sipa_test.o

obj-$(CONFIG_UNISOC_SIPA_ETH) += sipa_eth.o
obj-$(CONFIG_UNISOC_SIPA_USB) += sipa_usb.o
