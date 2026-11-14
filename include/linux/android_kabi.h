/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * android_kabi.h - Android kernel abi abstraction header
 *
 * Copyright (C) 2020-2025 Google, Inc.
 *
 * Heavily influenced by rh_kabi.h which came from the RHEL/CENTOS kernel and
 * was:
 *	Copyright (c) 2014 Don Zickus
 *	Copyright (c) 2015-2018 Jiri Benc
 *	Copyright (c) 2015 Sabrina Dubroca, Hannes Frederic Sowa
 *	Copyright (c) 2016-2018 Prarit Bhargava
 *	Copyright (c) 2017 Paolo Abeni, Larry Woodman
 *
 * These macros are to be used to try to help alleviate future kernel abi
 * changes that will occur as LTS and other kernel patches are merged into the
 * tree during a period in which the kernel abi is wishing to not be disturbed.
 *
 * There are two times these macros should be used:
 *  - Before the kernel abi is "frozen"
 *    Padding can be added to various kernel structures that have in the past
 *    been known to change over time.  That will give "room" in the structure
 *    that can then be used when fields are added so that the structure size
 *    will not change.
 *
 *  - After the kernel abi is "frozen"
 *    If a structure's field is changed to a type that is identical in size to
 *    the previous type, it can be changed with a union macro
 *    If a field is added to a structure, the padding fields can be used to add
 *    the new field in a "safe" way.
 */

#ifndef _ANDROID_KABI_H
#define _ANDROID_KABI_H

#include <linux/args.h>
#include <linux/compiler.h>
#include <linux/compiler_attributes.h>
#include <linux/stringify.h>

/*
 * Worker macros, don't use these, use the ones without a leading '_'
 */

#if defined(BUILD_VDSO) || defined(__DISABLE_EXPORTS)
#define __ANDROID_KABI_RULE(hint, target, value)
#else
#define __ANDROID_KABI_RULE(hint, target, value)			 \
	static const char CONCATENATE(__gendwarfksyms_rule_,		 \
				      __COUNTER__)[] __used __aligned(1) \
		__section(".discard.gendwarfksyms.kabi_rules") =	 \
			"1\0" #hint "\0" target "\0" value
#endif

#define _ANDROID_KABI_RULE(hint, target, value) \
	__ANDROID_KABI_RULE(hint, #target, #value)

#define _ANDROID_KABI_NORMAL_SIZE_ALIGN(_orig, _new)			\
	union {								\
		_Static_assert(						\
			sizeof(struct { _new; }) <=			\
				sizeof(struct { _orig; }),		\
			FILE_LINE ": " __stringify(_new)		\
				" is larger than " __stringify(_orig));	\
		_Static_assert(						\
			__alignof__(struct { _new; }) <=		\
				__alignof__(struct { _orig; }),		\
			FILE_LINE ": " __stringify(_orig)		\
				" is not aligned the same as "		\
				__stringify(_new));			\
	}

#if defined(BUILD_VDSO) || defined(__DISABLE_EXPORTS)
#define _ANDROID_KABI_REPLACE(_orig, _new)	_new
#else
#define _ANDROID_KABI_REPLACE(_orig, _new)		      \
	union {						      \
		_new;					      \
		struct {				      \
			_orig;				      \
		};					      \
		_ANDROID_KABI_NORMAL_SIZE_ALIGN(_orig, _new); \
	}
#endif


/*
 * Macros to use _before_ the ABI is frozen
 */

/*
 * ANDROID_KABI_RESERVE
 *   Reserve some "padding" in a structure for use by LTS backports.
 *   This normally placed at the end of a structure.
 *   number: the "number" of the padding variable in the structure.  Start with
 *   1 and go up.
 *
 *
 * ANDROID_BACKPORT_RESERVE
 *   Similar to ANDROID_KABI_RESERVE, but this is for planned feature backports
 *   (not for LTS).
 */
#define ANDROID_KABI_RESERVE(number)		u64 __kabi_reserved##number
#define ANDROID_BACKPORT_RESERVE(number)	u64 __kabi_reserved_backport##number

/*
 * Macros to use _after_ the ABI is frozen
 */

/*
 * ANDROID_KABI_DECLONLY(fqn)
 *   Treat the struct/union/enum fqn as a declaration, i.e. even if
 *   a definition is available, don't expand the contents.
 */
#define ANDROID_KABI_DECLONLY(fqn)	_ANDROID_KABI_RULE(declonly, fqn, /**/)

/*
 * ANDROID_KABI_ENUMERATOR_IGNORE(fqn, field)
 *   When expanding enum fqn, skip the provided field. This makes it
 *   possible to hide added enum fields from versioning.
 */
#define ANDROID_KABI_ENUMERATOR_IGNORE(fqn, field) \
	_ANDROID_KABI_RULE(enumerator_ignore, fqn field, /**/)

/*
 * ANDROID_KABI_ENUMERATOR_VALUE(fqn, field, value)
 *   When expanding enum fqn, use the provided value for the
 *   specified field. This makes it possible to override enumerator
 *   values when calculating versions.
 */
#define ANDROID_KABI_ENUMERATOR_VALUE(fqn, field, value) \
	_ANDROID_KABI_RULE(enumerator_value, fqn field, value)

/*
 * ANDROID_KABI_BYTE_SIZE(fqn, value)
 *   Set the byte_size attribute for the struct/union/enum fqn to
 *   value bytes.
 */
#define ANDROID_KABI_BYTE_SIZE(fqn, value) \
	_ANDROID_KABI_RULE(byte_size, fqn, value)

/*
 * ANDROID_KABI_TYPE_STRING(type, str)
 *   For the given type, override the type string used in symtypes
 *   output and version calculation with str.
 */
#define ANDROID_KABI_TYPE_STRING(type, str) \
	__ANDROID_KABI_RULE(type_string, type, str)

/*
 * ANDROID_KABI_IGNORE
 *   Add a new field that's ignored in versioning.
 */
#define ANDROID_KABI_IGNORE(n, _new)		 \
	union {					 \
		_new;				 \
		unsigned char __kabi_ignored##n; \
	}

/*
 * ANDROID_KABI_REPLACE
 *   Replace a field with a compatible new field.
 */
#define ANDROID_KABI_REPLACE(_oldtype, _oldname, _new) \
	_ANDROID_KABI_REPLACE(_oldtype __kabi_renamed##_oldname, struct { _new; })

/*
 * ANDROID_KABI_USE(number, _new)
 *   Use a previous padding entry that was defined with ANDROID_KABI_RESERVE
 *   number: the previous "number" of the padding variable
 *   _new: the variable to use now instead of the padding variable
 */
#define ANDROID_KABI_USE(number, _new) \
	_ANDROID_KABI_REPLACE(ANDROID_KABI_RESERVE(number), _new)

/*
 * ANDROID_KABI_USE2(number, _new1, _new2)
 *   Use a previous padding entry that was defined with ANDROID_KABI_RESERVE for
 *   two new variables that fit into 64 bits.  This is good for when you do not
 *   want to "burn" a 64bit padding variable for a smaller variable size if not
 *   needed.
 */
#define ANDROID_KABI_USE2(number, _new1, _new2) \
	_ANDROID_KABI_REPLACE(ANDROID_KABI_RESERVE(number), struct{ _new1; _new2; })

/*
 * ANDROID_BACKPORT_USE(number, _new)
 *   Use a previous padding entry that was defined with ANDROID_BACKPORT_RESERVE
 *   number: the previous "number" of the padding variable
 *   _new: the variable to use now instead of the padding variable
 */
#define ANDROID_BACKPORT_USE(number, _new) \
	_ANDROID_KABI_REPLACE(ANDROID_BACKPORT_RESERVE(number), _new)

#endif /* _ANDROID_KABI_H */
