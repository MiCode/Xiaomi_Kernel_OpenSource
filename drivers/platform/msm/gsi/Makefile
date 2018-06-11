obj-$(CONFIG_GSI) += gsi.o gsi_dbg.o

ifdef CONFIG_X86
ccflags-y += -DIPA_EMULATION_COMPILE=1
obj-$(CONFIG_GSI) += gsi_emulation.o
else
ccflags-y += -DIPA_EMULATION_COMPILE=0
endif
