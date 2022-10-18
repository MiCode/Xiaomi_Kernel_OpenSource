/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_IRQ_CONTROLLER_H_
#define _CAM_IRQ_CONTROLLER_H_

#include <linux/interrupt.h>

#define CAM_IRQ_BITS_PER_REGISTER      32

/*
 * enum cam_irq_priority_level:
 * @Brief:                  Priority levels for IRQ events.
 *                          Priority_0 events will be serviced before
 *                          Priority_1 if they these bits are set in the same
 *                          Status Read. And so on upto Priority_4.
 *
 *                          Default Priority is Priority_4.
 */
enum cam_irq_priority_level {
	CAM_IRQ_PRIORITY_0,
	CAM_IRQ_PRIORITY_1,
	CAM_IRQ_PRIORITY_2,
	CAM_IRQ_PRIORITY_3,
	CAM_IRQ_PRIORITY_4,
	CAM_IRQ_PRIORITY_MAX,
};

/**
 * enum cam_irq_event_group:
 * @brief: Event groups to filter out events while handling. Use group 0 as default.
 */
enum cam_irq_event_group {
	CAM_IRQ_EVT_GROUP_0,
	CAM_IRQ_EVT_GROUP_1,
	CAM_IRQ_EVT_GROUP_2,
};

#define CAM_IRQ_MAX_DEPENDENTS 2

/*
 * struct cam_irq_register_set:
 * @Brief:                  Structure containing offsets of IRQ related
 *                          registers belonging to a Set
 *
 * @mask_reg_offset:        Offset of IRQ MASK register
 * @clear_reg_offset:       Offset of IRQ CLEAR register
 * @status_reg_offset:      Offset of IRQ STATUS register
 */
struct cam_irq_register_set {
	uint32_t                       mask_reg_offset;
	uint32_t                       clear_reg_offset;
	uint32_t                       status_reg_offset;
};

/*
 * struct cam_irq_controller_reg_info:
 * @Brief:                  Structure describing the IRQ registers
 *
 * @num_registers:          Number of sets(mask/clear/status) of IRQ registers
 * @irq_reg_set:            Array of Register Set Offsets.
 *                          Length of array = num_registers
 * @global_clear_offset:    Offset of Global IRQ Clear register. This register
 *                          contains the BIT that needs to be set for the CLEAR
 *                          to take effect
 * @global_clear_bitmask:   Bitmask needed to be used in Global Clear register
 *                          for Clear IRQ cmd to take effect
 * @clear_all_bitmask:      Bitmask that specifies which bits should be written
 *                          to clear register when it is to be cleared forcefully
 */
struct cam_irq_controller_reg_info {
	uint32_t                      num_registers;
	struct cam_irq_register_set  *irq_reg_set;
	uint32_t                      global_clear_offset;
	uint32_t                      global_clear_bitmask;
	uint32_t                      clear_all_bitmask;
};

/*
 * struct cam_irq_th_payload:
 * @Brief:                  Event payload structure. This structure will be
 *                          passed to the Top Half handler functions.
 *
 * @handler_priv:           Private Data of handling object set when
 *                          subscribing to IRQ event.
 * @num_registers:          Length of evt_bit_mask Array below
 * @evt_status_arr:         Array of Status bitmask read from registers.
 *                          Length of array = num_registers
 * @evt_payload_priv:       Private payload pointer which can be set by Top
 *                          Half handler for use in Bottom Half.
 */
struct cam_irq_th_payload {
	void       *handler_priv;
	uint32_t    num_registers;
	uint32_t   *evt_status_arr;
	void       *evt_payload_priv;
};

/*
 * cam_irq_th_payload_init()
 *
 * @brief:              Initialize the top half payload structure
 *
 * @th_payload:         Top Half payload structure to Initialize
 *
 * @return:             Void
 */
static inline void cam_irq_th_payload_init(
	struct cam_irq_th_payload *th_payload) {
	th_payload->handler_priv = NULL;
	th_payload->evt_payload_priv = NULL;
}

typedef int (*CAM_IRQ_HANDLER_TOP_HALF)(uint32_t evt_id,
	struct cam_irq_th_payload *th_payload);

typedef int (*CAM_IRQ_HANDLER_BOTTOM_HALF)(void *handler_priv,
	void *evt_payload_priv);

typedef void (*CAM_IRQ_BOTTOM_HALF_ENQUEUE_FUNC)(void *bottom_half,
	void *bh_cmd, void *handler_priv, void *evt_payload_priv,
	CAM_IRQ_HANDLER_BOTTOM_HALF);

typedef int (*CAM_IRQ_GET_TASKLET_PAYLOAD_FUNC)(void *bottom_half,
	void **bh_cmd);

typedef void (*CAM_IRQ_PUT_TASKLET_PAYLOAD_FUNC)(void *bottom_half,
	void **bh_cmd);

struct cam_irq_bh_api {
	CAM_IRQ_BOTTOM_HALF_ENQUEUE_FUNC bottom_half_enqueue_func;
	CAM_IRQ_GET_TASKLET_PAYLOAD_FUNC get_bh_payload_func;
	CAM_IRQ_PUT_TASKLET_PAYLOAD_FUNC put_bh_payload_func;
};

/*
 * cam_irq_controller_init()
 *
 * @brief:              Create and Initialize IRQ Controller.
 *
 * @name:               Name of IRQ Controller block
 * @mem_base:           Mapped base address of register space to which
 *                      register offsets are added to access registers
 * @register_info:      Register Info structure associated with this Controller
 * @irq_controller:     Pointer to IRQ Controller that will be filled if
 *                      initialization is successful
 *
 * @return:             0: Success
 *                      Negative: Failure
 */
int cam_irq_controller_init(const char       *name,
	void __iomem                         *mem_base,
	struct cam_irq_controller_reg_info   *register_info,
	void                                **irq_controller);

/*
 * cam_irq_controller_subscribe_irq()
 *
 * @brief:               Subscribe to certain IRQ events.
 *
 * @irq_controller:      Pointer to IRQ Controller that controls this event IRQ
 * @priority:            Priority level of these events used if multiple events
 *                       are SET in the Status Register
 * @evt_bit_mask_arr:    evt_bit_mask that has the bits set for IRQs to
 *                       subscribe for
 * @handler_priv:        Private data that will be passed to the Top/Bottom Half
 *                       handler function
 * @top_half_handler:    Top half Handler callback function
 * @bottom_half_handler: Bottom half Handler callback function
 * @bottom_half:         Pointer to bottom_half implementation on which to
 *                       enqueue the event for further handling
 * @bottom_half_enqueue_func:
 *                       Function used to enqueue the bottom_half event
 * @evt_grp:             Event group to which this event must belong to (use group 0 as default)
 *
 * @return:              Positive: Success. Value represents handle which is
 *                                 to be used to unsubscribe
 *                       Negative: Failure
 */
int cam_irq_controller_subscribe_irq(void *irq_controller,
	enum cam_irq_priority_level        priority,
	uint32_t                          *evt_bit_mask_arr,
	void                              *handler_priv,
	CAM_IRQ_HANDLER_TOP_HALF           top_half_handler,
	CAM_IRQ_HANDLER_BOTTOM_HALF        bottom_half_handler,
	void                              *bottom_half,
	struct cam_irq_bh_api             *irq_bh_api,
	enum cam_irq_event_group           evt_grp);

/*
 * cam_irq_controller_unsubscribe_irq()
 *
 * @brief:               Unsubscribe to IRQ events previously subscribed to.
 *
 * @irq_controller:      Pointer to IRQ Controller that controls this event IRQ
 * @handle:              Handle returned on successful subscribe used to
 *                       identify the handler object
 *
 * @return:              0: Success
 *                       Negative: Failure
 */
int cam_irq_controller_unsubscribe_irq(void *irq_controller,
	uint32_t handle);

/*
 * cam_irq_controller_deinit()
 *
 * @brief:              Deinitialize IRQ Controller.
 *
 * @irq_controller:     Pointer to IRQ Controller that needs to be
 *                      deinitialized
 *
 * @return:             0: Success
 *                      Negative: Failure
 */
int cam_irq_controller_deinit(void **irq_controller);

/*
 * cam_irq_controller_handle_irq()
 *
 * @brief:              Function that should be registered with the IRQ line.
 *                      This is the first function to be called when the IRQ
 *                      is fired. It will read the Status register and Clear
 *                      the IRQ bits. It will then call the top_half handlers
 *                      and enqueue the result to bottom_half.
 *
 * @irq_num:            Number of IRQ line that was set that lead to this
 *                      function being called
 * @priv:               Private data registered with request_irq is passed back
 *                      here. This private data should be the irq_controller
 *                      structure.
 *
 * @return:             IRQ_HANDLED/IRQ_NONE
 */
irqreturn_t cam_irq_controller_handle_irq(int irq_num, void *priv, int evt_grp);

/*
 * cam_irq_controller_disable_irq()
 *
 * @brief:              Disable the interrupts on given controller.
 *                      Unsubscribe will disable the IRQ by default, so this is
 *                      only needed if between subscribe/unsubscribe there is
 *                      need to disable IRQ again
 *
 * @irq_controller:     Pointer to IRQ Controller that controls the registered
 *                      events to it.
 * @handle:             Handle returned on successful subscribe, used to
 *                      identify the handler object
 *
 * @return:             0: events found and disabled
 *                      Negative: events not registered on this controller
 */
int cam_irq_controller_disable_irq(void *irq_controller, uint32_t handle);

/*
 * cam_irq_controller_enable_irq()
 *
 * @brief:              Enable the interrupts on given controller.
 *                      Subscribe will enable the IRQ by default, so this is
 *                      only needed if between subscribe/unsubscribe there is
 *                      need to enable IRQ again
 *
 * @irq_controller:     Pointer to IRQ Controller that controls the registered
 *                      events to it.
 * @handle:             Handle returned on successful subscribe, used to
 *                      identify the handler object
 *
 * @return:             0: events found and enabled
 *                      Negative: events not registered on this controller
 */
int cam_irq_controller_enable_irq(void *irq_controller, uint32_t handle);

/*
 * cam_irq_controller_disable_all()
 *
 * @brief:              This function clears and masks all the irq bits
 *
 * @priv:               Private data registered with request_irq is passed back
 *                      here. This private data should be the irq_controller
 *                      structure.
 */
void cam_irq_controller_disable_all(void *priv);

/*
 * cam_irq_controller_update_irq()
 *
 * @brief:              Enable/Disable the interrupts on given controller.
 *                      Dynamically any irq can be disabled or enabled.
 *
 * @irq_controller:     Pointer to IRQ Controller that controls the registered
 *                      events to it.
 * @handle:             Handle returned on successful subscribe, used to
 *                      identify the handler object
 *
 * @enable:             Flag to indicate if disable or enable the irq.
 *
 * @irq_mask:           IRQ mask to be enabled or disabled.
 *
 * @return:             0: events found and enabled
 *                      Negative: events not registered on this controller
 */
int cam_irq_controller_update_irq(void *irq_controller, uint32_t handle,
	bool enable, uint32_t *irq_mask);

/**
 * cam_irq_controller_register_dependent
 * @brief:                 Register one IRQ controller as dependent for another IRQ controller
 *
 * @primary_controller:    Controller whose top half will invoke handle_irq function of secondary
 *                         controller
 * @secondary_controller:  Controller whose handle_irq function that will be invoked from primary
 *                         controller
 *
 * @return:                0: successfully registered
 *                         Negative: failed to register as dependent
 */
int cam_irq_controller_register_dependent(void *primary_controller, void *secondary_controller,
	uint32_t *mask);

/**
 * cam_irq_controller_register_dependent
 * @brief:                 Unregister previously registered dependent IRQ controller
 *
 * @primary_controller:    Controller whose top half will invoke handle_irq function of secondary
 *                         controller
 * @secondary_controller:  Controller whose handle_irq function that will be invoked from primary
 *                         controller
 *
 * @return:                0: successfully unregistered
 *                         Negative: failed to unregister dependent
 */
int cam_irq_controller_unregister_dependent(void *primary_controller, void *secondary_controller);

#endif /* _CAM_IRQ_CONTROLLER_H_ */
