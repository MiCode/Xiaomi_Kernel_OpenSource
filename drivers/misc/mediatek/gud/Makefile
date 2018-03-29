ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
	ifeq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6582" "mt6592"))
		# armv8
		ifeq ($(CONFIG_TRUSTONIC_TEE_VERSION), "")
			obj-$(CONFIG_TRUSTONIC_TEE_SUPPORT) += 302a/
		else
			obj-$(CONFIG_TRUSTONIC_TEE_SUPPORT) += $(subst ",,$(CONFIG_TRUSTONIC_TEE_VERSION))/
		endif
	else
		# armv7
		obj-$(CONFIG_TRUSTONIC_TEE_SUPPORT) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
	endif
endif
