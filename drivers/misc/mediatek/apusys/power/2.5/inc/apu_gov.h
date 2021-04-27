/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APU_GOV_H__
#define __APU_GOV_H__

#include <linux/devfreq.h>
#include <linux/list_sort.h>
#include <linux/list.h>

#include "apusys_power_user.h"
/* DEVFREQ governor name */
#define APUGOV_USR		"apuuser"
#define APUGOV_PASSIVE		"apupassive"
#define APUGOV_PASSIVE_PE	"apupassive-pe"
#define APUGOV_CONSTRAIN	"apuconstrain"
#define APUGOV_POLL_RATE	20 /* ms */
struct apu_dev;

struct apu_req {
	struct device *dev;
	struct list_head list;
	int pe_flag;
	int value;
};

struct apu_pe {
	struct work_struct pe_work;
	struct apu_req *req;
};

/**
 * struct apu_gov_data - void *data fed to struct devfreq
 *	and devfreq_add_device
 * @parent:	the devfreq instance of parent device.
 * @this:	the devfreq instance of own device.
 * @nb:		the notifier block for DEVFREQ_TRANSITION_NOTIFIER list
 * @user_freq	the freq that user expect to be
 * @valid	whether user_boost is valid or not.
 *
 * The devfreq_passive_data have to set the devfreq instance of parent
 * device with governors except for the passive governor. But, don't need to
 * initialize the 'this' and 'nb' field because the devfreq core will handle
 * them.
 */
struct apu_gov_data {
	struct devfreq *parent;
	struct devfreq *this;
	int max_opp;

	/*
	 * Below data is used for gov-constrain,
	 *
	 * threshold_opp:
	 *    The threshold that child can start voting parent.
	 *    for example:
	 *        threshold_opp = 2;
	 *        child wants to vote opp = 3;
	 *        --> no need to put in parent's child array
	 *
	 *        threshold_opp = 2;
	 *        child wants to vote opp = 0;
	 *        --> can put in parent's child array
	 *
	 * child_opp_limit:
	 *    The upper bound that child vote into parent
	 *    for example:
	 *        child_opp_limit = 2;
	 *        child current opp = 0;
	 *        --> child only can vote 2 into parent's child array.
	 */
	int threshold_opp;   /* located in child's gov */
	int child_opp_limit; /* located in parent's gov */

	/* req_parent: request hook on parent's head */
	struct apu_req req_parent;

	/*
	 * head: use to hook child's and self reqeusts
	 * req: request from user
	 * req_pe: request for power efficiency
	 */
	struct list_head head;
	struct apu_req req;

	/* gov_pe: vote to power efficiency governor */
	struct apu_pe gov_pe;

	/*
	 * DEVFREQ_PRECHANGE/DEVFREQ_POSTCHANGE
	 * will container_of(nb) to get apu_gov_data
	 */
	struct notifier_block nb;

	/* the depth of power topology */
	u32 depth;
};

void get_datas(struct apu_gov_data *gov_data,
	       struct apu_gov_data **pgov_data, struct apu_dev **ad,
	       struct device **dev);
int apu_cmp(void *priv, struct list_head *a, struct list_head *b);
struct apu_gov_data *apu_gov_init(struct device *dev,
				  struct devfreq_dev_profile *pf, const char **gov_name);
int apu_gov_setup(struct apu_dev *ad, void *data);
void apu_gov_unsetup(struct apu_dev *ad);
void apu_dump_list(struct apu_gov_data *gov_data);
void apu_dump_pe_gov(struct apu_dev *ad, struct list_head *head);

#endif
