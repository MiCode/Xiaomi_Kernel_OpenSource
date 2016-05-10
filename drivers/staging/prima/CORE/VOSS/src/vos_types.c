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


/**=========================================================================
  
  \file  vos_Types.c
  
  \brief virtual Operating System Servies (vOS)
               
   Basic type definitions 
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include "vos_types.h"
#include "vos_trace.h"

//#include "wlan_libra_config.h"

/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
  
  \brief vos_atomic_set_U32() - set a U32 variable atomically 
  
  \param pTarget - pointer to the v_U32_t to set.
  
  \param value - the value to set in the v_U32_t variable.
  
  \return This function returns the value previously in the v_U32_t before
          the new value is set.
    
  \sa vos_atomic_increment_U32(), vos_atomic_decrement_U32()
  
  --------------------------------------------------------------------------*/                                                 
v_U32_t vos_atomic_set_U32( v_U32_t *pTarget, v_U32_t value )
{
  v_U32_t oldval;
  unsigned long flags;

  if (pTarget == NULL)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "NULL ptr passed into %s",__func__);
     return 0;
  }
  local_irq_save(flags);
  oldval = *pTarget;
  *pTarget = value;
  local_irq_restore(flags);
  //  v_U32_t prev = atomic_read(pTarget);
  //  atomic_set(pTarget, value);
  return oldval;
}


/*----------------------------------------------------------------------------
  
  \brief vos_atomic_increment_U32() - Increment a U32 variable atomically 
  
  \param pTarget - pointer to the v_U32_t to increment.
  
  \return This function returns the value of the variable after the 
          increment occurs.
    
  \sa vos_atomic_decrement_U32(), vos_atomic_set_U32()
  
  --------------------------------------------------------------------------*/                                                 
v_U32_t vos_atomic_increment_U32( v_U32_t *pTarget )
{
  unsigned long flags;
  if (pTarget == NULL)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "NULL ptr passed into %s",__func__);
     return 0;
  }
  local_irq_save(flags);
  ++*pTarget; 
  local_irq_restore(flags);
  return *pTarget;
  //  return atomic_inc_return(pTarget);
}


/*----------------------------------------------------------------------------
  
  \brief vos_atomic_decrement_U32() - Decrement a U32 variable atomically 
  
  \param pTarget - pointer to the v_U32_t to decrement.
  
  \return This function returns the value of the variable after the 
          decrement occurs.
    
  \sa vos_atomic_increment_U32(), vos_atomic_set_U32()
  
  --------------------------------------------------------------------------*/                                                 
v_U32_t vos_atomic_decrement_U32( v_U32_t *pTarget )
{ 
  unsigned long flags;
  if (pTarget == NULL)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "NULL ptr passed into %s",__func__);
     return 0;
  }
   // return atomic_dec_return(pTarget);
   local_irq_save(flags);
   --*pTarget; 
   local_irq_restore(flags);
   return (*pTarget);
}

v_U32_t vos_atomic_increment_U32_by_value( v_U32_t *pTarget, v_U32_t value )
{
   unsigned long flags;
   if (pTarget == NULL)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "NULL ptr passed into %s",__func__);
      return 0;
   }
   local_irq_save(flags);
   *pTarget += value ;
   local_irq_restore(flags);
   return (*pTarget);
}    

v_U32_t vos_atomic_decrement_U32_by_value( v_U32_t *pTarget, v_U32_t value )
{
   unsigned long flags;
   if (pTarget == NULL)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "NULL ptr passed into %s",__func__);
      return 0;
   }
   local_irq_save(flags);
   *pTarget -= value ;
   local_irq_restore(flags);
   return (*pTarget);

}


v_U32_t vos_get_skip_ssid_check(void)
{
/**This is needed by only AMSS for interoperatability **/

    return 1;
}    


v_U32_t vos_get_skip_11e_check(void)
{
    /* this is needed only for AMSS for interopratability **/
    return 1;
}    
