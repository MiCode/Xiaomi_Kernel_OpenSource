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

/**=========================================================================

  \file  limSessionUtils.c
  \brief implementation for lim Session Utility  APIs
  \author Sunit Bhatia
  ========================================================================*/


/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include "aniGlobal.h"
#include "limDebug.h"
#include "limSession.h"
#include "limSessionUtils.h"
#include "limUtils.h"

/*--------------------------------------------------------------------------
  \brief peGetVhtCapable() - Returns the Vht capable from a valid session.

  This function itrates the session Table and returns the VHT capable from first valid session
   if no sessions are valid/present  it returns FALSE
    
  \param pMac                   - pointer to global adapter context
  \return                           - channel to scan from valid session else zero.
  
  \sa
  
  --------------------------------------------------------------------------*/
tANI_U8 peGetVhtCapable(tpAniSirGlobal pMac)

{
#ifdef WLAN_FEATURE_11AC
    tANI_U8 i;
    //assumption here is that all the sessions will be on the same channel.
    //This function will not work, once we have multiple channel support.
    for(i =0; i < pMac->lim.maxBssId; i++)
    {
        if(pMac->lim.gpSession[i].valid)
        {
            return(pMac->lim.gpSession[i].vhtCapability);  
        }
    }
#endif
    return FALSE;
}
/*--------------------------------------------------------------------------
  \brief peGetCurrentChannel() - Returns the  channel number for scanning, 
                                from a valid session.
   This function itrates the session Table and returns the channel number 
   from first valid session if no sessions are valid/present  it returns zero

  \param pMac                   - pointer to global adapter context
  \return                       - channel to scan from valid session else zero.
  \sa
  --------------------------------------------------------------------------*/
tANI_U8 peGetCurrentChannel(tpAniSirGlobal pMac)
{
    tANI_U8 i;
    //assumption here is that all the sessions will be on the same channel.
    //This function will not work, once we have multiple channel support.
    for(i =0; i < pMac->lim.maxBssId; i++)
    {
        if(pMac->lim.gpSession[i].valid)
        {
            return(pMac->lim.gpSession[i].currentOperChannel);
        }
    }
    return(HAL_INVALID_CHANNEL_ID);
}


/*--------------------------------------------------------------------------

  \brief peValidateJoinReq() - validates the Join request .

  This function is called to validate the Join Request for a BT-AMP station. If start BSS session is present
  this function returns TRUE else returns FALSE.
  PE will force SME to first issue ''START_BSS' request for BTAMP_STA, before sending a JOIN request.
    
  \param pMac                   - pointer to global adapter context
  \return                            - return TRUE if start BSS session is present else return FALSE.
  
  \sa
  --------------------------------------------------------------------------*/

tANI_U8 peValidateBtJoinRequest(tpAniSirGlobal pMac)
{

    tANI_U8 i;
    for(i =0; i < pMac->lim.maxBssId; i++)
    {
        if( (pMac->lim.gpSession[i].valid) && 
            (pMac->lim.gpSession[i].bssType == eSIR_BTAMP_STA_MODE) &&
            (pMac->lim.gpSession[i].statypeForBss == STA_ENTRY_SELF))
        {
            return(TRUE); 
        }

    }
    return(FALSE);

}

/*--------------------------------------------------------------------------
  \brief peGetValidPowerSaveSession() - Fetches the valid session for powersave .

  This function is called to check the valid session for power save, if more than one session is active , this function 
  it returns NULL.
  if there is only one valid "infrastructure" session present in "linkestablished" state this function returns sessionentry.
  For all other cases it returns NULL.
    
  \param pMac                   - pointer to global adapter context
  \return                            - return session is address if valid session is  present else return NULL.
  
  \sa
  --------------------------------------------------------------------------*/


tpPESession peGetValidPowerSaveSession(tpAniSirGlobal pMac)
{
    tANI_U8 i;
    tANI_U8 sessioncount = 0;
    tANI_U8 sessionId = 0;

    for(i = 0; i < pMac->lim.maxBssId; i++)
    {
        if( (pMac->lim.gpSession[i].valid == TRUE)&&
            (pMac->lim.gpSession[i].limSystemRole == eLIM_STA_ROLE)&&
            (pMac->lim.gpSession[i].limMlmState == eLIM_MLM_LINK_ESTABLISHED_STATE)) {
            sessioncount++;
            sessionId = i;

            if(sessioncount > 1)
            {
                return(NULL);
            }
        }

    }

    if( (pMac->lim.gpSession[sessionId].valid == TRUE)&&
        (pMac->lim.gpSession[sessionId].limSystemRole == eLIM_STA_ROLE)&&
        (pMac->lim.gpSession[sessionId].limMlmState == eLIM_MLM_LINK_ESTABLISHED_STATE))

    {
       return(&pMac->lim.gpSession[sessionId]);
    }
    return(NULL);
    
}
/*--------------------------------------------------------------------------
  \brief peIsAnySessionActive() - checks for the active session presence .

  This function returns TRUE if atleast one valid session is present else it returns FALSE
      
  \param pMac                   - pointer to global adapter context
  \return                            - return TRUE if atleast one session is active else return FALSE.
  
  \sa
  --------------------------------------------------------------------------*/


tANI_U8 peIsAnySessionActive(tpAniSirGlobal pMac)
{
    tANI_U8 i;
    for(i =0; i < pMac->lim.maxBssId; i++)
    {
        if(pMac->lim.gpSession[i].valid == TRUE) 
        {
            return(TRUE);
        }

    }
    return(FALSE);

}

/*--------------------------------------------------------------------------
  \brief pePrintActiveSession() - print all the active pesession present .

  This function print all the active pesession present

  \param pMac                   - pointer to global adapter context

  \sa
  --------------------------------------------------------------------------*/


void pePrintActiveSession(tpAniSirGlobal pMac)
{
    tANI_U8 i;
    for(i =0; i < pMac->lim.maxBssId; i++)
    {
        if(pMac->lim.gpSession[i].valid == TRUE)
        {
            limLog(pMac, LOGE, FL("Active sessionId: %d BSID: "MAC_ADDRESS_STR
                   "opmode = %d bssIdx = %d"), i,
                   MAC_ADDR_ARRAY(pMac->lim.gpSession[i].bssId),
                   pMac->lim.gpSession[i].operMode,
                   pMac->lim.gpSession[i].bssIdx);
        }
    }
    return;
}

/*--------------------------------------------------------------------------
  \brief isLimSessionOffChannel() - Determines if the there is any other off channel 
                                    session.

  This function returns TRUE if the session Id passed needs to be on a different
  channel than atleast one session already active.
    
  \param pMac                   - pointer to global adapter context
  \param sessionId              - session ID of the session to be verified.  
  
  \return tANI_U8               - Boolean value for off-channel operation.
  
  \sa
  --------------------------------------------------------------------------*/

tANI_U8
isLimSessionOffChannel(tpAniSirGlobal pMac, tANI_U8 sessionId)
{
    tANI_U8 i;

    if(sessionId >=  pMac->lim.maxBssId)
    {
        limLog(pMac, LOGE, FL("Invalid sessionId: %d \n "), sessionId);
        return FALSE;
    }

    for(i =0; i < pMac->lim.maxBssId; i++)
    {
        if( i == sessionId )
        {
          //Skip the sessionId that is to be joined.
          continue;
        }
        //if another ession is valid and it is on different channel
        //it is an off channel operation.
        if( (pMac->lim.gpSession[i].valid) && 
            (pMac->lim.gpSession[i].currentOperChannel != 
             pMac->lim.gpSession[sessionId].currentOperChannel) )
        {
            return TRUE;
        }
    }

    return FALSE;

}

/*--------------------------------------------------------------------------
  \brief peGetActiveSessionChannel() - Gets the operating channel of first  
                                    valid session. Returns 0 if there is no
                                    valid session.

  \param pMac                   - pointer to global adapter context
  
  \return tANI_U8               - operating channel.
  
  \sa
  --------------------------------------------------------------------------*/
void
peGetActiveSessionChannel (tpAniSirGlobal pMac, tANI_U8* resumeChannel, ePhyChanBondState* resumePhyCbState)
{
    tANI_U8 i;
    ePhyChanBondState prevPhyCbState = PHY_SINGLE_CHANNEL_CENTERED;

    // Initialize the pointers passed to INVALID values in case we don't find a valid session
    *resumeChannel = 0;
    *resumePhyCbState = 0;
    for(i =0; i < pMac->lim.maxBssId; i++)
    {
        if(pMac->lim.gpSession[i].valid)
        {
            *resumeChannel = pMac->lim.gpSession[i].currentOperChannel;
            *resumePhyCbState = pMac->lim.gpSession[i].htSecondaryChannelOffset;
            
#ifdef WLAN_FEATURE_11AC
            if ((pMac->lim.gpSession[i].vhtCapability))
            {
               /*Get 11ac cbState from 11n cbState*/
                *resumePhyCbState = limGet11ACPhyCBState(pMac, 
                                    pMac->lim.gpSession[i].currentOperChannel,
                                    pMac->lim.gpSession[i].htSecondaryChannelOffset,
                                    pMac->lim.gpSession[i].apCenterChan,
                                    &pMac->lim.gpSession[i]);
            }
#endif
            *resumePhyCbState = (*resumePhyCbState > prevPhyCbState )? *resumePhyCbState : prevPhyCbState;
            prevPhyCbState = *resumePhyCbState;
        }
    }
    return;
}

/*--------------------------------------------------------------------------
  \brief limIsChanSwitchRunning() - Check if channel switch is running on any  
                                    valid session.

  \param pMac                   - pointer to global adapter context
  
  \return tANI_U8               - 1 - if chann switching running.
                                  0 - if chann switching is not running. 
  
  \sa
  --------------------------------------------------------------------------*/
tANI_U8
limIsChanSwitchRunning (tpAniSirGlobal pMac)
{
    tANI_U8 i;

    for(i =0; i < pMac->lim.maxBssId; i++)
    {
        if(pMac->lim.gpSession[i].valid && 
            pMac->lim.gpSession[i].gLimSpecMgmt.dot11hChanSwState == eLIM_11H_CHANSW_RUNNING)
        {
            return 1;
        }
    }
    return 0;
}
/*--------------------------------------------------------------------------
  \brief limIsInQuietDuration() - Check if channel quieting is running on any  
                                    valid session.

  \param pMac                   - pointer to global adapter context
  
  \return tANI_U8               - 1 - if chann quiet running.
                                  0 - if chann quiet is not running. 
  
  \sa
  --------------------------------------------------------------------------*/
tANI_U8
limIsInQuietDuration (tpAniSirGlobal pMac)
{
    tANI_U8 i;

    for(i =0; i < pMac->lim.maxBssId; i++)
    {
        if(pMac->lim.gpSession[i].valid && 
            pMac->lim.gpSession[i].gLimSpecMgmt.quietState == eLIM_QUIET_RUNNING)
        {
            return 1;
        }
    }
    return 0;
}
/*--------------------------------------------------------------------------
  \brief limIsQuietBegin() - Check if channel quieting is begining on any  
                                    valid session.

  \param pMac                   - pointer to global adapter context
  
  \return tANI_U8               - 1 - if chann quiet running.
                                  0 - if chann quiet is not running. 
  
  \sa
  --------------------------------------------------------------------------*/
tANI_U8
limIsQuietBegin (tpAniSirGlobal pMac)
{
    tANI_U8 i;

    for(i =0; i < pMac->lim.maxBssId; i++)
    {
        if(pMac->lim.gpSession[i].valid && 
            pMac->lim.gpSession[i].gLimSpecMgmt.quietState == eLIM_QUIET_BEGIN)
        {
            return 1;
        }
    }
    return 0;
}

/*--------------------------------------------------------------------------
  \brief limIsInMCC() - Check if Device is in MCC.

  \param pMac                   - pointer to global adapter context
  
  \return tANI_U8               - TRUE - if in MCC.
                                  FALSE - NOT in MCC. 
  
  \sa
  --------------------------------------------------------------------------*/
tANI_U8
limIsInMCC (tpAniSirGlobal pMac)
{
    tANI_U8 i;
    tANI_U8 chan = 0;

    for(i = 0; i < pMac->lim.maxBssId; i++)
    {
        //if another session is valid and it is on different channel
        //it is an off channel operation.
        if( (pMac->lim.gpSession[i].valid) )
        { 
            if( chan == 0 )
            {
                chan = pMac->lim.gpSession[i].currentOperChannel;
            } 
            else if( chan != pMac->lim.gpSession[i].currentOperChannel)
            {
                return TRUE; 
            }        
        }
    }
    return FALSE;
}

/*--------------------------------------------------------------------------
  \brief peGetCurrentSTAsCount() - Returns total stations associated on 
                                      all session.

  \param pMac                   - pointer to global adapter context
  \return                       - Number of station active on all sessions.
  
  \sa
  --------------------------------------------------------------------------*/

tANI_U8 peGetCurrentSTAsCount(tpAniSirGlobal pMac)
{
    tANI_U8 i;
    tANI_U8 staCount = 0;
    for(i =0; i < pMac->lim.maxBssId; i++)
    {
        if(pMac->lim.gpSession[i].valid == TRUE) 
        {
           staCount += pMac->lim.gpSession[i].gLimNumOfCurrentSTAs;
        }
    }
    return staCount;
}

#ifdef FEATURE_WLAN_LFR
/*--------------------------------------------------------------------------
  \brief limIsFastRoamEnabled() - Check LFR is enabled or not

  This function returns the TRUE if LFR is enabled

  \param pMac        - pointer to global adapter context
  \param sessionId   - session ID is returned here, if session is found.

  \return int        - TRUE if enabled or else FALSE

  \sa
  --------------------------------------------------------------------------*/

tANI_U8 limIsFastRoamEnabled(tpAniSirGlobal pMac, tANI_U8 sessionId)
{
    if(TRUE == pMac->lim.gpSession[sessionId].valid)
    {
        if((eSIR_INFRASTRUCTURE_MODE == pMac->lim.gpSession[sessionId].bssType) &&
           (pMac->lim.gpSession[sessionId].isFastRoamIniFeatureEnabled))
        {
            return TRUE;
        }
    }

    return FALSE;
}
#endif

