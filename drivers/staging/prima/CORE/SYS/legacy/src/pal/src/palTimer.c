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

/** 
    Copyright (C) 2006 Airgo Networks, Incorporated

    This file contains function implementations for the Platform
    Abstration Layer.

 */

#include <halTypes.h>
#include <palTimer.h>
#include <vos_timer.h>
#include <vos_memory.h>

#ifndef FEATURE_WLAN_PAL_TIMER_DISABLE
typedef struct sPalTimer
{   
    palTimerCallback timerCallback;
    void *pContext;
    tHddHandle hHdd;         // not really needed when mapping to vos timers   
    tANI_U32 uTimerInterval; //meaningful only is fRestart is true
    tANI_BOOLEAN fRestart;
    
    vos_timer_t vosTimer;
    
} tPalTimer, *tpPalTimer;



v_VOID_t internalTimerCallback( v_PVOID_t userData )
{
    tPalTimer *pPalTimer = (tPalTimer *)userData;
    
    if ( pPalTimer )
    {
        if ( pPalTimer->timerCallback )
        {
            pPalTimer->timerCallback( pPalTimer->pContext );
        }
        
        if ( pPalTimer->fRestart )
        {
            palTimerStart( pPalTimer->hHdd, pPalTimer, pPalTimer->uTimerInterval, eANI_BOOLEAN_TRUE );
        }
    }
}

#ifdef TIMER_MANAGER
eHalStatus palTimerAlloc_debug( tHddHandle hHdd, tPalTimerHandle *phPalTimer, 
                          palTimerCallback pCallback, void *pContext, char* fileName, v_U32_t lineNum  )
{
   eHalStatus halStatus = eHAL_STATUS_FAILURE;
   tPalTimer *pPalTimer = NULL;
   VOS_STATUS vosStatus;
    
   do
   {
      // allocate the internal timer structure.
      pPalTimer = vos_mem_malloc( sizeof( tPalTimer ) );
      if ( NULL == pPalTimer ) break;
       
      // initialize the vos Timer that underlies the pal Timer.
      vosStatus = vos_timer_init_debug( &pPalTimer->vosTimer, VOS_TIMER_TYPE_SW, 
                                   internalTimerCallback, pPalTimer, fileName, lineNum );
      if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
      {
         // if fail to init the vos timer, free the memory and bail out.
         vos_mem_free( pPalTimer );
         break;
      }
      
      // initialize the info in the internal palTimer struct so we can 
      pPalTimer->timerCallback = pCallback;
      pPalTimer->pContext      = pContext;
      pPalTimer->hHdd          = hHdd;
      
      // return a 'handle' to the caller.
      *phPalTimer = pPalTimer;
      
      halStatus = eHAL_STATUS_SUCCESS;
      
   } while( 0 );   
       
    return( halStatus );
}
#else
eHalStatus palTimerAlloc( tHddHandle hHdd, tPalTimerHandle *phPalTimer, 
                          palTimerCallback pCallback, void *pContext )
{
   eHalStatus halStatus = eHAL_STATUS_FAILURE;
   tPalTimer *pPalTimer = NULL;
   VOS_STATUS vosStatus;
    
   do
   {
      // allocate the internal timer structure.
      pPalTimer = vos_mem_malloc( sizeof( tPalTimer ) );
      if ( NULL == pPalTimer ) break;
       
      // initialize the vos Timer that underlies the pal Timer.
      vosStatus = vos_timer_init( &pPalTimer->vosTimer, VOS_TIMER_TYPE_SW, 
                                   internalTimerCallback, pPalTimer );
      if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
      {
         // if fail to init the vos timer, free the memory and bail out.
         vos_mem_free( pPalTimer );
         break;
      }
      
      // initialize the info in the internal palTimer struct so we can 
      pPalTimer->timerCallback = pCallback;
      pPalTimer->pContext      = pContext;
      pPalTimer->hHdd          = hHdd;
      
      // return a 'handle' to the caller.
      *phPalTimer = pPalTimer;
      
      halStatus = eHAL_STATUS_SUCCESS;
      
   } while( 0 );   
       
    return( halStatus );
}
#endif


eHalStatus palTimerFree( tHddHandle hHdd, tPalTimerHandle hPalTimer )
{
   eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;
   VOS_STATUS vosStatus;
   tPalTimer *pPalTimer = (tPalTimer *)hPalTimer;
    
   do
   {
      if ( NULL == pPalTimer ) break;
   
      // Destroy the vos timer...      
      vosStatus = vos_timer_destroy( &pPalTimer->vosTimer );
      if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) ) break;
      
      // Free the memory for the intrnal timer struct...
      vos_mem_free( pPalTimer );
      
      status = eHAL_STATUS_SUCCESS;
      
   } while( 0 );
    
   return( status );
}


eHalStatus palTimerStart(tHddHandle hHdd, tPalTimerHandle hPalTimer, tANI_U32 uExpireTime, tANI_BOOLEAN fRestart)
{
   eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;
   VOS_STATUS vosStatus;
   tANI_U32 expireTimeInMS = 0;
   
   tPalTimer *pPalTimer = (tPalTimer *)hPalTimer;
    
   do 
   {
      if ( NULL == pPalTimer ) break;
       
      pPalTimer->fRestart = fRestart;
      pPalTimer->uTimerInterval = uExpireTime;
      
      // vos Timer takes expiration time in milliseconds.  palTimerStart and 
      // the uTimerIntervl in tPalTimer struct have expiration tiem in
      // microseconds.  Make and adjustment from microseconds to milliseconds
      // before calling the vos_timer_start().
      expireTimeInMS = uExpireTime / 1000;
      vosStatus = vos_timer_start( &pPalTimer->vosTimer, expireTimeInMS );
      if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) ) 
      {
         status = eHAL_STATUS_FAILURE;
         break;
      }
      
      status = eHAL_STATUS_SUCCESS;   
      
   } while( 0 );
    
   return( status );
}


eHalStatus palTimerStop(tHddHandle hHdd, tPalTimerHandle hPalTimer)
{
   eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;
   
   tPalTimer *pPalTimer = (tPalTimer *)hPalTimer;
    
   do 
   {
      if ( NULL == pPalTimer ) break;

      vos_timer_stop( &pPalTimer->vosTimer );
     
      // make sure the timer is not re-started.
      pPalTimer->fRestart = eANI_BOOLEAN_FALSE;

      status = eHAL_STATUS_SUCCESS;

   } while( 0 );   
   
   return( status );
}   

#endif




