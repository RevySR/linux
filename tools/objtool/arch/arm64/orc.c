// SPDX-License-Identifier: GPL-2.0-only
/*
 * Author: Madhavan T. Venkataraman (madvenka@linux.microsoft.com)
 *
 * Copyright (C) 2022 Microsoft Corporation
 */
#include <string.h>

#include <linux/objtool_types.h>

#include <objtool/insn.h>
#include <objtool/orc.h>

int init_orc_entry(struct orc_entry *orc, struct cfi_state *cfi,
		   struct instruction *insn)
{
	struct cfi_reg *fp = &cfi->regs[CFI_FP];

	memset(orc, 0, sizeof(*orc));

	orc->sp_reg = ORC_REG_SP;
	orc->fp_reg = ORC_REG_PREV_SP;
	orc->type = UNWIND_HINT_TYPE_CALL;

	if (!cfi || cfi->cfa.base == CFI_UNDEFINED ||
	    (cfi->type == UNWIND_HINT_TYPE_CALL && !fp->offset)) {
		/*
		 * The frame pointer has not been set up. This instruction is
		 * unreliable from an unwind perspective.
		 */
		return 0;
	}

	orc->sp_offset = cfi->cfa.offset;
	orc->fp_offset = fp->offset;
	orc->type = cfi->type;
	orc->signal = cfi->end;

	return 0;
}

static const char *reg_name(unsigned int reg)
{
	switch (reg) {
	case ORC_REG_PREV_SP:
		return "cfa";
	case ORC_REG_FP:
		return "x29";
	case ORC_REG_SP:
		return "sp";
	default:
		return "?";
	}
}

const char *orc_type_name(unsigned int type)
{
	switch (type) {
	case UNWIND_HINT_TYPE_CALL:
		return "call";
	case UNWIND_HINT_TYPE_REGS:
		return "regs";
	case UNWIND_HINT_TYPE_IRQ_STACK:
		return "irqstack";
	default:
		return "?";
	}
}

void orc_print_reg(unsigned int reg, int offset)
{
	if (reg == ORC_REG_UNDEFINED)
		printf("(und)");
	else
		printf("%s%+d", reg_name(reg), offset);
}

void orc_print_sp(void)
{
	printf(" cfa:");
}

void orc_print_fp(void)
{
	printf(" x29:");
}

bool orc_ignore_section(struct section *sec)
{
	return !strcmp(sec->name, ".head.text");
}
