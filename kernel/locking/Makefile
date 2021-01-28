# SPDX-License-Identifier: GPL-2.0
# Any varying coverage in these files is non-deterministic
# and is generally not a function of system call inputs.
KCOV_INSTRUMENT		:= n
UBSAN_SANITIZE_lockdep.o	:= n
ccflags-y += -I$(srctree)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
obj-y += mutex.o semaphore.o rwsem.o percpu-rwsem.o

ifdef CONFIG_FUNCTION_TRACER
CFLAGS_REMOVE_lockdep.o = $(CC_FLAGS_FTRACE)
CFLAGS_REMOVE_lockdep_proc.o = $(CC_FLAGS_FTRACE)
CFLAGS_REMOVE_mutex-debug.o = $(CC_FLAGS_FTRACE)
CFLAGS_REMOVE_rtmutex-debug.o = $(CC_FLAGS_FTRACE)
endif

ifeq ($(CONFIG_PROVE_LOCKING),y)
#ccflags-y += -DMTK_LOCK_DEBUG_NEW_DEPENDENCY
endif

ifeq ($(CONFIG_LOCKDEP),y)
ifneq (y, $(filter y, $(CONFIG_KASAN) $(CONFIG_UBSAN)))
#ccflags-y += -DMTK_LOCK_MONITOR
ccflags-y += -DMTK_LOCK_DEBUG_HELD_LOCK
endif
endif

ifeq ($(CONFIG_DEBUG_SPINLOCK),y)
ifeq ($(CONFIG_MACH_MTxxxx),y)
ccflags-y += -DMTK_DEBUG_SPINLOCK_V2
else
ccflags-y += -DMTK_DEBUG_SPINLOCK_V1
endif
endif

obj-$(CONFIG_DEBUG_MUTEXES) += mutex-debug.o
obj-$(CONFIG_LOCKDEP) += lockdep.o
ifeq ($(CONFIG_PROC_FS),y)
obj-$(CONFIG_LOCKDEP) += lockdep_proc.o lockdep_test.o
endif
obj-$(CONFIG_SMP) += spinlock.o
obj-$(CONFIG_LOCK_SPIN_ON_OWNER) += osq_lock.o
obj-$(CONFIG_PROVE_LOCKING) += spinlock.o
obj-$(CONFIG_QUEUED_SPINLOCKS) += qspinlock.o
obj-$(CONFIG_RT_MUTEXES) += rtmutex.o
obj-$(CONFIG_DEBUG_RT_MUTEXES) += rtmutex-debug.o
obj-$(CONFIG_DEBUG_SPINLOCK) += spinlock.o
obj-$(CONFIG_DEBUG_SPINLOCK) += spinlock_debug.o
obj-$(CONFIG_RWSEM_GENERIC_SPINLOCK) += rwsem-spinlock.o
obj-$(CONFIG_RWSEM_XCHGADD_ALGORITHM) += rwsem-xadd.o
obj-$(CONFIG_QUEUED_RWLOCKS) += qrwlock.o
obj-$(CONFIG_LOCK_TORTURE_TEST) += locktorture.o
obj-$(CONFIG_WW_MUTEX_SELFTEST) += test-ww_mutex.o
