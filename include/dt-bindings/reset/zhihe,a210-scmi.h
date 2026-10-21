/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause) */
#ifndef _DT_BINDINGS_RESET_ZHIHE_A210_SCMI_H_
#define _DT_BINDINGS_RESET_ZHIHE_A210_SCMI_H_

/* SCMI reset IDs; existing entries must not be renumbered. */
#define A210_RESET_UART_SRST(n) (n)
#define A210_RESET_UART_PRST(n) (10 + (n))
#define A210_RESET_PERI1_PAD 20
#define A210_RESET_PERI2_PAD 21
#define A210_RESET_PERI3_PAD 22
#define A210_RESET_AON_I2C1 23

#endif
