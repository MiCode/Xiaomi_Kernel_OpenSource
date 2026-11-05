#ifndef _CAMERA_DYNAMIC_RESERVE_POOL_H
#define _CAMERA_DYNAMIC_RESERVE_POOL_H


extern void camera_sys_heap_reserve_pool_init(const char *name,
		struct qcom_system_heap *sys_heap);
extern void camera_sys_heap_reserve_pool_deinit(struct qcom_system_heap *sys_heap);
extern struct page *get_page_from_camera_dynamic_reserve_pool(
                struct qcom_system_heap *sys_heap,
                bool served_app,
                struct dynamic_page_pool *cam_reserve_pool);
extern bool is_allow_from_reserve_pool(struct dynamic_page_pool *reserve_pool);
#endif /* _CAMERA_DYNAMIC_RESERVE_POOL_H */
