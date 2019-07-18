# We can build either as part of a standalone Kernel build or as
# an external module.  Determine which mechanism is being used
ifeq ($(MODNAME),)
	KERNEL_BUILD := 1
else
	KERNEL_BUILD := 0
endif



ifeq ($(KERNEL_BUILD), 1)
	# These are configurable via Kconfig for kernel-based builds
	# Need to explicitly configure for Android-based builds
	AUDIO_BLD_DIR := $(ANDROID_BUILD_TOP)/kernel/msm-4.9
	AUDIO_ROOT := $(AUDIO_BLD_DIR)/techpack/audio
endif

ifeq ($(KERNEL_BUILD), 0)
	ifeq ($(CONFIG_ARCH_SDM845), y)
		include $(AUDIO_ROOT)/config/sdm845auto.conf
		export
		INCS    +=  -include $(AUDIO_ROOT)/config/sdm845autoconf.h
	endif
	ifeq ($(CONFIG_ARCH_SDM670), y)
		include $(AUDIO_ROOT)/config/sdm710auto.conf
		export
		INCS    +=  -include $(AUDIO_ROOT)/config/sdm710autoconf.h
	endif
endif

# As per target team, build is done as follows:
# Defconfig : build with default flags
# Slub      : defconfig  + CONFIG_SLUB_DEBUG := y +
#	      CONFIG_SLUB_DEBUG_ON := y + CONFIG_PAGE_POISONING := y
# Perf      : Using appropriate msmXXXX-perf_defconfig
#
# Shipment builds (user variants) should not have any debug feature
# enabled. This is identified using 'TARGET_BUILD_VARIANT'. Slub builds
# are identified using the CONFIG_SLUB_DEBUG_ON configuration. Since
# there is no other way to identify defconfig builds, QTI internal
# representation of perf builds (identified using the string 'perf'),
# is used to identify if the build is a slub or defconfig one. This
# way no critical debug feature will be enabled for perf and shipment
# builds. Other OEMs are also protected using the TARGET_BUILD_VARIANT
# config.

############ UAPI ############
UAPI_DIR :=	uapi
UAPI_INC :=	-I$(AUDIO_ROOT)/include/$(UAPI_DIR)

############ COMMON ############
COMMON_DIR :=	include
COMMON_INC :=	-I$(AUDIO_ROOT)/$(COMMON_DIR)

############ TAS2562 ############

# for TAS2562 Codec
ifdef CONFIG_SND_SOC_TAS2562
	TAS2562_OBJS += tas2562-codec.o
	TAS2562_OBJS += tas2562-regmap.o
	TAS2562_OBJS += tas2562-misc.o
endif

LINUX_INC +=	-Iinclude/linux

INCS +=		$(COMMON_INC) \
		$(UAPI_INC)

EXTRA_CFLAGS += $(INCS)


CDEFINES +=	-DANI_LITTLE_BYTE_ENDIAN \
		-DANI_LITTLE_BIT_ENDIAN \
		-DDOT11F_LITTLE_ENDIAN_HOST \
		-DANI_COMPILER_TYPE_GCC \
		-DANI_OS_TYPE_ANDROID=6 \
		-DPTT_SOCK_SVC_ENABLE \
		-Wall\
		-Werror\
		-D__linux__

KBUILD_CPPFLAGS += $(CDEFINES)

# Module information used by KBuild framework
obj-$(CONFIG_SND_SOC_TAS2562) += tas2562_dlkm.o
tas2562_dlkm-y := $(TAS2562_OBJS)

# inject some build related information
DEFINES += -DBUILD_TIMESTAMP=\"$(shell date -u +'%Y-%m-%dT%H:%M:%SZ')\"
