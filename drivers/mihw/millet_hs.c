#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include "millet.h"

static int hs_sendmsg(struct task_struct *tsk,
                struct millet_data *data, struct millet_sock *sk)
{
        int ret = 0;

        if (!sk || !data) {
                pr_err("%s input invalid\n", __FUNCTION__);
                return RET_ERR;
        }

        data->msg_type = MSG_TO_USER;
        data->owner = HANDSHK_TYPE;
	ret = millet_sendto_user(tsk, data, sk);

	return ret;
}

static void hs_recv_hook(void *nouse, unsigned int len)
{
	struct millet_data data;

	millet_sendmsg(HANDSHK_TYPE, current, &data);
}

static void hs_init_millet(struct millet_sock *sk)
{
	if (sk)
		sk->mod[HANDSHK_TYPE].monitor = SIG_TYPE;
}

static int __init millet_hs_init(void)
{

	pr_info("hs_register_hooks(millet hooks) success\n");
	register_millet_hook(HANDSHK_TYPE, hs_recv_hook, hs_sendmsg,
		hs_init_millet);

	return RET_OK;
}

static void __exit millet_hs_exit(void)
{
	unregister_millet_hook(HANDSHK_TYPE);
}

module_init(millet_hs_init);
module_exit(millet_hs_exit);

MODULE_LICENSE("GPL");
