include $(srctree)/drivers/misc/mediatek/Makefile.custom
#CONFIG_MTK_CCCI_DRIVER :=m
ifeq ($(CONFIG_MTK_CCCI_EXT), y)

#include $(MTK_ROOT_BUILD)/Makefile

#CCCI_PLATFORM_DRIVER_DIR :=  $(wildcard $(obj)/$(call to-root,$(obj))mediatek/platform/$(call lc,\
#                               $(CONFIG_MTK_PLATFORM))/kernel/drivers/ccci)
#CCCI_PLATFORM_DRIVER_DIR :=  $(srctree)/mediatek/platform/$(call lc,$(CONFIG_MTK_PLATFORM))/kernel/drivers/dual_ccci
CCCI_PLATFORM_DRIVER_DIR := $(srctree)/drivers/misc/mediatek/dual_ccci
ifeq ($(CCCI_PLATFORM_DRIVER_DIR),)
$(error Invalid ccci platform path, please check)
endif

obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/

obj-$(CONFIG_MTK_CCCI_DRIVER) += ccci.o
ccci-y :=ccci_init.o
ccci-y +=ccci_hw.o
ccci-y +=ccci_logical.o
ccci-y +=ccci_md_main.o
ccci-y +=ccci_chrdev.o
ccci-y +=ccci_tty.o
ccci-y +=ccci_ipc.o
#ccci-y +=ccci_pmic.o
ccci-y +=ccci_fs_main.o
ccci-y +=ccci_rpc_main.o
ccci-y +=ccmni_net.o
ccci-y +=ccmni_v2_net.o
ccci-y +=ccmni.o
ccci-y +=ccmni_pfp.o
ccci-y +=ccci_mk_node.o
ccci-y +=ccci_statistics.o

#include $(CCCI_PLATFORM_DRIVER_DIR)/Makefile

ifeq ($(CONFIG_MTK_CCCI_DRIVER), y)
ccflags-y += -DENABLE_CCCI_DRV_BUILDIN
endif

clean-files += $(patsubst $(obj)/%,%,$(wildcard $(obj)/*~))

ccflags-y  += -I$(srctree)/../bionic/libc/kernel/common/linux  -I$(srctree)/$(obj)/include  -I$(CCCI_PLATFORM_DRIVER_DIR)/include

ifneq ($(TARGET_BUILD_VARIANT),user)
#ccflags-y  += -DCCCI_DEBUG_ON -D__CCCI_LOG__  -DCCCI_MD_DEBUG_ON  #-save-temps
ccflags-y  += -DCCCI_DEBUG_ON  -DCCCI_MD_DEBUG_ON  #-save-temps
#ccflags-y  += -DENCRYPT_DEBUG -DCCCI_RPC_DEBUG_ON
endif
ccflags-y  += -D__CCCI_LOG__

else 
obj- :=dummy.o


endif
