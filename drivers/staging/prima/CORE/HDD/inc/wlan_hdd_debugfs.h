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

#ifndef _WLAN_HDD_DEBUGFS_H
#define _WLAN_HDD_DEBUGFS_H

#ifdef WLAN_OPEN_SOURCE
VOS_STATUS hdd_debugfs_init(hdd_adapter_t *pAdapter);
void hdd_debugfs_exit(hdd_context_t *pHddCtx);
#else
inline VOS_STATUS hdd_debugfs_init(hdd_adapter_t *pAdapter)
{
    return VOS_STATUS_SUCCESS;
}
inline void hdd_debugfs_exit(hdd_context_t *pHddCtx)
{
}
#endif /* #ifdef WLAN_OPEN_SOURCE */
#endif /* #ifndef _WLAN_HDD_DEBUGFS_H */

