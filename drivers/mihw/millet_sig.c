#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/freezer.h>
#include <linux/signal.h>
#include "millet.h"

static int last_report_task;
void millet_sig(int sig, struct task_struct *killer, struct task_struct *dst)
{
	struct millet_data data;

	if (sig == SIGKILL
			|| sig == SIGTERM
			|| sig == SIGABRT
			|| sig == SIGQUIT) {

		data.mod.k_priv.sig.caller_task = current;
		data.mod.k_priv.sig.killed_task = dst;
		data.mod.k_priv.sig.reason = KILLED_BY_PRO;
		millet_sendmsg(SIG_TYPE, dst, &data);
	}
}

static int signals_sendmsg(struct task_struct *tsk,
		struct millet_data *data, struct millet_sock *sk)
{
	int ret = 0;

	if (!sk || !data || !tsk) {
		pr_err("%s input invalid\n", __FUNCTION__);
		return RET_ERR;
	}

	data->mod.k_priv.sig.killed_pid = task_tgid_nr(tsk);
	data->uid = task_uid(tsk).val;
	data->msg_type = MSG_TO_USER;
	data->owner = SIG_TYPE;

	if (frozen_task_group(tsk)
		&& (data->mod.k_priv.sig.killed_pid != *(int *)sk->mod[SIG_TYPE].priv)) {
		*(int *)sk->mod[SIG_TYPE].priv = data->mod.k_priv.sig.killed_pid;
		ret = millet_sendto_user(tsk, data, sk);
	}

	return ret;
}

static void signas_init_millet(struct millet_sock *sk)
{
	if (sk) {
		sk->mod[SIG_TYPE].monitor = SIG_TYPE;
		sk->mod[SIG_TYPE].priv = (void *)&last_report_task;
	}
}

static int __init sig_mod_init(void)
{
	register_millet_hook(SIG_TYPE, NULL,
		signals_sendmsg, signas_init_millet);

	return 0;
}

module_init(sig_mod_init);
MODULE_LICENSE("GPL");

