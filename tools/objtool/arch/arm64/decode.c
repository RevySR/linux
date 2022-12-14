// SPDX-License-Identifier: GPL-2.0-only
/*
 * Author: Madhavan T. Venkataraman (madvenka@linux.microsoft.com)
 *
 * Copyright (C) 2022 Microsoft Corporation
 */

#include <stdio.h>
#include <stdlib.h>

#include <objtool/check.h>

int arch_decode_instruction(struct objtool_file *file,
			    const struct section *sec,
			    unsigned long offset, unsigned int maxlen,
			    unsigned int *len, enum insn_type *type,
			    unsigned long *immediate,
			    struct list_head *ops_list)
{
	return 0;
}
