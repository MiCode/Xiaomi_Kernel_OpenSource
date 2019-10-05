/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*external display dummy driver*/
/*
 *
void ext_disp_dummy(void)
{

}
*/

#include "disp_session.h"

void external_display_control_init(void)
{
}

int external_display_switch_mode(enum DISP_MODE mode,
		unsigned int *session_created, unsigned int session)
{
	return 0;
}

int external_display_wait_for_vsync(void *config, unsigned int session)
{
	return 0;
}

