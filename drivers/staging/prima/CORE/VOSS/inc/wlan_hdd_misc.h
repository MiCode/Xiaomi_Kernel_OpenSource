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

#ifndef WLAN_HDD_MISC_H
#define WLAN_HDD_MISC_H

#ifdef MSM_PLATFORM
#ifdef QC_WLAN_CHIPSET_PRIMA
#define WLAN_INI_FILE              "wlan/prima/WCNSS_qcom_cfg.ini"
#define WLAN_CFG_FILE              "wlan/prima/WCNSS_cfg.dat"
#define WLAN_FW_FILE               ""
#define WLAN_NV_FILE               "wlan/prima/WCNSS_qcom_wlan_nv.bin"
#define WLAN_DICT_FILE             "wlan/prima/WCNSS_qcom_wlan_dictionary.dat"
#define WLAN_COUNTRY_INFO_FILE     "wlan/prima/WCNSS_wlan_country_info.dat"
#define WLAN_HO_CFG_FILE           "wlan/prima/WCNSS_wlan_ho_config"
#else
#define WLAN_INI_FILE              "wlan/volans/WCN1314_qcom_cfg.ini"
#define WLAN_CFG_FILE              "wlan/volans/WCN1314_cfg.dat"
#define WLAN_FW_FILE               "wlan/volans/WCN1314_qcom_fw.bin"
#define WLAN_NV_FILE               "wlan/volans/WCN1314_qcom_wlan_nv.bin"
#define WLAN_DICT_FILE             ""
#define WLAN_COUNTRY_INFO_FILE     "wlan/volans/WCN1314_wlan_country_info.dat"
#define WLAN_HO_CFG_FILE           "wlan/volans/WCN1314_wlan_ho_config"
#endif // ANI_CHIPSET
#else
#define WLAN_INI_FILE              "wlan/qcom_cfg.ini"
#define WLAN_CFG_FILE              "wlan/cfg.dat"
#define WLAN_FW_FILE               "wlan/qcom_fw.bin"
#define WLAN_NV_FILE               "wlan/qcom_wlan_nv.bin"
#define WLAN_DICT_FILE             ""
#define WLAN_COUNTRY_INFO_FILE     "wlan/wlan_country_info.dat"
#define WLAN_HO_CFG_FILE           "wlan/wlan_ho_config"
#endif // MSM_PLATFORM


VOS_STATUS hdd_request_firmware(char *pfileName,v_VOID_t *pCtx,v_VOID_t **ppfw_data, v_SIZE_t *pSize);

VOS_STATUS hdd_release_firmware(char *pFileName,v_VOID_t *pCtx);

VOS_STATUS hdd_get_cfg_file_size(v_VOID_t *pCtx, char *pFileName, v_SIZE_t *pBufSize);

VOS_STATUS hdd_read_cfg_file(v_VOID_t *pCtx, char *pFileName, v_VOID_t *pBuffer, v_SIZE_t *pBufSize);
#if 0

VOS_STATUS hdd_release_firmware(char *pFileName,v_VOID_t *pCtx);

VOS_STATUS hdd_get_cfg_file_size(v_VOID_t *pCtx, char *pFileName, v_SIZE_t *pBufSize);

VOS_STATUS hdd_read_cfg_file(v_VOID_t *pCtx, char *pFileName, v_VOID_t *pBuffer, v_SIZE_t *pBufSize);

#endif

tVOS_CONCURRENCY_MODE hdd_get_concurrency_mode ( void );

#endif /* WLAN_HDD_MISC_H */

