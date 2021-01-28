# File: drivers/crypto/bcm/Makefile
#
# Makefile for crypto acceleration files for Broadcom SPU driver
#
# Uncomment to enable debug tracing in the SPU driver.
# CFLAGS_util.o := -DDEBUG
# CFLAGS_cipher.o := -DDEBUG
# CFLAGS_spu.o := -DDEBUG
# CFLAGS_spu2.o := -DDEBUG
ifeq ($(cc-name),clang)
ccflags-y += -Wno-error=enum-conversion
endif

obj-$(CONFIG_CRYPTO_DEV_BCM_SPU) := bcm_crypto_spu.o

ifneq ($(cc-name),clang)
CFLAGS_spu2.o = -Wno-maybe-uninitialized
endif
bcm_crypto_spu-objs :=  util.o spu.o spu2.o cipher.o

ccflags-y += -I. -DBCMDRIVER
