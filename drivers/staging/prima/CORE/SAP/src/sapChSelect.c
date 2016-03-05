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

/*===========================================================================

                      s a p C h S e l e c t . C
  OVERVIEW:

  This software unit holds the implementation of the WLAN SAP modules
  functions for channel selection.

  DEPENDENCIES:

  Are listed for each API below.
===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.



  when        who       what, where, why
----------    ---       --------------------------------------------------------
2010-03-15  SOFTAP      Created module

===========================================================================*/


/*--------------------------------------------------------------------------
  Include Files
------------------------------------------------------------------------*/
#include "vos_trace.h"
#include "csrApi.h"
#include "sme_Api.h"
#include "sapChSelect.h"
#include "sapInternal.h"
#ifdef ANI_OS_TYPE_QNX
#include "stdio.h"
#endif

/*--------------------------------------------------------------------------
  Function definitions
--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------
  Defines
--------------------------------------------------------------------------*/
#define SAP_DEBUG

#define IS_RSSI_VALID( extRssi, rssi ) \
( \
   ((extRssi < rssi)?eANI_BOOLEAN_TRUE:eANI_BOOLEAN_FALSE) \
)

#ifdef FEATURE_WLAN_CH_AVOID
extern safeChannelType safeChannels[];
#endif /* FEATURE_WLAN_CH_AVOID */

/*==========================================================================
  FUNCTION    sapCleanupChannelList

  DESCRIPTION 
    Function sapCleanupChannelList frees up the memory allocated to the channel list.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    NULL
   
  RETURN VALUE
    NULL
============================================================================*/

void sapCleanupChannelList(void)
{
    v_PVOID_t pvosGCtx = vos_get_global_context(VOS_MODULE_ID_SAP, NULL);
    ptSapContext pSapCtx;

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO, 
               "Cleaning up the channel list structure");

    if (NULL == pvosGCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_FATAL,
                   "SAP Global Context is NULL");
        return ;
    }

    pSapCtx = VOS_GET_SAP_CB(pvosGCtx);
    if (NULL == pSapCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_FATAL,
                   "SAP Context is NULL");
        return ;
    }

    pSapCtx->SapChnlList.numChannel = 0;
    vos_mem_free(pSapCtx->SapChnlList.channelList);
    pSapCtx->SapChnlList.channelList = NULL;
}

/*==========================================================================
  FUNCTION    sapSetPreferredChannel

  DESCRIPTION 
    Function sapSetPreferredChannel sets the channel list which has been configured
    into sap context (pSapCtx) which will be used at the time of best channel selection. 

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    *ptr: pointer having the command followed by the arguments in string format
   
  RETURN VALUE
    int:  return 0 when success else returns error code.
============================================================================*/

int sapSetPreferredChannel(tANI_U8* ptr)
{

    v_PVOID_t pvosGCtx = vos_get_global_context(VOS_MODULE_ID_SAP, NULL);
    ptSapContext pSapCtx;
    tANI_U8* param;
    int tempInt;
    int j;

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, 
               "Enter: %s", __func__);

    if (NULL == pvosGCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_FATAL,
                   "SAP Global Context is NULL");
        return -EINVAL;
    }

    pSapCtx = VOS_GET_SAP_CB(pvosGCtx);
    if (NULL == pSapCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_FATAL,
                   "SAP Context is NULL");
        return -EINVAL;
    }

    if (NULL != pSapCtx->SapChnlList.channelList)
    {
        sapCleanupChannelList();
    }

    param = strchr(ptr, ' ');
    /*no argument after the command*/
    if (NULL == param)
    {
       return -EINVAL;
    }

    /*no space after the command*/
    else if (SPACE_ASCII_VALUE != *param)
    {
        return -EINVAL;
    }

    param++;

    /*removing empty spaces*/
    while((SPACE_ASCII_VALUE  == *param)&& ('\0' !=  *param) ) param++;

    /*no argument followed by spaces*/
    if('\0' == *param)
    {
        return -EINVAL;
    }

    /*getting the first argument ie the number of channels*/
    if (sscanf(param, "%d ", &tempInt) != 1)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "Cannot get number of channels from input", __func__);
        return -EINVAL;
    }

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, 
               "Number of channel added are: %d", tempInt);

    if (tempInt <= 0 || tempInt > 255)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "Invalid Number of channel received", __func__);
        return -EINVAL;
    }

    /*allocating space for the desired number of channels*/
    pSapCtx->SapChnlList.channelList = (v_U8_t *)vos_mem_malloc(tempInt);

    if (NULL ==  pSapCtx->SapChnlList.channelList)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "In %s, VOS_MALLOC_ERR", __func__);
        return -EINVAL;
    }

    pSapCtx->SapChnlList.numChannel = tempInt;
    for(j=0;j<pSapCtx->SapChnlList.numChannel;j++)
    {

        /*param pointing to the beginning of first space after number of channels*/
        param = strpbrk( param, " " ); 
        /*no channel list after the number of channels argument*/
        if (NULL == param)
        {
            sapCleanupChannelList();
            return -EINVAL;
        }

        param++;

        /*removing empty space*/
        while((SPACE_ASCII_VALUE == *param) && ('\0' != *param) ) param++;

        /*no channel list after the number of channels argument and spaces*/
        if( '\0' == *param )
        {
            sapCleanupChannelList();
            return -EINVAL;
        }

        if (sscanf(param, "%d ", &tempInt) != 1)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "Cannot read channel number", __func__);
            sapCleanupChannelList();
            return -EINVAL;
        }
        if (tempInt < 0 || tempInt > 255)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "Invalid channel number received", __func__);
            sapCleanupChannelList();
            return -EINVAL;
        }

        pSapCtx->SapChnlList.channelList[j] = tempInt;

        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, 
                   "Channel %d added to preferred channel list",
                   pSapCtx->SapChnlList.channelList[j] );

    }

    /*extra arguments check*/
    param = strpbrk( param, " " );
    if (NULL != param)
    {
        while((SPACE_ASCII_VALUE == *param) && ('\0' != *param) ) param++;

        if('\0' !=  *param)
        {
            sapCleanupChannelList();
            return -EINVAL;
        }
    }

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, 
               "Exit: %s", __func__);

    return 0;
}

/*==========================================================================
  FUNCTION    sapSelectPreferredChannelFromChannelList

  DESCRIPTION 
    Function sapSelectPreferredChannelFromChannelList calculates the best channel
    among the configured channel list. If channel list not configured then returns 
    the best channel calculated among all the channel list.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    *pSpectInfoParams  : Pointer to tSapChSelSpectInfo structure
    bestChNum: best channel already calculated among all the chanels
    pSapCtx: having info of channel list from which best channel is selected 
   
  RETURN VALUE
    v_U8_t:  best channel
============================================================================*/
v_U8_t sapSelectPreferredChannelFromChannelList(v_U8_t bestChNum, 
                                                ptSapContext pSapCtx, 
                                                tSapChSelSpectInfo *pSpectInfoParams)
{
    v_U8_t j = 0;
    v_U8_t count = 0;

    //If Channel List is not Configured don't do anything
    //Else return the Best Channel from the Channel List
    if((NULL == pSapCtx->SapChnlList.channelList) || 
       (NULL == pSpectInfoParams) || 
       (0 == pSapCtx->SapChnlList.numChannel))
    {
        return bestChNum;
    }

    if (bestChNum > 0 && bestChNum <= 252)
    {
        for(count=0; count < pSpectInfoParams->numSpectChans ; count++)
        {
            bestChNum = (v_U8_t)pSpectInfoParams->pSpectCh[count].chNum;
            // Select the best channel from allowed list
            for(j=0;j< pSapCtx->SapChnlList.numChannel;j++)
            {
                if( (pSapCtx->SapChnlList.channelList[j]) == bestChNum)
                {
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, 
                               "Best channel computed from Channel List is: %d", 
                               bestChNum);
                    return bestChNum;
                }
            }
        }

        return SAP_CHANNEL_NOT_SELECTED;
    }
    else
        return SAP_CHANNEL_NOT_SELECTED;
}


/*==========================================================================
  FUNCTION    sapChanSelInit

  DESCRIPTION 
    Function sapChanSelInit allocates the memory, intializes the
         structures used by the channel selection algorithm

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    *pSpectInfoParams  : Pointer to tSapChSelSpectInfo structure
   
  RETURN VALUE
    v_BOOL_t:  Success or FAIL
  
  SIDE EFFECTS 
============================================================================*/
v_BOOL_t sapChanSelInit(tHalHandle halHandle, tSapChSelSpectInfo *pSpectInfoParams)
{
    tSapSpectChInfo *pSpectCh = NULL;
    v_U8_t *pChans = NULL;
    v_U16_t channelnum = 0;
    tpAniSirGlobal pMac = PMAC_STRUCT(halHandle);
#ifdef FEATURE_WLAN_CH_AVOID
    v_U16_t i;
    v_BOOL_t chSafe = VOS_TRUE;
#endif /* FEATURE_WLAN_CH_AVOID */

    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s", __func__);

    // Channels for that 2.4GHz band
    //Considered only for 2.4GHz need to change in future to support 5GHz support
    pSpectInfoParams->numSpectChans = pMac->scan.base20MHzChannels.numChannels;
       
    // Allocate memory for weight computation of 2.4GHz
    pSpectCh = (tSapSpectChInfo *)vos_mem_malloc((pSpectInfoParams->numSpectChans) * sizeof(*pSpectCh));

    if(pSpectCh == NULL) {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR, "In %s, VOS_MALLOC_ERR", __func__);
        return eSAP_FALSE;
    }

    vos_mem_zero(pSpectCh, (pSpectInfoParams->numSpectChans) * sizeof(*pSpectCh));

    // Initialize the pointers in the DfsParams to the allocated memory
    pSpectInfoParams->pSpectCh = pSpectCh;

    pChans = pMac->scan.base20MHzChannels.channelList;

    // Fill the channel number in the spectrum in the operating freq band
    for (channelnum = 0; channelnum < pSpectInfoParams->numSpectChans; channelnum++) {
#ifdef FEATURE_WLAN_CH_AVOID
        chSafe = VOS_TRUE;
        for(i = 0; i < NUM_20MHZ_RF_CHANNELS; i++)
        {
            if((safeChannels[i].channelNumber == *pChans) &&
               (VOS_FALSE == safeChannels[i].isSafe))
            {
               VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                         "%s : CH %d is not safe", __func__, *pChans);
               chSafe = VOS_FALSE;
               break;
            }
        }
#endif /* FEATURE_WLAN_CH_AVOID */

        if(*pChans == 14 ) //OFDM rates are not supported on channel 14
        {
            pChans++;
            continue;
        }
#ifdef FEATURE_WLAN_CH_AVOID
        if (VOS_TRUE == chSafe)
        {
#endif /* FEATURE_WLAN_CH_AVOID */
           VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_DEBUG,
                     "%s : Available Ch %d",
                     __func__, *pChans);
           pSpectCh->chNum = *pChans;
           pSpectCh->valid = eSAP_TRUE;
           // Initialise for all channels
           pSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
           // Initialise 20MHz for all the Channels
           pSpectCh->channelWidth = SOFTAP_HT20_CHANNELWIDTH;
           pSpectCh++;
#ifdef FEATURE_WLAN_CH_AVOID
        }
#endif /* FEATURE_WLAN_CH_AVOID */
        pChans++;
    }
    return eSAP_TRUE;
}

/*==========================================================================
  FUNCTION    sapweightRssiCount

  DESCRIPTION 
    Function weightRssiCount calculates the channel weight due to rssi
         and data count(here number of BSS observed)
         
  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    rssi        : Max signal strength receieved from a BSS for the channel
    count       : Number of BSS observed in the channel
   
  RETURN VALUE
    v_U32_t     : Calculated channel weight based on above two
  
  SIDE EFFECTS 
============================================================================*/
v_U32_t sapweightRssiCount(v_S7_t rssi, v_U16_t count)
{
    v_S31_t     rssiWeight=0;
    v_S31_t     countWeight=0;
    v_U32_t     rssicountWeight=0;
    
    // Weight from RSSI
    rssiWeight = SOFTAP_RSSI_WEIGHT * (rssi - SOFTAP_MIN_RSSI)
                 /(SOFTAP_MAX_RSSI - SOFTAP_MIN_RSSI);

    if(rssiWeight > SOFTAP_RSSI_WEIGHT)
        rssiWeight = SOFTAP_RSSI_WEIGHT;
    else if (rssiWeight < 0)
        rssiWeight = 0;

    // Weight from data count
    countWeight = SOFTAP_COUNT_WEIGHT * (count - SOFTAP_MIN_COUNT)
                  /(SOFTAP_MAX_COUNT - SOFTAP_MIN_COUNT);

    if(countWeight > SOFTAP_COUNT_WEIGHT)
        countWeight = SOFTAP_COUNT_WEIGHT;
    else if (countWeight < 0)
        countWeight = 0;

    rssicountWeight =  rssiWeight + countWeight;      

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, rssiWeight=%d, countWeight=%d, rssicountWeight=%d",
              __func__, rssiWeight, countWeight, rssicountWeight);
    
    return(rssicountWeight);
}


/*==========================================================================
  FUNCTION    sapInterferenceRssiCount

  DESCRIPTION
    Function sapInterferenceRssiCount Considers the Adjacent channel rssi
    and data count(here number of BSS observed)

  DEPENDENCIES
    NA.

  PARAMETERS

    pSpectCh    : Channel Information

  RETURN VALUE
    NA.

  SIDE EFFECTS
============================================================================*/
void sapInterferenceRssiCount(tSapSpectChInfo *pSpectCh)
{
    tSapSpectChInfo *pExtSpectCh = NULL;
    v_S31_t rssi;

    if (NULL == pSpectCh)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: pSpectCh is NULL", __func__);
        return;
    }

    switch(pSpectCh->chNum)
    {
        case CHANNEL_1:
            pExtSpectCh = (pSpectCh + 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 2);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 3);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 4);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FOURTH_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
        break;

        case CHANNEL_2:
            pExtSpectCh = (pSpectCh - 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 2);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 3);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 4);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            break;
        case CHANNEL_3:
            pExtSpectCh = (pSpectCh - 2);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 2);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 3);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 4);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FOURTH_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            break;
        case CHANNEL_4:
            pExtSpectCh = (pSpectCh - 3);
            if(pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 2);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 2);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 3);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 4);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FOURTH_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            break;

        case CHANNEL_5:
        case CHANNEL_6:
        case CHANNEL_7:
            pExtSpectCh = (pSpectCh - 4);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FOURTH_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 3);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 2);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 1);
            if(pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 2);
            if(pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 3);
            if(pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 4);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FOURTH_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            break;

        case CHANNEL_8:
            pExtSpectCh = (pSpectCh - 4);
            if(pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FOURTH_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }

            pExtSpectCh = (pSpectCh - 3);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 2);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 2);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 3);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            break;

        case CHANNEL_9:
            pExtSpectCh = (pSpectCh - 4);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FOURTH_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }

            pExtSpectCh = (pSpectCh - 3);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 2);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 2);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            break;

        case CHANNEL_10:
            pExtSpectCh = (pSpectCh - 4);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FOURTH_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }

            pExtSpectCh = (pSpectCh - 3);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 2);
            if(pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            break;

        case CHANNEL_11:
            pExtSpectCh = (pSpectCh - 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 2);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 3);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 4);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FOURTH_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            break;

        default:
            break;
    }
}

/*==========================================================================
  FUNCTION    sapComputeSpectWeight

  DESCRIPTION 
    Main function for computing the weight of each channel in the
    spectrum based on the RSSI value of the BSSes on the channel
    and number of BSS
    
  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    pSpectInfoParams       : Pointer to the tSpectInfoParams structure
    halHandle              : Pointer to HAL handle
    pResult                : Pointer to tScanResultHandle
   
  RETURN VALUE
    void     : NULL
  
  SIDE EFFECTS 
============================================================================*/
void sapComputeSpectWeight( tSapChSelSpectInfo* pSpectInfoParams,
                             tHalHandle halHandle, tScanResultHandle pResult)
{
    v_S7_t rssi = 0;
    v_U8_t chn_num = 0;
    v_U8_t channel_id = 0;

    tCsrScanResultInfo *pScanResult;
    tSapSpectChInfo *pSpectCh   = pSpectInfoParams->pSpectCh;
    v_U32_t operatingBand;
    v_U16_t channelWidth;
    v_U16_t secondaryChannelOffset;
    v_U16_t centerFreq;
    v_U16_t vhtSupport;
    v_U32_t ieLen = 0;
    tSirProbeRespBeacon *pBeaconStruct;
    tpAniSirGlobal  pMac = (tpAniSirGlobal) halHandle;

    pBeaconStruct = vos_mem_malloc(sizeof(tSirProbeRespBeacon));
    if ( NULL == pBeaconStruct )
    {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Unable to allocate memory in sapComputeSpectWeight\n");
        return;
    }
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, Computing spectral weight", __func__);

    /**
    * Soft AP specific channel weight calculation using DFS formula
    */
    ccmCfgGetInt( halHandle, WNI_CFG_SAP_CHANNEL_SELECT_OPERATING_BAND, &operatingBand);

    pScanResult = sme_ScanResultGetFirst(halHandle, pResult);

    while (pScanResult) {
        pSpectCh = pSpectInfoParams->pSpectCh;
        // Defining the default values, so that any value will hold the default values
        channelWidth = eHT_CHANNEL_WIDTH_20MHZ;
        secondaryChannelOffset = PHY_SINGLE_CHANNEL_CENTERED;
        vhtSupport = 0;
        centerFreq = 0;

        if (pScanResult->BssDescriptor.ieFields != NULL)
        {
            ieLen = (pScanResult->BssDescriptor.length + sizeof(tANI_U16) + sizeof(tANI_U32) - sizeof(tSirBssDescription));
            vos_mem_set((tANI_U8 *) pBeaconStruct, sizeof(tSirProbeRespBeacon), 0);

            if ((sirParseBeaconIE(pMac, pBeaconStruct,(tANI_U8 *)( pScanResult->BssDescriptor.ieFields), ieLen)) == eSIR_SUCCESS)
            {
                if (pBeaconStruct->HTCaps.present && pBeaconStruct->HTInfo.present)
                {
                    channelWidth = pBeaconStruct->HTCaps.supportedChannelWidthSet;
                    secondaryChannelOffset = pBeaconStruct->HTInfo.secondaryChannelOffset;
                    if(pBeaconStruct->VHTOperation.present)
                    {
                        vhtSupport = pBeaconStruct->VHTOperation.present;
                        if(pBeaconStruct->VHTOperation.chanWidth > WNI_CFG_VHT_CHANNEL_WIDTH_20_40MHZ)
                        {
                            channelWidth = eHT_CHANNEL_WIDTH_80MHZ;
                            centerFreq = pBeaconStruct->VHTOperation.chanCenterFreqSeg1;
                        }
                    }
                }
            }
        }
        // Processing for each tCsrScanResultInfo in the tCsrScanResult DLink list
        for (chn_num = 0; chn_num < pSpectInfoParams->numSpectChans; chn_num++) {

            /*
             *  if the Beacon has channel ID, use it other wise we will 
             *  rely on the channelIdSelf
             */
            if(pScanResult->BssDescriptor.channelId == 0)
                channel_id = pScanResult->BssDescriptor.channelIdSelf;
            else
                channel_id = pScanResult->BssDescriptor.channelId;

            if (pSpectCh && (channel_id == pSpectCh->chNum)) {
                if (pSpectCh->rssiAgr < pScanResult->BssDescriptor.rssi)
                    pSpectCh->rssiAgr = pScanResult->BssDescriptor.rssi;

                ++pSpectCh->bssCount; // Increment the count of BSS

                if(operatingBand) // Connsidering the Extension Channel only in a channels
                {
                    /* Updating the received ChannelWidth */
                    if (pSpectCh->channelWidth != channelWidth)
                        pSpectCh->channelWidth = channelWidth;
                    /* If received ChannelWidth is other than HT20, we need to update the extension channel Params as well */
                    /* channelWidth == 0, HT20 */
                    /* channelWidth == 1, HT40 */
                    /* channelWidth == 2, VHT80*/
                    switch(pSpectCh->channelWidth)
                    {
                        case eHT_CHANNEL_WIDTH_40MHZ: //HT40
                            switch( secondaryChannelOffset)
                            {
                                tSapSpectChInfo *pExtSpectCh = NULL;
                                case PHY_DOUBLE_CHANNEL_LOW_PRIMARY: // Above the Primary Channel
                                    pExtSpectCh = (pSpectCh + 1);
                                    if(pExtSpectCh != NULL)
                                    {
                                        ++pExtSpectCh->bssCount;
                                        rssi = pSpectCh->rssiAgr + SAP_SUBBAND1_RSSI_EFFECT_PRIMARY;
                                        // REducing the rssi by -20 and assigning it to Extension channel
                                        if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                        {
                                            pExtSpectCh->rssiAgr = rssi;
                                        }
                                        if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                            pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                    }
                                break;

                                case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY: // Below the Primary channel
                                    pExtSpectCh = (pSpectCh - 1);
                                    if(pExtSpectCh != NULL) 
                                    {
                                        rssi = pSpectCh->rssiAgr + SAP_SUBBAND1_RSSI_EFFECT_PRIMARY;
                                        if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                        {
                                            pExtSpectCh->rssiAgr = rssi;
                                        }
                                        if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                            pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                        ++pExtSpectCh->bssCount;
                                    }
                                break;
                            }
                        break;
                        case eHT_CHANNEL_WIDTH_80MHZ: // VHT80
                            if((centerFreq - channel_id) == 6)
                            {
                                tSapSpectChInfo *pExtSpectCh = NULL;
                                pExtSpectCh = (pSpectCh + 1);
                                if(pExtSpectCh != NULL)
                                {
                                    ++pExtSpectCh->bssCount;
                                    rssi = pSpectCh->rssiAgr + SAP_SUBBAND1_RSSI_EFFECT_PRIMARY;
                                    if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                    {
                                        pExtSpectCh->rssiAgr = rssi; // Reducing the rssi by -20 and assigning it to Subband 1
                                    }
                                    if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                        pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                }
                                pExtSpectCh = (pSpectCh + 2);
                                if(pExtSpectCh != NULL)
                                {
                                    ++pExtSpectCh->bssCount;
                                    rssi = pSpectCh->rssiAgr + SAP_SUBBAND2_RSSI_EFFECT_PRIMARY;
                                    if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                    {
                                        pExtSpectCh->rssiAgr = rssi; // Reducing the rssi by -30 and assigning it to Subband 2
                                    }
                                    if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                        pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                }
                                pExtSpectCh = (pSpectCh + 3);
                                if(pExtSpectCh != NULL)
                                {
                                    ++pExtSpectCh->bssCount;
                                    rssi = pSpectCh->rssiAgr + SAP_SUBBAND3_RSSI_EFFECT_PRIMARY;
                                    if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                    {
                                        pExtSpectCh->rssiAgr = rssi; // Reducing the rssi by -40 and assigning it to Subband 3
                                    }
                                    if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                        pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                }
                            }
                            else if((centerFreq - channel_id) == 2)
                            {
                                tSapSpectChInfo *pExtSpectCh = NULL;
                                pExtSpectCh = (pSpectCh - 1 );
                                if(pExtSpectCh != NULL)
                                {
                                    ++pExtSpectCh->bssCount;
                                    rssi = pSpectCh->rssiAgr + SAP_SUBBAND1_RSSI_EFFECT_PRIMARY;
                                    if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                    {
                                        pExtSpectCh->rssiAgr = rssi;
                                    }
                                    if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                        pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                }
                                pExtSpectCh = (pSpectCh + 1);
                                if(pExtSpectCh != NULL)
                                {
                                    ++pExtSpectCh->bssCount;
                                    rssi = pSpectCh->rssiAgr + SAP_SUBBAND1_RSSI_EFFECT_PRIMARY;
                                    if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                    {
                                        pExtSpectCh->rssiAgr = rssi;
                                    }
                                    if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                        pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                }
                                pExtSpectCh = (pSpectCh + 2);
                                if(pExtSpectCh != NULL)
                                {
                                    ++pExtSpectCh->bssCount;
                                    rssi = pSpectCh->rssiAgr + SAP_SUBBAND2_RSSI_EFFECT_PRIMARY;
                                    if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                    {
                                        pExtSpectCh->rssiAgr = rssi;
                                    }
                                    if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                        pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                }
                            }
                            else if((centerFreq - channel_id) == -2)
                            {
                                tSapSpectChInfo *pExtSpectCh = NULL;
                                pExtSpectCh = (pSpectCh - 1 );
                                if(pExtSpectCh != NULL)
                                {
                                    ++pExtSpectCh->bssCount;
                                    rssi = pSpectCh->rssiAgr + SAP_SUBBAND1_RSSI_EFFECT_PRIMARY;
                                    if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                    {
                                        pExtSpectCh->rssiAgr = rssi;
                                    }
                                    if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                        pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                }
                                pExtSpectCh = (pSpectCh - 2);
                                if(pExtSpectCh != NULL)
                                {
                                    ++pExtSpectCh->bssCount;
                                    rssi = pSpectCh->rssiAgr + SAP_SUBBAND2_RSSI_EFFECT_PRIMARY;
                                    if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                    {
                                        pExtSpectCh->rssiAgr = rssi;
                                    }
                                    if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                        pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                }
                                pExtSpectCh = (pSpectCh + 1);
                                if(pExtSpectCh != NULL)
                                {
                                    ++pExtSpectCh->bssCount;
                                    rssi = pSpectCh->rssiAgr + SAP_SUBBAND1_RSSI_EFFECT_PRIMARY;
                                    if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                    {
                                        pExtSpectCh->rssiAgr = rssi;
                                    }
                                    if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                        pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                }
                            }
                            else if((centerFreq - channel_id) == -6)
                            {
                                tSapSpectChInfo *pExtSpectCh = NULL;
                                pExtSpectCh = (pSpectCh - 1 );
                                if(pExtSpectCh != NULL)
                                {
                                    ++pExtSpectCh->bssCount;
                                    rssi = pSpectCh->rssiAgr + SAP_SUBBAND1_RSSI_EFFECT_PRIMARY;
                                    if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                    {
                                        pExtSpectCh->rssiAgr = rssi;
                                    }
                                    if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                        pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                }
                                pExtSpectCh = (pSpectCh - 2);
                                if(pExtSpectCh != NULL)
                                {
                                    ++pExtSpectCh->bssCount;
                                    rssi = pSpectCh->rssiAgr + SAP_SUBBAND2_RSSI_EFFECT_PRIMARY;
                                    if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                    {
                                        pExtSpectCh->rssiAgr = rssi;
                                    }
                                    if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                        pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                }
                                pExtSpectCh = (pSpectCh - 3);
                                if(pExtSpectCh != NULL)
                                {
                                    ++pExtSpectCh->bssCount;
                                    rssi = pSpectCh->rssiAgr + SAP_SUBBAND3_RSSI_EFFECT_PRIMARY;
                                    if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                    {
                                        pExtSpectCh->rssiAgr = rssi;
                                    }
                                    if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                        pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                }
                            }
                        break;
                    }
                }
                else if(operatingBand == RF_SUBBAND_2_4_GHZ)
                {
                     sapInterferenceRssiCount(pSpectCh);
                }

                VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "In %s, bssdes.ch_self=%d, bssdes.ch_ID=%d, bssdes.rssi=%d, SpectCh.bssCount=%d, pScanResult=%p, ChannelWidth %d, secondaryChanOffset %d, center frequency %d \n",
                  __func__, pScanResult->BssDescriptor.channelIdSelf, pScanResult->BssDescriptor.channelId, pScanResult->BssDescriptor.rssi, pSpectCh->bssCount, pScanResult,pSpectCh->channelWidth,secondaryChannelOffset,centerFreq);
                 pSpectCh++;
                 break;
           } else {
             pSpectCh++;
           }
        }

        pScanResult = sme_ScanResultGetNext(halHandle, pResult);
    }

    // Calculate the weights for all channels in the spectrum pSpectCh
    pSpectCh = pSpectInfoParams->pSpectCh;

    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, Spectrum Channels Weight", __func__);

    for (chn_num = 0; chn_num < (pSpectInfoParams->numSpectChans); chn_num++) {

        /*
          rssi : Maximum received signal strength among all BSS on that channel
          bssCount : Number of BSS on that channel
        */

        rssi = (v_S7_t)pSpectCh->rssiAgr;

        pSpectCh->weight = SAPDFS_NORMALISE_1000 * sapweightRssiCount(rssi, pSpectCh->bssCount);

        //------ Debug Info ------
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
             "In %s, Chan=%d Weight= %d rssiAgr=%d bssCount=%d", __func__,
             pSpectCh->chNum, pSpectCh->weight,
             pSpectCh->rssiAgr, pSpectCh->bssCount);
        //------ Debug Info ------
        pSpectCh++;
    }
    vos_mem_free(pBeaconStruct);
}

/*==========================================================================
  FUNCTION    sapChanSelExit

  DESCRIPTION 
    Exit function for free out the allocated memory, to be called 
    at the end of the dfsSelectChannel function
    
  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    pSpectInfoParams       : Pointer to the tSapChSelSpectInfo structure
   
  RETURN VALUE
    void     : NULL
  
  SIDE EFFECTS 
============================================================================*/
void sapChanSelExit( tSapChSelSpectInfo *pSpectInfoParams )
{
    // Free all the allocated memory
    vos_mem_free(pSpectInfoParams->pSpectCh);
}

/*==========================================================================
  FUNCTION    sapSortChlWeight

  DESCRIPTION 
    Funtion to sort the channels with the least weight first
    
  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    pSpectInfoParams       : Pointer to the tSapChSelSpectInfo structure
   
  RETURN VALUE
    void     : NULL
  
  SIDE EFFECTS 
============================================================================*/
void sapSortChlWeight(tSapChSelSpectInfo *pSpectInfoParams)
{
    tSapSpectChInfo temp;

    tSapSpectChInfo *pSpectCh = NULL;
    v_U32_t i = 0, j = 0, minWeightIndex = 0;

    pSpectCh = pSpectInfoParams->pSpectCh;
#ifdef SOFTAP_CHANNEL_RANGE
    // Sorting the channels as per weights
    for (i = 0; i < pSpectInfoParams->numSpectChans; i++) {
        minWeightIndex = i;
        for( j = i + 1; j < pSpectInfoParams->numSpectChans; j++) {
            if(pSpectCh[j].weight < pSpectCh[minWeightIndex].weight) {
                minWeightIndex = j; 
            }
        }
        if(minWeightIndex != i) {
            vos_mem_copy(&temp, &pSpectCh[minWeightIndex], sizeof(*pSpectCh));
            vos_mem_copy(&pSpectCh[minWeightIndex], &pSpectCh[i], sizeof(*pSpectCh));
            vos_mem_copy(&pSpectCh[i], &temp, sizeof(*pSpectCh));
        }
    }
#else
    // Sorting the channels as per weights
    for (i = 0; i < SPECT_24GHZ_CH_COUNT; i++) {
        minWeightIndex = i;
        for( j = i + 1; j < SPECT_24GHZ_CH_COUNT; j++) {
            if(pSpectCh[j].weight < pSpectCh[minWeightIndex].weight) {
                minWeightIndex = j; 
            }
        }
        if(minWeightIndex != i) {
            vos_mem_copy(&temp, &pSpectCh[minWeightIndex], sizeof(*pSpectCh));
            vos_mem_copy(&pSpectCh[minWeightIndex], &pSpectCh[i], sizeof(*pSpectCh));
            vos_mem_copy(&pSpectCh[i], &temp, sizeof(*pSpectCh));
        }
    }
#endif

    /* For testing */
    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, Sorted Spectrum Channels Weight", __func__);
    pSpectCh = pSpectInfoParams->pSpectCh;
    for (j = 0; j < (pSpectInfoParams->numSpectChans); j++) {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, Channel=%d Weight= %d rssi=%d bssCount=%d",
                    __func__, pSpectCh->chNum, pSpectCh->weight, pSpectCh->rssiAgr, pSpectCh->bssCount);
        pSpectCh++;
    }

}

/*==========================================================================
  FUNCTION    sapSelectChannel

  DESCRIPTION 
    Runs a algorithm to select the best channel to operate in based on BSS 
    rssi and bss count on each channel
    
  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    halHandle       : Pointer to HAL handle
    pResult         : Pointer to tScanResultHandle
   
  RETURN VALUE
    v_U8_t          : Success - channel number, Fail - zero
  
  SIDE EFFECTS 
============================================================================*/
v_U8_t sapSelectChannel(tHalHandle halHandle, ptSapContext pSapCtx,  tScanResultHandle pScanResult)
{
    // DFS param object holding all the data req by the algo
    tSapChSelSpectInfo oSpectInfoParams = {NULL,0}; 
    tSapChSelSpectInfo *pSpectInfoParams = &oSpectInfoParams; // Memory? NB    
    v_U8_t bestChNum = 0;
#ifdef SOFTAP_CHANNEL_RANGE
    v_U32_t startChannelNum;
    v_U32_t endChannelNum;
    v_U32_t operatingBand = 0;
    v_U8_t  count = 0;
#endif    
    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, Running SAP Ch Select", __func__);

    // Set to zero tSapChSelParams
    //vos_mem_zero(&sapChSelParams, sizeof(sapChSelParams));

    // Initialize the structure pointed by pSpectInfoParams
    if(sapChanSelInit( halHandle, pSpectInfoParams) != eSAP_TRUE ) {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR, "In %s, Ch Select initialization failed", __func__);
        return SAP_CHANNEL_NOT_SELECTED;
    }

    // Compute the weight of the entire spectrum in the operating band
    sapComputeSpectWeight( pSpectInfoParams, halHandle, pScanResult);

    // Sort the 20M channel list as per the computed weights, lesser weight first.
    sapSortChlWeight(pSpectInfoParams);

#ifdef SOFTAP_CHANNEL_RANGE
    ccmCfgGetInt( halHandle, WNI_CFG_SAP_CHANNEL_SELECT_START_CHANNEL, &startChannelNum);
    ccmCfgGetInt( halHandle, WNI_CFG_SAP_CHANNEL_SELECT_END_CHANNEL, &endChannelNum);
    ccmCfgGetInt( halHandle, WNI_CFG_SAP_CHANNEL_SELECT_OPERATING_BAND, &operatingBand);

    /*Loop till get the best channel in the given range */
    for(count=0; count < pSpectInfoParams->numSpectChans ; count++)
    {
        if((startChannelNum <= pSpectInfoParams->pSpectCh[count].chNum)&&
          ( endChannelNum >= pSpectInfoParams->pSpectCh[count].chNum))
        {
            if(bestChNum == 0)
            {
                bestChNum = (v_U8_t)pSpectInfoParams->pSpectCh[count].chNum;
            }
            else
            {
                if(operatingBand == RF_SUBBAND_2_4_GHZ)
                {
                    /* Give preference to Non-overlap channels */
                    if(((pSpectInfoParams->pSpectCh[count].chNum == CHANNEL_1) ||
                      (pSpectInfoParams->pSpectCh[count].chNum == CHANNEL_6) ||
                      (pSpectInfoParams->pSpectCh[count].chNum == CHANNEL_11))&&
                      (pSpectInfoParams->pSpectCh[count].weight == 0))
                      {
                           bestChNum = (v_U8_t)pSpectInfoParams->pSpectCh[count].chNum;
                           break;
                      }
                }
            }
         }
      }
#else
    // Get the first channel in sorted array as best 20M Channel
    bestChNum = (v_U8_t)pSpectInfoParams->pSpectCh[0].chNum;
#endif

    //Select Best Channel from Channel List if Configured
    bestChNum = sapSelectPreferredChannelFromChannelList(bestChNum, pSapCtx, pSpectInfoParams);

    // Free all the allocated memory
    sapChanSelExit(pSpectInfoParams);

    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, Running SAP Ch select Completed, Ch=%d",
                        __func__, bestChNum);
    if (bestChNum > 0 && bestChNum <= 252)
        return bestChNum;
    else
        return SAP_CHANNEL_NOT_SELECTED;
}

