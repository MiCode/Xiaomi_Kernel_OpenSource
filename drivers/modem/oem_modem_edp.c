
#include <linux/module.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/err.h>
#include <linux/edp.h>
#include <linux/oem_modem_edp.h>

static void oem_modem_edp_worker(struct work_struct *ws)
{
	struct oem_modem_edp *modem_edp = container_of(ws, struct oem_modem_edp,
							edp_work);
	int ret;

	EDP_DBG("%s(%d): BEGIN\n", __func__, __LINE__);

	if (!modem_edp) {
		ret = -EINVAL;
		pr_err("%s(%d): invalid argument, errno(%d)!\n", __func__, __LINE__, ret);
		goto done;
	}

	mutex_lock(&modem_edp->edp_mutex);
	if (!modem_edp->edp_initialized) {
		ret = -EPERM;
		pr_err("%s(%d): edp client is not initialized, errno(%d)!\n", __func__, __LINE__, ret);
		goto done;
	}

	ret = edp_update_client_request(modem_edp->edp_client,
			modem_edp->active_estate, NULL);
	if (ret) {
		pr_err("%s(%d): unable to set estate-%d, errno(%d)!\n", __func__, __LINE__,
			modem_edp->active_estate, ret);
		goto done;
	}

done:
	mutex_unlock(&modem_edp->edp_mutex);
	EDP_DBG("%s(%d): END\n", __func__, __LINE__);
	return;
}

ssize_t oem_modem_estate_show(struct oem_modem_edp *modem_edp, struct device_attribute *attr,
			char *buf)
{
	int count = 0;

	EDP_DBG("%s(%d): BEGIN\n", __func__, __LINE__);

	if (!modem_edp) {
		pr_err("%s(%d): invalid argument!\n", __func__, __LINE__);
		return count;
	}

	mutex_lock(&modem_edp->edp_mutex);
	count = sprintf(buf, "%d - %d\n", modem_edp->active_estate,
			modem_edp->edp_client->states[modem_edp->active_estate]);
	mutex_unlock(&modem_edp->edp_mutex);

	EDP_DBG("%s(%d): END\n", __func__, __LINE__);
	return count;
}

EXPORT_SYMBOL_GPL(oem_modem_estate_show);

ssize_t oem_modem_estate_store(struct oem_modem_edp *modem_edp, struct device_attribute *attr,
			const char *buf, size_t size)
{
	int estate;
	int ret;

	EDP_DBG("%s(%d): BEGIN\n", __func__, __LINE__);

	if (!modem_edp) {
		ret = -EINVAL;
		pr_err("%s(%d): invalid argument, errno(%d)!\n", __func__, __LINE__, ret);
		return ret;
	}

	mutex_lock(&modem_edp->edp_mutex);
	ret = sscanf(buf, "%d", &estate);
	if (ret != 1 || estate < 0 ||
		estate >= modem_edp->edp_client->num_states) {
		ret = -EINVAL;
		pr_err("%s(%d): invalid argument, errno(%d)!\n", __func__, __LINE__, ret);
		goto done;
	}

	if (!modem_edp->edp_initialized) {
		ret = -EPERM;
		pr_err("%s(%d): edp client is not initialized, errno(%d)!\n", __func__, __LINE__, ret);
		goto done;
	}

	modem_edp->active_estate = estate;
	ret = edp_update_client_request(modem_edp->edp_client,
			modem_edp->active_estate, NULL);
	if (ret) {
		pr_err("%s(%d): unable to set estate-%d, errno(%d)!\n", __func__, __LINE__,
			modem_edp->active_estate, ret);
		goto done;
	}
	ret = size;

done:
	mutex_unlock(&modem_edp->edp_mutex);
	EDP_DBG("%s(%d): END\n", __func__, __LINE__);
	return ret;
}

EXPORT_SYMBOL_GPL(oem_modem_estate_store);

int oem_modem_edp_update_client_request_sync(struct oem_modem_edp *modem_edp, int estate)
{
	int ret;

	EDP_DBG("%s(%d): BEGIN\n", __func__, __LINE__);

	if (!modem_edp) {
		ret = -EINVAL;
		pr_err("%s(%d): invalid argument, errno(%d)!\n", __func__, __LINE__, ret);
		return ret;
	}

	mutex_lock(&modem_edp->edp_mutex);
	if (!modem_edp->edp_initialized) {
		ret = -EPERM;
		pr_err("%s(%d): edp client is not initialized, errno(%d)!\n", __func__, __LINE__, ret);
		goto done;
	}

	modem_edp->active_estate = estate;
	ret = edp_update_client_request(modem_edp->edp_client,
			modem_edp->active_estate, NULL);
	if (ret) {
		pr_err("%s(%d): unable to set estate-%d, errno(%d)!\n", __func__, __LINE__,
			modem_edp->active_estate, ret);
		goto done;
	}

done:
	mutex_unlock(&modem_edp->edp_mutex);
	EDP_DBG("%s(%d): END\n", __func__, __LINE__);
	return ret;
}

EXPORT_SYMBOL_GPL(oem_modem_edp_update_client_request_sync);

int oem_modem_edp_update_client_request_async(struct oem_modem_edp *modem_edp, int estate)
{
	int ret;

	EDP_DBG("%s(%d): BEGIN\n", __func__, __LINE__);

	if (!modem_edp) {
		ret = -EINVAL;
		pr_err("%s(%d): invalid argument, errno(%d)!\n", __func__, __LINE__, ret);
		goto done;
	}
	if (estate < 0 || estate >= modem_edp->edp_client->num_states) {
		ret = -EINVAL;
		pr_err("%s(%d): invalid argument, errno(%d)!\n", __func__, __LINE__, ret);
		goto done;
	}
	modem_edp->active_estate = estate;
	queue_work(modem_edp->edp_wq, &modem_edp->edp_work);

done:
	EDP_DBG("%s(%d): END\n", __func__, __LINE__);
	return ret;

}

EXPORT_SYMBOL_GPL(oem_modem_edp_update_client_request_async);

int oem_modem_edp_init(struct oem_modem_edp *modem_edp)
{
	struct edp_manager *edp_mgr;
	int ret;

	EDP_DBG("%s(%d): BEGIN\n", __func__, __LINE__);

	if (!modem_edp) {
		pr_err("%s(%d): invalid argument!\n", __func__, __LINE__);
		return -EINVAL;
	}

	modem_edp->active_estate = 0;	/* default: E0 */

	mutex_init(&modem_edp->edp_mutex);
	if (modem_edp->edp_initialized) {
		pr_err("%s(%d): edp client is already initialized!\n",
			__func__, __LINE__);
		return -EPERM;
	}

	mutex_lock(&modem_edp->edp_mutex);
	edp_mgr = edp_get_manager(modem_edp->edp_mgr_name);
	if (!edp_mgr) {
		ret = -EINVAL;
		pr_err("%s(%d): can't get edp manger %s, errno(%d)!\n",
			__func__, __LINE__, modem_edp->edp_mgr_name, ret);
		goto get_edp_mgr_failed;
	}

	ret = edp_register_client(edp_mgr, modem_edp->edp_client);
	if (ret) {
		pr_err("%s(%d): can't register edp client, errno(%d)!\n",
			__func__, __LINE__, ret);
		goto reg_edp_client_failed;
	}

	modem_edp->edp_wq = create_workqueue("oem_modem_edp_wq");
	INIT_WORK(&modem_edp->edp_work, oem_modem_edp_worker);
	modem_edp->edp_initialized = 1;
	mutex_unlock(&modem_edp->edp_mutex);

	/* Request default estate - E0 */
	ret = oem_modem_edp_update_client_request_sync(modem_edp, 0);
	if (ret) {
		pr_err("%s(%d): request default E0 estate failed, errno(%d)!\n", __func__, __LINE__, ret);
		goto request_edp_estate_failed;
	}

	EDP_DBG("%s(%d): END\n", __func__, __LINE__);
	return ret;

request_edp_estate_failed:
	edp_unregister_client(modem_edp->edp_client);
reg_edp_client_failed:
get_edp_mgr_failed:
	mutex_unlock(&modem_edp->edp_mutex);
	mutex_destroy(&modem_edp->edp_mutex);
	EDP_DBG("%s(%d): END\n", __func__, __LINE__);
	return ret;
}

EXPORT_SYMBOL_GPL(oem_modem_edp_init);

void oem_modem_edp_remove(struct oem_modem_edp *modem_edp)
{
	EDP_DBG("%s(%d): BEGIN\n", __func__, __LINE__);

	if (!modem_edp) {
		pr_err("%s(%d): invalid argument!\n", __func__, __LINE__);
		return;
	}

	if (modem_edp->edp_initialized) {
		cancel_work_sync(&modem_edp->edp_work);
		edp_unregister_client(modem_edp->edp_client);
		destroy_workqueue(modem_edp->edp_wq);
		mutex_destroy(&modem_edp->edp_mutex);
	}

	EDP_DBG("%s(%d): END\n", __func__, __LINE__);
}

EXPORT_SYMBOL_GPL(oem_modem_edp_remove);
