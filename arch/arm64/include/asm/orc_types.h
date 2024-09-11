/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Author: Madhavan T. Venkataraman (madvenka@linux.microsoft.com)
 *
 * Copyright (C) 2022 Microsoft Corporation
 */

#ifndef _ORC_TYPES_H
#define _ORC_TYPES_H

#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/orc_entry.h>

/*
 * The ORC_REG_* registers are base registers which are used to find other
 * registers on the stack.
 *
 * ORC_REG_PREV_SP, also known as DWARF Call Frame Address (CFA), is the
 * address of the previous frame: the caller's SP before it called the current
 * function.
 *
 * ORC_REG_UNDEFINED means the corresponding register's value didn't change in
 * the current frame.
 *
 * We only use base registers SP and FP -- which the previous SP is based on --
 * and PREV_SP and UNDEFINED -- which the previous FP is based on.
 */
#define ORC_REG_UNDEFINED		0
#define ORC_REG_PREV_SP			1
#define ORC_REG_SP			2
#define ORC_REG_FP			3
#define ORC_REG_MAX			4

#define ORC_TYPE_UNDEFINED		0
#define ORC_TYPE_END_OF_STACK		1
#define ORC_TYPE_CALL			2
#define ORC_TYPE_REGS			3
#define ORC_TYPE_REGS_PARTIAL		4

#endif /* _ORC_TYPES_H */
