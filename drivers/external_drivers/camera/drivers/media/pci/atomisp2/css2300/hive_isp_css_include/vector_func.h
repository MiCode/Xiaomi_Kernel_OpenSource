#ifndef __VECTOR_FUNC_H_INCLUDED__
#define __VECTOR_FUNC_H_INCLUDED__

#include "storage_class.h"

#include "vector_func_local.h"

#ifndef __INLINE_VECTOR_FUNC__
#define STORAGE_CLASS_VECTOR_FUNC_H STORAGE_CLASS_EXTERN
#define STORAGE_CLASS_VECTOR_FUNC_C 
#include "vector_func_public.h"
#else  /* __INLINE_VECTOR_FUNC__ */
#define STORAGE_CLASS_VECTOR_FUNC_H STORAGE_CLASS_INLINE
#define STORAGE_CLASS_VECTOR_FUNC_C STORAGE_CLASS_INLINE
#include "vector_func_private.h"
#endif /* __INLINE_VECTOR_FUNC__ */

#endif /* __VECTOR_FUNC_H_INCLUDED__ */
