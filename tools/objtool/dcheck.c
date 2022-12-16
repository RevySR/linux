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

int check(struct objtool_file *file)
{
	if (!opts.stackval)
		return 1;

	return decode_instructions(file);
}
