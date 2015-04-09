#
# Makefile for kernel SPMI framework.
#
obj-$(CONFIG_MSM_SPMI)			+= spmi.o spmi-resources.o
obj-$(CONFIG_MSM_SPMI_PMIC_ARB)		+= spmi-pmic-arb.o
obj-$(CONFIG_MSM_QPNP_INT)		+= qpnp-int.o

ifdef CONFIG_DEBUG_FS
obj-$(CONFIG_MSM_SPMI)			+= spmi-dbgfs.o
endif
