// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2015-2017 Josh Poimboeuf <jpoimboe@redhat.com>
 */
#include <asm/orc_types.h>

#include <objtool/builtin.h>
#include <objtool/insn.h>
#include <objtool/warn.h>

unsigned long nr_insns;

/*
 * Call the arch-specific instruction decoder for all the instructions and add
 * them to the global instruction list.
 */
int decode_instructions(struct objtool_file *file)
{
	struct section *sec;
	struct symbol *func;
	unsigned long offset;
	struct instruction *insn;
	int ret;

	for_each_sec(file, sec) {
		struct instruction *insns = NULL;
		u8 prev_len = 0;
		u8 idx = 0;

		if (!(sec->sh.sh_flags & SHF_EXECINSTR))
			continue;

		if (strcmp(sec->name, ".altinstr_replacement") &&
		    strcmp(sec->name, ".altinstr_aux") &&
		    strncmp(sec->name, ".discard.", 9))
			sec->text = true;

		if (!strcmp(sec->name, ".noinstr.text") ||
		    !strcmp(sec->name, ".entry.text") ||
		    !strcmp(sec->name, ".cpuidle.text") ||
		    !strncmp(sec->name, ".text..__x86.", 13))
			sec->noinstr = true;

		/*
		 * .init.text code is ran before userspace and thus doesn't
		 * strictly need retpolines, except for modules which are
		 * loaded late, they very much do need retpoline in their
		 * .init.text
		 */
		if (!strcmp(sec->name, ".init.text") && !opts.module)
			sec->init = true;

		for (offset = 0; offset < sec->sh.sh_size; offset += insn->len) {
			if (!insns || idx == INSN_CHUNK_MAX) {
				insns = calloc(sizeof(*insn), INSN_CHUNK_SIZE);
				if (!insns) {
					WARN("malloc failed");
					return -1;
				}
				idx = 0;
			} else {
				idx++;
			}
			insn = &insns[idx];
			insn->idx = idx;

			INIT_LIST_HEAD(&insn->call_node);
			insn->sec = sec;
			insn->offset = offset;
			insn->prev_len = prev_len;

			ret = arch_decode_instruction(file, sec, offset,
						      sec->sh.sh_size - offset,
						      insn);
			if (ret)
				return ret;

			prev_len = insn->len;

			/*
			 * By default, "ud2" is a dead end unless otherwise
			 * annotated, because GCC 7 inserts it for certain
			 * divide-by-zero cases.
			 */
			if (insn->type == INSN_BUG)
				insn->dead_end = true;

			hash_add(file->insn_hash, &insn->hash, sec_offset_hash(sec, insn->offset));
			nr_insns++;
		}

//		printf("%s: last chunk used: %d\n", sec->name, (int)idx);

		sec_for_each_sym(sec, func) {
			if (func->type != STT_NOTYPE && func->type != STT_FUNC)
				continue;

			if (func->offset == sec->sh.sh_size) {
				/* Heuristic: likely an "end" symbol */
				if (func->type == STT_NOTYPE)
					continue;
				WARN("%s(): STT_FUNC at end of section",
				     func->name);
				return -1;
			}

			if (func->embedded_insn || func->alias != func)
				continue;

			if (!find_insn(file, sec, func->offset)) {
				WARN("%s(): can't find starting instruction",
				     func->name);
				return -1;
			}

			sym_for_each_insn(file, func, insn) {
				insn->sym = func;
				if (func->type == STT_FUNC &&
				    insn->type == INSN_ENDBR &&
				    list_empty(&insn->call_node)) {
					if (insn->offset == func->offset) {
						list_add_tail(&insn->call_node, &file->endbr_list);
						file->nr_endbr++;
					} else {
						file->nr_endbr_int++;
					}
				}
			}
		}
	}

	if (opts.stats)
		printf("nr_insns: %lu\n", nr_insns);

	return 0;
}
