// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2015-2017 Josh Poimboeuf <jpoimboe@redhat.com>
 */

#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/mman.h>

#include <objtool/objtool.h>
#include <objtool/builtin.h>
#include <objtool/insn.h>

/*
 * Find the destination instructions for all jumps.
 */
static void add_jump_destinations(struct objtool_file *file)
{
	struct instruction *insn;
	struct reloc *reloc;
	struct section *dest_sec;
	unsigned long dest_off;

	for_each_insn(file, insn) {
		if (insn->type != INSN_CALL &&
		    insn->type != INSN_JUMP_CONDITIONAL &&
		    insn->type != INSN_JUMP_UNCONDITIONAL) {
			continue;
		}

		reloc = insn_reloc(file, insn);
		if (!reloc) {
			dest_sec = insn->sec;
			dest_off = arch_jump_destination(insn);
		} else if (reloc->sym->type == STT_SECTION) {
			dest_sec = reloc->sym->sec;
			dest_off = arch_dest_reloc_offset(reloc_addend(reloc));
		} else if (reloc->sym->sec->idx) {
			dest_sec = reloc->sym->sec;
			dest_off = reloc->sym->sym.st_value +
				   arch_dest_reloc_offset(reloc_addend(reloc));
		} else {
			/* non-func asm code jumping to another file */
			continue;
		}

		insn->jump_dest = find_insn(file, dest_sec, dest_off);
	}
}

int check(struct objtool_file *file)
{
	int ret;

	if (!opts.stackval)
		return 1;

	ret = decode_instructions(file);
	if (ret)
		return ret;

	add_jump_destinations(file);

	return 0;
}
