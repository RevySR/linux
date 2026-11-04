/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#ifndef _ASM_RISCV_PGTABLE_32_H
#define _ASM_RISCV_PGTABLE_32_H

#include <asm-generic/pgtable-nopmd.h>
#include <linux/bits.h>
#include <linux/const.h>
#include <asm/errata_list.h>

/* Size of region mapped by a page global directory */
#define PGDIR_SHIFT     22
#define PGDIR_SIZE      (_AC(1, UL) << PGDIR_SHIFT)
#define PGDIR_MASK      (~(PGDIR_SIZE - 1))

#define MAX_POSSIBLE_PHYSMEM_BITS 34

/*
 * rv32 PTE format:
 * | XLEN-1  10 | 9             8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0
 *       PFN      reserved for SW   D   A   G   U   X   W   R   V
 */
#ifdef CONFIG_ERRATA_THEAD_SV32_PBMT
/*
 * T-Head cores implement page-based memory types in Sv32 as well, with the
 * PPN limited to bits [29:10]. This covers the 32-bit physical addresses that
 * the kernel uses on rv32.
 *
 * [31:30] T-Head Sv32 Memory Type definitions, encoded as for Svpbmt:
 *
 *  00 - PMA    Normal Cacheable, No change to implied PMA memory type
 *  01 - NC     Non-cacheable, idempotent, weakly-ordered Main Memory
 *  10 - IO     Non-cacheable, non-idempotent, strongly-ordered I/O memory
 *  11 - Rsvd   Reserved for future standard use
 */
#define _PAGE_PFN_MASK  GENMASK(29, 10)

#define _PAGE_NOCACHE_THEAD	(1UL << 30)
#define _PAGE_IO_THEAD		(1UL << 31)
#define _PAGE_MTMASK_THEAD	(_PAGE_NOCACHE_THEAD | _PAGE_IO_THEAD)

static inline u32 riscv_page_mtmask(void)
{
	u32 val;

	ALT_THEAD_SV32_PBMT(val, _PAGE_MTMASK);
	return val;
}

static inline u32 riscv_page_nocache(void)
{
	u32 val;

	ALT_THEAD_SV32_PBMT(val, _PAGE_NOCACHE);
	return val;
}

static inline u32 riscv_page_io(void)
{
	u32 val;

	ALT_THEAD_SV32_PBMT(val, _PAGE_IO);
	return val;
}

#define _PAGE_NOCACHE		riscv_page_nocache()
#define _PAGE_IO		riscv_page_io()
#define _PAGE_MTMASK		riscv_page_mtmask()
#else
#define _PAGE_PFN_MASK  GENMASK(31, 10)

#define _PAGE_NOCACHE		0
#define _PAGE_IO		0
#define _PAGE_MTMASK		0
#endif

/* Set of bits to preserve across pte_modify() */
#define _PAGE_CHG_MASK  (~(unsigned long)(_PAGE_PRESENT | _PAGE_READ |	\
					  _PAGE_WRITE | _PAGE_EXEC |	\
					  _PAGE_USER | _PAGE_GLOBAL |	\
					  _PAGE_MTMASK))

static inline bool pgtable_l4_enabled(void) { return false; }
static inline bool pgtable_l5_enabled(void) { return false; }

#endif /* _ASM_RISCV_PGTABLE_32_H */
