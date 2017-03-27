/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _QMI_H_
#define _QMI_H_
int ath10k_snoc_pd_restart_enable(struct ath10k *ar);
int ath10k_snoc_modem_ssr_register_notifier(struct ath10k *ar);
int ath10k_snoc_modem_ssr_unregister_notifier(struct ath10k *ar);
int ath10k_snoc_pdr_unregister_notifier(struct ath10k *ar);

#endif
