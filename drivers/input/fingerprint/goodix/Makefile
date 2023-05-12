# obj-$(CONFIG_FINGERPRINT_GOODIX_FOD) += gf_spi.o platform.o netlink.o
ifeq ($(CONFIG_BUILD_QGKI), y)
	export CONFIG_FINGERPRINT_GOODIX_FOD = y
else
	export CONFIG_FINGERPRINT_GOODIX_FOD = m
endif

obj-$(CONFIG_FINGERPRINT_GOODIX_FOD) += goodix_fp.o
goodix_fp-y := gf_spi.o platform.o netlink.o