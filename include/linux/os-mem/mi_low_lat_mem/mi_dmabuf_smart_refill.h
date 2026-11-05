#ifndef _DSR_H_
#define _DSR_H_
struct mi_smart_refill_data {
	int id;
	unsigned int order;
	int count;
	int active;
	struct list_head list;
	int pool_req_max;
	int stat;
	u64 dmabuf_alloc_from_pool_cnt;
	u64 dmabuf_alloc_from_buddy_cnt;
};

struct pool_info {
	int target_order_9;
	int target_order_4;
	int cur_order_9;
	int cur_order_4;
	int pre_order_9;
	int pre_order_4;
};

extern void mi_smart_refill_dynamic_page_pool_refill(int id, int count);
extern void mi_smart_refill_pools_init(struct list_head *list,
		int *dmabuf_pool_nums);
extern int mi_smart_refill_init(int *dmabuf_pool_nums);
extern void mi_smart_refill_update(void);
extern void mi_compact_pool_stat_update(struct pool_info *p_info);
extern struct list_head mi_smart_refill_pools;
extern int scene_code;
#endif /* _DSR_H_ */
