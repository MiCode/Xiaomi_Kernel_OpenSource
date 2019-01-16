#
# Arch-specific CryptoAPI modules.
#

obj-$(CONFIG_CRYPTO_AES_ARM) += aes-arm.o
obj-$(CONFIG_CRYPTO_SHA1_ARM) += sha1-arm.o


ifdef CONFIG_CRYPTO_AES_ARM32_CE
KBUILD_AFLAGS      :=$(KBUILD_AFLAGS:-msoft-float=-Wa,-mfpu=crypto-neon-fp-armv8)
aes-arm-y  := aes-armv8-aarch32.o aes_glue.o aes-armv8-bcm.o aes-armv8-bcm-glue.o
else
aes-arm-y  := aes-armv4.o aes_glue.o
endif

sha1-arm-y := sha1-armv4-large.o sha1_glue.o
