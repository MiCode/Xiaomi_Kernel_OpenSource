/* SPDX-License-Identifier: GPL-2.0 */
#if IS_ENABLED(CONFIG_SPRD_TYPEC_DP_ALTMODE)
int sprd_dp_altmode_probe(struct typec_altmode *alt);
void sprd_dp_altmode_remove(struct typec_altmode *alt);
#else
int sprd_dp_altmode_probe(struct typec_altmode *alt) { return -ENOTSUPP; }
void sprd_dp_altmode_remove(struct typec_altmode *alt) { }
#endif /* CONFIG_TYPEC_DP_ALTMODE */
