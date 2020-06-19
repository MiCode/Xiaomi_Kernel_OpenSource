obj-y += dfd.o

ifeq ($(CONFIG_MACH_MT6761),y)
obj-y += mt6765/
else ifeq ($(CONFIG_MACH_MT6893),y)
obj-y += mt6885/
else
obj-$(CONFIG_MTK_DFD_INTERNAL_DUMP) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif
