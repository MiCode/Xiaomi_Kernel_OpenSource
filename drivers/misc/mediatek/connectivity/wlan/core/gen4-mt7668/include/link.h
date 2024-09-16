/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/link.h#1
*/

/*! \file   link.h
*    \brief  Definition for simple doubly linked list operations.
*
*    In this file we define the simple doubly linked list data structure and its
*    operation MACROs and INLINE functions.
*/

#ifndef _LINK_H
#define _LINK_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "gl_typedef.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/* May cause page fault & unalignment issue (data abort) */
#define INVALID_LINK_POISON1    ((VOID *) 0x00100101)

/* Used to verify that nonbody uses non-initialized link entries. */
#define INVALID_LINK_POISON2    ((VOID *) 0x00100201)

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
/* Simple Doubly Linked List Structures - Entry Part */
typedef struct _LINK_ENTRY_T {
	struct _LINK_ENTRY_T *prNext, *prPrev;
} LINK_ENTRY_T, *P_LINK_ENTRY_T;

/* Simple Doubly Linked List Structures - List Part */
typedef struct _LINK_T {
	P_LINK_ENTRY_T prNext;
	P_LINK_ENTRY_T prPrev;
	UINT_32 u4NumElem;
} LINK_T, *P_LINK_T;

struct LINK_MGMT {
	LINK_T rUsingLink;
	LINK_T rFreeLink;
};

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#if 0				/* No one use it, temporarily mark it for [Lint - Info 773] */
#define LINK_ADDR(rLink)        { (P_LINK_ENTRY_T)(&(rLink)), (P_LINK_ENTRY_T)(&(rLink)), 0 }

#define LINK_DECLARATION(rLink) \
	struct _LINK_T rLink = LINK_ADDR(rLink)
#endif

#define LINK_INITIALIZE(prLink) \
	do { \
		((P_LINK_T)(prLink))->prNext = (P_LINK_ENTRY_T)(prLink); \
		((P_LINK_T)(prLink))->prPrev = (P_LINK_ENTRY_T)(prLink); \
		((P_LINK_T)(prLink))->u4NumElem = 0; \
	} while (0)

#define LINK_MGMT_INIT(prLinkMgmt) \
	do { \
		LINK_INITIALIZE( \
			&((struct LINK_MGMT *)prLinkMgmt)->rUsingLink); \
		LINK_INITIALIZE( \
			&((struct LINK_MGMT *)prLinkMgmt)->rFreeLink); \
	} while (0)

#define LINK_MGMT_GET_ENTRY(prLinkMgmt, prEntry, EntryType, memType) \
	do { \
		LINK_REMOVE_HEAD(&((struct LINK_MGMT *)prLinkMgmt)->rFreeLink, \
			prEntry, EntryType*); \
		if (!prEntry) \
		prEntry = kalMemAlloc(sizeof(EntryType), memType); \
	} while (0)

#define LINK_MGMT_UNINIT(prLinkMgmt, EntryType, memType) \
	do { \
		EntryType *prEntry = NULL; \
		P_LINK_T prFreeList = \
			&((struct LINK_MGMT *)prLinkMgmt)->rFreeLink; \
		P_LINK_T prUsingList = \
			&((struct LINK_MGMT *)prLinkMgmt)->rUsingLink; \
		LINK_REMOVE_HEAD(prFreeList, prEntry, EntryType *); \
		while (prEntry) { \
			kalMemFree(prEntry, memType, sizeof(EntryType)); \
			LINK_REMOVE_HEAD(prFreeList, prEntry, EntryType *); \
		} \
		LINK_REMOVE_HEAD(prUsingList, prEntry, EntryType *); \
		while (prEntry) { \
			kalMemFree(prEntry, memType, sizeof(EntryType)); \
			LINK_REMOVE_HEAD(prUsingList, prEntry, EntryType *); \
		} \
	} while (0)


#define LINK_ENTRY_INITIALIZE(prEntry) \
	do { \
		((P_LINK_ENTRY_T)(prEntry))->prNext = (P_LINK_ENTRY_T)NULL; \
		((P_LINK_ENTRY_T)(prEntry))->prPrev = (P_LINK_ENTRY_T)NULL; \
	} while (0)

#define LINK_ENTRY_INVALID(prEntry) \
	do { \
		((P_LINK_ENTRY_T)(prEntry))->prNext = (P_LINK_ENTRY_T)INVALID_LINK_POISON1; \
		((P_LINK_ENTRY_T)(prEntry))->prPrev = (P_LINK_ENTRY_T)INVALID_LINK_POISON2; \
	} while (0)

#define LINK_IS_EMPTY(prLink)           (((P_LINK_T)(prLink))->prNext == (P_LINK_ENTRY_T)(prLink))

/* NOTE: We should do memory zero before any LINK been initiated, so we can check
 * if it is valid before parsing the LINK.
 */
#define LINK_IS_INVALID(prLink)         (((P_LINK_T)(prLink))->prNext == (P_LINK_ENTRY_T)NULL)

#define LINK_IS_VALID(prLink)           (((P_LINK_T)(prLink))->prNext != (P_LINK_ENTRY_T)NULL)

#define LINK_ENTRY(ptr, type, member)   ENTRY_OF(ptr, type, member)

/* Insert an entry into a link list's head */
#define LINK_INSERT_HEAD(prLink, prEntry) \
	{ \
	    linkAdd(prEntry, prLink); \
	    ((prLink)->u4NumElem)++; \
	}

/* Append an entry into a link list's tail */
#define LINK_INSERT_TAIL(prLink, prEntry) \
	{ \
	    linkAddTail(prEntry, prLink); \
	    ((prLink)->u4NumElem)++; \
	}

/* Peek head entry, but keep still in link list */
#define LINK_PEEK_HEAD(prLink, _type, _member) \
	( \
	    LINK_IS_EMPTY(prLink) ? \
	    NULL : LINK_ENTRY((prLink)->prNext, _type, _member) \
	)

/* Peek tail entry, but keep still in link list */
#define LINK_PEEK_TAIL(prLink, _type, _member) \
	( \
	    LINK_IS_EMPTY(prLink) ? \
	    NULL : LINK_ENTRY((prLink)->prPrev, _type, _member) \
	)

/* Get first entry from a link list */
/* NOTE: We assume the link entry located at the beginning of "prEntry Type",
 * so that we can cast the link entry to other data type without doubts.
 * And this macro also decrease the total entry count at the same time.
 */
#define LINK_REMOVE_HEAD(prLink, prEntry, _P_TYPE) \
	{ \
	    ASSERT(prLink); \
	    if (LINK_IS_EMPTY(prLink)) { \
		prEntry = (_P_TYPE)NULL; \
	    } \
	    else { \
		prEntry = (_P_TYPE)(((P_LINK_T)(prLink))->prNext); \
		linkDel((P_LINK_ENTRY_T)prEntry); \
		((prLink)->u4NumElem)--; \
	    } \
	}

/* Assume the link entry located at the beginning of prEntry Type.
 * And also decrease the total entry count.
 */
#define LINK_REMOVE_KNOWN_ENTRY(prLink, prEntry) \
	{ \
	    ASSERT(prLink); \
	    ASSERT(prEntry); \
	    linkDel((P_LINK_ENTRY_T)prEntry); \
	    ((prLink)->u4NumElem)--; \
	}

/* Iterate over a link list */
#define LINK_FOR_EACH(prEntry, prLink) \
	for (prEntry = (prLink)->prNext; \
	 prEntry != (P_LINK_ENTRY_T)(prLink); \
	 prEntry = (P_LINK_ENTRY_T)prEntry->prNext)

/* Iterate over a link list backwards */
#define LINK_FOR_EACH_PREV(prEntry, prLink) \
	for (prEntry = (prLink)->prPrev; \
	 prEntry != (P_LINK_ENTRY_T)(prLink); \
	 prEntry = (P_LINK_ENTRY_T)prEntry->prPrev)

/* Iterate over a link list safe against removal of link entry */
#define LINK_FOR_EACH_SAFE(prEntry, prNextEntry, prLink) \
	for (prEntry = (prLink)->prNext, prNextEntry = prEntry->prNext; \
	 prEntry != (P_LINK_ENTRY_T)(prLink); \
	 prEntry = prNextEntry, prNextEntry = prEntry->prNext)

/* Iterate over a link list of given type */
#define LINK_FOR_EACH_ENTRY(prObj, prLink, rMember, _TYPE) \
	for (prObj = LINK_ENTRY((prLink)->prNext, _TYPE, rMember); \
	 &prObj->rMember != (P_LINK_ENTRY_T)(prLink); \
	 prObj = LINK_ENTRY(prObj->rMember.prNext, _TYPE, rMember))

/* Iterate backwards over a link list of given type */
#define LINK_FOR_EACH_ENTRY_PREV(prObj, prLink, rMember, _TYPE) \
	for (prObj = LINK_ENTRY((prLink)->prPrev, _TYPE, rMember);  \
	 &prObj->rMember != (P_LINK_ENTRY_T)(prLink); \
	 prObj = LINK_ENTRY(prObj->rMember.prPrev, _TYPE, rMember))

/* Iterate over a link list of given type safe against removal of link entry */
#define LINK_FOR_EACH_ENTRY_SAFE(prObj, prNextObj, prLink, rMember, _TYPE) \
	for (prObj = LINK_ENTRY((prLink)->prNext, _TYPE, rMember),  \
	 prNextObj = LINK_ENTRY(prObj->rMember.prNext, _TYPE, rMember); \
	 &prObj->rMember != (P_LINK_ENTRY_T)(prLink); \
	 prObj = prNextObj, \
	 prNextObj = LINK_ENTRY(prNextObj->rMember.prNext, _TYPE, rMember))

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
/*----------------------------------------------------------------------------*/
/*!
* \brief This function is only for internal link list manipulation.
*
* \param[in] prNew  Pointer of new link head
* \param[in] prPrev Pointer of previous link head
* \param[in] prNext Pointer of next link head
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
static __KAL_INLINE__ VOID __linkAdd(IN P_LINK_ENTRY_T prNew, IN P_LINK_ENTRY_T prPrev, IN P_LINK_ENTRY_T prNext)
{
	prNext->prPrev = prNew;
	prNew->prNext = prNext;
	prNew->prPrev = prPrev;
	prPrev->prNext = prNew;
}				/* end of __linkAdd() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will add a new entry after the specified link head.
*
* \param[in] prNew  New entry to be added
* \param[in] prHead Specified link head to add it after
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
static __KAL_INLINE__ VOID linkAdd(IN P_LINK_ENTRY_T prNew, IN P_LINK_T prLink)
{
	__linkAdd(prNew, (P_LINK_ENTRY_T) prLink, prLink->prNext);
}				/* end of linkAdd() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will add a new entry before the specified link head.
*
* \param[in] prNew  New entry to be added
* \param[in] prHead Specified link head to add it before
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
static __KAL_INLINE__ VOID linkAddTail(IN P_LINK_ENTRY_T prNew, IN P_LINK_T prLink)
{
	__linkAdd(prNew, prLink->prPrev, (P_LINK_ENTRY_T) prLink);
}				/* end of linkAddTail() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is only for internal link list manipulation.
*
* \param[in] prPrev Pointer of previous link head
* \param[in] prNext Pointer of next link head
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
static __KAL_INLINE__ VOID __linkDel(IN P_LINK_ENTRY_T prPrev, IN P_LINK_ENTRY_T prNext)
{
	prNext->prPrev = prPrev;
	prPrev->prNext = prNext;
}				/* end of __linkDel() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will delete a specified entry from link list.
*        NOTE: the entry is in an initial state.
*
* \param prEntry    Specified link head(entry)
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
static __KAL_INLINE__ VOID linkDel(IN P_LINK_ENTRY_T prEntry)
{
	__linkDel(prEntry->prPrev, prEntry->prNext);

	LINK_ENTRY_INITIALIZE(prEntry);
}				/* end of linkDel() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will delete a specified entry from link list and then add it
*        after the specified link head.
*
* \param[in] prEntry        Specified link head(entry)
* \param[in] prOtherHead    Another link head to add it after
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
static __KAL_INLINE__ VOID linkMove(IN P_LINK_ENTRY_T prEntry, IN P_LINK_T prLink)
{
	__linkDel(prEntry->prPrev, prEntry->prNext);
	linkAdd(prEntry, prLink);
}				/* end of linkMove() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will delete a specified entry from link list and then add it
*        before the specified link head.
*
* \param[in] prEntry        Specified link head(entry)
* \param[in] prOtherHead    Another link head to add it before
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
static __KAL_INLINE__ VOID linkMoveTail(IN P_LINK_ENTRY_T prEntry, IN P_LINK_T prLink)
{
	__linkDel(prEntry->prPrev, prEntry->prNext);
	linkAddTail(prEntry, prLink);
}				/* end of linkMoveTail() */

#endif /* _LINK_H */
