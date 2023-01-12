# SPDX-License-Identifier: GPL-2.0-only

# auto-detect subdirs
ifeq ($(CONFIG_ARCH_KONA), y)
include $(srctree)/techpack/display/config/konadisp.conf
endif

ifeq ($(CONFIG_ARCH_KONA), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/konadispconf.h
endif

ifeq ($(CONFIG_ARCH_LAHAINA), y)
     ifeq ($(CONFIG_QGKI), y)
		include $(srctree)/techpack/display/config/lahainadisp.conf
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/lahainadispconf.h
     else
		include $(srctree)/techpack/display/config/gki_lahainadisp.conf
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/gki_lahainadispconf.h
     endif
endif

ifeq ($(CONFIG_ARCH_HOLI), y)
     ifeq ($(CONFIG_QGKI), y)
		include $(srctree)/techpack/display/config/holidisp.conf
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/holidispconf.h
     else
		include $(srctree)/techpack/display/config/gki_holidisp.conf
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/gki_holidispconf.h
     endif
endif

LINUXINCLUDE    += \
		   -I$(srctree)/techpack/display/include/uapi/display \
		   -I$(srctree)/techpack/display/include
USERINCLUDE     += -I$(srctree)/techpack/display/include/uapi/display

ifeq ($(CONFIG_ARCH_LITO), y)
include $(srctree)/techpack/display/config/saipdisp.conf
endif

ifeq ($(CONFIG_ARCH_LITO), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/saipdispconf.h
endif

ifeq ($(CONFIG_ARCH_BENGAL), y)
include $(srctree)/techpack/display/config/bengaldisp.conf
endif

ifeq ($(CONFIG_ARCH_BENGAL), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/bengaldispconf.h
endif

ifeq ($(CONFIG_ARCH_MONACO), y)
include $(srctree)/techpack/display/config/monacodisp.conf
endif

ifeq ($(CONFIG_ARCH_MONACO), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/monacodispconf.h
endif

obj-$(CONFIG_DRM_MSM) += msm/

ifeq ($(CONFIG_ARCH_SDXLEMUR), y)
include $(srctree)/techpack/display/config/sdxlemurdisp.conf
endif

ifeq ($(CONFIG_ARCH_SDXLEMUR), y)
LINUXINCLUDE    += -include $(srctree)/techpack/display/config/sdxlemurdispconf.h
endif

obj-$(CONFIG_DRM_QPIC_DISPLAY) += tinydrm/
