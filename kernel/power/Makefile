
ccflags-$(CONFIG_PM_DEBUG)	:= -DDEBUG

include $(srctree)/drivers/misc/mediatek/Makefile.custom

tuxonice_core-y := tuxonice_modules.o

obj-$(CONFIG_TOI)		+= tuxonice_builtin.o

tuxonice_core-$(CONFIG_PM_DEBUG)	+= tuxonice_alloc.o

# Compile these in after allocation debugging, if used.

tuxonice_core-y += tuxonice_sysfs.o tuxonice_highlevel.o \
		tuxonice_io.o tuxonice_pagedir.o tuxonice_prepare_image.o \
		tuxonice_extent.o tuxonice_pageflags.o tuxonice_ui.o \
		tuxonice_power_off.o tuxonice_atomic_copy.o

tuxonice_core-$(CONFIG_TOI_CHECKSUM)	+= tuxonice_checksum.o

tuxonice_core-$(CONFIG_NET)	+= tuxonice_storage.o tuxonice_netlink.o

obj-$(CONFIG_TOI_CORE)		+= tuxonice_core.o
obj-$(CONFIG_TOI_PRUNE)		+= tuxonice_prune.o
obj-$(CONFIG_TOI_INCREMENTAL)	+= tuxonice_incremental.o
obj-$(CONFIG_TOI_CRYPTO)	+= tuxonice_compress.o

tuxonice_bio-y := tuxonice_bio_core.o tuxonice_bio_chains.o \
		tuxonice_bio_signature.o

obj-$(CONFIG_TOI_SWAP)		+= tuxonice_bio.o tuxonice_swap.o
obj-$(CONFIG_TOI_FILE)		+= tuxonice_bio.o tuxonice_file.o
obj-$(CONFIG_TOI_CLUSTER)	+= tuxonice_cluster.o

obj-$(CONFIG_TOI_USERUI)	+= tuxonice_userui.o

obj-y				+= qos.o
obj-$(CONFIG_PM)		+= main.o
obj-$(CONFIG_VT_CONSOLE_SLEEP)	+= console.o
obj-$(CONFIG_FREEZER)		+= process.o
obj-$(CONFIG_SUSPEND)		+= suspend.o
obj-$(CONFIG_PM_TEST_SUSPEND)	+= suspend_test.o
obj-$(CONFIG_HIBERNATION)	+= hibernate.o snapshot.o swap.o user.o \
				   block_io.o
obj-$(CONFIG_EARLYSUSPEND)	+= earlysuspend.o
obj-$(CONFIG_PM_AUTOSLEEP)	+= autosleep.o
obj-$(CONFIG_FB_EARLYSUSPEND)	+= fbearlysuspend.o
obj-$(CONFIG_PM_WAKELOCKS)	+= wakelock.o
obj-$(CONFIG_SUSPEND_TIME)	+= suspend_time.o

obj-$(CONFIG_MAGIC_SYSRQ)	+= poweroff.o

obj-$(CONFIG_SUSPEND)	+= wakeup_reason.o

obj-$(CONFIG_HAS_SBSUSPEND)	+= sbsuspend.o