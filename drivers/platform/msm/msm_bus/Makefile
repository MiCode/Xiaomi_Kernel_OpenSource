#
# Makefile for msm-bus driver specific files
#
obj-y +=  msm_bus_core.o msm_bus_client_api.o
obj-$(CONFIG_OF) += msm_bus_of.o
obj-$(CONFIG_MSM_RPM_SMD) += msm_bus_rpm_smd.o

ifdef CONFIG_BUS_TOPOLOGY_ADHOC
	obj-y += msm_bus_fabric_adhoc.o msm_bus_arb_adhoc.o msm_bus_rules.o \
		msm_bus_bimc_adhoc.o msm_bus_noc_adhoc.o
	obj-$(CONFIG_OF) += msm_bus_of_adhoc.o
	obj-$(CONFIG_DEBUG_BUS_VOTER) += msm_bus_dbg_voter.o
	obj-$(CONFIG_CORESIGHT) +=  msm_buspm_coresight_adhoc.o
else
	obj-y += msm_bus_fabric.o msm_bus_config.o msm_bus_arb.o \
		msm_bus_bimc.o msm_bus_noc.o
	obj-$(CONFIG_CORESIGHT) +=  msm_buspm_coresight.o
ifdef CONFIG_ARCH_MSM8974
	obj-$(CONFIG_ARCH_MSM8974) += msm_bus_board_8974.o
else
	obj-y += msm_bus_id.o
endif
endif


obj-$(CONFIG_DEBUG_FS) += msm_bus_dbg.o
