#ifndef __STORAGE_CLASS_H_INCLUDED__
#define __STORAGE_CLASS_H_INCLUDED__

#define STORAGE_CLASS_EXTERN extern

#if defined(_MSC_VER)
#define STORAGE_CLASS_INLINE static __inline
#elif defined(__HIVECC)
#define STORAGE_CLASS_INLINE static inline
#else
#define STORAGE_CLASS_INLINE static inline
#endif

#endif /* __STORAGE_CLASS_H_INCLUDED__ */
