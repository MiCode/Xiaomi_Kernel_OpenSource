/*! \file
    \brief  Declaration of library functions

    Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
*/


#ifndef _OSAL_TYPEDEF_H_
#define _OSAL_TYPEDEF_H_

#ifndef _TYPEDEFS_H	/*fix redifine*/
typedef char		INT8, *PINT8, **PPINT8;
#endif

typedef void VOID, *PVOID, **PPVOID;

typedef char		*PINT8, **PPINT8;
typedef short		INT16, *PINT16, **PPINT16;
typedef int			INT32, *PINT32, **PPINT32;
typedef long long	INT64, *PINT64, **PPINT64;

typedef unsigned char		UINT8, *PUINT8, **PPUINT8;
typedef unsigned short		UINT16, *PUINT16, **PPUINT16;
typedef unsigned int		UINT32, *PUINT32, **PPUINT32;
typedef unsigned long long	UINT64, *PUINT64, **PPUINT64;

typedef int MTK_WCN_BOOL;
#ifndef MTK_WCN_BOOL_TRUE
#define MTK_WCN_BOOL_FALSE               ((MTK_WCN_BOOL) 0)
#define MTK_WCN_BOOL_TRUE                ((MTK_WCN_BOOL) 1)
#endif

#endif /*_OSAL_TYPEDEF_H_*/
