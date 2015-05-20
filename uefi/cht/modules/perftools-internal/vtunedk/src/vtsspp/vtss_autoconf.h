/**
 * Automatically generated file; DO NOT EDIT.
 * vtsspp.ko Configuration for 3.10.20-263219-g7d0da2c Kernel Configuration
 */
#ifndef _VTSS_AUTOCONF_H_
#define _VTSS_AUTOCONF_H_

#if !defined(__i386__) && !defined(__x86_64__)
#error "Only i386 or x86_64 architecture is supported"
#endif

#ifndef CONFIG_MODULES
#error "The kernel should be compiled with CONFIG_MODULES=y"
#endif /* CONFIG_MODULES */

#ifndef CONFIG_MODULE_UNLOAD
#error "The kernel should be compiled with CONFIG_MODULE_UNLOAD=y"
#endif /* CONFIG_MODULE_UNLOAD */

#ifndef CONFIG_SMP
#error "The kernel should be compiled with CONFIG_SMP=y"
#endif /* CONFIG_SMP */

#ifndef CONFIG_KPROBES
#error "The kernel should be compiled with CONFIG_KPROBES=y"
#endif /* CONFIG_KPROBES */

#define VTSS_AUTOCONF_CPUMASK_PARSELIST_USER 1
#define VTSS_AUTOCONF_DPATH_PATH 1
#define VTSS_AUTOCONF_DUMP_TRACE_HAVE_BP 1
#define VTSS_AUTOCONF_INIT_WORK_TWO_ARGS 1
#define VTSS_AUTOCONF_KMAP_ATOMIC_ONE_ARG 1
#define VTSS_AUTOCONF_KPROBE_FLAGS 1
#define VTSS_AUTOCONF_KPROBE_SYMBOL_NAME 1
#define VTSS_AUTOCONF_MODULE_MUTEX 1
#define VTSS_AUTOCONF_NAMEIDATA_CLEANUP 1
#define VTSS_AUTOCONF_RING_BUFFER_ALLOC_READ_PAGE 1
#define VTSS_AUTOCONF_RING_BUFFER_LOST_EVENTS 1
#define VTSS_AUTOCONF_STACKTRACE_OPS_WALK_STACK 1
#define VTSS_AUTOCONF_SYSTEM_UNBOUND_WQ 1
#define VTSS_AUTOCONF_TASK_REAL_PARENT 1
#define VTSS_AUTOCONF_TRACE_EVENTS_SCHED 1
#define VTSS_AUTOCONF_USER_COPY_WITHOUT_CHECK 1
#define VTSS_AUTOCONF_X86_UNIREGS 1

#include "vtss_version.h"

#ifndef VTSS_VERSION_MAJOR
#define VTSS_VERSION_MAJOR    1
#endif /* VTSS_VERSION_MAJOR */
#ifndef VTSS_VERSION_MINOR
#define VTSS_VERSION_MINOR    0
#endif /* VTSS_VERSION_MINOR */
#ifndef VTSS_VERSION_REVISION
#define VTSS_VERSION_REVISION 0
#endif /* VTSS_VERSION_REVISION */
#ifndef VTSS_VERSION_STRING
#define VTSS_VERSION_STRING   "v1.0.0-custom"
#endif /* VTSS_VERSION_STRING */

#endif /* _VTSS_AUTOCONF_H_ */
