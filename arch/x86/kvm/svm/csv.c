// SPDX-License-Identifier: GPL-2.0-only
/*
 * CSV driver for KVM
 *
 * HYGON CSV support
 *
 * Copyright (C) Hygon Info Technologies Ltd.
 */

#include <linux/kvm_host.h>
#include <linux/psp-sev.h>
#include <linux/psp-csv.h>
#include <linux/memory.h>
#include <linux/kvm_types.h>
#include <linux/vmalloc.h>
#include <asm/cacheflush.h>
#include <asm/e820/api.h>
#include <asm/csv.h>
#include "kvm_cache_regs.h"
#include "svm.h"
#include "csv.h"
#include "x86.h"

#undef  pr_fmt
#define pr_fmt(fmt) "CSV: " fmt

struct encrypt_data_block {
	struct {
		u64 npages:	12;
		u64 pfn:	52;
	} entry[512];
};

struct kvm_csv_info {
	struct kvm_sev_info *sev;

	bool csv3_active;	/* CSV3 enabled guest */

	struct list_head smr_list; /* List of guest secure memory regions */
	unsigned long nodemask; /* Nodemask where CSV3 guest's memory resides */
};

struct kvm_svm_csv {
	struct kvm_svm kvm_svm;
	struct kvm_csv_info csv_info;
};

struct secure_memory_region {
	struct list_head list;
	u64 npages;
	u64 hpa;
};

static struct kvm_x86_ops csv_x86_ops;

static inline struct kvm_svm_csv *to_kvm_svm_csv(struct kvm *kvm)
{
	return (struct kvm_svm_csv *)container_of(kvm, struct kvm_svm, kvm);
}

static bool csv3_guest(struct kvm *kvm)
{
	struct kvm_csv_info *csv = &to_kvm_svm_csv(kvm)->csv_info;

	return sev_es_guest(kvm) && csv->csv3_active;
}

static int csv_sync_vmsa(struct vcpu_svm *svm)
{
	struct sev_es_save_area *save = svm->sev_es.vmsa;

	/* Check some debug related fields before encrypting the VMSA */
	if (svm->vcpu.guest_debug || (svm->vmcb->save.dr7 & ~DR7_FIXED_1))
		return -EINVAL;

	memcpy(save, &svm->vmcb->save, sizeof(svm->vmcb->save));

	/* Sync registgers per spec. */
	save->rax = svm->vcpu.arch.regs[VCPU_REGS_RAX];
	save->rdx = svm->vcpu.arch.regs[VCPU_REGS_RDX];
	save->rip = svm->vcpu.arch.regs[VCPU_REGS_RIP];
	save->xcr0 = svm->vcpu.arch.xcr0;
	save->xss  = svm->vcpu.arch.ia32_xss;

	return 0;
}

static int __csv_issue_cmd(int fd, int id, void *data, int *error)
{
	struct fd f;
	int ret;

	f = fdget(fd);
	if (!f.file)
		return -EBADF;

	ret = sev_issue_cmd_external_user(f.file, id, data, error);

	fdput(f);
	return ret;
}

static int csv_issue_cmd(struct kvm *kvm, int id, void *data, int *error)
{
	struct kvm_sev_info *sev = &to_kvm_svm(kvm)->sev_info;

	return __csv_issue_cmd(sev->fd, id, data, error);
}

static int csv3_guest_init(struct kvm *kvm, struct kvm_sev_cmd *argp)
{
	struct kvm_sev_info *sev = &to_kvm_svm(kvm)->sev_info;
	struct kvm_csv_info *csv = &to_kvm_svm_csv(kvm)->csv_info;
	struct kvm_csv3_init_data params;

	if (unlikely(csv->csv3_active))
		return -EINVAL;

	if (unlikely(!sev->es_active))
		return -EINVAL;

	if (copy_from_user(&params, (void __user *)(uintptr_t)argp->data,
			   sizeof(params)))
		return -EFAULT;

	csv->csv3_active = true;
	csv->sev = sev;
	csv->nodemask = (unsigned long)params.nodemask;

	return 0;
}

static int csv3_set_guest_private_memory(struct kvm *kvm)
{
	struct kvm_memslots *slots = kvm_memslots(kvm);
	struct kvm_memory_slot *memslot;
	struct secure_memory_region *smr;
	struct kvm_sev_info *sev = &to_kvm_svm(kvm)->sev_info;
	struct kvm_csv_info *csv = &to_kvm_svm_csv(kvm)->csv_info;
	struct csv3_data_set_guest_private_memory *set_guest_private_memory;
	struct csv3_data_memory_region *regions;
	nodemask_t nodemask;
	nodemask_t *nodemask_ptr;

	LIST_HEAD(tmp_list);
	struct list_head *pos, *q;
	u32 i = 0, count = 0, remainder;
	int ret = 0, error;
	u64 size = 0, nr_smr = 0, nr_pages = 0;
	u32 smr_entry_shift;
	int bkt;

	unsigned int flags = FOLL_HWPOISON;
	int npages;
	struct page *page;

	if (!csv3_guest(kvm))
		return -ENOTTY;

	nodes_clear(nodemask);
	for_each_set_bit(i, &csv->nodemask, BITS_PER_LONG)
		if (i < MAX_NUMNODES)
			node_set(i, nodemask);

	nodemask_ptr = csv->nodemask ? &nodemask : &node_online_map;

	set_guest_private_memory = kzalloc(sizeof(*set_guest_private_memory),
					GFP_KERNEL_ACCOUNT);
	if (!set_guest_private_memory)
		return -ENOMEM;

	regions = kzalloc(PAGE_SIZE, GFP_KERNEL_ACCOUNT);
	if (!regions) {
		kfree(set_guest_private_memory);
		return -ENOMEM;
	}

	/* Get guest secure memory size */
	kvm_for_each_memslot(memslot, bkt, slots) {
		npages = get_user_pages_unlocked(memslot->userspace_addr, 1,
						&page, flags);
		if (npages != 1)
			continue;

		nr_pages += memslot->npages;

		put_page(page);
	}

	/*
	 * NPT secure memory size
	 *
	 * PTEs_entries = nr_pages
	 * PDEs_entries = nr_pages / 512
	 * PDPEs_entries = nr_pages / (512 * 512)
	 * PML4Es_entries = nr_pages / (512 * 512 * 512)
	 *
	 * Totals_entries = nr_pages + nr_pages / 512 + nr_pages / (512 * 512) +
	 *		nr_pages / (512 * 512 * 512) <= nr_pages + nr_pages / 256
	 *
	 * Total_NPT_size = (Totals_entries / 512) * PAGE_SIZE = ((nr_pages +
	 *      nr_pages / 256) / 512) * PAGE_SIZE = nr_pages * 8 + nr_pages / 32
	 *      <= nr_pages * 9
	 *
	 */
	smr_entry_shift = csv_get_smr_entry_shift();
	size = ALIGN((nr_pages << PAGE_SHIFT), 1UL << smr_entry_shift) +
		ALIGN(nr_pages * 9, 1UL << smr_entry_shift);
	nr_smr = size >> smr_entry_shift;
	remainder = nr_smr;
	for (i = 0; i < nr_smr; i++) {
		smr = kzalloc(sizeof(*smr), GFP_KERNEL_ACCOUNT);
		if (!smr) {
			ret = -ENOMEM;
			goto e_free_smr;
		}

		smr->hpa = csv_alloc_from_contiguous((1UL << smr_entry_shift),
						nodemask_ptr,
						get_order(1 << smr_entry_shift));
		if (!smr->hpa) {
			kfree(smr);
			ret = -ENOMEM;
			goto e_free_smr;
		}

		smr->npages = ((1UL << smr_entry_shift) >> PAGE_SHIFT);
		list_add_tail(&smr->list, &tmp_list);

		regions[count].size = (1UL << smr_entry_shift);
		regions[count].base_address = smr->hpa;
		count++;

		if (count >= (PAGE_SIZE / sizeof(regions[0])) || (remainder == count)) {
			set_guest_private_memory->nregions = count;
			set_guest_private_memory->handle = sev->handle;
			set_guest_private_memory->regions_paddr = __sme_pa(regions);

			/* set secury memory region for launch enrypt data */
			ret = csv_issue_cmd(kvm, CSV3_CMD_SET_GUEST_PRIVATE_MEMORY,
					set_guest_private_memory, &error);
			if (ret)
				goto e_free_smr;

			memset(regions, 0, PAGE_SIZE);
			remainder -= count;
			count = 0;
		}
	}

	list_splice(&tmp_list, &csv->smr_list);

	goto done;

e_free_smr:
	if (!list_empty(&tmp_list)) {
		list_for_each_safe(pos, q, &tmp_list) {
			smr = list_entry(pos, struct secure_memory_region, list);
			if (smr) {
				csv_release_to_contiguous(smr->hpa,
							smr->npages << PAGE_SHIFT);
				list_del(&smr->list);
				kfree(smr);
			}
		}
	}
done:
	kfree(set_guest_private_memory);
	kfree(regions);
	return ret;
}

static int csv3_launch_encrypt_data(struct kvm *kvm, struct kvm_sev_cmd *argp)
{
	struct kvm_csv_info *csv = &to_kvm_svm_csv(kvm)->csv_info;
	struct kvm_csv3_launch_encrypt_data params;
	struct csv3_data_launch_encrypt_data *encrypt_data = NULL;
	struct encrypt_data_block *blocks = NULL;
	u8 *data = NULL;
	u32 offset;
	u32 num_entries, num_entries_in_block;
	u32 num_blocks, num_blocks_max;
	u32 i, n;
	unsigned long pfn, pfn_sme_mask;
	int ret = 0;

	if (!csv3_guest(kvm))
		return -ENOTTY;

	if (copy_from_user(&params, (void __user *)(uintptr_t)argp->data,
			   sizeof(params))) {
		ret = -EFAULT;
		goto exit;
	}

	if ((params.len & ~PAGE_MASK) || !params.len || !params.uaddr) {
		ret = -EINVAL;
		goto exit;
	}

	/* Allocate all the guest memory from CMA */
	ret = csv3_set_guest_private_memory(kvm);
	if (ret)
		goto exit;

	num_entries = params.len / PAGE_SIZE;
	num_entries_in_block = ARRAY_SIZE(blocks->entry);
	num_blocks = (num_entries + num_entries_in_block - 1) / num_entries_in_block;
	num_blocks_max = ARRAY_SIZE(encrypt_data->data_blocks);

	if (num_blocks >= num_blocks_max) {
		ret = -EINVAL;
		goto exit;
	}

	data = vzalloc(params.len);
	if (!data) {
		ret = -ENOMEM;
		goto exit;
	}
	if (copy_from_user(data, (void __user *)params.uaddr, params.len)) {
		ret = -EFAULT;
		goto data_free;
	}

	blocks = vzalloc(num_blocks * sizeof(*blocks));
	if (!blocks) {
		ret = -ENOMEM;
		goto data_free;
	}

	for (offset = 0, i = 0, n = 0; offset < params.len; offset += PAGE_SIZE) {
		pfn = vmalloc_to_pfn(offset + data);
		pfn_sme_mask = __sme_set(pfn << PAGE_SHIFT) >> PAGE_SHIFT;
		if (offset && ((blocks[n].entry[i].pfn + 1) == pfn_sme_mask))
			blocks[n].entry[i].npages += 1;
		else {
			if (offset) {
				i = (i + 1) % num_entries_in_block;
				n = (i == 0) ? (n + 1) : n;
			}
			blocks[n].entry[i].pfn = pfn_sme_mask;
			blocks[n].entry[i].npages = 1;
		}
	}

	encrypt_data = kzalloc(sizeof(*encrypt_data), GFP_KERNEL);
	if (!encrypt_data) {
		ret = -ENOMEM;
		goto block_free;
	}

	encrypt_data->handle = csv->sev->handle;
	encrypt_data->length = params.len;
	encrypt_data->gpa = params.gpa;
	for (i = 0; i <= n; i++) {
		encrypt_data->data_blocks[i] =
		__sme_set(vmalloc_to_pfn((void *)blocks + i * sizeof(*blocks)) << PAGE_SHIFT);
	}

	clflush_cache_range(data, params.len);
	ret = csv_issue_cmd(kvm, CSV3_CMD_LAUNCH_ENCRYPT_DATA,
			    encrypt_data, &argp->error);

	kfree(encrypt_data);
block_free:
	vfree(blocks);
data_free:
	vfree(data);
exit:
	return ret;
}

static int csv3_launch_encrypt_vmcb(struct kvm *kvm, struct kvm_sev_cmd *argp)
{
	struct kvm_csv_info *csv = &to_kvm_svm_csv(kvm)->csv_info;
	struct csv3_data_launch_encrypt_vmcb *encrypt_vmcb = NULL;
	struct kvm_vcpu *vcpu;
	int ret = 0;
	unsigned long i = 0;

	if (!csv3_guest(kvm))
		return -ENOTTY;

	encrypt_vmcb = kzalloc(sizeof(*encrypt_vmcb), GFP_KERNEL);
	if (!encrypt_vmcb) {
		ret = -ENOMEM;
		goto exit;
	}

	kvm_for_each_vcpu(i, vcpu, kvm) {
		struct vcpu_svm *svm = to_svm(vcpu);

		ret = csv_sync_vmsa(svm);
		if (ret)
			goto e_free;
		clflush_cache_range(svm->sev_es.vmsa, PAGE_SIZE);
		clflush_cache_range(svm->vmcb, PAGE_SIZE);
		encrypt_vmcb->handle = csv->sev->handle;
		encrypt_vmcb->vcpu_id = i;
		encrypt_vmcb->vmsa_addr = __sme_pa(svm->sev_es.vmsa);
		encrypt_vmcb->vmsa_len = PAGE_SIZE;
		encrypt_vmcb->shadow_vmcb_addr = __sme_pa(svm->vmcb);
		encrypt_vmcb->shadow_vmcb_len = PAGE_SIZE;
		ret = csv_issue_cmd(kvm, CSV3_CMD_LAUNCH_ENCRYPT_VMCB,
				    encrypt_vmcb, &argp->error);
		if (ret)
			goto e_free;

		svm->current_vmcb->pa = encrypt_vmcb->secure_vmcb_addr;
		svm->vcpu.arch.guest_state_protected = true;
	}

e_free:
	kfree(encrypt_vmcb);
exit:
	return ret;
}

static int csv_mem_enc_op(struct kvm *kvm, void __user *argp)
{
	struct kvm_sev_cmd sev_cmd;
	int r = -EINVAL;

	if (!argp)
		return 0;

	if (copy_from_user(&sev_cmd, argp, sizeof(struct kvm_sev_cmd)))
		return -EFAULT;

	mutex_lock(&kvm->lock);

	switch (sev_cmd.id) {
	case KVM_CSV3_INIT:
		r = csv3_guest_init(kvm, &sev_cmd);
		break;
	case KVM_CSV3_LAUNCH_ENCRYPT_DATA:
		r = csv3_launch_encrypt_data(kvm, &sev_cmd);
		break;
	case KVM_CSV3_LAUNCH_ENCRYPT_VMCB:
		r = csv3_launch_encrypt_vmcb(kvm, &sev_cmd);
		break;
	default:
		mutex_unlock(&kvm->lock);
		if (likely(csv_x86_ops.mem_enc_ioctl))
			r = csv_x86_ops.mem_enc_ioctl(kvm, argp);
		goto out;
	}

	mutex_unlock(&kvm->lock);

	if (copy_to_user(argp, &sev_cmd, sizeof(struct kvm_sev_cmd)))
		r = -EFAULT;

out:
	return r;
}

void __init csv_init(struct kvm_x86_ops *ops)
{
	if (boot_cpu_has(X86_FEATURE_CSV3)) {
		memcpy(&csv_x86_ops, ops, sizeof(struct kvm_x86_ops));

		ops->mem_enc_ioctl = csv_mem_enc_op;
		ops->vm_size = sizeof(struct kvm_svm_csv);
	}
}
