#ifndef __INPUT_FORMATTER_PUBLIC_H_INCLUDED__
#define __INPUT_FORMATTER_PUBLIC_H_INCLUDED__

#include <stdbool.h>

#include "system_types.h"

/*! Reset INPUT_FORMATTER[ID]
 
 \param	ID[in]				INPUT_FORMATTER identifier

 \return none, reset(INPUT_FORMATTER[ID])
 */
extern void input_formatter_rst(
	const input_formatter_ID_t		ID);

/*! Set the blocking mode of INPUT_FORMATTER[ID]
 
 \param	ID[in]				INPUT_FORMATTER identifier
 \param	enable[in]			blocking enable flag

 \use
	- In HW, the capture unit will deliver an infinite stream of frames,
	  the input formatter will synchronise on the first SOF. In simulation
	  there are only a fixed number of frames, presented only once. By
	  enabling blocking the inputformatter will wait on the first presented
	  frame, thus avoiding race in the simulation setup.

 \return none, INPUT_FORMATTER[ID].blocking_mode = enable
 */
extern void input_formatter_set_fifo_blocking_mode(
	const input_formatter_ID_t		ID,
	const bool						enable);

/*! Return the data alignment of INPUT_FORMATTER[ID]
 
 \param	ID[in]				INPUT_FORMATTER identifier

 \return alignment(INPUT_FORMATTER[ID].data)
 */
extern unsigned int input_formatter_get_alignment(
	const input_formatter_ID_t		ID);

/*! Read the control registers of INPUT_FORMATTER[ID]
 
 \param	ID[in]				INPUT_FORMATTER identifier
 \param	state[out]			input formatter state structure

 \return none, state = INPUT_FORMATTER[ID].state
 */
extern void input_formatter_get_state(
	const input_formatter_ID_t		ID,
	input_formatter_state_t			*state);

/*! Write to a control register of INPUT_FORMATTER[ID]
 
 \param	ID[in]				INPUT_FORMATTER identifier
 \param	reg_addr[in]		register byte address
 \param value[in]			The data to be written

 \return none, INPUT_FORMATTER[ID].ctrl[reg] = value
 */
STORAGE_CLASS_INPUT_FORMATTER_H void input_formatter_reg_store(
	const input_formatter_ID_t	ID,
	const unsigned int			reg_addr,
	const hrt_data				value);

/*! Read from a control register of INPUT_FORMATTER[ID]
 
 \param	ID[in]				INPUT_FORMATTER identifier
 \param	reg_addr[in]		register byte address
 \param value[in]			The data to be written

 \return INPUT_FORMATTER[ID].ctrl[reg]
 */
STORAGE_CLASS_INPUT_FORMATTER_H hrt_data input_formatter_reg_load(
	const input_formatter_ID_t	ID,
	const unsigned int			reg_addr);

#endif /* __INPUT_FORMATTER_PUBLIC_H_INCLUDED__ */
