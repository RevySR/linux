// SPDX-License-Identifier: GPL-2.0-only
/*
 * Hygon CSV support
 *
 * This file is shared between decompression boot code and running
 * linux kernel.
 *
 * Copyright (C) Hygon Info Technologies Ltd.
 */

#define CPUID_VENDOR_HygonGenuine_ebx	0x6f677948
#define CPUID_VENDOR_HygonGenuine_ecx	0x656e6975
#define CPUID_VENDOR_HygonGenuine_edx	0x6e65476e

#define MSR_CSV3_ENABLED_BIT		30
#define MSR_CSV3_ENABLED		BIT_ULL(MSR_CSV3_ENABLED_BIT)
