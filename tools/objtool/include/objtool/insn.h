/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2017 Josh Poimboeuf <jpoimboe@redhat.com>
 */

#ifndef _INSN_H
#define _INSN_H

#include <objtool/objtool.h>
#include <objtool/arch.h>

struct insn_state {
	struct cfi_state cfi;
	unsigned int uaccess_stack;
	bool uaccess;
	bool df;
	bool noinstr;
	s8 instr;
};

#define INSN_CHUNK_BITS		8
#define INSN_CHUNK_SIZE		(1 << INSN_CHUNK_BITS)
#define INSN_CHUNK_MAX		(INSN_CHUNK_SIZE - 1)

struct instruction {
	struct hlist_node hash;
	struct list_head call_node;
	struct section *sec;
	unsigned long offset;
	unsigned long immediate;

	u8 len;
	u8 prev_len;
	u8 type;
	s8 instr;

	u32 idx			: INSN_CHUNK_BITS,
	    dead_end		: 1,
	    ignore		: 1,
	    ignore_alts		: 1,
	    hint		: 1,
	    save		: 1,
	    restore		: 1,
	    retpoline_safe	: 1,
	    noendbr		: 1,
	    unret		: 1,
	    visited		: 4,
	    no_reloc		: 1;
		/* 10 bit hole */

	struct alt_group *alt_group;
	struct instruction *jump_dest;
	struct instruction *first_jump_src;
	union {
		struct symbol *_call_dest;
		struct reloc *_jump_table;
	};
	struct alternative *alts;
	struct symbol *sym;
	struct stack_op *stack_ops;
	struct cfi_state *cfi;
};

static inline struct symbol *insn_func(struct instruction *insn)
{
	struct symbol *sym = insn->sym;

	if (sym && sym->type != STT_FUNC)
		sym = NULL;

	return sym;
}

static inline bool is_static_jump(struct instruction *insn)
{
	return insn->type == INSN_JUMP_CONDITIONAL ||
	       insn->type == INSN_JUMP_UNCONDITIONAL;
}

static inline bool is_dynamic_jump(struct instruction *insn)
{
	return insn->type == INSN_JUMP_DYNAMIC ||
	       insn->type == INSN_JUMP_DYNAMIC_CONDITIONAL;
}

static inline bool is_jump(struct instruction *insn)
{
	return is_static_jump(insn) || is_dynamic_jump(insn);
}

void init_insn_state(struct objtool_file *file, struct insn_state *state,
		     struct section *sec);
struct instruction *find_insn(struct objtool_file *file,
			      struct section *sec, unsigned long offset);
struct instruction *find_last_insn(struct objtool_file *file,
				   struct section *sec);
struct instruction *next_insn_same_sec(struct objtool_file *file,
				       struct instruction *insn);
struct instruction *next_insn_same_func(struct objtool_file *file,
					struct instruction *insn);
struct instruction *prev_insn_same_sec(struct objtool_file *file,
				       struct instruction *insn);
struct instruction *prev_insn_same_sym(struct objtool_file *file,
				       struct instruction *insn);

struct reloc *insn_reloc(struct objtool_file *file, struct instruction *insn);
bool insn_can_reloc(struct instruction *insn);
bool insn_cfi_match(struct instruction *insn, struct cfi_state *cfi2,
		    bool print);
bool is_first_func_insn(struct objtool_file *file,
			struct instruction *insn, struct symbol *sym);

/* temporily add for revert */
#ifdef __aarch64__
int read_unwind_hints(struct objtool_file *file);
int decode_instructions(struct objtool_file *file);
#endif

#define sec_for_each_insn(file, _sec, insn)				\
	for (insn = find_insn(file, _sec, 0);				\
	     insn && insn->sec == _sec;					\
	     insn = next_insn_same_sec(file, insn))

#define for_each_insn(file, insn)					\
	for (struct section *__sec, *__fake = (struct section *)1;	\
	     __fake; __fake = NULL)					\
		for_each_sec(file, __sec)				\
			sec_for_each_insn(file, __sec, insn)

#define func_for_each_insn(file, func, insn)				\
	for (insn = find_insn(file, func->sec, func->offset);		\
	     insn;							\
	     insn = next_insn_same_func(file, insn))

#define sym_for_each_insn(file, sym, insn)				\
	for (insn = find_insn(file, sym->sec, sym->offset);		\
	     insn && insn->offset < sym->offset + sym->len;		\
	     insn = next_insn_same_sec(file, insn))

#define sym_for_each_insn_continue_reverse(file, sym, insn)		\
	for (insn = prev_insn_same_sec(file, insn);			\
	     insn && insn->offset >= sym->offset;			\
	     insn = prev_insn_same_sec(file, insn))

#define sec_for_each_insn_from(file, insn)				\
	for (; insn; insn = next_insn_same_sec(file, insn))

#define sec_for_each_insn_continue(file, insn)				\
	for (insn = next_insn_same_sec(file, insn); insn;		\
	     insn = next_insn_same_sec(file, insn))

#endif /* _INSN_H */
