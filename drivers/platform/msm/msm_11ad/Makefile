obj-$(CONFIG_MSM_11AD) += msm_11ad_proxy.o

msm_11ad_proxy-y := msm_11ad.o
subdir-ccflags-y += -D__CHECK_ENDIAN__

# need to locate wil_platform.h
WIL_11AD_PATH = drivers/net/wireless/ath/wil6210
subdir-ccflags-y += -I$(WIL_11AD_PATH)

