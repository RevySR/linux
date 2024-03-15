// SPDX-License-Identifier: GPL-2.0-only
/*
 * HYGON CSV support
 *
 * Copyright (C) Hygon Info Technologies Ltd.
 */

#include <linux/preempt.h>
#include <linux/smp.h>
#include <linux/memblock.h>
#include <asm/mem_encrypt.h>
#include <asm/csv.h>

#include "../mm/mm_internal.h"
#include "csv-shared.c"

u32 vendor_ebx __section(".data") = 0;
u32 vendor_ecx __section(".data") = 0;
u32 vendor_edx __section(".data") = 0;

struct secure_call_pages {
	struct csv3_secure_call_cmd page_a;
	struct csv3_secure_call_cmd page_b;
};

bool noinstr csv3_active(void)
{
	if (vendor_ebx == 0 || vendor_ecx == 0 || vendor_edx == 0) {
		u32 eax = 0;

		native_cpuid(&eax, &vendor_ebx, &vendor_ecx, &vendor_edx);
	}

	/* HygonGenuine */
	if (vendor_ebx == CPUID_VENDOR_HygonGenuine_ebx &&
	    vendor_ecx == CPUID_VENDOR_HygonGenuine_ecx &&
	    vendor_edx == CPUID_VENDOR_HygonGenuine_edx)
		return !!(sev_status & MSR_CSV3_ENABLED);
	else
		return false;
}

void __init csv_early_reset_memory(struct boot_params *bp)
{
	if (!csv3_active())
		return;

	csv3_scan_secure_call_pages(bp);
	csv3_early_secure_call(0, 0, CSV3_SECURE_CMD_RESET);
}

void __init csv_early_update_memory_dec(u64 vaddr, u64 pages)
{
	if (!csv3_active())
		return;

	if (pages)
		csv3_early_secure_call(__pa(vaddr), pages, CSV3_SECURE_CMD_DEC);
}

void __init csv_early_update_memory_enc(u64 vaddr, u64 pages)
{
	if (!csv3_active())
		return;

	if (pages)
		csv3_early_secure_call(__pa(vaddr), pages, CSV3_SECURE_CMD_ENC);
}
