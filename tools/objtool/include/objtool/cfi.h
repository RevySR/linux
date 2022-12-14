/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2015-2017 Josh Poimboeuf <jpoimboe@redhat.com>
 */

#ifndef _OBJTOOL_CFI_H
#define _OBJTOOL_CFI_H

#include <arch/cfi_regs.h>
#include <linux/list.h>

#define CFI_UNDEFINED		-1
#define CFI_CFA			-2
#define CFI_SP_INDIRECT		-3
#define CFI_BP_INDIRECT		-4

struct cfi_reg {
	int base;
	int offset;
};

struct cfi_init_state {
	struct cfi_reg regs[CFI_NUM_REGS];
	struct cfi_reg cfa;
};

struct cfi_state {
	struct hlist_node hash; /* must be first, cficmp() */
	struct cfi_reg regs[CFI_NUM_REGS];
	struct cfi_reg vals[CFI_NUM_REGS];
	struct cfi_reg cfa;
	int stack_size;
	int drap_reg, drap_offset;
	unsigned char type;
	bool bp_scratch;
	bool drap;
	bool signal;
	bool end;
	bool force_undefined;
};

void init_cfi_state(struct cfi_state *cfi);
bool cficmp(struct cfi_state *cfi1, struct cfi_state *cfi2);
struct cfi_state *cfi_hash_find_or_add(struct cfi_state *cfi);
void cfi_hash_add(struct cfi_state *cfi);
void *cfi_hash_alloc(unsigned long size);
void set_func_state(struct cfi_state *state);

extern unsigned long nr_cfi, nr_cfi_reused, nr_cfi_cache;
extern struct cfi_init_state initial_func_cfi;
extern struct cfi_state init_cfi;
extern struct cfi_state func_cfi;
extern struct cfi_state force_undefined_cfi;

#endif /* _OBJTOOL_CFI_H */
