# SPDX-License-Identifier: GPL-2.0-only

ifeq ($(CONFIG_ARCH_LAHAINA), y)
LINUXINCLUDE    += -include $(srctree)/techpack/dataipa/config/dataipa.h
ifeq ($(CONFIG_QGKI),y)
include $(srctree)/techpack/dataipa/config/dataipa_QGKI.conf
ifeq ($(CONFIG_LOCALVERSION), "-qgki-debug")
include $(srctree)/techpack/dataipa/config/dataipa_debug.conf
LINUXINCLUDE    += -include $(srctree)/techpack/dataipa/config/dataipa_debug.h
endif
else
include $(srctree)/techpack/dataipa/config/dataipa_GKI.conf
endif
endif

ifeq ($(CONFIG_ARCH_HOLI), y)
LINUXINCLUDE    += -include $(srctree)/techpack/dataipa/config/dataipa.h
ifeq ($(CONFIG_QGKI),y)
include $(srctree)/techpack/dataipa/config/dataipa_QGKI.conf
ifeq ($(CONFIG_LOCALVERSION), "-qgki-debug")
include $(srctree)/techpack/dataipa/config/dataipa_debug.conf
LINUXINCLUDE    += -include $(srctree)/techpack/dataipa/config/dataipa_debug.h
endif
else
include $(srctree)/techpack/dataipa/config/dataipa_GKI.conf
endif
endif

ifeq ($(CONFIG_QTI_QUIN_GVM),y)
LINUXINCLUDE    += -include $(srctree)/techpack/dataipa/config/dataipa_gsi_V1.h
ifeq ($(CONFIG_QGKI),y)
include $(srctree)/techpack/dataipa/config/dataipa_gsi_V1_QGKI.conf
ifeq ($(CONFIG_LOCALVERSION), "-qgki-debug")
include $(srctree)/techpack/dataipa/config/dataipa_debug.conf
LINUXINCLUDE    += -include $(srctree)/techpack/dataipa/config/dataipa_debug.h
endif
else
include $(srctree)/techpack/dataipa/config/dataipa_gsi_V1_GKI.conf
endif
endif

DATAIPADRVTOP = $(srctree)/techpack/dataipa/drivers/platform/msm

ifneq (,$(filter $(CONFIG_IPA3),y m))
LINUXINCLUDE += -I$(DATAIPADRVTOP)/gsi
LINUXINCLUDE += -I$(DATAIPADRVTOP)/ipa
LINUXINCLUDE += -I$(DATAIPADRVTOP)/ipa/ipa_v3
LINUXINCLUDE += -I$(DATAIPADRVTOP)/ipa/ipa_v3/ipahal
LINUXINCLUDE += -I$(DATAIPADRVTOP)/ipa/ipa_clients
endif

ifneq (,$(filter $(CONFIG_IPA3_REGDUMP),y m))
LINUXINCLUDE += -I$(DATAIPADRVTOP)/ipa/ipa_v3/dump
endif

ifneq (,$(filter $(CONFIG_IPA3_REGDUMP_IPA_4_5),y m))
LINUXINCLUDE += -I$(DATAIPADRVTOP)/ipa/ipa_v3/dump/ipa4.5
endif


obj-$(CONFIG_GSI) += gsi/
obj-$(CONFIG_IPA3) += ipa/
