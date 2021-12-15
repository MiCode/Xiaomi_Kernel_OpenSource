/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef MTK_TYPEC_SWITCH_H
#define MTK_TYPEC_SWITCH_H

/*
 * struct fusb304
 */
struct fusb304 {
	struct device *dev;
	struct pinctrl *pinctrl;
	struct pinctrl_state *sel_up;
	struct pinctrl_state *sel_down;
	struct pinctrl_state *enable;
	struct pinctrl_state *disable;
};

/*
 * struct fusb340
 */
struct fusb340 {
	struct device *dev;
	struct pinctrl *pinctrl;
	struct pinctrl_state *sel_up;
	struct pinctrl_state *sel_down;
	struct pinctrl_state *enable;
	struct pinctrl_state *disable;
	struct mutex lock;
};

/*
 * struct ptn36241g
 */
struct ptn36241g {
	struct device *dev;
	struct pinctrl *pinctrl;
	struct pinctrl_state *c1_active;
	struct pinctrl_state *c1_sleep;
	struct pinctrl_state *c2_active;
	struct pinctrl_state *c2_sleep;
};

/*
 * struct mtk_typec_switch
 */
struct mtk_typec_switch {
	struct device *dev;
	struct typec_switch *sw;
	struct typec_mux *mux;
	int orientation;
	struct mutex lock;
	struct fusb304 *fusb;
	struct ptn36241g *ptn;
	struct fusb340 *fusb_2;
};

int ptn36241g_init(struct ptn36241g *ptn);
int ptn36241g_set_conf(struct ptn36241g *ptn, int orientation);

int fusb304_init(struct fusb304 *ptn);
int fusb304_set_conf(struct fusb304 *fusb, int orientation);

int fusb340_init(struct fusb340 *ptn);
int fusb340_set_conf(struct fusb340 *fusb, int orientation);

#endif	/* MTK_TYPEC_SWITCH */
