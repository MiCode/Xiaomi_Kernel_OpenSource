#ifndef __INPUT_SYSTEM_PUBLIC_H_INCLUDED__
#define __INPUT_SYSTEM_PUBLIC_H_INCLUDED__

#include "stdbool.h"

typedef struct input_system_state_s		input_system_state_t;
typedef struct receiver_state_s			receiver_state_t;

/*! Read the state of INPUT_SYSTEM[ID]

 \param	ID[in]				INPUT_SYSTEM identifier
 \param	state[out]			input system state structure

 \return none, state = INPUT_SYSTEM[ID].state
 */
extern void input_system_get_state(
	const input_system_ID_t		ID,
	input_system_state_t		*state);

/*! Read the state of RECEIVER[ID]

 \param	ID[in]				RECEIVER identifier
 \param	state[out]			receiver state structure

 \return none, state = RECEIVER[ID].state
 */
extern void receiver_get_state(
	const rx_ID_t				ID,
	receiver_state_t			*state);

/*! Flag whether a MIPI format is YUV420

 \param	mipi_format[in]		MIPI format

 \return mipi_format == YUV420
 */
extern bool is_mipi_format_yuv420(
	const mipi_format_t			mipi_format);

/*! Set compression parameters for cfg[cfg_ID] of RECEIVER[ID]

 \param	ID[in]				RECEIVER identifier
 \param	cfg_ID[in]			Configuration identifier
 \param	comp[in]			Compression method
 \param	pred[in]			Predictor method

 \NOTE: the storage of compression configuration is
        implementation specific. The config can be
        carried either on MIPI ports or on MIPI channels

 \return none, RECEIVER[ID].cfg[cfg_ID] = {comp, pred}
 */
extern void receiver_set_compression(
	const rx_ID_t				ID,
	const unsigned int			cfg_ID,
	const mipi_compressor_t		comp,
	const mipi_predictor_t		pred);

/*! Enable PORT[port_ID] of RECEIVER[ID]

 \param	ID[in]				RECEIVER identifier
 \param	port_ID[in]			mipi PORT identifier
 \param	cnd[in]				irq predicate

 \return None, enable(RECEIVER[ID].PORT[port_ID])
 */
extern void receiver_port_enable(
	const rx_ID_t				ID,
	const mipi_port_ID_t		port_ID,
	const bool					cnd);

/*! Flag if PORT[port_ID] of RECEIVER[ID] is enabled

 \param	ID[in]				RECEIVER identifier
 \param	port_ID[in]			mipi PORT identifier

 \return enable(RECEIVER[ID].PORT[port_ID]) == true
 */
extern bool is_receiver_port_enabled(
	const rx_ID_t				ID,
	const mipi_port_ID_t		port_ID);

/*! Enable the IRQ channels of PORT[port_ID] of RECEIVER[ID]

 \param	ID[in]				RECEIVER identifier
 \param	port_ID[in]			mipi PORT identifier
 \param	irq_info[in]		irq channels

 \return None, enable(RECEIVER[ID].PORT[port_ID].irq_info)
 */
extern void receiver_irq_enable(
	const rx_ID_t				ID,
	const mipi_port_ID_t		port_ID,
	const rx_irq_info_t			irq_info);

/*! Return the IRQ status of PORT[port_ID] of RECEIVER[ID]

 \param	ID[in]				RECEIVER identifier
 \param	port_ID[in]			mipi PORT identifier

 \return RECEIVER[ID].PORT[port_ID].irq_info
 */
extern rx_irq_info_t receiver_get_irq_info(
	const rx_ID_t				ID,
	const mipi_port_ID_t		port_ID);

/*! Clear the IRQ status of PORT[port_ID] of RECEIVER[ID]

 \param	ID[in]				RECEIVER identifier
 \param	port_ID[in]			mipi PORT identifier
 \param	irq_info[in]		irq status

 \return None, clear(RECEIVER[ID].PORT[port_ID].irq_info)
 */
extern void receiver_irq_clear(
	const rx_ID_t				ID,
	const mipi_port_ID_t		port_ID,
	const rx_irq_info_t			irq_info);

/*! Write to a control register of INPUT_SYSTEM[ID]

 \param	ID[in]				INPUT_SYSTEM identifier
 \param	reg[in]				register index
 \param value[in]			The data to be written

 \return none, INPUT_SYSTEM[ID].ctrl[reg] = value
 */
STORAGE_CLASS_INPUT_SYSTEM_H void input_system_reg_store(
	const input_system_ID_t		ID,
	const unsigned int			reg,
	const hrt_data				value);

/*! Read from a control register of INPUT_SYSTEM[ID]

 \param	ID[in]				INPUT_SYSTEM identifier
 \param	reg[in]				register index
 \param value[in]			The data to be written

 \return INPUT_SYSTEM[ID].ctrl[reg]
 */
STORAGE_CLASS_INPUT_SYSTEM_H hrt_data input_system_reg_load(
	const input_system_ID_t		ID,
	const unsigned int			reg);

/*! Write to a control register of RECEIVER[ID]

 \param	ID[in]				RECEIVER identifier
 \param	reg[in]				register index
 \param value[in]			The data to be written

 \return none, RECEIVER[ID].ctrl[reg] = value
 */
STORAGE_CLASS_INPUT_SYSTEM_H void receiver_reg_store(
	const rx_ID_t				ID,
	const unsigned int			reg,
	const hrt_data				value);

/*! Read from a control register of RECEIVER[ID]

 \param	ID[in]				RECEIVER identifier
 \param	reg[in]				register index
 \param value[in]			The data to be written

 \return RECEIVER[ID].ctrl[reg]
 */
STORAGE_CLASS_INPUT_SYSTEM_H hrt_data receiver_reg_load(
	const rx_ID_t				ID,
	const unsigned int			reg);

/*! Write to a control register of PORT[port_ID] of RECEIVER[ID]

 \param	ID[in]				RECEIVER identifier
 \param	port_ID[in]			mipi PORT identifier
 \param	reg[in]				register index
 \param value[in]			The data to be written

 \return none, RECEIVER[ID].PORT[port_ID].ctrl[reg] = value
 */
STORAGE_CLASS_INPUT_SYSTEM_H void receiver_port_reg_store(
	const rx_ID_t				ID,
	const mipi_port_ID_t		port_ID,
	const unsigned int			reg,
	const hrt_data				value);

/*! Read from a control register PORT[port_ID] of of RECEIVER[ID]

 \param	ID[in]				RECEIVER identifier
 \param	port_ID[in]			mipi PORT identifier
 \param	reg[in]				register index
 \param value[in]			The data to be written

 \return RECEIVER[ID].PORT[port_ID].ctrl[reg]
 */
STORAGE_CLASS_INPUT_SYSTEM_H hrt_data receiver_port_reg_load(
	const rx_ID_t				ID,
	const mipi_port_ID_t		port_ID,
	const unsigned int			reg);

/*! Write to a control register of SUB_SYSTEM[sub_ID] of INPUT_SYSTEM[ID]

 \param	ID[in]				INPUT_SYSTEM identifier
 \param	port_ID[in]			sub system identifier
 \param	reg[in]				register index
 \param value[in]			The data to be written

 \return none, INPUT_SYSTEM[ID].SUB_SYSTEM[sub_ID].ctrl[reg] = value
 */
STORAGE_CLASS_INPUT_SYSTEM_H void input_system_sub_system_reg_store(
	const input_system_ID_t		ID,
	const sub_system_ID_t		sub_ID,
	const unsigned int			reg,
	const hrt_data				value);

/*! Read from a control register SUB_SYSTEM[sub_ID] of INPUT_SYSTEM[ID]

 \param	ID[in]				INPUT_SYSTEM identifier
 \param	port_ID[in]			sub system identifier
 \param	reg[in]				register index
 \param value[in]			The data to be written

 \return INPUT_SYSTEM[ID].SUB_SYSTEM[sub_ID].ctrl[reg]
 */
STORAGE_CLASS_INPUT_SYSTEM_H hrt_data input_system_sub_system_reg_load(
	const input_system_ID_t		ID,
	const sub_system_ID_t		sub_ID,
	const unsigned int			reg);

#endif /* __INPUT_SYSTEM_PUBLIC_H_INCLUDED__ */
