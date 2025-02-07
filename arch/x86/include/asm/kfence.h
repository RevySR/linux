/* SPDX-License-Identifier: GPL-2.0 */
/*
 * x86 KFENCE support.
 *
 * Copyright (C) 2020, Google LLC.
 */

#ifndef _ASM_X86_KFENCE_H
#define _ASM_X86_KFENCE_H

#ifndef MODULE

#include <linux/bug.h>
#include <linux/kfence.h>

#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/set_memory.h>
#include <asm/tlbflush.h>

/* Force 4K pages for __kfence_pool. */
static inline bool arch_kfence_init_pool(struct kfence_pool_area *kpa)
{
	char *__kfence_pool = kpa->addr;
	unsigned long addr;

	for (addr = (unsigned long)__kfence_pool; is_kfence_address_area((void *)addr, kpa);
	     addr += PAGE_SIZE) {
		unsigned int level;

		if (!lookup_address(addr, &level))
			return false;

		if (level != PG_LEVEL_4K)
			set_memory_4k(addr, 1);
	}

	return true;
}

/* Protect the given page and flush TLB. */
static inline bool kfence_protect_page(unsigned long addr, bool protect)
{
	unsigned int level;
	pte_t *pte = lookup_address(addr, &level);

	if (WARN_ON(!pte || level != PG_LEVEL_4K))
		return false;

	/*
	 * We need to avoid IPIs, as we may get KFENCE allocations or faults
	 * with interrupts disabled. Therefore, the below is best-effort, and
	 * does not flush TLBs on all CPUs. We can tolerate some inaccuracy;
	 * lazy fault handling takes care of faults after the page is PRESENT.
	 */

	if (protect)
		set_pte(pte, __pte(pte_val(*pte) & ~_PAGE_PRESENT));
	else
		set_pte(pte, __pte(pte_val(*pte) | _PAGE_PRESENT));

	/*
	 * Flush this CPU's TLB, assuming whoever did the allocation/free is
	 * likely to continue running on this CPU.
	 */
	preempt_disable();
	flush_tlb_one_kernel(addr);
	preempt_enable();
	return true;
}

/* Like pud_free_pmd_page(), but reverse set_memory_4k(). */
static inline void free_old_pud_pages(pud_t *pud, unsigned long addr)
{
	pmd_t *pmd = pud_pgtable(*pud);
	pte_t *pte;
	int i;

	/* Revert counting in __split_large_page(). */
	if (virt_addr_valid(addr)) {
		unsigned long pfn = PFN_DOWN(__pa(addr));

		if (pfn_range_is_mapped(pfn, pfn + 1)) {
			update_page_count(PG_LEVEL_1G, 1);
			update_page_count(PG_LEVEL_4K, -PTRS_PER_PTE * PTRS_PER_PTE);
		}
	}

	for (i = 0; i < PTRS_PER_PMD; i++) {
		if (!pmd_none(pmd[i])) {
			pte = (pte_t *)pmd_page_vaddr(pmd[i]);
			free_page((unsigned long)pte);
		}
	}

	free_page((unsigned long)pmd);
}

/*
 * This function is used to recover TLB to 1G kernel mapping.
 * The caller MUST make sure there're no other active kfence
 * pools in this 1G area.
 */
static inline bool arch_kfence_free_pool(unsigned long addr)
{
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud, new_pud, old_pud;

	addr = ALIGN_DOWN(addr, PUD_SIZE);

	pgd = pgd_offset_k(addr);
	if (pgd_none(*pgd))
		return false;

	p4d = p4d_offset(pgd, addr);
	if (p4d_none(*p4d))
		return false;

	if (p4d_large(*p4d) || !p4d_present(*p4d))
		return false;

	pud = pud_offset(p4d, addr);
	if (pud_none(*pud))
		return false;

	if (pud_large(*pud) || !pud_present(*pud))
		return false;

	new_pud = pfn_pud((unsigned long)__phys_to_pfn(__pa(addr)),
			  __pgprot(__PAGE_KERNEL_LARGE));

	old_pud = xchg(pud, new_pud);

	flush_tlb_kernel_range(addr, addr + PUD_SIZE);
	free_old_pud_pages(&old_pud, addr);

	return true;
}

#endif /* !MODULE */

#endif /* _ASM_X86_KFENCE_H */
