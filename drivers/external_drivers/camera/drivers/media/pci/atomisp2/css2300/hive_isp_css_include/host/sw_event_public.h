/****************************************************************
 *
 * Time   : 2012-09-06, 11:16.
 * Author : zhengjie.lu@intel.com
 * Comment:
 * - Initial version.
 *
 ****************************************************************/

#ifndef __SW_EVENT_PUBLIC_H_INCLUDED__
#define __SW_EVENT_PUBLIC_H_INCLUDED__

#include <stdbool.h>
#include "system_types.h"

/**
 * @brief Encode the information into the software-event.
 * Encode a certain amount of information into a signel software-event.
 *
 * @param[in]	in	The inputs of the encoder.
 * @param[in]	nr	The number of inputs.
 * @param[out]	out	The output of the encoder.
 *
 * @return true if it is successfull.
 */
STORAGE_CLASS_SW_EVENT_H bool encode_sw_event(
	uint32_t	*in,
	uint32_t	nr,
	uint32_t	*out);
#endif /* __SW_EVENT_PUBLIC_H_INCLUDED__ */

