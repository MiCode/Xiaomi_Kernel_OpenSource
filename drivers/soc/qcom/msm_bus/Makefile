#
# Makefile for msm-bus driver specific files
#
obj-y +=  msm_bus_core.o msm_bus_client_api.o
obj-$(CONFIG_OF) += msm_bus_of.o
obj-$(CONFIG_MSM_RPM_SMD) += msm_bus_rpm_smd.o
obj-y += msm_bus_fabric_adhoc.o msm_bus_arb_adhoc.o msm_bus_rules.o \
	msm_bus_bimc_adhoc.o msm_bus_noc_adhoc.o
obj-$(CONFIG_OF) += msm_bus_of_adhoc.o
obj-$(CONFIG_DEBUG_BUS_VOTER) += msm_bus_dbg_voter.o
obj-$(CONFIG_CORESIGHT) +=  msm_buspm_coresight_adhoc.o

obj-$(CONFIG_DEBUG_FS) += msm_bus_dbg.o
