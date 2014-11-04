#ifndef __INPUT_FORMATTER_H_INCLUDED__
#define __INPUT_FORMATTER_H_INCLUDED__

/*
 * This file is included on every cell {SP,ISP,host} and on every system
 * that uses the input system device(s). It defines the API to DLI bridge
 *
 * System and cell specific interfaces and inline code are included
 * conditionally through Makefile path settings.
 *
 *  - .        system and cell agnostic interfaces, constants and identifiers
 *	- public:  system agnostic, cell specific interfaces
 *	- private: system dependent, cell specific interfaces & inline implementations
 *	- global:  system specific constants and identifiers
 *	- local:   system and cell specific constants and identifiers
 */

#include "storage_class.h"

#include "system_local.h"
#include "input_formatter_local.h"

#ifndef __INLINE_INPUT_FORMATTER__
#define STORAGE_CLASS_INPUT_FORMATTER_H STORAGE_CLASS_EXTERN
#define STORAGE_CLASS_INPUT_FORMATTER_C 
#include "input_formatter_public.h"
#else  /* __INLINE_INPUT_FORMATTER__ */
#define STORAGE_CLASS_INPUT_FORMATTER_H STORAGE_CLASS_INLINE
#define STORAGE_CLASS_INPUT_FORMATTER_C STORAGE_CLASS_INLINE
#include "input_formatter_private.h"
#endif /* __INLINE_INPUT_FORMATTER__ */

#endif /* __INPUT_FORMATTER_H_INCLUDED__ */
