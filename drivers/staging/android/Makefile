ccflags-y += -I$(src)			# needed for trace events

##obj-y					+= ion/

obj-$(CONFIG_ASHMEM)			+= ashmem.o
obj-$(CONFIG_ANDROID_VSOC)		+= vsoc.o

##   check if it's common-kenrel project
##   6779 is common-kernel project,
## we keep it as common-kernel code

# ifneq (,$(filter $(strip $(CONFIG_MTK_PLATFORM)), "mt6761" "mt6765"))
# $(info GM-T sample test pass 2 mtk ion)
# endif

ifeq ($(CONFIG_MTK_ION),y)
$(info ION_DEBUG mtk ion)
obj-y += mtk_ion/
else
$(info ION_DEBUG aosp ion)
obj-y += aosp_ion/
endif
