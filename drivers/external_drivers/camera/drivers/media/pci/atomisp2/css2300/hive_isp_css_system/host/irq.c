#include "irq.h"

#ifndef __INLINE_GP_DEVICE__
#define __INLINE_GP_DEVICE__
#endif
#include "gp_device.h"

/* MW: This is an HRT backend function from "thread" */
#include "platform_support.h"	/* hrt_sleep() */

STORAGE_CLASS_INLINE void irq_wait_for_write_complete(
	const irq_ID_t		ID);

STORAGE_CLASS_INLINE bool any_irq_channel_enabled(
	const irq_ID_t				ID);

STORAGE_CLASS_INLINE irq_ID_t virq_get_irq_id(
	const virq_id_t		irq_ID,
	unsigned int		*channel_ID);

#ifndef __INLINE_IRQ__
#include "irq_private.h"
#endif /* __INLINE_IRQ__ */

void irq_clear_all(
	const irq_ID_t				ID)
{
	hrt_data	mask = 0xFFFFFFFF;

assert(ID < N_IRQ_ID);

	irq_reg_store(ID,
		_HRT_IRQ_CONTROLLER_CLEAR_REG_IDX, mask);
return;
}

/*
 * Do we want the user to be able to set the signalling method ?
 */
void irq_enable_channel(
	const irq_ID_t				ID,
    const unsigned int			irq_id)
{
	unsigned int mask = irq_reg_load(ID,
		_HRT_IRQ_CONTROLLER_MASK_REG_IDX);
	unsigned int enable = irq_reg_load(ID,
		_HRT_IRQ_CONTROLLER_ENABLE_REG_IDX);
	unsigned int edge_in = irq_reg_load(ID,
		_HRT_IRQ_CONTROLLER_EDGE_REG_IDX);
	unsigned int me = 1U << irq_id;

assert(irq_id < hrt_isp_css_irq_num_irqs);

	mask |= me;
	enable |= me;
	edge_in |= me;	/* rising edge */

/* to avoid mishaps configuration must follow the following order */

/* mask this interrupt */
	irq_reg_store(ID,
		_HRT_IRQ_CONTROLLER_MASK_REG_IDX, mask & ~me);
/* rising edge at input */
	irq_reg_store(ID,
		_HRT_IRQ_CONTROLLER_EDGE_REG_IDX, edge_in);
/* enable interrupt to output */
	irq_reg_store(ID,
		_HRT_IRQ_CONTROLLER_ENABLE_REG_IDX, enable);
/* clear current irq only */
	irq_reg_store(ID,
		_HRT_IRQ_CONTROLLER_CLEAR_REG_IDX, me);
/* unmask interrupt from input */
	irq_reg_store(ID,
		_HRT_IRQ_CONTROLLER_MASK_REG_IDX, mask);

	irq_wait_for_write_complete(ID);

return;
}

void irq_enable_pulse(
	const irq_ID_t	ID,
	bool 			pulse)
{
	unsigned int edge_out = 0x0;
	if (pulse) {
		edge_out = 0xffffffff;
	}
	/* output is given as edge, not pulse */
	irq_reg_store(ID,
		_HRT_IRQ_CONTROLLER_EDGE_NOT_PULSE_REG_IDX, edge_out);
return;
}

void irq_disable_channel(
	const irq_ID_t				ID,
	const unsigned int			irq_id)
{
	unsigned int mask = irq_reg_load(ID,
		_HRT_IRQ_CONTROLLER_MASK_REG_IDX);
	unsigned int enable = irq_reg_load(ID,
		_HRT_IRQ_CONTROLLER_ENABLE_REG_IDX);
	unsigned int me = 1U << irq_id;

assert(irq_id < hrt_isp_css_irq_num_irqs);

	mask &= ~me;
	enable &= ~me;

/* enable interrupt to output */
	irq_reg_store(ID,
		_HRT_IRQ_CONTROLLER_ENABLE_REG_IDX, enable);
/* unmask interrupt from input */
	irq_reg_store(ID,
		_HRT_IRQ_CONTROLLER_MASK_REG_IDX, mask);
/* clear current irq only */
	irq_reg_store(ID,
		_HRT_IRQ_CONTROLLER_CLEAR_REG_IDX, me);

	irq_wait_for_write_complete(ID);

return;
}

enum hrt_isp_css_irq_status irq_get_channel_id(
	const irq_ID_t				ID,
	unsigned int				*irq_id)
{
	unsigned int irq_status = irq_reg_load(ID,
		_HRT_IRQ_CONTROLLER_STATUS_REG_IDX);
	enum hrt_isp_css_irq idx = hrt_isp_css_irq_num_irqs;
	enum hrt_isp_css_irq_status status = hrt_isp_css_irq_status_success;

assert(irq_id != NULL);

/* find the first irq bit */
	for (idx = 0; idx < hrt_isp_css_irq_num_irqs; idx++) {
		if (irq_status & (1U << idx))
			break;
	}
	if (idx == hrt_isp_css_irq_num_irqs)
		return hrt_isp_css_irq_status_error;

/* now check whether there are more bits set */
	if (irq_status != (1U << idx))
		status = hrt_isp_css_irq_status_more_irqs;

	irq_reg_store(ID,
		_HRT_IRQ_CONTROLLER_CLEAR_REG_IDX, 1U << idx);

	irq_wait_for_write_complete(ID);

	if (irq_id)
		*irq_id = (unsigned int)idx;

return status;
}

void irq_raise(
	const irq_ID_t				ID,
	const irq_sw_channel_id_t	irq_id)
{
assert(ID == IRQ0_ID);
assert(IRQ_BASE[ID] != (hrt_address)-1);
assert(irq_id < N_IRQ_SW_CHANNEL_ID);
	(void)ID;

/* The SW IRQ pins are remapped to offset zero */
	gp_device_reg_store(GP_DEVICE0_ID,
		_REG_GP_IRQ_REQUEST_ADDR,
		(1U<<(irq_id - hrt_isp_css_irq_sw_0)));
#ifdef HRT_CSIM
	hrt_sleep();
#endif
	gp_device_reg_store(GP_DEVICE0_ID,
		_REG_GP_IRQ_REQUEST_ADDR, 0);
return;
}

void irq_controller_get_state(
	const irq_ID_t				ID,
	irq_controller_state_t		*state)
{
assert(ID < N_IRQ_ID);
assert(state != NULL);
	if (state == NULL)
		return;

	state->irq_edge = irq_reg_load(ID,
		_HRT_IRQ_CONTROLLER_EDGE_REG_IDX);
	state->irq_mask = irq_reg_load(ID,
		_HRT_IRQ_CONTROLLER_MASK_REG_IDX);
	state->irq_status = irq_reg_load(ID,
		_HRT_IRQ_CONTROLLER_STATUS_REG_IDX);
	state->irq_enable = irq_reg_load(ID,
		_HRT_IRQ_CONTROLLER_ENABLE_REG_IDX);
	state->irq_level_not_pulse = irq_reg_load(ID,
		_HRT_IRQ_CONTROLLER_EDGE_NOT_PULSE_REG_IDX);
return;
}

void virq_enable_channel(
	const virq_id_t				irq_ID,
	const bool					en)
{
	unsigned int	channel_ID;
	irq_ID_t		ID = virq_get_irq_id(irq_ID, &channel_ID);
	
assert(ID < N_IRQ_ID);

	if (en) {
/* */
		irq_enable_channel(ID, channel_ID);
	} else {
/* */
		irq_disable_channel(ID, channel_ID);
	}
return;
}


void virq_clear_all(void)
{
	irq_clear_all(IRQ0_ID);
return;
}

enum hrt_isp_css_irq_status virq_get_channel_signals(
	virq_info_t					*irq_info)
{
	enum hrt_isp_css_irq_status irq_status = hrt_isp_css_irq_status_error;

assert(irq_info != NULL);


	if (any_irq_channel_enabled(IRQ0_ID)) {
		hrt_data	irq_data = irq_reg_load(IRQ0_ID,
			_HRT_IRQ_CONTROLLER_STATUS_REG_IDX);

		if (irq_data != 0) {
/* The error condition is an IRQ pulse received with no IRQ status written */
			irq_status = hrt_isp_css_irq_status_success;
		}
		if (irq_info != NULL)
			irq_info->irq_status_reg[IRQ0_ID] |= irq_data;

		irq_reg_store(IRQ0_ID,
			_HRT_IRQ_CONTROLLER_CLEAR_REG_IDX, irq_data);

		irq_wait_for_write_complete(IRQ0_ID);
	}

return irq_status;
}

void virq_clear_info(
	virq_info_t					*irq_info)
{
	irq_info->irq_status_reg[IRQ0_ID] = 0;
return;
}

enum hrt_isp_css_irq_status virq_get_channel_id(
	virq_id_t					*irq_id)
{
	unsigned int	_irq_id;
	enum hrt_isp_css_irq_status	irq_status = irq_get_channel_id(IRQ0_ID, &_irq_id);
	if (irq_id != NULL) {
		*irq_id = (virq_id_t)_irq_id;
	}
return irq_status;
}

STORAGE_CLASS_INLINE void irq_wait_for_write_complete(
	const irq_ID_t		ID)
{
assert(ID < N_IRQ_ID);
assert(IRQ_BASE[ID] != (hrt_address)-1);
	(void)device_load_uint32(IRQ_BASE[ID] +
		_HRT_IRQ_CONTROLLER_ENABLE_REG_IDX*sizeof(hrt_data));
#ifdef HRT_CSIM
	hrt_sleep();
#endif
return;
}

STORAGE_CLASS_INLINE bool any_irq_channel_enabled(
	const irq_ID_t				ID)
{
	hrt_data	en_reg;

assert(ID < N_IRQ_ID);

	en_reg = irq_reg_load(ID,
		_HRT_IRQ_CONTROLLER_ENABLE_REG_IDX);

return (en_reg != 0);
}

STORAGE_CLASS_INLINE irq_ID_t virq_get_irq_id(
	const virq_id_t		irq_ID,
	unsigned int		*channel_ID)
{
	irq_ID_t ID = IRQ0_ID;

assert(channel_ID != NULL);
	if (channel_ID == NULL) {
		return N_IRQ_ID;
	}

	*channel_ID = (unsigned int)irq_ID;

return ID;
}
