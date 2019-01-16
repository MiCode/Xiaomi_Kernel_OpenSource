/*
 * kernel/power/tuxonice_atomic_copy.h
 *
 * Copyright 2008-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * Distributed under GPLv2.
 *
 * Routines for doing the atomic save/restore.
 */

enum {
	ATOMIC_ALL_STEPS,
	ATOMIC_STEP_SYSCORE_RESUME,
	ATOMIC_STEP_IRQS,
	ATOMIC_STEP_CPU_HOTPLUG,
	ATOMIC_STEP_PLATFORM_FINISH,
	ATOMIC_STEP_DEVICE_RESUME,
	ATOMIC_STEP_DPM_COMPLETE,
	ATOMIC_STEP_PLATFORM_END,
};

int toi_go_atomic(pm_message_t state, int toi_time);
void toi_end_atomic(int stage, int toi_time, int error);
