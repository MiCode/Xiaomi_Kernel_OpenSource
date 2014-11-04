/****************************************************************
 *
 * Time: 2012-09-12, 14:23.
 * Author: zhengjie.lu@intel.com
 * Comment:
 * - Extend the bit number per information. In the previous
 *   version, the bit number per information can be 32, 16,
 *   or 8. In this version, it is determined by the formula
 *   "32-bit / number of information".
 *
 * Time   : 2012-09-06, 11:16.
 * Author : zhengjie.lu@intel.com
 * Comment:
 * - Initial version.
 *
 ****************************************************************/

#include "sw_event.h"

#include <stdbool.h>		/* bool */
#include <stddef.h>		/* NULL */

#include "assert_support.h"	/* OP___assert() */

/****************************************
 *
 * Local declarations.
 *
 ****************************************/
/* end of local declarations */

#ifndef __INLINE_SW_EVENT__
#include "sw_event_private.h"
#endif /* __INLINE_SW_EVENT__ */

/**
 * @brief Encode the information into the software-event.
 * Refer to "sw_event_public.h" for details.
 */
STORAGE_CLASS_SW_EVENT_C bool
encode_sw_event(
	uint32_t	*in,
	uint32_t	nr,
	uint32_t	*out)
{
	bool ret;
	uint32_t nr_of_bits;
	uint32_t i;

OP___assert (nr > 0 && nr <= MAX_NR_OF_PAYLOADS_PER_SW_EVENT);

	/* initialize the output */
	*out = 0;
	
	/* get the number of bits per information */
	nr_of_bits = sizeof(uint32_t) * 8 / nr;

	/* compress the all inputs into a signle output */
	for (i = 0; i < nr; i++) {
		*out <<= nr_of_bits;
		*out |= in[i];
	}

	/* get the return value */
	ret = (nr > 0 && nr <= MAX_NR_OF_PAYLOADS_PER_SW_EVENT);

	return ret;
}

