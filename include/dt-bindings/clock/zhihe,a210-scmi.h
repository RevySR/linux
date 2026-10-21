/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause) */
#ifndef _DT_BINDINGS_CLOCK_ZHIHE_A210_SCMI_H_
#define _DT_BINDINGS_CLOCK_ZHIHE_A210_SCMI_H_

/* SCMI clock IDs; existing entries must not be renumbered. */
#define A210_CLK_UART_SCLK(n) (n)
#define A210_CLK_UART_PCLK(n) (10 + (n))
#define A210_CLK_PERI1_PAD 20
#define A210_CLK_PERI2_PAD 21
#define A210_CLK_PERI3_PAD 22
#define A210_CLK_AON_SYS 23
#define A210_CLK_AON_I2C1_IC 24
#define A210_CLK_AON_I2C1_PCLK 25

#endif
