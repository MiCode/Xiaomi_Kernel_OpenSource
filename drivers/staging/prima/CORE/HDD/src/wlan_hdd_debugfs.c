/*
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
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

#ifdef WLAN_OPEN_SOURCE
#include <wlan_hdd_includes.h>
#include <wlan_hdd_wowl.h>
#include <vos_sched.h>

#define MAX_USER_COMMAND_SIZE_WOWL_ENABLE 8
#define MAX_USER_COMMAND_SIZE_WOWL_PATTERN 512
#define MAX_USER_COMMAND_SIZE_FRAME 4096

static ssize_t __wcnss_wowenable_write(struct file *file,
               const char __user *buf, size_t count, loff_t *ppos)
{
    hdd_adapter_t *pAdapter;
    hdd_context_t *pHddCtx;
    char cmd[MAX_USER_COMMAND_SIZE_WOWL_ENABLE + 1];
    char *sptr, *token;
    v_U8_t wow_enable = 0;
    v_U8_t wow_mp = 0;
    v_U8_t wow_pbm = 0;

    ENTER();

    pAdapter = (hdd_adapter_t *)file->private_data;
    if ((NULL == pAdapter) || (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                  "%s: Invalid adapter or adapter has invalid magic.",
                  __func__);
        return -EINVAL;
    }
    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    if (0 != wlan_hdd_validate_context(pHddCtx))
    {
        return -EINVAL;
    }

    if (!sme_IsFeatureSupportedByFW(WOW))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Wake-on-Wireless feature is not supported "
                  "in firmware!", __func__);

        return -EINVAL;
    }

    if (count > MAX_USER_COMMAND_SIZE_WOWL_ENABLE)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Command length is larger than %d bytes.",
                  __func__, MAX_USER_COMMAND_SIZE_WOWL_ENABLE);

        return -EINVAL;
    }

    /* Get command from user */
    if (copy_from_user(cmd, buf, count))
        return -EFAULT;
    cmd[count] = '\0';
    sptr = cmd;

    /* Get enable or disable wow */
    token = strsep(&sptr, " ");
    if (!token)
        return -EINVAL;
    if (kstrtou8(token, 0, &wow_enable))
        return -EINVAL;

    /* Disable wow */
    if (!wow_enable) {
        if (!hdd_exit_wowl(pAdapter, eWOWL_EXIT_USER))
        {
          VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                    "%s: hdd_exit_wowl failed!", __func__);

          return -EFAULT;
        }

        return count;
    }

    /* Get enable or disable magic packet mode */
    token = strsep(&sptr, " ");
    if (!token)
        return -EINVAL;
    if (kstrtou8(token, 0, &wow_mp))
        return -EINVAL;
    if (wow_mp > 1)
        wow_mp = 1;

    /* Get enable or disable pattern byte matching mode */
    token = strsep(&sptr, " ");
    if (!token)
        return -EINVAL;
    if (kstrtou8(token, 0, &wow_pbm))
        return -EINVAL;
    if (wow_pbm > 1)
        wow_pbm = 1;

    if (!hdd_enter_wowl(pAdapter, wow_mp, wow_pbm))
    {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: hdd_enter_wowl failed!", __func__);

      return -EFAULT;
    }
    EXIT();
    return count;
}

static ssize_t wcnss_wowenable_write(struct file *file,
               const char __user *buf, size_t count, loff_t *ppos)
{
    ssize_t ret;

    vos_ssr_protect(__func__);
    ret = __wcnss_wowenable_write(file, buf, count, ppos);
    vos_ssr_unprotect(__func__);

    return ret;
}

static ssize_t __wcnss_wowpattern_write(struct file *file,
               const char __user *buf, size_t count, loff_t *ppos)
{
    hdd_adapter_t *pAdapter;
    hdd_context_t *pHddCtx;
    char cmd[MAX_USER_COMMAND_SIZE_WOWL_PATTERN + 1];
    char *sptr, *token;
    v_U8_t pattern_idx = 0;
    v_U8_t pattern_offset = 0;
    char *pattern_buf;
    char *pattern_mask;

    ENTER();

    pAdapter = (hdd_adapter_t *)file->private_data;
    if ((NULL == pAdapter) || (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s: Invalid adapter or adapter has invalid magic.",
                   __func__);

        return -EINVAL;
    }
    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    if (0 != wlan_hdd_validate_context(pHddCtx))
    {
        return -EINVAL;
    }
    if (!sme_IsFeatureSupportedByFW(WOW))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: Wake-on-Wireless feature is not supported "
                   "in firmware!", __func__);

        return -EINVAL;
    }

    if (count > MAX_USER_COMMAND_SIZE_WOWL_PATTERN)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: Command length is larger than %d bytes.",
                   __func__, MAX_USER_COMMAND_SIZE_WOWL_PATTERN);

        return -EINVAL;
    }

    /* Get command from user */
    if (copy_from_user(cmd, buf, count))
        return -EFAULT;
    cmd[count] = '\0';
    sptr = cmd;

    /* Get pattern idx */
    token = strsep(&sptr, " ");
    if (!token)
        return -EINVAL;

    if (kstrtou8(token, 0, &pattern_idx))
        return -EINVAL;

    /* Get pattern offset */
    token = strsep(&sptr, " ");

    /* Delete pattern if no further argument */
    if (!token) {
        hdd_del_wowl_ptrn_debugfs(pAdapter, pattern_idx);

        return count;
    }

    if (kstrtou8(token, 0, &pattern_offset))
        return -EINVAL;

    /* Get pattern */
    token = strsep(&sptr, " ");
    if (!token)
        return -EINVAL;

    pattern_buf = token;

    /* Get pattern mask */
    token = strsep(&sptr, " ");
    if (!token)
        return -EINVAL;

    pattern_mask = token;
    pattern_mask[strlen(pattern_mask) - 1] = '\0';

    hdd_add_wowl_ptrn_debugfs(pAdapter, pattern_idx, pattern_offset,
                              pattern_buf, pattern_mask);
    EXIT();
    return count;
}

static ssize_t wcnss_wowpattern_write(struct file *file,
               const char __user *buf, size_t count, loff_t *ppos)
{
    ssize_t ret;

    vos_ssr_protect(__func__);
    ret = __wcnss_wowpattern_write(file, buf, count, ppos);
    vos_ssr_unprotect(__func__);

    return ret;
}

static ssize_t __wcnss_patterngen_write(struct file *file,
               const char __user *buf, size_t count, loff_t *ppos)
{
    hdd_adapter_t *pAdapter;
    hdd_context_t *pHddCtx;
    tSirAddPeriodicTxPtrn *addPeriodicTxPtrnParams;
    tSirDelPeriodicTxPtrn *delPeriodicTxPtrnParams;
    char *cmd, *sptr, *token;
    v_U8_t pattern_idx = 0;
    v_U8_t pattern_duration = 0;
    char *pattern_buf;
    v_U16_t pattern_len = 0;
    v_U16_t i = 0;

    ENTER();

    pAdapter = (hdd_adapter_t *)file->private_data;
    if ((NULL == pAdapter) || (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s: Invalid adapter or adapter has invalid magic.",
                   __func__);

        return -EINVAL;
    }
    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    if (0 != wlan_hdd_validate_context(pHddCtx))
    {
        return -EINVAL;
    }

    if (!sme_IsFeatureSupportedByFW(WLAN_PERIODIC_TX_PTRN))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: Periodic Tx Pattern Offload feature is not supported "
                   "in firmware!", __func__);

        return -EINVAL;
    }

    /* Get command from user */
    if (count <= MAX_USER_COMMAND_SIZE_FRAME)
        cmd = vos_mem_malloc(count + 1);
    else
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: Command length is larger than %d bytes.",
                   __func__, MAX_USER_COMMAND_SIZE_FRAME);

        return -EINVAL;
    }

    if (!cmd)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Memory allocation for cmd failed!", __func__);

        return -EFAULT;
    }

    if (copy_from_user(cmd, buf, count))
    {
        vos_mem_free(cmd);
        return -EFAULT;
    }
    cmd[count] = '\0';
    sptr = cmd;

    /* Get pattern idx */
    token = strsep(&sptr, " ");
    if (!token)
        goto failure;
    if (kstrtou8(token, 0, &pattern_idx))
        goto failure;

    if (pattern_idx > (MAXNUM_PERIODIC_TX_PTRNS - 1))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: Pattern index %d is not in the range (0 ~ %d).",
                   __func__, pattern_idx, MAXNUM_PERIODIC_TX_PTRNS - 1);

        goto failure;
    }

    /* Get pattern duration */
    token = strsep(&sptr, " ");
    if (!token)
        goto failure;
    if (kstrtou8(token, 0, &pattern_duration))
        goto failure;

    /* Delete pattern using index if duration is 0 */
    if (!pattern_duration)
    {
        delPeriodicTxPtrnParams =
            vos_mem_malloc(sizeof(tSirDelPeriodicTxPtrn));
        if (!delPeriodicTxPtrnParams)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: Memory allocation for delPeriodicTxPtrnParams "
                      "failed!", __func__);

            vos_mem_free(cmd);
            return -EFAULT;
        }

        delPeriodicTxPtrnParams->ucPatternIdBitmap = 1 << pattern_idx;
        vos_mem_copy(delPeriodicTxPtrnParams->macAddress,
                     pAdapter->macAddressCurrent.bytes, 6);

        /* Delete pattern */
        if (eHAL_STATUS_SUCCESS != sme_DelPeriodicTxPtrn(pHddCtx->hHal,
                                                delPeriodicTxPtrnParams))
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                       "%s: sme_DelPeriodicTxPtrn() failed!", __func__);

            vos_mem_free(delPeriodicTxPtrnParams);
            goto failure;
        }

        vos_mem_free(delPeriodicTxPtrnParams);
        vos_mem_free(cmd);
        return count;
    }

    /* Check if it's in connected state only when adding patterns */
    if (!hdd_connIsConnected(WLAN_HDD_GET_STATION_CTX_PTR(pAdapter)))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: Not in Connected state!", __func__);

        goto failure;
    }

    /* Get pattern */
    token = strsep(&sptr, " ");
    if (!token)
        goto failure;

    pattern_buf = token;
    pattern_buf[strlen(pattern_buf) - 1] = '\0';
    pattern_len = strlen(pattern_buf);

    /* Since the pattern is a hex string, 2 characters represent 1 byte. */
    if (pattern_len % 2)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: Malformed pattern!", __func__);

        goto failure;
    }
    else
        pattern_len >>= 1;

    if (pattern_len < 14 || pattern_len > PERIODIC_TX_PTRN_MAX_SIZE)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: Not an 802.3 frame!", __func__);

        goto failure;
    }

    addPeriodicTxPtrnParams = vos_mem_malloc(sizeof(tSirAddPeriodicTxPtrn));
    if (!addPeriodicTxPtrnParams)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Memory allocation for addPeriodicTxPtrnParams "
                  "failed!", __func__);

        vos_mem_free(cmd);
        return -EFAULT;
    }

    addPeriodicTxPtrnParams->ucPtrnId = pattern_idx;
    addPeriodicTxPtrnParams->usPtrnIntervalMs = pattern_duration * 500;
    addPeriodicTxPtrnParams->ucPtrnSize = pattern_len;
    vos_mem_copy(addPeriodicTxPtrnParams->macAddress,
                 pAdapter->macAddressCurrent.bytes, 6);

    /* Extract the pattern */
    for(i = 0; i < addPeriodicTxPtrnParams->ucPtrnSize; i++)
    {
        addPeriodicTxPtrnParams->ucPattern[i] =
        (hdd_parse_hex(pattern_buf[0]) << 4) + hdd_parse_hex(pattern_buf[1]);

        /* Skip to next byte */
        pattern_buf += 2;
    }

    /* Add pattern */
    if (eHAL_STATUS_SUCCESS != sme_AddPeriodicTxPtrn(pHddCtx->hHal,
                                            addPeriodicTxPtrnParams))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: sme_AddPeriodicTxPtrn() failed!", __func__);

        vos_mem_free(addPeriodicTxPtrnParams);
        goto failure;
    }

    vos_mem_free(addPeriodicTxPtrnParams);
    vos_mem_free(cmd);
    EXIT();
    return count;

failure:
    vos_mem_free(cmd);
    return -EINVAL;
}

static ssize_t wcnss_patterngen_write(struct file *file,
               const char __user *buf, size_t count, loff_t *ppos)
{
    ssize_t ret;

    vos_ssr_protect(__func__);
    ret = __wcnss_patterngen_write(file, buf, count, ppos);
    vos_ssr_unprotect(__func__);

    return ret;

}

static int __wcnss_debugfs_open(struct inode *inode, struct file *file)
{
    hdd_adapter_t *pAdapter;
    hdd_context_t *pHddCtx;

    ENTER();

    pAdapter = (hdd_adapter_t *)file->private_data;
    if ((NULL == pAdapter) || (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s: Invalid adapter or adapter has invalid magic.",
                   __func__);
        return -EINVAL;
    }
    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    if (0 != wlan_hdd_validate_context(pHddCtx))
    {
        return -EINVAL;
    }

    if (inode->i_private)
    {
        file->private_data = inode->i_private;
    }
    EXIT();
    return 0;
}

static int wcnss_debugfs_open(struct inode *inode, struct file *file)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wcnss_debugfs_open(inode, file);
    vos_ssr_unprotect(__func__);

    return ret;
}

static const struct file_operations fops_wowenable = {
    .write = wcnss_wowenable_write,
    .open = wcnss_debugfs_open,
    .owner = THIS_MODULE,
    .llseek = default_llseek,
};

static const struct file_operations fops_wowpattern = {
    .write = wcnss_wowpattern_write,
    .open = wcnss_debugfs_open,
    .owner = THIS_MODULE,
    .llseek = default_llseek,
};

static const struct file_operations fops_patterngen = {
    .write = wcnss_patterngen_write,
    .open = wcnss_debugfs_open,
    .owner = THIS_MODULE,
    .llseek = default_llseek,
};

VOS_STATUS hdd_debugfs_init(hdd_adapter_t *pAdapter)
{
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    pHddCtx->debugfs_phy = debugfs_create_dir("wlan_wcnss", 0);

    if (NULL == pHddCtx->debugfs_phy)
        return VOS_STATUS_E_FAILURE;

    if (NULL == debugfs_create_file("wow_enable", S_IRUSR | S_IWUSR,
        pHddCtx->debugfs_phy, pAdapter, &fops_wowenable))
        return VOS_STATUS_E_FAILURE;

    if (NULL == debugfs_create_file("wow_pattern", S_IRUSR | S_IWUSR,
        pHddCtx->debugfs_phy, pAdapter, &fops_wowpattern))
        return VOS_STATUS_E_FAILURE;

    if (NULL == debugfs_create_file("pattern_gen", S_IRUSR | S_IWUSR,
        pHddCtx->debugfs_phy, pAdapter, &fops_patterngen))
        return VOS_STATUS_E_FAILURE;

    return VOS_STATUS_SUCCESS;
}

void hdd_debugfs_exit(hdd_context_t *pHddCtx)
{
    debugfs_remove_recursive(pHddCtx->debugfs_phy);
}
#endif /* #ifdef WLAN_OPEN_SOURCE */

