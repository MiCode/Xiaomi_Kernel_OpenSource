/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
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
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/*============================================================================
 * @file wlan_hdd_wowl.c
 *
 * Copyright (c) 2009 QUALCOMM Incorporated.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 *
 * ==========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/

#include <wlan_hdd_includes.h>
#include <wlan_hdd_wowl.h>

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

#define WOWL_PTRN_MAX_SIZE          128
#define WOWL_PTRN_MASK_MAX_SIZE      16
#define WOWL_MAX_PTRNS_ALLOWED       16
#define WOWL_INTER_PTRN_TOKENIZER   ';'
#define WOWL_INTRA_PTRN_TOKENIZER   ':'

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

static char *g_hdd_wowl_ptrns[WOWL_MAX_PTRNS_ALLOWED]; //Patterns 0-15
static v_BOOL_t g_hdd_wowl_ptrns_debugfs[WOWL_MAX_PTRNS_ALLOWED] = {0};
static v_U8_t g_hdd_wowl_ptrns_count = 0;

int hdd_parse_hex(unsigned char c)
{
  if (c >= '0' && c <= '9')
    return c-'0';
  if (c >= 'a' && c <= 'f')
    return c-'a'+10;
  if (c >= 'A' && c <= 'F')
    return c-'A'+10;

  return 0;
}

static inline int find_ptrn_len(const char* ptrn)
{
  int len = 0;
  while (*ptrn != '\0' && *ptrn != WOWL_INTER_PTRN_TOKENIZER)
  {
    len++; ptrn++;
  }
  return len;
}

static void hdd_wowl_callback( void *pContext, eHalStatus halStatus )
{
  VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
      "%s: Return code = (%d)", __func__, halStatus );
}

#ifdef WLAN_WAKEUP_EVENTS
static void hdd_wowl_wakeIndication_callback( void *pContext,
    tpSirWakeReasonInd pWakeReasonInd )
{
  VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: Wake Reason %d",
      __func__, pWakeReasonInd->ulReason );
  hdd_exit_wowl( (hdd_adapter_t *)pContext, eWOWL_EXIT_WAKEIND );
}
#endif

static void dump_hdd_wowl_ptrn(tSirWowlAddBcastPtrn *ptrn)
{
  int i;

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: ucPatetrnId = 0x%x", __func__, 
      ptrn->ucPatternId);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: ucPatternByteOffset = 0x%x", __func__, 
      ptrn->ucPatternByteOffset);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: ucPatternSize = 0x%x", __func__, 
      ptrn->ucPatternSize);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: ucPatternMaskSize = 0x%x", __func__, 
      ptrn->ucPatternMaskSize);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: Pattern: ", __func__);
  for(i = 0; i < ptrn->ucPatternSize; i++)
     VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO," %02X", ptrn->ucPattern[i]);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: PatternMask: ", __func__);
  for(i = 0; i<ptrn->ucPatternMaskSize; i++)
     VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,"%02X", ptrn->ucPatternMask[i]);
}


/**============================================================================
  @brief hdd_add_wowl_ptrn() - Function which will add the WoWL pattern to be
  used when PBM filtering is enabled

  @param ptrn : [in]  pointer to the pattern string to be added

  @return     : FALSE if any errors encountered
              : TRUE otherwise
  ===========================================================================*/
v_BOOL_t hdd_add_wowl_ptrn (hdd_adapter_t *pAdapter, const char * ptrn) 
{
  tSirWowlAddBcastPtrn localPattern;
  int i, first_empty_slot, len, offset;
  eHalStatus halStatus;
  const char *temp;
  tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
  v_U8_t sessionId = pAdapter->sessionId;

  len = find_ptrn_len(ptrn);

  /* There has to have atleast 1 byte for each field (pattern size, mask size,
   * pattern, mask) e.g. PP:QQ:RR:SS ==> 11 chars */
  while ( len >= 11 ) 
  {
    first_empty_slot = -1;

    // Find an empty slot to store the pattern
    for (i=0; i<WOWL_MAX_PTRNS_ALLOWED; i++)
    {
      if(g_hdd_wowl_ptrns[i] == NULL) {
        first_empty_slot = i;
        break;
      }
    }

    // Maximum number of patterns have been configured already
    if(first_empty_slot == -1)
    {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, 
          "%s: Cannot add anymore patterns. No free slot!", __func__);
      return VOS_FALSE;
    }

    // Detect duplicate pattern
    for (i=0; i<WOWL_MAX_PTRNS_ALLOWED; i++)
    {
      if(g_hdd_wowl_ptrns[i] == NULL) continue;

      if(!memcmp(ptrn, g_hdd_wowl_ptrns[i], len))
      {
        // Pattern Already configured, skip to next pattern
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, 
            "Trying to add duplicate WoWL pattern. Skip it!");
        ptrn += len; 
        goto next_ptrn;
      }
    }

    //Validate the pattern
    if(ptrn[2] != WOWL_INTRA_PTRN_TOKENIZER || 
       ptrn[5] != WOWL_INTRA_PTRN_TOKENIZER)
    {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, 
          "%s: Malformed pattern string. Skip!", __func__);
      ptrn += len; 
      goto next_ptrn;
    }

    // Extract the pattern size
    localPattern.ucPatternSize = 
      ( hdd_parse_hex( ptrn[0] ) * 0x10 ) + hdd_parse_hex( ptrn[1] );

    // Extract the pattern mask size
    localPattern.ucPatternMaskSize = 
      ( hdd_parse_hex( ptrn[3] ) * 0x10 ) + hdd_parse_hex( ptrn[4] );

    if(localPattern.ucPatternSize > SIR_WOWL_BCAST_PATTERN_MAX_SIZE ||
       localPattern.ucPatternMaskSize > WOWL_PTRN_MASK_MAX_SIZE)
    {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, 
          "%s: Invalid length specified. Skip!", __func__);
      ptrn += len; 
      goto next_ptrn;
    }

    //compute the offset of tokenizer after the pattern
    offset = 5 + 2*localPattern.ucPatternSize + 1;
    if(offset >= len || ptrn[offset] != WOWL_INTRA_PTRN_TOKENIZER) 
    {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, 
          "%s: Malformed pattern string..skip!", __func__);
      ptrn += len; 
      goto next_ptrn;
    }

    //compute the end of pattern sring
    offset = offset + 2*localPattern.ucPatternMaskSize;
    if(offset+1 != len) //offset begins with 0
    {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, 
          "%s: Malformed pattern string...skip!", __func__);
      ptrn += len; 
      goto next_ptrn;
    }

    temp = ptrn;

    // Now advance to where pattern begins 
    ptrn += 6; 

    // Extract the pattern
    for(i=0; i < localPattern.ucPatternSize; i++)
    {
      localPattern.ucPattern[i] = 
        (hdd_parse_hex( ptrn[0] ) * 0x10 ) + hdd_parse_hex( ptrn[1] );
      ptrn += 2; //skip to next byte
    }

    ptrn++; // Skip over the ':' seperator after the pattern

    // Extract the pattern Mask
    for(i=0; i < localPattern.ucPatternMaskSize; i++)
    {
      localPattern.ucPatternMask[i] = 
        (hdd_parse_hex( ptrn[0] ) * 0x10 ) + hdd_parse_hex( ptrn[1] );
      ptrn += 2; //skip to next byte
    }

    //All is good. Store the pattern locally
    g_hdd_wowl_ptrns[first_empty_slot] = (char*) kmalloc(len+1, GFP_KERNEL); 
    if(g_hdd_wowl_ptrns[first_empty_slot] == NULL) 
    {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, 
          "%s: kmalloc failure", __func__);
      return VOS_FALSE;
    }

    memcpy(g_hdd_wowl_ptrns[first_empty_slot], temp, len);
    g_hdd_wowl_ptrns[first_empty_slot][len] = '\0';
    localPattern.ucPatternId = first_empty_slot;
    localPattern.ucPatternByteOffset = 0;

    // Register the pattern downstream
    halStatus = sme_WowlAddBcastPattern( hHal, &localPattern, sessionId );
    if ( !HAL_STATUS_SUCCESS( halStatus ) )
    {
      // Add failed, so invalidate the local storage
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, 
          "sme_WowlAddBcastPattern failed with error code (%d)", halStatus );
      kfree(g_hdd_wowl_ptrns[first_empty_slot]);
      g_hdd_wowl_ptrns[first_empty_slot] = NULL;
    }

    dump_hdd_wowl_ptrn(&localPattern);
 
 next_ptrn:
    if (*ptrn ==  WOWL_INTER_PTRN_TOKENIZER)
    {
      ptrn += 1; // move past the tokenizer
      len = find_ptrn_len(ptrn);
      continue;
    }
    else 
      break;
  }

  return VOS_TRUE;
}

/**============================================================================
  @brief hdd_del_wowl_ptrn() - Function which will remove a WoWL pattern

  @param ptrn : [in]  pointer to the pattern string to be removed

  @return     : FALSE if any errors encountered
              : TRUE otherwise
  ===========================================================================*/
v_BOOL_t hdd_del_wowl_ptrn (hdd_adapter_t *pAdapter, const char * ptrn) 
{
  tSirWowlDelBcastPtrn delPattern;
  unsigned char id;
  tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
  v_BOOL_t patternFound = VOS_FALSE;
  eHalStatus halStatus;
  v_U8_t sessionId = pAdapter->sessionId;

  // Detect pattern
  for (id=0; id<WOWL_MAX_PTRNS_ALLOWED && g_hdd_wowl_ptrns[id] != NULL; id++)
  {
    if(!strcmp(ptrn, g_hdd_wowl_ptrns[id]))
    {
      patternFound = VOS_TRUE;
      break;
    }
  }

  // If pattern present, remove it from downstream
  if(patternFound)
  {
    delPattern.ucPatternId = id;
    halStatus = sme_WowlDelBcastPattern( hHal, &delPattern, sessionId );
    if ( HAL_STATUS_SUCCESS( halStatus ) )
    {
      // Remove from local storage as well
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, 
          "Deleted pattern with id %d [%s]", id, g_hdd_wowl_ptrns[id]);

      kfree(g_hdd_wowl_ptrns[id]);
      g_hdd_wowl_ptrns[id] = NULL;
      return VOS_TRUE;
    }
  }
  return VOS_FALSE;
}

/**============================================================================
  @brief hdd_add_wowl_ptrn_debugfs() - Function which will add a WoW pattern
  sent from debugfs interface

  @param pAdapter       : [in] pointer to the adapter
         pattern_idx    : [in] index of the pattern to be added
         pattern_offset : [in] offset of the pattern in the frame payload
         pattern_buf    : [in] pointer to the pattern hex string to be added
         pattern_mask   : [in] pointer to the pattern mask hex string

  @return               : FALSE if any errors encountered
                        : TRUE otherwise
  ===========================================================================*/
v_BOOL_t hdd_add_wowl_ptrn_debugfs(hdd_adapter_t *pAdapter, v_U8_t pattern_idx,
                                   v_U8_t pattern_offset, char *pattern_buf,
                                   char *pattern_mask)
{
  tSirWowlAddBcastPtrn localPattern;
  eHalStatus halStatus;
  tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
  v_U8_t sessionId = pAdapter->sessionId;
  v_U16_t pattern_len, mask_len, i;

  if (pattern_idx > (WOWL_MAX_PTRNS_ALLOWED - 1))
  {
    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               "%s: WoW pattern index %d is out of range (0 ~ %d).",
               __func__, pattern_idx, WOWL_MAX_PTRNS_ALLOWED - 1);

    return VOS_FALSE;
  }

  pattern_len = strlen(pattern_buf);

  /* Since the pattern is a hex string, 2 characters represent 1 byte. */
  if (pattern_len % 2)
  {
    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               "%s: Malformed WoW pattern!", __func__);

    return VOS_FALSE;
  }
  else
    pattern_len >>= 1;

  if (!pattern_len || pattern_len > WOWL_PTRN_MAX_SIZE)
  {
    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               "%s: WoW pattern length %d is out of range (1 ~ %d).",
               __func__, pattern_len, WOWL_PTRN_MAX_SIZE);

    return VOS_FALSE;
  }

  localPattern.ucPatternId = pattern_idx;
  localPattern.ucPatternByteOffset = pattern_offset;
  localPattern.ucPatternSize = pattern_len;
  if (localPattern.ucPatternSize > SIR_WOWL_BCAST_PATTERN_MAX_SIZE) {
    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               "%s: WoW pattern size (%d) greater than max (%d)",
               __func__, localPattern.ucPatternSize,
               SIR_WOWL_BCAST_PATTERN_MAX_SIZE);
    return VOS_FALSE;
  }
  /* Extract the pattern */
  for (i = 0; i < localPattern.ucPatternSize; i++)
  {
    localPattern.ucPattern[i] =
      (hdd_parse_hex(pattern_buf[0]) << 4) + hdd_parse_hex(pattern_buf[1]);

    /* Skip to next byte */
    pattern_buf += 2;
  }

  /* Get pattern mask size by pattern length */
  localPattern.ucPatternMaskSize = pattern_len >> 3;
  if (pattern_len % 8)
    localPattern.ucPatternMaskSize += 1;

  mask_len = strlen(pattern_mask);
  if ((mask_len % 2) || (localPattern.ucPatternMaskSize != (mask_len >> 1)))
  {
    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               "%s: Malformed WoW pattern mask!", __func__);

    return VOS_FALSE;
  }
  if (localPattern.ucPatternMaskSize > WOWL_PTRN_MASK_MAX_SIZE) {
    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               "%s: WoW pattern mask size (%d) greater than max (%d)",
               __func__, localPattern.ucPatternMaskSize, WOWL_PTRN_MASK_MAX_SIZE);
    return VOS_FALSE;
  }
  /* Extract the pattern mask */
  for (i = 0; i < localPattern.ucPatternMaskSize; i++)
  {
    localPattern.ucPatternMask[i] =
      (hdd_parse_hex(pattern_mask[0]) << 4) + hdd_parse_hex(pattern_mask[1]);

    /* Skip to next byte */
    pattern_mask += 2;
  }

  /* Register the pattern downstream */
  halStatus = sme_WowlAddBcastPattern(hHal, &localPattern, sessionId);

  if (!HAL_STATUS_SUCCESS(halStatus))
  {
    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               "%s: sme_WowlAddBcastPattern failed with error code (%d).",
               __func__, halStatus);

    return VOS_FALSE;
  }

  /* All is good. */
  if (!g_hdd_wowl_ptrns_debugfs[pattern_idx])
  {
    g_hdd_wowl_ptrns_debugfs[pattern_idx] = 1;
    g_hdd_wowl_ptrns_count++;
  }

  dump_hdd_wowl_ptrn(&localPattern);

  return VOS_TRUE;
}

/**============================================================================
  @brief hdd_del_wowl_ptrn_debugfs() - Function which will remove a WoW pattern
  sent from debugfs interface

  @param pAdapter    : [in] pointer to the adapter
         pattern_idx : [in] index of the pattern to be removed

  @return            : FALSE if any errors encountered
                     : TRUE otherwise
  ===========================================================================*/
v_BOOL_t hdd_del_wowl_ptrn_debugfs(hdd_adapter_t *pAdapter, v_U8_t pattern_idx)
{
  tSirWowlDelBcastPtrn delPattern;
  tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
  eHalStatus halStatus;
  v_U8_t sessionId = pAdapter->sessionId;

  if (pattern_idx > (WOWL_MAX_PTRNS_ALLOWED - 1))
  {
    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               "%s: WoW pattern index %d is not in the range (0 ~ %d).",
               __func__, pattern_idx, WOWL_MAX_PTRNS_ALLOWED - 1);

    return VOS_FALSE;
  }

  if (!g_hdd_wowl_ptrns_debugfs[pattern_idx])
  {
    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               "%s: WoW pattern %d is not in the table.",
               __func__, pattern_idx);

    return VOS_FALSE;
  }

  delPattern.ucPatternId = pattern_idx;
  halStatus = sme_WowlDelBcastPattern(hHal, &delPattern, sessionId);

  if (!HAL_STATUS_SUCCESS(halStatus))
  {
    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               "%s: sme_WowlDelBcastPattern failed with error code (%d).",
               __func__, halStatus);

    return VOS_FALSE;
  }

  g_hdd_wowl_ptrns_debugfs[pattern_idx] = 0;
  g_hdd_wowl_ptrns_count--;

  return VOS_TRUE;
}

/**============================================================================
  @brief hdd_enter_wowl() - Function which will enable WoWL. Atleast one
  of MP and PBM must be enabled

  @param enable_mp  : [in] Whether to enable magic packet WoWL mode
  @param enable_pbm : [in] Whether to enable pattern byte matching WoWL mode

  @return           : FALSE if any errors encountered
                    : TRUE otherwise
  ===========================================================================*/
v_BOOL_t hdd_enter_wowl (hdd_adapter_t *pAdapter, v_BOOL_t enable_mp, v_BOOL_t enable_pbm)
{
  tSirSmeWowlEnterParams wowParams;
  eHalStatus halStatus;
  tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);

  vos_mem_zero( &wowParams, sizeof( tSirSmeWowlEnterParams ) );

  wowParams.ucPatternFilteringEnable = enable_pbm;
  wowParams.ucMagicPktEnable = enable_mp;
  if(enable_mp)
  {
    vos_copy_macaddr( (v_MACADDR_t *)&(wowParams.magicPtrn),
                    &(pAdapter->macAddressCurrent) );
  }

#ifdef WLAN_WAKEUP_EVENTS
  wowParams.ucWoWEAPIDRequestEnable = VOS_TRUE;
  wowParams.ucWoWEAPOL4WayEnable = VOS_TRUE;
  wowParams.ucWowNetScanOffloadMatch = VOS_TRUE;
  wowParams.ucWowGTKRekeyError = VOS_TRUE;
  wowParams.ucWoWBSSConnLoss = VOS_TRUE;
#endif // WLAN_WAKEUP_EVENTS

  // Request to put Libra into WoWL
  halStatus = sme_EnterWowl( hHal, hdd_wowl_callback, 
                             pAdapter,
#ifdef WLAN_WAKEUP_EVENTS
                             hdd_wowl_wakeIndication_callback,
                             pAdapter,
#endif // WLAN_WAKEUP_EVENTS
                             &wowParams, pAdapter->sessionId);

  if ( !HAL_STATUS_SUCCESS( halStatus ) )
  {
    if ( eHAL_STATUS_PMC_PENDING != halStatus )
    {
      // We failed to enter WoWL
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, 
          "sme_EnterWowl failed with error code (%d)", halStatus );
      return VOS_FALSE;
    }
  }
  return VOS_TRUE;
}

/**============================================================================
  @brief hdd_exit_wowl() - Function which will disable WoWL

  @param wowlExitSrc: To know is wowl exiting because of wakeup pkt or user
                      explicitly disabling WoWL
  @return           : FALSE if any errors encountered
                    : TRUE otherwise
  ===========================================================================*/
v_BOOL_t hdd_exit_wowl (hdd_adapter_t*pAdapter, tWowlExitSource wowlExitSrc)
{
  tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
  eHalStatus halStatus;

  halStatus = sme_ExitWowl( hHal, wowlExitSrc );
  if ( !HAL_STATUS_SUCCESS( halStatus ) )
  {
    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
      "sme_ExitWowl failed with error code (%d)", halStatus );
    return VOS_FALSE;
  }

  return VOS_TRUE;
}

/**============================================================================
  @brief hdd_init_wowl() - Init function which will initialize the WoWL module
  and perform any required intial configuration 

  @return           : FALSE if any errors encountered
                    : TRUE otherwise
  ===========================================================================*/
v_BOOL_t hdd_init_wowl (hdd_adapter_t*pAdapter) 
{
  hdd_context_t *pHddCtx = NULL;
  pHddCtx = pAdapter->pHddCtx;

  memset(g_hdd_wowl_ptrns, 0, sizeof(g_hdd_wowl_ptrns));

  //Add any statically configured patterns 
  hdd_add_wowl_ptrn(pAdapter, pHddCtx->cfg_ini->wowlPattern); 

  return VOS_TRUE;
}
