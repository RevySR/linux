/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * CSV driver for KVM
 *
 * HYGON CSV support
 *
 * Copyright (C) Hygon Info Technologies Ltd.
 */

#ifndef __SVM_CSV_H
#define __SVM_CSV_H

#include <asm/processor-hygon.h>

#ifdef CONFIG_HYGON_CSV

void __init csv_init(struct kvm_x86_ops *ops);
void csv_exit(void);

#else	/* !CONFIG_HYGON_CSV */

static inline void __init csv_init(struct kvm_x86_ops *ops) { }
static inline void csv_exit(void) { }

#endif	/* CONFIG_HYGON_CSV */

#endif	/* __SVM_CSV_H */
