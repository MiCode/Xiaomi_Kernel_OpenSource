/*
 * This header provides constants for TEGRA pinctrl bindings.
 */

#ifndef _DT_BINDINGS_PINCTRL_TEGRA_H
#define _DT_BINDINGS_PINCTRL_TEGRA_H

/*
 * Enable/disable for diffeent dt properties. This is applicable for
 * properties nvidia,enable-input, nvidia,tristate, nvidia,open-drain,
 * nvidia,lock, nvidia,rcv-sel, nvidia,high-speed-mode, nvidia,schmitt.
 */
#define TEGRA_PIN_DISABLE		0
#define TEGRA_PIN_ENABLE		1

/* Input/output */
#define TEGRA_PIN_OUTPUT		0
#define TEGRA_PIN_INPUT			1

/* Pull up/down/normal */
#define TEGRA_PIN_PUPD_NORMAL		0
#define TEGRA_PIN_PUPD_PULL_DOWN	1
#define TEGRA_PIN_PUPD_PULL_UP		2
#define TEGRA_PIN_PULL_NONE		0
#define TEGRA_PIN_PULL_DOWN		1
#define TEGRA_PIN_PULL_UP		2

/* Tristate/normal */
#define TEGRA_PIN_NORMAL		0
#define TEGRA_PIN_TRISTATE		1

/* Lock enable/disable */
#define TEGRA_PIN_LOCK_DISABLE		0
#define TEGRA_PIN_LOCK_ENABLE		1

/* Open drain enable/disable */
#define TEGRA_PIN_OPEN_DRAIN_DISABLE	0
#define TEGRA_PIN_OPEN_DRAIN_ENABLE	1

/* High speed mode */
#define TEGRA_PIN_DRIVE_HIGH_SPEED_MODE_DISABLE 0
#define TEGRA_PIN_DRIVE_HIGH_SPEED_MODE_ENABLE	1

/* Schmitt enable/disable*/
#define TEGRA_PIN_DRIVE_SCHMITT_DISABLE		0
#define TEGRA_PIN_DRIVE_SCHMITT_ENABLE		1

/* Low power mode */
#define TEGRA_PIN_LP_DRIVE_DIV_8		0
#define TEGRA_PIN_LP_DRIVE_DIV_4		1
#define TEGRA_PIN_LP_DRIVE_DIV_2		2
#define TEGRA_PIN_LP_DRIVE_DIV_1		3

#define TEGRA_PIN_SLEW_RATE_FASTEST		0
#define TEGRA_PIN_SLEW_RATE_FAST		1
#define TEGRA_PIN_SLEW_RATE_SLOW		2
#define TEGRA_PIN_SLEW_RATE_SLOWEST		3

#endif

