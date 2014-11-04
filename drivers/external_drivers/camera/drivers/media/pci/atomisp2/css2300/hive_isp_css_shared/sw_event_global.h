/****************************************************************
 *
 * Time   : 2012-09-12, 19:22.
 * Author : zhengjie.lu@intel.com
 * Comment:
 * - Define the software event IDs.
 *
 * Time   : 2012-09-06, 11:16.
 * Author : zhengjie.lu@intel.com
 * Comment:
 * - Initial version.
 *
 ****************************************************************/

#ifndef __SW_EVENT_GLOBAL_H_INCLUDED__
#define __SW_EVENT_GLOBAL_H_INCLUDED__

#define MAX_NR_OF_PAYLOADS_PER_SW_EVENT 4

#define SP_SW_EVENT_ID_0	0	/* for the error		*/
#define SP_SW_EVENT_ID_1	1	/* for the host2sp_buffer_queue */
#define SP_SW_EVENT_ID_2	2	/* for the sp2host_buffer_queue */
#define SP_SW_EVENT_ID_3	3	/* for the sp2host_event_queue  */

#endif /* __SW_EVENT_GLOBAL_H_INCLUDED__ */

