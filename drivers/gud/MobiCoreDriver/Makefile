#
# Makefile for the <t-base core driver
#

GUD_ROOT_FOLDER := drivers/gud/

# add our modules to kernel.
obj-$(CONFIG_MOBICORE_DRIVER) += mcDrvModule.o

mcDrvModule-y := \
	admin.o \
	api.o \
	client.o \
	clientlib.o \
	clock.o \
	fastcall.o \
	logging.o \
	main.o \
	mcp.o \
	mmu.o \
	pm.o \
	scheduler.o \
	session.o

# Release mode by default
ccflags-y += -DNDEBUG
ccflags-y += -Wno-declaration-after-statement

ccflags-$(CONFIG_MOBICORE_DEBUG) += -DDEBUG

# MobiCore Driver includes
ccflags-y += -I$(GUD_ROOT_FOLDER)/MobiCoreDriver

