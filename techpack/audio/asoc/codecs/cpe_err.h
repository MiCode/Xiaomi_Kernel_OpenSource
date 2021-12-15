/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015, 2017, The Linux Foundation. All rights reserved.
 */

#ifndef __CPE_ERR__
#define __CPE_ERR__

#include <linux/errno.h>

/* ERROR CODES */
/* Success. The operation completed with no errors. */
#define CPE_EOK          0x00000000
/* General failure. */
#define CPE_EFAILED      0x00000001
/* Bad operation parameter. */
#define CPE_EBADPARAM    0x00000002
/* Unsupported routine or operation. */
#define CPE_EUNSUPPORTED 0x00000003
/* Unsupported version. */
#define CPE_EVERSION     0x00000004
/* Unexpected problem encountered. */
#define CPE_EUNEXPECTED  0x00000005
/* Unhandled problem occurred. */
#define CPE_EPANIC       0x00000006
/* Unable to allocate resource. */
#define CPE_ENORESOURCE  0x00000007
/* Invalid handle. */
#define CPE_EHANDLE      0x00000008
/* Operation is already processed. */
#define CPE_EALREADY     0x00000009
/* Operation is not ready to be processed. */
#define CPE_ENOTREADY    0x0000000A
/* Operation is pending completion. */
#define CPE_EPENDING     0x0000000B
/* Operation could not be accepted or processed. */
#define CPE_EBUSY        0x0000000C
/* Operation aborted due to an error. */
#define CPE_EABORTED     0x0000000D
/* Operation preempted by a higher priority. */
#define CPE_EPREEMPTED   0x0000000E
/* Operation requests intervention to complete. */
#define CPE_ECONTINUE    0x0000000F
/* Operation requests immediate intervention to complete. */
#define CPE_EIMMEDIATE   0x00000010
/* Operation is not implemented. */
#define CPE_ENOTIMPL     0x00000011
/* Operation needs more data or resources. */
#define CPE_ENEEDMORE    0x00000012
/* Operation does not have memory. */
#define CPE_ENOMEMORY    0x00000014
/* Item does not exist. */
#define CPE_ENOTEXIST    0x00000015
/* Operation is finished. */
#define CPE_ETERMINATED  0x00000016
/* Max count for adsp error code sent to HLOS*/
#define CPE_ERR_MAX      (CPE_ETERMINATED + 1)


/* ERROR STRING */
/* Success. The operation completed with no errors. */
#define CPE_EOK_STR          "CPE_EOK"
/* General failure. */
#define CPE_EFAILED_STR      "CPE_EFAILED"
/* Bad operation parameter. */
#define CPE_EBADPARAM_STR    "CPE_EBADPARAM"
/* Unsupported routine or operation. */
#define CPE_EUNSUPPORTED_STR "CPE_EUNSUPPORTED"
/* Unsupported version. */
#define CPE_EVERSION_STR     "CPE_EVERSION"
/* Unexpected problem encountered. */
#define CPE_EUNEXPECTED_STR  "CPE_EUNEXPECTED"
/* Unhandled problem occurred. */
#define CPE_EPANIC_STR       "CPE_EPANIC"
/* Unable to allocate resource. */
#define CPE_ENORESOURCE_STR  "CPE_ENORESOURCE"
/* Invalid handle. */
#define CPE_EHANDLE_STR      "CPE_EHANDLE"
/* Operation is already processed. */
#define CPE_EALREADY_STR     "CPE_EALREADY"
/* Operation is not ready to be processed. */
#define CPE_ENOTREADY_STR    "CPE_ENOTREADY"
/* Operation is pending completion. */
#define CPE_EPENDING_STR     "CPE_EPENDING"
/* Operation could not be accepted or processed. */
#define CPE_EBUSY_STR        "CPE_EBUSY"
/* Operation aborted due to an error. */
#define CPE_EABORTED_STR     "CPE_EABORTED"
/* Operation preempted by a higher priority. */
#define CPE_EPREEMPTED_STR   "CPE_EPREEMPTED"
/* Operation requests intervention to complete. */
#define CPE_ECONTINUE_STR    "CPE_ECONTINUE"
/* Operation requests immediate intervention to complete. */
#define CPE_EIMMEDIATE_STR   "CPE_EIMMEDIATE"
/* Operation is not implemented. */
#define CPE_ENOTIMPL_STR     "CPE_ENOTIMPL"
/* Operation needs more data or resources. */
#define CPE_ENEEDMORE_STR    "CPE_ENEEDMORE"
/* Operation does not have memory. */
#define CPE_ENOMEMORY_STR    "CPE_ENOMEMORY"
/* Item does not exist. */
#define CPE_ENOTEXIST_STR    "CPE_ENOTEXIST"
/* Operation is finished. */
#define CPE_ETERMINATED_STR  "CPE_ETERMINATED"
/* Unexpected error code. */
#define CPE_ERR_MAX_STR      "CPE_ERR_MAX"


struct cpe_err_code {
	int     lnx_err_code;
	char    *cpe_err_str;
};


static struct cpe_err_code cpe_err_code_info[CPE_ERR_MAX+1] = {
	{ 0, CPE_EOK_STR},
	{ -ENOTRECOVERABLE, CPE_EFAILED_STR},
	{ -EINVAL, CPE_EBADPARAM_STR},
	{ -EOPNOTSUPP, CPE_EUNSUPPORTED_STR},
	{ -ENOPROTOOPT, CPE_EVERSION_STR},
	{ -ENOTRECOVERABLE, CPE_EUNEXPECTED_STR},
	{ -ENOTRECOVERABLE, CPE_EPANIC_STR},
	{ -ENOSPC, CPE_ENORESOURCE_STR},
	{ -EBADR, CPE_EHANDLE_STR},
	{ -EALREADY, CPE_EALREADY_STR},
	{ -EPERM, CPE_ENOTREADY_STR},
	{ -EINPROGRESS, CPE_EPENDING_STR},
	{ -EBUSY, CPE_EBUSY_STR},
	{ -ECANCELED, CPE_EABORTED_STR},
	{ -EAGAIN, CPE_EPREEMPTED_STR},
	{ -EAGAIN, CPE_ECONTINUE_STR},
	{ -EAGAIN, CPE_EIMMEDIATE_STR},
	{ -EAGAIN, CPE_ENOTIMPL_STR},
	{ -ENODATA, CPE_ENEEDMORE_STR},
	{ -EADV, CPE_ERR_MAX_STR},
	{ -ENOMEM, CPE_ENOMEMORY_STR},
	{ -ENODEV, CPE_ENOTEXIST_STR},
	{ -EADV, CPE_ETERMINATED_STR},
	{ -EADV, CPE_ERR_MAX_STR},
};

static inline int cpe_err_get_lnx_err_code(u32 cpe_error)
{
	if (cpe_error > CPE_ERR_MAX)
		return cpe_err_code_info[CPE_ERR_MAX].lnx_err_code;
	else
		return cpe_err_code_info[cpe_error].lnx_err_code;
}

static inline char *cpe_err_get_err_str(u32 cpe_error)
{
	if (cpe_error > CPE_ERR_MAX)
		return cpe_err_code_info[CPE_ERR_MAX].cpe_err_str;
	else
		return cpe_err_code_info[cpe_error].cpe_err_str;
}

#endif
