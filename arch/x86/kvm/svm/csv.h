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

/* same to the ring buffer max num */
#define SVM_RING_BUFFER_MAX 4094

struct csv_ringbuf_info_item {
	struct page **pages;
	uintptr_t hdr_vaddr;
	uintptr_t trans_vaddr;
	uintptr_t data_vaddr;
	uintptr_t trans_uaddr;
	uintptr_t hdr_uaddr;
	unsigned long trans_len;
	unsigned long hdr_len;
	unsigned long n;
};

struct csv_ringbuf_infos {
	struct csv_ringbuf_info_item *item[SVM_RING_BUFFER_MAX];
	int num;
};

#ifdef CONFIG_HYGON_CSV

/*
 * Hooks table: a table of function and variable pointers filled in
 * when module init.
 */
extern struct hygon_kvm_hooks_table {
	bool sev_hooks_installed;
	bool *sev_enabled;
	unsigned long *sev_me_mask;
	int (*sev_issue_cmd)(struct kvm *kvm, int id, void *data, int *error);
	unsigned long (*get_num_contig_pages)(unsigned long idx,
					      struct page **inpages,
					      unsigned long npages);
	struct page **(*sev_pin_memory)(struct kvm *kvm, unsigned long uaddr,
					unsigned long ulen, unsigned long *n,
					int write);
	void (*sev_unpin_memory)(struct kvm *kvm, struct page **pages,
				 unsigned long npages);
	void (*sev_clflush_pages)(struct page *pages[], unsigned long npages);
} hygon_kvm_hooks;

void __init csv_init(struct kvm_x86_ops *ops);
void csv_exit(void);
int csv_alloc_trans_mempool(void);
void csv_free_trans_mempool(void);

void csv2_sync_reset_vmsa(struct vcpu_svm *svm);
void csv2_free_reset_vmsa(struct vcpu_svm *svm);
int csv2_setup_reset_vmsa(struct vcpu_svm *svm);

#else	/* !CONFIG_HYGON_CSV */

static inline void __init csv_init(struct kvm_x86_ops *ops) { }
static inline void csv_exit(void) { }
static inline int csv_alloc_trans_mempool(void) { return -ENOMEM; }
static inline void csv_free_trans_mempool(void) { }

static inline void csv2_sync_reset_vmsa(struct vcpu_svm *svm) { }
static inline void csv2_free_reset_vmsa(struct vcpu_svm *svm) { }
static inline int csv2_setup_reset_vmsa(struct vcpu_svm *svm) { return 0; }

#endif	/* CONFIG_HYGON_CSV */

#endif	/* __SVM_CSV_H */
