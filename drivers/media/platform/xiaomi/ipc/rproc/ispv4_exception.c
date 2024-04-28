#include "ispv4_exception.h"

static int ispv4_exception_notify(struct xm_ispv4_rproc *rp,
				  void *data, int len)
{
	int ret = -1;
	if (rp->mbox_excep_notify != NULL)
		ret = rp->mbox_excep_notify(rp->mbox_excep_notify_priv,
					    data, len);
	return ret;
}

void register_exception_cb(struct xm_ispv4_rproc *rp,
			   int (*cb)(void *, void *, int),
			   void *priv)
{
	rp->mbox_excep_notify = cb;
	rp->mbox_excep_notify_priv = priv;
}

void ispv4_mbox_excep_cb(struct mbox_client *cl, void *mssg)
{
	struct xm_ispv4_rproc *rp =
		container_of(cl, struct xm_ispv4_rproc, mbox_exception);
	int ret;
	(void)mssg;

	pr_info("ispv4 mbox exception handler cb\n");

	//TODO: data?
	ret = ispv4_exception_notify(rp, NULL, 0);
	if (ret)
		pr_err("ispv4 mbox exception notfiy hal fail\n");
	else
		pr_info("ispv4 mbox exception notfiy hal success\n");
}

void ispv4_mbox_excep_init(struct xm_ispv4_rproc *rp)
{
	rp->mbox_exception.dev = rp->dev;
	rp->mbox_exception.tx_block = false;
	rp->mbox_exception.tx_tout = 1000;
	rp->mbox_exception.knows_txdone = false;
	rp->mbox_exception.rx_callback = ispv4_mbox_excep_cb;
}

int ispv4_mbox_excep_boot(struct xm_ispv4_rproc *rp)
{
	ispv4_mbox_excep_init(rp);
	rp->mbox_exception_chan = mbox_request_channel(&rp->mbox_exception, 3);
	if (IS_ERR_OR_NULL(rp->mbox_exception_chan)) {
		dev_err(rp->dev, "exception mbox chan request fail\n");
		return PTR_ERR(rp->mbox_exception_chan);
	}

	return 0;
}

void ispv4_mbox_excep_deboot(struct xm_ispv4_rproc *rp)
{
	if (!IS_ERR_OR_NULL(rp->mbox_exception_chan))
		mbox_free_channel(rp->mbox_exception_chan);
}
