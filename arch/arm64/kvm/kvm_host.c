// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2024 Ant Group.
 */
#include <linux/kvm_host.h>
#include <kvm/arm_pmu.h>
#include <asm/arm_pmuv3.h>

#include "vgic/vgic.h"

static enum kvm_mode kvm_mode = KVM_MODE_DEFAULT;

static int __init early_kvm_mode_cfg(char *arg)
{
	if (!arg)
		return -EINVAL;

	if (strcmp(arg, "none") == 0) {
		kvm_mode = KVM_MODE_NONE;
		return 0;
	}

	if (!is_hyp_mode_available()) {
		pr_warn_once("KVM is not available. Ignoring kvm-arm.mode\n");
		return 0;
	}

	if (strcmp(arg, "protected") == 0) {
		if (!is_kernel_in_hyp_mode())
			kvm_mode = KVM_MODE_PROTECTED;
		else
			pr_warn_once("Protected KVM not available with VHE\n");

		return 0;
	}

	if (strcmp(arg, "nvhe") == 0 && !WARN_ON(is_kernel_in_hyp_mode())) {
		kvm_mode = KVM_MODE_DEFAULT;
		return 0;
	}

	if (strcmp(arg, "nested") == 0 && !WARN_ON(!is_kernel_in_hyp_mode())) {
		kvm_mode = KVM_MODE_NV;
		return 0;
	}

	return -EINVAL;
}
early_param("kvm-arm.mode", early_kvm_mode_cfg);

enum kvm_mode kvm_get_mode(void)
{
	return kvm_mode;
}

struct gic_kvm_info *gic_kvm_info;

void __init vgic_set_kvm_info(const struct gic_kvm_info *info)
{
	BUG_ON(gic_kvm_info != NULL);
	gic_kvm_info = kmalloc(sizeof(*info), GFP_KERNEL);
	if (gic_kvm_info)
		*gic_kvm_info = *info;
}

DEFINE_STATIC_KEY_FALSE(kvm_arm_pmu_available);

LIST_HEAD(arm_pmus);
DEFINE_MUTEX(arm_pmus_lock);

void kvm_host_pmu_init(struct arm_pmu *pmu)
{
	struct arm_pmu_entry *entry;

	/*
	 * Check the sanitised PMU version for the system, as KVM does not
	 * support implementations where PMUv3 exists on a subset of CPUs.
	 */
	if (!pmuv3_implemented(kvm_arm_pmu_get_pmuver_limit()))
		return;

	mutex_lock(&arm_pmus_lock);

	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		goto out_unlock;

	entry->arm_pmu = pmu;
	list_add_tail(&entry->entry, &arm_pmus);

	if (list_is_singular(&arm_pmus))
		static_branch_enable(&kvm_arm_pmu_available);

out_unlock:
	mutex_unlock(&arm_pmus_lock);
}

u8 kvm_arm_pmu_get_pmuver_limit(void)
{
	u64 tmp;

	tmp = read_sanitised_ftr_reg(SYS_ID_AA64DFR0_EL1);
	tmp = cpuid_feature_cap_perfmon_field(tmp,
					      ID_AA64DFR0_EL1_PMUVer_SHIFT,
					      ID_AA64DFR0_EL1_PMUVer_V3P5);
	return FIELD_GET(ARM64_FEATURE_MASK(ID_AA64DFR0_EL1_PMUVer), tmp);
}
