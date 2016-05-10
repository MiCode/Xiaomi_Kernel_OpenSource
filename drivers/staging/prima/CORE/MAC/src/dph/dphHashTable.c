/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This file dphHashTable.cc implements the member functions of
 * DPH hash table class.
 *
 * Author:      Sandesh Goel
 * Date:        02/25/02
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#include "palTypes.h"
#include "cfgApi.h"
#include "schApi.h"
#include "dphGlobal.h"
#include "limDebug.h"


#include "halMsgApi.h" 

// ---------------------------------------------------------------------
/**
 * dphHashTableClass()
 *
 * FUNCTION:
 * Constructor function
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param None
 * @return None
 */

void dphHashTableClassInit(tpAniSirGlobal pMac, dphHashTableClass* pDphHashTable)
{
  tANI_U16 i;

  for (i=0; i<pDphHashTable->size; i++)
    {
      pDphHashTable->pHashTable[i] = 0;
    }

  for (i=0; i<pDphHashTable->size; i++)
    {
      pDphHashTable->pDphNodeArray[i].valid = 0;
      pDphHashTable->pDphNodeArray[i].added = 0;
      pDphHashTable->pDphNodeArray[i].assocId = i;
    }
    
}

// ---------------------------------------------------------------------
/**
 * hashFunction
 *
 * FUNCTION:
 * Hashing function
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param staAddr MAC address of the station
 * @return None
 */

tANI_U16 hashFunction(tpAniSirGlobal pMac, tANI_U8 staAddr[], tANI_U16 numSta)
{
  int i;
  tANI_U16 sum = 0;
  
  for (i=0; i<6; i++)
    sum += staAddr[i];
   
  return (sum % numSta);
}

// ---------------------------------------------------------------------
/**
 * dphLookupHashEntry
 *
 * FUNCTION:
 * Look up an entry in hash table
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param staAddr MAC address of the station
 * @param pStaId pointer to the Station ID assigned to the station
 * @return pointer to STA hash entry if lookup was a success \n
 *         NULL if lookup was a failure
 */

tpDphHashNode dphLookupHashEntry(tpAniSirGlobal pMac, tANI_U8 staAddr[], tANI_U16 *pAssocId, 
                                 dphHashTableClass* pDphHashTable)
{
    tpDphHashNode ptr = NULL;
    tANI_U16 index = hashFunction(pMac, staAddr, pDphHashTable->size);

    for (ptr = pDphHashTable->pHashTable[index]; ptr; ptr = ptr->next)
        {
            if (dphCompareMacAddr(staAddr, ptr->staAddr))
                {
                    *pAssocId = ptr->assocId;
                    break;
                }
        }
    return ptr;
}

// ---------------------------------------------------------------------
/**
 * dphGetHashEntry
 *
 * FUNCTION:
 * Get a pointer to the hash node
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param staId Station ID
 * @return pointer to STA hash entry if lookup was a success \n
 *         NULL if lookup was a failure
 */

tpDphHashNode dphGetHashEntry(tpAniSirGlobal pMac, tANI_U16 peerIdx, dphHashTableClass* pDphHashTable)
{
    if (peerIdx < pDphHashTable->size)
    {
        if (pDphHashTable->pDphNodeArray[peerIdx].added)
          return &pDphHashTable->pDphNodeArray[peerIdx];
        else
            return NULL;
    }
    else
        return NULL;

}

static inline tpDphHashNode getNode(tpAniSirGlobal pMac, tANI_U8 assocId, dphHashTableClass* pDphHashTable)
{
    return &pDphHashTable->pDphNodeArray[assocId];
}




// ---------------------------------------------------------------------
/**
 * dphLookupAssocId
 *
 * FUNCTION:
 * This function looks up assocID given the station Id. It traverses the complete table to do this.
 * Need to find an efficient way to do this.
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param pMac pointer to global Mac structure.
 * @param staIdx station ID
 * @param *assocId pointer to associd to be returned by this function.
 * @return pointer to the dph node.
 */
tpDphHashNode dphLookupAssocId(tpAniSirGlobal pMac,  tANI_U16 staIdx, tANI_U16* assocId, dphHashTableClass* pDphHashTable)
{
    tANI_U8 i;

    for(i=0; i<pDphHashTable->size; i++)
        {
            if( (pDphHashTable->pDphNodeArray[i].added) &&
                (pDphHashTable->pDphNodeArray[i].staIndex == staIdx))
                {
                    *assocId = i;
                    break;
                }

        }
    if(i==pDphHashTable->size)
        return NULL;
    return &pDphHashTable->pDphNodeArray[i];

}




/** -------------------------------------------------------------
\fn dphInitStaState
\brief Initialize STA state. this function saves the staId from the current entry in the DPH table with given assocId
\ if validStaIdx flag is set. Otherwise it sets the staId to invalid.
\param  tpAniSirGlobal    pMac
\param  tSirMacAddr staAddr
\param  tANI_U16 assocId
\param  tANI_U8 validStaIdx -   true ==> the staId in the DPH entry with given assocId is valid and restore it back.
\                                              false ==> set the staId to invalid.
\return tpDphHashNode - DPH hash node if found.
  -------------------------------------------------------------*/

tpDphHashNode dphInitStaState(tpAniSirGlobal pMac, tSirMacAddr staAddr,
      tANI_U16 assocId, tANI_U8 validStaIdx, dphHashTableClass* pDphHashTable)
{
    tANI_U32 val;

    tpDphHashNode pStaDs;
    tANI_U16 staIdx = HAL_STA_INVALID_IDX;

    if (assocId >= pDphHashTable->size)
    {
        PELOGE(limLog(pMac, LOGE, FL("Invalid Assoc Id %d"), assocId);)
        return NULL;
    }

    pStaDs = getNode(pMac, (tANI_U8) assocId, pDphHashTable);
    staIdx = pStaDs->staIndex;

    PELOG1(limLog(pMac, LOG1, FL("Assoc Id %d, Addr %08X"), assocId, pStaDs);)

    // Clear the STA node except for the next pointer (last 4 bytes)
    vos_mem_set( (tANI_U8 *) pStaDs, sizeof(tDphHashNode) - sizeof(tpDphHashNode), 0);

    // Initialize the assocId
    pStaDs->assocId = assocId;
    if(true == validStaIdx)
      pStaDs->staIndex = staIdx;
    else
      pStaDs->staIndex = HAL_STA_INVALID_IDX;

    // Initialize STA mac address
    vos_mem_copy( pStaDs->staAddr, staAddr, sizeof(tSirMacAddr));

    // Initialize fragmentation threshold
    if (wlan_cfgGetInt(pMac, WNI_CFG_FRAGMENTATION_THRESHOLD, &val) != eSIR_SUCCESS)
        limLog(pMac, LOGP, FL("could not retrieve fragmentation threshold"));
    else
        pStaDs->fragSize = (tANI_U16) val;

    pStaDs->added = 1;
    pStaDs->encPolicy = HAL_ENC_POLICY_NULL;

#ifdef WMM_APSD
    pStaDs->stopQueue = 0;
    pStaDs->spStatus = 0;
    pStaDs->apsdMaxSpLen = 0;
    pStaDs->acMode[0] = pStaDs->acMode[1] = pStaDs->acMode[2] = pStaDs->acMode[3] =  0;
#endif /* WMM_APSD */
    pStaDs->valid = 1;
    return pStaDs;
}

// ---------------------------------------------------------------------
/**
 * dphAddHashEntry
 *
 * FUNCTION:
 * Add entry to hash table
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param staAddr MAC address of the station
 * @param staId Station ID assigned to the station
 * @return Pointer to STA hash entry
 */

tpDphHashNode dphAddHashEntry(tpAniSirGlobal pMac, tSirMacAddr staAddr, tANI_U16 assocId, dphHashTableClass* pDphHashTable)
{
    tpDphHashNode ptr, node;
    tANI_U16 index = hashFunction(pMac, staAddr, pDphHashTable->size);

    PELOG1(limLog(pMac, LOG1, FL("assocId %d index %d STA addr"),
           assocId, index);
    dphPrintMacAddr(pMac, staAddr, LOG1);)

    if (assocId >= pDphHashTable->size)
    {
        PELOGE(limLog(pMac, LOGE, FL("invalid STA id %d"), assocId);)
        return NULL;
    }

    if (pDphHashTable->pDphNodeArray[assocId].added)
    {
        PELOGE(limLog(pMac, LOGE, FL("already added STA %d"), assocId);)
        return NULL;
    }

    for (ptr = pDphHashTable->pHashTable[index]; ptr; ptr = ptr->next)
    {
        if (ptr == ptr->next)
        {
            PELOGE(limLog(pMac, LOGE, FL("Infinite Loop"));)
            return NULL;
        }

        if (dphCompareMacAddr(staAddr, ptr->staAddr) || ptr->assocId== assocId)
            break;
    }

    if (ptr)
    {
        // Duplicate entry
        limLog(pMac, LOGE, FL("assocId %d hashIndex %d entry exists"),
                     assocId, index);
        return NULL;
    }
    else
    {
        if (dphInitStaState(pMac, staAddr, assocId, false, pDphHashTable) == NULL)
        {
            PELOGE(limLog(pMac, LOGE, FL("could not Init STAid=%d"), assocId);)
                    return NULL;
        }

        // Add the node to the link list
        pDphHashTable->pDphNodeArray[assocId].next = pDphHashTable->pHashTable[index];
        pDphHashTable->pHashTable[index] = &pDphHashTable->pDphNodeArray[assocId];

        node = pDphHashTable->pHashTable[index];
        return node;
    }
}

// ---------------------------------------------------------------------
/**
 * dphDeleteHashEntry
 *
 * FUNCTION:
 * Delete entry from hash table
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param staAddr MAC address of the station
 * @param staId Station ID assigned to the station
 * @return eSIR_SUCCESS if successful,\n
 *         eSIR_FAILURE otherwise
 */

tSirRetStatus dphDeleteHashEntry(tpAniSirGlobal pMac, tSirMacAddr staAddr, tANI_U16 assocId, dphHashTableClass* pDphHashTable)
{
  tpDphHashNode ptr, prev;
  tANI_U16 index = hashFunction(pMac, staAddr, pDphHashTable->size);


  PELOG1(limLog(pMac, LOG1, FL("assocId %d index %d STA addr"),
                  assocId, index);
  dphPrintMacAddr(pMac, staAddr, LOG1);)

  if (assocId >= pDphHashTable->size)
  {
      PELOGE(limLog(pMac, LOGE, FL("invalid STA id %d"), assocId);)
      return eSIR_FAILURE;
  }

  if (pDphHashTable->pDphNodeArray[assocId].added == 0)
  {
      PELOGE(limLog(pMac, LOGE, FL("STA %d never added"), assocId);)
      return eSIR_FAILURE;
  }


  for (prev = 0, ptr = pDphHashTable->pHashTable[index];
       ptr;
       prev = ptr, ptr = ptr->next)
  {
    if (dphCompareMacAddr(staAddr, ptr->staAddr))
      break;
    if (prev == ptr)
    {
        PELOGE(limLog(pMac, LOGE, FL("Infinite Loop"));)
        return eSIR_FAILURE;
    }
  }

  if (ptr)
    {
      /// Delete the entry after invalidating it
      ptr->valid = 0;
      memset(ptr->staAddr, 0, sizeof(ptr->staAddr));
      if (prev == 0)
         pDphHashTable->pHashTable[index] = ptr->next;
      else
         prev->next = ptr->next;
      ptr->added = 0;
      ptr->next = 0;
    }
  else
    {
      /// Entry not present
      PELOGE(limLog(pMac, LOGE, FL("Entry not present STA addr"));
      dphPrintMacAddr(pMac, staAddr, LOGE);)
      return eSIR_FAILURE;
    }

  return eSIR_SUCCESS;
}

// ---------------------------------------------------------------------
/**
 * dphPrintMacAddr
 *
 * FUNCTION:
 * Print a MAC address
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param addr MAC address
 * @return None
 */

void
dphPrintMacAddr(tpAniSirGlobal pMac, tANI_U8 addr[], tANI_U32 level)
{
    limLog(pMac, (tANI_U16) level, FL("MAC ADDR = %d:%d:%d:%d:%d:%d"),
           addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

// ---------------------------------------------------------------------
