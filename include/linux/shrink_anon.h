/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2023 Unisoc Communications Inc.
 */

int register_unisoc_shrink_anon_notifier(struct notifier_block *nb);
int unregister_unisoc_shrink_anon_notifier(struct notifier_block *nb);
extern int unisoc_shrink_anon_notifier_call_chain(void *data);
