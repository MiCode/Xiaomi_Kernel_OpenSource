/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include "cam_io_util.h"
#include "cam_irq_controller.h"
#include "cam_debug_util.h"

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
 */
struct cam_irq_evt_handler {
	enum cam_irq_priority_level        priority;
	uint32_t                          *evt_bit_mask_arr;
	void                              *handler_priv;
	CAM_IRQ_HANDLER_TOP_HALF           top_half_handler;
	CAM_IRQ_HANDLER_BOTTOM_HALF        bottom_half_handler;
	void                              *bottom_half;
	CAM_IRQ_BOTTOM_HALF_ENQUEUE_FUNC   bottom_half_enqueue_func;
	struct list_head                   list_node;
	struct list_head                   th_list_node;
	int                                index;
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
 */
struct cam_irq_register_obj {
	uint32_t                     index;
	uint32_t                     mask_reg_offset;
	uint32_t                     clear_reg_offset;
	uint32_t                     status_reg_offset;
	uint32_t                     top_half_enable_mask[CAM_IRQ_PRIORITY_MAX];
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
 * @evt_handler_list_head:  List of all event handlers
 * @th_list_head:           List of handlers sorted by priority
 * @hdl_idx:                Unique identity of handler assigned on Subscribe.
 *                          Used to Unsubscribe.
 * @rw_lock:                Read-Write Lock for use by controller
 */
struct cam_irq_controller {
	const char                     *name;
	void __iomem                   *mem_base;
	uint32_t                        num_registers;
	struct cam_irq_register_obj    *irq_register_arr;
	uint32_t                       *irq_status_arr;
	uint32_t                        global_clear_offset;
	uint32_t                        global_clear_bitmask;
	struct list_head                evt_handler_list_head;
	struct list_head                th_list_head[CAM_IRQ_PRIORITY_MAX];
	uint32_t                        hdl_idx;
	rwlock_t                        rw_lock;
	struct cam_irq_th_payload       th_payload;
};

int cam_irq_controller_deinit(void **irq_controller)
{
	struct cam_irq_controller *controller = *irq_controller;
	struct cam_irq_evt_handler *evt_handler = NULL;

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
		CAM_ERR(CAM_ISP, "Invalid parameters");
		rc = -EINVAL;
		return rc;
	}

	controller = kzalloc(sizeof(struct cam_irq_controller), GFP_KERNEL);
	if (!controller) {
		CAM_DBG(CAM_ISP, "Failed to allocate IRQ Controller");
		return -ENOMEM;
	}

	controller->irq_register_arr = kzalloc(register_info->num_registers *
		sizeof(struct cam_irq_register_obj), GFP_KERNEL);
	if (!controller->irq_register_arr) {
		CAM_DBG(CAM_ISP, "Failed to allocate IRQ register Arr");
		rc = -ENOMEM;
		goto reg_alloc_error;
	}

	controller->irq_status_arr = kzalloc(register_info->num_registers *
		sizeof(uint32_t), GFP_KERNEL);
	if (!controller->irq_status_arr) {
		CAM_DBG(CAM_ISP, "Failed to allocate IRQ status Arr");
		rc = -ENOMEM;
		goto status_alloc_error;
	}

	controller->th_payload.evt_status_arr =
		kzalloc(register_info->num_registers * sizeof(uint32_t),
		GFP_KERNEL);
	if (!controller->th_payload.evt_status_arr) {
		CAM_DBG(CAM_ISP, "Failed to allocate BH payload bit mask Arr");
		rc = -ENOMEM;
		goto evt_mask_alloc_error;
	}

	controller->name = name;

	CAM_DBG(CAM_ISP, "num_registers: %d", register_info->num_registers);
	for (i = 0; i < register_info->num_registers; i++) {
		controller->irq_register_arr[i].index = i;
		controller->irq_register_arr[i].mask_reg_offset =
			register_info->irq_reg_set[i].mask_reg_offset;
		controller->irq_register_arr[i].clear_reg_offset =
			register_info->irq_reg_set[i].clear_reg_offset;
		controller->irq_register_arr[i].status_reg_offset =
			register_info->irq_reg_set[i].status_reg_offset;
		CAM_DBG(CAM_ISP, "i %d mask_reg_offset: 0x%x", i,
			controller->irq_register_arr[i].mask_reg_offset);
		CAM_DBG(CAM_ISP, "i %d clear_reg_offset: 0x%x", i,
			controller->irq_register_arr[i].clear_reg_offset);
		CAM_DBG(CAM_ISP, "i %d status_reg_offset: 0x%x", i,
			controller->irq_register_arr[i].status_reg_offset);
	}
	controller->num_registers        = register_info->num_registers;
	controller->global_clear_bitmask = register_info->global_clear_bitmask;
	controller->global_clear_offset  = register_info->global_clear_offset;
	controller->mem_base             = mem_base;

	CAM_DBG(CAM_ISP, "global_clear_bitmask: 0x%x",
		controller->global_clear_bitmask);
	CAM_DBG(CAM_ISP, "global_clear_offset: 0x%x",
		controller->global_clear_offset);
	CAM_DBG(CAM_ISP, "mem_base: %pK", (void __iomem *)controller->mem_base);

	INIT_LIST_HEAD(&controller->evt_handler_list_head);
	for (i = 0; i < CAM_IRQ_PRIORITY_MAX; i++)
		INIT_LIST_HEAD(&controller->th_list_head[i]);

	rwlock_init(&controller->rw_lock);

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

int cam_irq_controller_subscribe_irq(void *irq_controller,
	enum cam_irq_priority_level        priority,
	uint32_t                          *evt_bit_mask_arr,
	void                              *handler_priv,
	CAM_IRQ_HANDLER_TOP_HALF           top_half_handler,
	CAM_IRQ_HANDLER_BOTTOM_HALF        bottom_half_handler,
	void                              *bottom_half,
	CAM_IRQ_BOTTOM_HALF_ENQUEUE_FUNC   bottom_half_enqueue_func)
{
	struct cam_irq_controller  *controller  = irq_controller;
	struct cam_irq_evt_handler *evt_handler = NULL;
	int                         i;
	int                         rc = 0;
	uint32_t                    irq_mask;
	unsigned long               flags;

	if (!controller || !handler_priv || !evt_bit_mask_arr) {
		CAM_ERR(CAM_ISP,
			"Inval params: ctlr=%pK hdl_priv=%pK bit_mask_arr=%pK",
			controller, handler_priv, evt_bit_mask_arr);
		return -EINVAL;
	}

	if (!top_half_handler) {
		CAM_ERR(CAM_ISP, "Missing top half handler");
		return -EINVAL;
	}

	if (bottom_half_handler &&
		(!bottom_half || !bottom_half_enqueue_func)) {
		CAM_ERR(CAM_ISP,
			"Invalid params: bh_handler=%pK bh=%pK bh_enq_f=%pK",
			bottom_half_handler,
			bottom_half,
			bottom_half_enqueue_func);
		return -EINVAL;
	}

	if (priority >= CAM_IRQ_PRIORITY_MAX) {
		CAM_ERR(CAM_ISP, "Invalid priority=%u, max=%u", priority,
			CAM_IRQ_PRIORITY_MAX);
		return -EINVAL;
	}

	evt_handler = kzalloc(sizeof(struct cam_irq_evt_handler), GFP_KERNEL);
	if (!evt_handler) {
		CAM_DBG(CAM_ISP, "Error allocating hlist_node");
		return -ENOMEM;
	}

	evt_handler->evt_bit_mask_arr = kzalloc(sizeof(uint32_t) *
		controller->num_registers, GFP_KERNEL);
	if (!evt_handler->evt_bit_mask_arr) {
		CAM_DBG(CAM_ISP, "Error allocating hlist_node");
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
	evt_handler->bottom_half_enqueue_func = bottom_half_enqueue_func;
	evt_handler->index                    = controller->hdl_idx++;

	/* Avoid rollover to negative values */
	if (controller->hdl_idx > 0x3FFFFFFF)
		controller->hdl_idx = 1;

	write_lock_irqsave(&controller->rw_lock, flags);
	for (i = 0; i < controller->num_registers; i++) {
		controller->irq_register_arr[i].top_half_enable_mask[priority]
			|= evt_bit_mask_arr[i];

		irq_mask = cam_io_r_mb(controller->mem_base +
			controller->irq_register_arr[i].mask_reg_offset);
		irq_mask |= evt_bit_mask_arr[i];

		cam_io_w_mb(irq_mask, controller->mem_base +
			controller->irq_register_arr[i].mask_reg_offset);
	}
	write_unlock_irqrestore(&controller->rw_lock, flags);

	list_add_tail(&evt_handler->list_node,
		&controller->evt_handler_list_head);
	list_add_tail(&evt_handler->th_list_node,
		&controller->th_list_head[priority]);

	return evt_handler->index;

free_evt_handler:
	kfree(evt_handler);
	evt_handler = NULL;

	return rc;
}

int cam_irq_controller_enable_irq(void *irq_controller, uint32_t handle)
{
	struct cam_irq_controller  *controller  = irq_controller;
	struct cam_irq_evt_handler *evt_handler = NULL;
	struct cam_irq_evt_handler *evt_handler_temp;
	unsigned long               flags;
	unsigned int                i;
	uint32_t                    irq_mask;
	uint32_t                    found = 0;
	int                         rc = -EINVAL;

	if (!controller)
		return rc;

	list_for_each_entry_safe(evt_handler, evt_handler_temp,
		&controller->evt_handler_list_head, list_node) {
		if (evt_handler->index == handle) {
			CAM_DBG(CAM_ISP, "enable item %d", handle);
			found = 1;
			rc = 0;
			break;
		}
	}

	if (!found)
		return rc;

	write_lock_irqsave(&controller->rw_lock, flags);
	for (i = 0; i < controller->num_registers; i++) {
		controller->irq_register_arr[i].
		top_half_enable_mask[evt_handler->priority] |=
			evt_handler->evt_bit_mask_arr[i];

		irq_mask = cam_io_r_mb(controller->mem_base +
			controller->irq_register_arr[i].
			mask_reg_offset);
		irq_mask |= evt_handler->evt_bit_mask_arr[i];

		cam_io_w_mb(irq_mask, controller->mem_base +
		controller->irq_register_arr[i].mask_reg_offset);
	}
	write_unlock_irqrestore(&controller->rw_lock, flags);

	return rc;
}

int cam_irq_controller_disable_irq(void *irq_controller, uint32_t handle)
{
	struct cam_irq_controller  *controller  = irq_controller;
	struct cam_irq_evt_handler *evt_handler = NULL;
	struct cam_irq_evt_handler *evt_handler_temp;
	unsigned long               flags;
	unsigned int                i;
	uint32_t                    irq_mask;
	uint32_t                    found = 0;
	int                         rc = -EINVAL;

	if (!controller)
		return rc;

	list_for_each_entry_safe(evt_handler, evt_handler_temp,
		&controller->evt_handler_list_head, list_node) {
		if (evt_handler->index == handle) {
			CAM_DBG(CAM_ISP, "disable item %d", handle);
			found = 1;
			rc = 0;
			break;
		}
	}

	if (!found)
		return rc;

	write_lock_irqsave(&controller->rw_lock, flags);
	for (i = 0; i < controller->num_registers; i++) {
		controller->irq_register_arr[i].
		top_half_enable_mask[evt_handler->priority] &=
			~(evt_handler->evt_bit_mask_arr[i]);

		irq_mask = cam_io_r_mb(controller->mem_base +
			controller->irq_register_arr[i].
			mask_reg_offset);
		irq_mask &= ~(evt_handler->evt_bit_mask_arr[i]);

		cam_io_w_mb(irq_mask, controller->mem_base +
			controller->irq_register_arr[i].
			mask_reg_offset);

		/* Clear the IRQ bits of this handler */
		cam_io_w_mb(evt_handler->evt_bit_mask_arr[i],
			controller->mem_base +
			controller->irq_register_arr[i].
			clear_reg_offset);

		if (controller->global_clear_offset)
			cam_io_w_mb(
				controller->global_clear_bitmask,
				controller->mem_base +
				controller->global_clear_offset);
	}
	write_unlock_irqrestore(&controller->rw_lock, flags);

	return rc;
}

int cam_irq_controller_unsubscribe_irq(void *irq_controller,
	uint32_t handle)
{
	struct cam_irq_controller  *controller  = irq_controller;
	struct cam_irq_evt_handler *evt_handler = NULL;
	struct cam_irq_evt_handler *evt_handler_temp;
	uint32_t                    i;
	uint32_t                    found = 0;
	uint32_t                    irq_mask;
	unsigned long               flags;
	int                         rc = -EINVAL;

	list_for_each_entry_safe(evt_handler, evt_handler_temp,
		&controller->evt_handler_list_head, list_node) {
		if (evt_handler->index == handle) {
			CAM_DBG(CAM_ISP, "unsubscribe item %d", handle);
			list_del_init(&evt_handler->list_node);
			list_del_init(&evt_handler->th_list_node);
			found = 1;
			rc = 0;
			break;
		}
	}

	if (found) {
		write_lock_irqsave(&controller->rw_lock, flags);
		for (i = 0; i < controller->num_registers; i++) {
			controller->irq_register_arr[i].
				top_half_enable_mask[evt_handler->priority] &=
				~(evt_handler->evt_bit_mask_arr[i]);

			irq_mask = cam_io_r_mb(controller->mem_base +
				controller->irq_register_arr[i].
				mask_reg_offset);
			irq_mask &= ~(evt_handler->evt_bit_mask_arr[i]);

			cam_io_w_mb(irq_mask, controller->mem_base +
				controller->irq_register_arr[i].
				mask_reg_offset);

			/* Clear the IRQ bits of this handler */
			cam_io_w_mb(evt_handler->evt_bit_mask_arr[i],
				controller->mem_base +
				controller->irq_register_arr[i].
				clear_reg_offset);
			if (controller->global_clear_offset)
				cam_io_w_mb(
					controller->global_clear_bitmask,
					controller->mem_base +
					controller->global_clear_offset);
		}
		write_unlock_irqrestore(&controller->rw_lock, flags);

		kfree(evt_handler->evt_bit_mask_arr);
		kfree(evt_handler);
	}

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
	struct cam_irq_evt_handler  *evt_handler)
{
	int i;

	for (i = 0; i < controller->num_registers; i++) {
		if (evt_handler->evt_bit_mask_arr[i] &
			controller->irq_status_arr[i])
			return true;
	}

	return false;
}

static void cam_irq_controller_th_processing(
	struct cam_irq_controller      *controller,
	struct list_head               *th_list_head)
{
	struct cam_irq_evt_handler     *evt_handler = NULL;
	struct cam_irq_th_payload      *th_payload = &controller->th_payload;
	bool                            is_irq_match;
	int                             rc = -EINVAL;
	int                             i;

	CAM_DBG(CAM_ISP, "Enter");

	if (list_empty(th_list_head))
		return;

	list_for_each_entry(evt_handler, th_list_head, th_list_node) {
		is_irq_match = cam_irq_controller_match_bit_mask(controller,
			evt_handler);

		if (!is_irq_match)
			continue;

		CAM_DBG(CAM_ISP, "match found");

		cam_irq_th_payload_init(th_payload);
		th_payload->handler_priv  = evt_handler->handler_priv;
		th_payload->num_registers = controller->num_registers;
		for (i = 0; i < controller->num_registers; i++) {
			th_payload->evt_status_arr[i] =
				controller->irq_status_arr[i] &
				evt_handler->evt_bit_mask_arr[i];
		}

		/*
		 * irq_status_arr[0] is dummy argument passed. the entire
		 * status array is passed in th_payload.
		 */
		if (evt_handler->top_half_handler)
			rc = evt_handler->top_half_handler(
				controller->irq_status_arr[0],
				(void *)th_payload);

		if (!rc && evt_handler->bottom_half_handler) {
			CAM_DBG(CAM_ISP, "Enqueuing bottom half for %s",
				controller->name);
			if (evt_handler->bottom_half_enqueue_func) {
				evt_handler->bottom_half_enqueue_func(
					evt_handler->bottom_half,
					evt_handler->handler_priv,
					th_payload->evt_payload_priv,
					evt_handler->bottom_half_handler);
			}
		}
	}

	CAM_DBG(CAM_ISP, "Exit");
}

irqreturn_t cam_irq_controller_handle_irq(int irq_num, void *priv)
{
	struct cam_irq_controller  *controller  = priv;
	bool         need_th_processing[CAM_IRQ_PRIORITY_MAX] = {false};
	int          i;
	int          j;

	if (!controller)
		return IRQ_NONE;

	CAM_DBG(CAM_ISP, "locking controller %pK name %s rw_lock %pK",
		controller, controller->name, &controller->rw_lock);
	read_lock(&controller->rw_lock);
	for (i = 0; i < controller->num_registers; i++) {
		controller->irq_status_arr[i] = cam_io_r_mb(
			controller->mem_base +
			controller->irq_register_arr[i].status_reg_offset);
		cam_io_w_mb(controller->irq_status_arr[i],
			controller->mem_base +
			controller->irq_register_arr[i].clear_reg_offset);
		CAM_DBG(CAM_ISP, "Read irq status%d (0x%x) = 0x%x", i,
			controller->irq_register_arr[i].status_reg_offset,
			controller->irq_status_arr[i]);
		for (j = 0; j < CAM_IRQ_PRIORITY_MAX; j++) {
			if (controller->irq_register_arr[i].
				top_half_enable_mask[j] &
				controller->irq_status_arr[i])
				need_th_processing[j] = true;
				CAM_DBG(CAM_ISP,
					"i %d j %d need_th_processing = %d",
					i, j, need_th_processing[j]);
		}
	}
	CAM_DBG(CAM_ISP, "unlocked controller %pK name %s rw_lock %pK",
		controller, controller->name, &controller->rw_lock);

	CAM_DBG(CAM_ISP, "Status Registers read Successful");

	if (controller->global_clear_offset)
		cam_io_w_mb(controller->global_clear_bitmask,
			controller->mem_base + controller->global_clear_offset);

	CAM_DBG(CAM_ISP, "Status Clear done");

	for (i = 0; i < CAM_IRQ_PRIORITY_MAX; i++) {
		if (need_th_processing[i]) {
			CAM_DBG(CAM_ISP, "Invoke TH processing");
			cam_irq_controller_th_processing(controller,
				&controller->th_list_head[i]);
		}
	}
	read_unlock(&controller->rw_lock);

	return IRQ_HANDLED;
}
