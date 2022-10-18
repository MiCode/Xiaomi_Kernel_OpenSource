// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/spinlock_types.h>
#include <linux/list.h>
#include <linux/ratelimit.h>

#include "cam_io_util.h"
#include "cam_irq_controller.h"
#include "cam_debug_util.h"
#include "cam_common_util.h"

/**
 * struct cam_irq_evt_handler:
 * @Brief:                  Event handler information
 *
 * @priority:               Priority level of this event
 * @evt_bit_mask_arr:       evt_bit_mask that has the bits set for IRQs to
 *                          subscribe for
 * @handler_priv:           Private data that will be passed to the Top/Bottom
 *                          Half handler function
 * @top_half_handler:       Top half Handler callback function
 * @bottom_half_handler:    Bottom half Handler callback function
 * @bottom_half:            Pointer to bottom_half implementation on which to
 *                          enqueue the event for further handling
 * @bottom_half_enqueue_func:
 *                          Function used to enqueue the bottom_half event
 * @list_node:              list_head struct used for overall handler List
 * @th_list_node:           list_head struct used for top half handler List
 * @index:                  Unique id of the event
 * @group:                  Group to which the event belongs
 */
struct cam_irq_evt_handler {
	enum cam_irq_priority_level        priority;
	uint32_t                          *evt_bit_mask_arr;
	void                              *handler_priv;
	CAM_IRQ_HANDLER_TOP_HALF           top_half_handler;
	CAM_IRQ_HANDLER_BOTTOM_HALF        bottom_half_handler;
	void                              *bottom_half;
	struct cam_irq_bh_api              irq_bh_api;
	struct list_head                   list_node;
	struct list_head                   th_list_node;
	int                                index;
	int                                group;
};

/**
 * struct cam_irq_register_obj:
 * @Brief:                  Structure containing information related to
 *                          a particular register Set
 *
 * @index:                  Index of set in Array
 * @mask_reg_offset:        Offset of IRQ MASK register
 * @clear_reg_offset:       Offset of IRQ CLEAR register
 * @status_reg_offset:      Offset of IRQ STATUS register
 * @top_half_enable_mask:   Array of enabled bit_mask sorted by priority
 * @aggr_mask:              Aggregate mask to keep track of the overall mask
 *                          after subscribe/unsubscribe calls
 * @dependent_read_mask:    Mask to check if any dependent controllers' read needs to triggered
 *                          from independent controller
 */
struct cam_irq_register_obj {
	uint32_t                     index;
	uint32_t                     mask_reg_offset;
	uint32_t                     clear_reg_offset;
	uint32_t                     status_reg_offset;
	uint32_t                     top_half_enable_mask[CAM_IRQ_PRIORITY_MAX];
	uint32_t                     aggr_mask;
	uint32_t                     dependent_read_mask[CAM_IRQ_MAX_DEPENDENTS];
};

/**
 * struct cam_irq_controller:
 *
 * @brief:                  IRQ Controller structure.
 *
 * @name:                   Name of IRQ Controller block
 * @mem_base:               Mapped base address of register space to which
 *                          register offsets are added to access registers
 * @num_registers:          Number of sets(mask/clear/status) of IRQ registers
 * @irq_register_arr:       Array of Register object associated with this
 *                          Controller
 * @irq_status_arr:         Array of IRQ Status values
 * @global_clear_offset:    Offset of Global IRQ Clear register. This register
 *                          contains the BIT that needs to be set for the CLEAR
 *                          to take effect
 * @global_clear_bitmask:   Bitmask needed to be used in Global Clear register
 *                          for Clear IRQ cmd to take effect
 * @clear_all_bitmask:      Bitmask that specifies which bits should be written to clear register
 *                          when it is to be cleared forcefully
 * @evt_handler_list_head:  List of all event handlers
 * @th_list_head:           List of handlers sorted by priority
 * @hdl_idx:                Unique identity of handler assigned on Subscribe.
 *                          Used to Unsubscribe.
 * @th_payload:             Payload structure to be passed to top half handler
 * @is_dependent:           Flag to indicate is this controller is dependent on another controller
 * @dependent_controller:   Array of controllers that depend on this controller
 * @delayed_global_clear:   Flag to indicate if this controller issues global clear after dependent
 *                          controllers are handled
 * @lock:                   Lock to be used by controller, Use mutex lock in presil mode,
 *                          and spinlock in regular case
 */
struct cam_irq_controller {
	const char                     *name;
	void __iomem                   *mem_base;
	uint32_t                        num_registers;
	struct cam_irq_register_obj    *irq_register_arr;
	uint32_t                       *irq_status_arr;
	uint32_t                        global_clear_offset;
	uint32_t                        global_clear_bitmask;
	uint32_t                        clear_all_bitmask;
	struct list_head                evt_handler_list_head;
	struct list_head                th_list_head[CAM_IRQ_PRIORITY_MAX];
	uint32_t                        hdl_idx;
	struct cam_irq_th_payload       th_payload;
	bool                            is_dependent;
	struct cam_irq_controller      *dependent_controller[CAM_IRQ_MAX_DEPENDENTS];
	bool                            delayed_global_clear;

#ifdef CONFIG_CAM_PRESIL
	struct mutex                    lock;
#else
	spinlock_t                      lock;
#endif
};

#ifdef CONFIG_CAM_PRESIL
static inline void cam_irq_controller_lock_init(struct cam_irq_controller *controller)
{
	mutex_init(&controller->lock);
}

static inline unsigned long cam_irq_controller_lock_irqsave(
	struct cam_irq_controller *controller)
{
	mutex_lock(&controller->lock);

	return 0;
}

static inline void cam_irq_controller_unlock_irqrestore(
	struct cam_irq_controller *controller, unsigned long flags)
{
	mutex_unlock(&controller->lock);
}

static inline void cam_irq_controller_lock(struct cam_irq_controller *controller)
{
	mutex_lock(&controller->lock);
}

static inline void cam_irq_controller_unlock(struct cam_irq_controller *controller)
{
	mutex_unlock(&controller->lock);
}
#else
static inline void cam_irq_controller_lock_init(struct cam_irq_controller *controller)
{
	spin_lock_init(&controller->lock);
}

static inline unsigned long cam_irq_controller_lock_irqsave(
	struct cam_irq_controller *controller)
{
	unsigned long flags = 0;

	if (!in_irq())
		spin_lock_irqsave(&controller->lock, flags);

	return flags;
}

static inline void cam_irq_controller_unlock_irqrestore(
	struct cam_irq_controller *controller, unsigned long flags)
{
	if (!in_irq())
		spin_unlock_irqrestore(&controller->lock, flags);
}

static inline void cam_irq_controller_lock(struct cam_irq_controller *controller)
{
	spin_lock(&controller->lock);
}

static inline void cam_irq_controller_unlock(struct cam_irq_controller *controller)
{
	spin_unlock(&controller->lock);
}
#endif



int cam_irq_controller_unregister_dependent(void *primary_controller, void *secondary_controller)
{
	struct cam_irq_controller *ctrl_primary, *ctrl_secondary;
	int i, dep_idx;

	if (!primary_controller || !secondary_controller) {
		CAM_ERR(CAM_IRQ_CTRL, "invalid args: %pK, %pK", primary_controller,
			secondary_controller);
		return -EINVAL;
	}

	ctrl_primary = primary_controller;
	ctrl_secondary = secondary_controller;

	for (i = 0; i < CAM_IRQ_MAX_DEPENDENTS; i++) {
		if (ctrl_primary->dependent_controller[i] == ctrl_secondary)
			break;
	}
	if (i == CAM_IRQ_MAX_DEPENDENTS) {
		CAM_ERR(CAM_IRQ_CTRL, "could not find %s as a dependent of %s)",
			ctrl_secondary->name, ctrl_primary->name);
		return -EINVAL;
	}
	dep_idx = i;

	ctrl_primary->dependent_controller[dep_idx] = NULL;
	for (i = 0; i < ctrl_primary->num_registers; i++)
		ctrl_primary->irq_register_arr[i].dependent_read_mask[dep_idx] = 0;
	ctrl_secondary->is_dependent = false;
	ctrl_primary->delayed_global_clear = false;

	CAM_DBG(CAM_IRQ_CTRL, "successfully unregistered %s as dependent of %s",
		ctrl_secondary->name, ctrl_primary->name);

	return 0;
}

int cam_irq_controller_register_dependent(void *primary_controller, void *secondary_controller,
	uint32_t *mask)
{
	struct cam_irq_controller *ctrl_primary, *ctrl_secondary;
	int i, dep_idx;

	if (!primary_controller || !secondary_controller) {
		CAM_ERR(CAM_IRQ_CTRL, "invalid args: %pK, %pK", primary_controller,
			secondary_controller);
		return -EINVAL;
	}

	ctrl_primary = primary_controller;
	ctrl_secondary = secondary_controller;

	for (i = 0; i < CAM_IRQ_MAX_DEPENDENTS; i++) {
		if (!ctrl_primary->dependent_controller[i])
			break;
	}
	if (i == CAM_IRQ_MAX_DEPENDENTS) {
		CAM_ERR(CAM_IRQ_CTRL, "reached maximum dependents (%s - %s)",
			ctrl_primary->name, ctrl_secondary->name);
		return -ENOMEM;
	}
	dep_idx = i;

	ctrl_primary->dependent_controller[dep_idx] = secondary_controller;
	for (i = 0; i < ctrl_primary->num_registers; i++)
		ctrl_primary->irq_register_arr[i].dependent_read_mask[dep_idx] = mask[i];
	ctrl_secondary->is_dependent = true;

	/**
	 * NOTE: For dependent controllers that should not issue global clear command,
	 * set their global_clear_offset to 0
	 */
	if (!ctrl_secondary->global_clear_offset)
		ctrl_primary->delayed_global_clear = true;

	CAM_DBG(CAM_IRQ_CTRL, "successfully registered %s as dependent of %s", ctrl_secondary->name,
		ctrl_primary->name);
	return 0;
}

static inline void cam_irq_controller_clear_irq(
	struct cam_irq_controller  *controller,
	struct cam_irq_evt_handler *evt_handler)
{
	struct cam_irq_register_obj *irq_register;
	int i;

	/* Don't clear in IRQ context since global clear will be issued after
	 * top half processing
	 */
	if (in_irq())
		return;

	for (i = 0; i < controller->num_registers; i++) {
		irq_register = &controller->irq_register_arr[i];
		cam_io_w_mb(evt_handler->evt_bit_mask_arr[i],
				controller->mem_base +
				irq_register->clear_reg_offset);
	}

	if (controller->global_clear_offset)
		cam_io_w_mb(controller->global_clear_bitmask,
				controller->mem_base +
				controller->global_clear_offset);
}

int cam_irq_controller_deinit(void **irq_controller)
{
	struct cam_irq_controller *controller = *irq_controller;
	struct cam_irq_evt_handler *evt_handler = NULL;

	if (!controller) {
		CAM_ERR(CAM_IRQ_CTRL, "Null Pointer");
		return -EINVAL;
	}

	while (!list_empty(&controller->evt_handler_list_head)) {
		evt_handler = list_first_entry(
			&controller->evt_handler_list_head,
			struct cam_irq_evt_handler, list_node);
		list_del_init(&evt_handler->list_node);
		kfree(evt_handler->evt_bit_mask_arr);
		kfree(evt_handler);
	}

	kfree(controller->th_payload.evt_status_arr);
	kfree(controller->irq_status_arr);
	kfree(controller->irq_register_arr);
	kfree(controller);
	*irq_controller = NULL;
	return 0;
}

int cam_irq_controller_init(const char       *name,
	void __iomem                         *mem_base,
	struct cam_irq_controller_reg_info   *register_info,
	void                                **irq_controller)
{
	struct cam_irq_controller *controller = NULL;
	int i, rc = 0;

	*irq_controller = NULL;

	if (!register_info->num_registers || !register_info->irq_reg_set ||
		!name || !mem_base) {
		CAM_ERR(CAM_IRQ_CTRL, "Invalid parameters");
		rc = -EINVAL;
		return rc;
	}

	controller = kzalloc(sizeof(struct cam_irq_controller), GFP_KERNEL);
	if (!controller) {
		CAM_DBG(CAM_IRQ_CTRL, "Failed to allocate IRQ Controller");
		return -ENOMEM;
	}

	controller->irq_register_arr = kzalloc(register_info->num_registers *
		sizeof(struct cam_irq_register_obj), GFP_KERNEL);
	if (!controller->irq_register_arr) {
		CAM_DBG(CAM_IRQ_CTRL, "Failed to allocate IRQ register Arr");
		rc = -ENOMEM;
		goto reg_alloc_error;
	}

	controller->irq_status_arr = kzalloc(register_info->num_registers *
		sizeof(uint32_t), GFP_KERNEL);
	if (!controller->irq_status_arr) {
		CAM_DBG(CAM_IRQ_CTRL, "Failed to allocate IRQ status Arr");
		rc = -ENOMEM;
		goto status_alloc_error;
	}

	controller->th_payload.evt_status_arr =
		kzalloc(register_info->num_registers * sizeof(uint32_t),
		GFP_KERNEL);
	if (!controller->th_payload.evt_status_arr) {
		CAM_DBG(CAM_IRQ_CTRL,
			"Failed to allocate BH payload bit mask Arr");
		rc = -ENOMEM;
		goto evt_mask_alloc_error;
	}

	controller->name = name;

	CAM_DBG(CAM_IRQ_CTRL, "num_registers: %d",
		register_info->num_registers);
	for (i = 0; i < register_info->num_registers; i++) {
		controller->irq_register_arr[i].index = i;
		controller->irq_register_arr[i].mask_reg_offset =
			register_info->irq_reg_set[i].mask_reg_offset;
		controller->irq_register_arr[i].clear_reg_offset =
			register_info->irq_reg_set[i].clear_reg_offset;
		controller->irq_register_arr[i].status_reg_offset =
			register_info->irq_reg_set[i].status_reg_offset;
		CAM_DBG(CAM_IRQ_CTRL, "i %d mask_reg_offset: 0x%x", i,
			controller->irq_register_arr[i].mask_reg_offset);
		CAM_DBG(CAM_IRQ_CTRL, "i %d clear_reg_offset: 0x%x", i,
			controller->irq_register_arr[i].clear_reg_offset);
		CAM_DBG(CAM_IRQ_CTRL, "i %d status_reg_offset: 0x%x", i,
			controller->irq_register_arr[i].status_reg_offset);
	}
	controller->num_registers        = register_info->num_registers;
	controller->global_clear_bitmask = register_info->global_clear_bitmask;
	controller->global_clear_offset  = register_info->global_clear_offset;
	controller->clear_all_bitmask    = register_info->clear_all_bitmask;
	controller->mem_base             = mem_base;
	controller->is_dependent         = false;

	CAM_DBG(CAM_IRQ_CTRL, "global_clear_bitmask: 0x%x",
		controller->global_clear_bitmask);
	CAM_DBG(CAM_IRQ_CTRL, "global_clear_offset: 0x%x",
		controller->global_clear_offset);
	CAM_DBG(CAM_IRQ_CTRL, "mem_base: %pK",
		(void __iomem *)controller->mem_base);

	INIT_LIST_HEAD(&controller->evt_handler_list_head);
	for (i = 0; i < CAM_IRQ_PRIORITY_MAX; i++)
		INIT_LIST_HEAD(&controller->th_list_head[i]);

	cam_irq_controller_lock_init(controller);

	controller->hdl_idx = 1;
	*irq_controller = controller;

	return rc;

evt_mask_alloc_error:
	kfree(controller->irq_status_arr);
status_alloc_error:
	kfree(controller->irq_register_arr);
reg_alloc_error:
	kfree(controller);

	return rc;
}

static inline void __cam_irq_controller_disable_irq(
	struct cam_irq_controller  *controller,
	struct cam_irq_evt_handler *evt_handler)
{
	struct cam_irq_register_obj *irq_register;
	uint32_t *update_mask;
	int i, priority;

	update_mask = evt_handler->evt_bit_mask_arr;
	priority    = evt_handler->priority;

	for (i = 0; i < controller->num_registers; i++) {
		irq_register = &controller->irq_register_arr[i];
		irq_register->top_half_enable_mask[priority] &= ~update_mask[i];
		irq_register->aggr_mask &= ~update_mask[i];
		cam_io_w_mb(irq_register->aggr_mask, controller->mem_base +
			irq_register->mask_reg_offset);
	}
}

static inline void __cam_irq_controller_enable_irq(
	struct cam_irq_controller  *controller,
	struct cam_irq_evt_handler *evt_handler)
{
	struct cam_irq_register_obj *irq_register;
	uint32_t *update_mask;
	int i, priority;

	update_mask = evt_handler->evt_bit_mask_arr;
	priority    = evt_handler->priority;

	for (i = 0; i < controller->num_registers; i++) {
		irq_register = &controller->irq_register_arr[i];
		irq_register->top_half_enable_mask[priority] |= update_mask[i];
		irq_register->aggr_mask |= update_mask[i];
		cam_io_w_mb(irq_register->aggr_mask, controller->mem_base +
			irq_register->mask_reg_offset);
	}
}

int cam_irq_controller_subscribe_irq(void *irq_controller,
	enum cam_irq_priority_level        priority,
	uint32_t                          *evt_bit_mask_arr,
	void                              *handler_priv,
	CAM_IRQ_HANDLER_TOP_HALF           top_half_handler,
	CAM_IRQ_HANDLER_BOTTOM_HALF        bottom_half_handler,
	void                              *bottom_half,
	struct cam_irq_bh_api             *irq_bh_api,
	enum cam_irq_event_group           evt_grp)
{
	struct cam_irq_controller  *controller  = irq_controller;
	struct cam_irq_evt_handler *evt_handler = NULL;
	int                         i;
	int                         rc = 0;
	unsigned long               flags = 0;

	if (!controller || !handler_priv || !evt_bit_mask_arr) {
		CAM_ERR(CAM_IRQ_CTRL,
			"Inval params: ctlr=%pK hdl_priv=%pK bit_mask_arr=%pK",
			controller, handler_priv, evt_bit_mask_arr);
		return -EINVAL;
	}

	if (!top_half_handler) {
		CAM_ERR(CAM_IRQ_CTRL, "Missing top half handler");
		return -EINVAL;
	}

	if (bottom_half_handler &&
		(!bottom_half || !irq_bh_api)) {
		CAM_ERR(CAM_IRQ_CTRL,
			"Invalid params: bh_handler=%pK bh=%pK bh_enq_f=%pK",
			bottom_half_handler,
			bottom_half,
			irq_bh_api);
		return -EINVAL;
	}

	if (irq_bh_api &&
		(!irq_bh_api->bottom_half_enqueue_func ||
		!irq_bh_api->get_bh_payload_func ||
		!irq_bh_api->put_bh_payload_func)) {
		CAM_ERR(CAM_IRQ_CTRL,
			"Invalid: enqueue_func=%pK get_bh=%pK put_bh=%pK",
			irq_bh_api->bottom_half_enqueue_func,
			irq_bh_api->get_bh_payload_func,
			irq_bh_api->put_bh_payload_func);
		return -EINVAL;
	}

	if (priority >= CAM_IRQ_PRIORITY_MAX) {
		CAM_ERR(CAM_IRQ_CTRL, "Invalid priority=%u, max=%u", priority,
			CAM_IRQ_PRIORITY_MAX);
		return -EINVAL;
	}

	evt_handler = kzalloc(sizeof(struct cam_irq_evt_handler), GFP_KERNEL);
	if (!evt_handler) {
		CAM_DBG(CAM_IRQ_CTRL, "Error allocating hlist_node");
		return -ENOMEM;
	}

	evt_handler->evt_bit_mask_arr = kzalloc(sizeof(uint32_t) *
		controller->num_registers, GFP_KERNEL);
	if (!evt_handler->evt_bit_mask_arr) {
		CAM_DBG(CAM_IRQ_CTRL, "Error allocating hlist_node");
		rc = -ENOMEM;
		goto free_evt_handler;
	}

	INIT_LIST_HEAD(&evt_handler->list_node);
	INIT_LIST_HEAD(&evt_handler->th_list_node);

	for (i = 0; i < controller->num_registers; i++)
		evt_handler->evt_bit_mask_arr[i] = evt_bit_mask_arr[i];

	evt_handler->priority                 = priority;
	evt_handler->handler_priv             = handler_priv;
	evt_handler->top_half_handler         = top_half_handler;
	evt_handler->bottom_half_handler      = bottom_half_handler;
	evt_handler->bottom_half              = bottom_half;
	evt_handler->index                    = controller->hdl_idx++;
	evt_handler->group                    = evt_grp;

	if (irq_bh_api)
		evt_handler->irq_bh_api       = *irq_bh_api;

	/* Avoid rollover to negative values */
	if (controller->hdl_idx > 0x3FFFFFFF)
		controller->hdl_idx = 1;

	flags = cam_irq_controller_lock_irqsave(controller);

	__cam_irq_controller_enable_irq(controller, evt_handler);

	list_add_tail(&evt_handler->list_node,
		&controller->evt_handler_list_head);
	list_add_tail(&evt_handler->th_list_node,
		&controller->th_list_head[priority]);

	cam_irq_controller_unlock_irqrestore(controller, flags);

	return evt_handler->index;

free_evt_handler:
	kfree(evt_handler);
	evt_handler = NULL;

	return rc;
}

static inline int cam_irq_controller_find_event_handle(struct cam_irq_controller *controller,
	uint32_t handle, struct cam_irq_evt_handler **found_evt_handler)
{
	struct cam_irq_evt_handler  *evt_handler, *evt_handler_temp;
	int rc = -EINVAL;

	list_for_each_entry_safe(evt_handler, evt_handler_temp,
		&controller->evt_handler_list_head, list_node) {
		if (evt_handler->index == handle) {
			rc = 0;
			*found_evt_handler = evt_handler;
			break;
		}
	}

	return rc;
}

int cam_irq_controller_enable_irq(void *irq_controller, uint32_t handle)
{
	struct cam_irq_controller   *controller  = irq_controller;
	struct cam_irq_evt_handler  *evt_handler = NULL;
	unsigned long               flags = 0;
	int                         rc = 0;

	if (!controller)
		return rc;

	flags = cam_irq_controller_lock_irqsave(controller);

	rc = cam_irq_controller_find_event_handle(controller, handle,
		&evt_handler);
	if (rc)
		goto end;

	CAM_DBG(CAM_IRQ_CTRL, "enable event %d", handle);
	__cam_irq_controller_enable_irq(controller, evt_handler);

end:
	cam_irq_controller_unlock_irqrestore(controller, flags);

	return rc;
}

int cam_irq_controller_disable_irq(void *irq_controller, uint32_t handle)
{
	struct cam_irq_controller   *controller  = irq_controller;
	struct cam_irq_evt_handler  *evt_handler = NULL;
	unsigned long               flags = 0;
	int                         rc = 0;

	if (!controller)
		return rc;

	flags = cam_irq_controller_lock_irqsave(controller);

	rc = cam_irq_controller_find_event_handle(controller, handle,
		&evt_handler);
	if (rc)
		goto end;

	CAM_DBG(CAM_IRQ_CTRL, "disable event %d", handle);
	__cam_irq_controller_disable_irq(controller, evt_handler);
	cam_irq_controller_clear_irq(controller, evt_handler);

end:
	cam_irq_controller_unlock_irqrestore(controller, flags);

	return rc;
}

int cam_irq_controller_unsubscribe_irq(void *irq_controller,
	uint32_t handle)
{
	struct cam_irq_controller   *controller  = irq_controller;
	struct cam_irq_evt_handler  *evt_handler = NULL;
	unsigned long               flags = 0;
	int                         rc = 0;

	flags = cam_irq_controller_lock_irqsave(controller);


	rc = cam_irq_controller_find_event_handle(controller, handle,
		&evt_handler);
	if (rc)
		goto end;

	list_del_init(&evt_handler->list_node);
	list_del_init(&evt_handler->th_list_node);

	__cam_irq_controller_disable_irq(controller, evt_handler);
	cam_irq_controller_clear_irq(controller, evt_handler);

	kfree(evt_handler->evt_bit_mask_arr);
	kfree(evt_handler);

end:
	cam_irq_controller_unlock_irqrestore(controller, flags);

	return rc;
}

/**
 * cam_irq_controller_match_bit_mask()
 *
 * @Brief:                This function checks if any of the enabled IRQ bits
 *                        for a certain handler is Set in the Status values
 *                        of the controller.
 *
 * @controller:           IRQ Controller structure
 * @evt_handler:          Event handler structure
 *
 * @Return:               True: If any interested IRQ Bit is Set
 *                        False: Otherwise
 */
static bool cam_irq_controller_match_bit_mask(
	struct cam_irq_controller   *controller,
	struct cam_irq_evt_handler  *evt_handler,
	int                          evt_grp)
{
	int i;

	if (evt_handler->group != evt_grp)
		return false;

	for (i = 0; i < controller->num_registers; i++) {
		if (evt_handler->evt_bit_mask_arr[i] &
			controller->irq_status_arr[i])
			return true;
	}

	return false;
}

static void __cam_irq_controller_th_processing(
	struct cam_irq_controller      *controller,
	struct list_head               *th_list_head,
	int                             evt_grp)
{
	struct cam_irq_evt_handler     *evt_handler = NULL;
	struct cam_irq_evt_handler     *evt_handler_tmp = NULL;
	struct cam_irq_th_payload      *th_payload = &controller->th_payload;
	bool                            is_irq_match;
	int                             rc = -EINVAL;
	int                             i;
	void                           *bh_cmd = NULL;
	struct cam_irq_bh_api          *irq_bh_api = NULL;

	CAM_DBG(CAM_IRQ_CTRL, "Enter");

	if (list_empty(th_list_head))
		return;

	list_for_each_entry_safe(evt_handler, evt_handler_tmp, th_list_head, th_list_node) {
		is_irq_match = cam_irq_controller_match_bit_mask(controller, evt_handler, evt_grp);

		if (!is_irq_match)
			continue;

		CAM_DBG(CAM_IRQ_CTRL, "match found");

		cam_irq_th_payload_init(th_payload);
		th_payload->handler_priv  = evt_handler->handler_priv;
		th_payload->num_registers = controller->num_registers;
		for (i = 0; i < controller->num_registers; i++) {
			th_payload->evt_status_arr[i] =
				controller->irq_status_arr[i] &
				evt_handler->evt_bit_mask_arr[i];
		}

		irq_bh_api = &evt_handler->irq_bh_api;
		bh_cmd = NULL;

		if (evt_handler->bottom_half_handler) {
			rc = irq_bh_api->get_bh_payload_func(
				evt_handler->bottom_half, &bh_cmd);
			if (rc || !bh_cmd) {
				CAM_ERR_RATE_LIMIT(CAM_ISP,
					"No payload, IRQ handling frozen for %s",
					controller->name);
				continue;
			}
		}

		/*
		 * irq_status_arr[0] is dummy argument passed. the entire
		 * status array is passed in th_payload.
		 */
		if (evt_handler->top_half_handler)
			rc = evt_handler->top_half_handler(
				controller->irq_status_arr[0],
				(void *)th_payload);

		if (rc && bh_cmd) {
			irq_bh_api->put_bh_payload_func(
				evt_handler->bottom_half, &bh_cmd);
			continue;
		}

		if (evt_handler->bottom_half_handler) {
			CAM_DBG(CAM_IRQ_CTRL, "Enqueuing bottom half for %s",
				controller->name);
			irq_bh_api->bottom_half_enqueue_func(
				evt_handler->bottom_half,
				bh_cmd,
				evt_handler->handler_priv,
				th_payload->evt_payload_priv,
				evt_handler->bottom_half_handler);
		}
	}

	CAM_DBG(CAM_IRQ_CTRL, "Exit");
}

void cam_irq_controller_disable_all(void *priv)
{
	struct cam_irq_controller  *controller  = priv;
	struct cam_irq_register_obj *irq_register;

	uint32_t i = 0;

	if (!controller)
		return;

	for (i = 0; i < controller->num_registers; i++) {
		irq_register = &controller->irq_register_arr[i];
		memset(irq_register->top_half_enable_mask, 0, CAM_IRQ_PRIORITY_MAX);
		irq_register->aggr_mask = 0;
		cam_io_w_mb(0x0, controller->mem_base + irq_register->mask_reg_offset);
		cam_io_w_mb(controller->clear_all_bitmask, controller->mem_base +
			irq_register->clear_reg_offset);
	}

	if (controller->global_clear_offset && !controller->delayed_global_clear) {
		cam_io_w_mb(controller->global_clear_bitmask,
			controller->mem_base + controller->global_clear_offset);
		CAM_DBG(CAM_IRQ_CTRL, "Global Clear done from %s",
			controller->name);
	}
}

static void __cam_irq_controller_read_registers(struct cam_irq_controller *controller)
{
	struct cam_irq_register_obj *irq_register;
	int i;

	for (i = 0; i < controller->num_registers; i++) {
		irq_register = &controller->irq_register_arr[i];

		/* Skip register read if we are not going to process it */
		if (!irq_register->aggr_mask) {
			controller->irq_status_arr[i] = 0;
			continue;
		}

		controller->irq_status_arr[i] = cam_io_r(controller->mem_base +
			irq_register->status_reg_offset);

		CAM_DBG(CAM_IRQ_CTRL, "(%s) Read irq status%d (0x%x) = 0x%x", controller->name, i,
			controller->irq_register_arr[i].status_reg_offset,
			controller->irq_status_arr[i]);

		cam_io_w(controller->irq_status_arr[i],
			controller->mem_base + irq_register->clear_reg_offset);
	}

	if (controller->global_clear_offset && !controller->delayed_global_clear) {
		cam_io_w_mb(controller->global_clear_bitmask,
			controller->mem_base + controller->global_clear_offset);
		CAM_DBG(CAM_IRQ_CTRL, "Global Clear done from %s",
			controller->name);
	}
}

static void cam_irq_controller_read_registers(struct cam_irq_controller *controller)
{
	struct cam_irq_register_obj *irq_register;
	struct cam_irq_controller *dep_controller;
	bool need_reg_read[CAM_IRQ_MAX_DEPENDENTS] = {false};
	int i, j;

	__cam_irq_controller_read_registers(controller);

	for (i = 0; i < controller->num_registers; i++) {
		irq_register = &controller->irq_register_arr[i];
		for (j = 0; j < CAM_IRQ_MAX_DEPENDENTS; j++) {
			if (irq_register->dependent_read_mask[j] & controller->irq_status_arr[i])
				need_reg_read[j] = true;
			CAM_DBG(CAM_IRQ_CTRL, "(%s) reg:%d dep:%d need_reg_read = %d",
				controller->name, i, j, need_reg_read[j]);
		}
	}

	for (j = 0; j < CAM_IRQ_MAX_DEPENDENTS; j++) {
		if (need_reg_read[j]) {
			dep_controller = controller->dependent_controller[j];
			CAM_DBG(CAM_IRQ_CTRL, "Reading dependent registers for %s",
				dep_controller->name);
			cam_irq_controller_lock(dep_controller);
			__cam_irq_controller_read_registers(dep_controller);
			cam_irq_controller_unlock(dep_controller);
		}
	}

	if (controller->global_clear_offset && controller->delayed_global_clear) {
		cam_io_w_mb(controller->global_clear_bitmask,
			controller->mem_base + controller->global_clear_offset);
		CAM_DBG(CAM_IRQ_CTRL, "Delayed Global Clear done from %s",
			controller->name);
	}
}

static void cam_irq_controller_process_th(struct cam_irq_controller *controller, int evt_grp)
{
	struct cam_irq_register_obj *irq_register;
	bool need_th_processing[CAM_IRQ_PRIORITY_MAX] = {false};
	int i, j;


	for (i = 0; i < controller->num_registers; i++) {
		irq_register = &controller->irq_register_arr[i];
		for (j = 0; j < CAM_IRQ_PRIORITY_MAX; j++) {
			if (irq_register->top_half_enable_mask[j] & controller->irq_status_arr[i])
				need_th_processing[j] = true;
			CAM_DBG(CAM_IRQ_CTRL, "reg:%d priority:%d need_th_processing = %d",
				i, j, need_th_processing[j]);
		}
	}

	for (i = 0; i < CAM_IRQ_PRIORITY_MAX; i++) {
		if (need_th_processing[i]) {
			CAM_DBG(CAM_IRQ_CTRL, "(%s) Invoke TH processing priority:%d",
				controller->name, i);
			__cam_irq_controller_th_processing(controller, &controller->th_list_head[i],
				evt_grp);
		}
	}
}

irqreturn_t cam_irq_controller_handle_irq(int irq_num, void *priv, int evt_grp)
{
	struct cam_irq_controller *controller  = priv;

	if (unlikely(!controller))
		return IRQ_NONE;

	CAM_DBG(CAM_IRQ_CTRL,
		"Locking: %s IRQ Controller: [%pK], lock handle: %pK",
		controller->name, controller, &controller->lock);
	cam_irq_controller_lock(controller);

	if (!controller->is_dependent)
		cam_irq_controller_read_registers(controller);

	cam_irq_controller_process_th(controller, evt_grp);

	cam_irq_controller_unlock(controller);
	CAM_DBG(CAM_IRQ_CTRL,
		"Unlocked: %s IRQ Controller: %pK, lock handle: %pK",
		controller->name, controller, &controller->lock);

	return IRQ_HANDLED;
}

int cam_irq_controller_update_irq(void *irq_controller, uint32_t handle,
	bool enable, uint32_t *irq_mask)
{
	struct cam_irq_controller   *controller  = irq_controller;
	struct cam_irq_evt_handler  *evt_handler = NULL;
	unsigned long               flags = 0;
	unsigned int                i;
	int                         rc = 0;

	if (!controller)
		return rc;

	flags = cam_irq_controller_lock_irqsave(controller);

	rc = cam_irq_controller_find_event_handle(controller, handle,
		&evt_handler);
	if (rc)
		goto end;

	__cam_irq_controller_disable_irq(controller, evt_handler);
	for (i = 0; i < controller->num_registers; i++) {
		if (enable) {
			evt_handler->evt_bit_mask_arr[i] |= irq_mask[i];
		} else {
			evt_handler->evt_bit_mask_arr[i] &= ~irq_mask[i];
		}
	}
	__cam_irq_controller_enable_irq(controller, evt_handler);
	cam_irq_controller_clear_irq(controller, evt_handler);

end:
	cam_irq_controller_unlock_irqrestore(controller, flags);

	return rc;
}
