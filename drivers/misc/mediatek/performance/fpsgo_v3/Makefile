ifeq ($(CONFIG_MTK_FPSGO_V3),$(filter $(CONFIG_MTK_FPSGO_V3),m))
ccflags-y += -DCONFIG_MTK_FPSGO_V3
subdir-ccflags-y += -DCONFIG_MTK_FPSGO_V3
endif

FPSGO_TOP = $(srctree)/drivers/misc/mediatek/performance/fpsgo_v3/
BASE_TOP = $(FPSGO_TOP)/base
FBT_TOP = $(FPSGO_TOP)/fbt
FSTB_TOP = $(FPSGO_TOP)/fstb
COM_TOP = $(FPSGO_TOP)/composer
UTCH_TOP = $(FPSGO_TOP)/utch

ccflags-y += \
        -I$(srctree)/include/ \
        -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/ \
        -I$(srctree)/drivers/gpu/mediatek/ged/include/ \
        -I$(BASE_TOP)/include/ \
        -I$(FBT_TOP)/include/ \
        -I$(FSTB_TOP)/ \
        -I$(COM_TOP)/include/ \
				-I$(srctree)/kernel/ \
				-I$(srctree)/drivers/misc/mediatek/sched/ \
				-I$(UTCH_TOP)/ \

subdir-ccflags-y += \
        -I$(srctree)/include/ \
        -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/ \
        -I$(srctree)/drivers/gpu/mediatek/ged/include/ \
        -I$(BASE_TOP)/include/ \
        -I$(FBT_TOP)/include/ \
        -I$(FSTB_TOP)/ \
        -I$(COM_TOP)/include/ \
				-I$(srctree)/kernel/ \
				-I$(srctree)/drivers/misc/mediatek/sched/ \
				-I$(UTCH_TOP)/ \


#fpsgo_main
MTK_FPSGO_MAIN += fpsgo_main.o

#base
include $(FPSGO_TOP)/base/build.mk
MTK_FPSGO_BASE = $(mtk_fpsgo_base_objs:%.o=base/%.o)

#composer
include $(FPSGO_TOP)/composer/build.mk
MTK_FPSGO_COMPOSER = $(mtk_fpsgo_composer_objs:%.o=composer/%.o)

#fbt
include $(FPSGO_TOP)/fbt/build.mk
ccflags-y += -DNR_FREQ_CPU=16
MTK_FPSGO_FBT = $(mtk_fpsgo_fbt_objs:%.o=fbt/%.o)

#fstb
include $(FPSGO_TOP)/fstb/build.mk
MTK_FPSGO_FSTB = $(mtk_fpsgo_fstb_objs:%.o=fstb/%.o)

#tchbst
include $(FPSGO_TOP)/utch/build.mk
MTK_FPSGO_UTCH = $(mtk_fpsgo_utch_objs:%.o=utch/%.o)

MTK_FPSGO_SRC = $(MTK_FPSGO_MAIN) $(MTK_FPSGO_BASE) $(MTK_FPSGO_COMPOSER) $(MTK_FPSGO_FBT) $(MTK_FPSGO_FSTB) $(MTK_FPSGO_UTCH)
#MTK_FPSGO_SRC = $(MTK_FPSGO_COMPOSER)

obj-$(CONFIG_MTK_FPSGO_V3) += mtk_fpsgo.o
mtk_fpsgo-objs += $(MTK_FPSGO_SRC)

