// SPDX-License-Identifier: GPL-2.0-only
/*
 * HYGON CSV interface
 *
 * Copyright (C) 2024 Hygon Info Technologies Ltd.
 *
 * Author: Liyang Han <hanliyang@hygon.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/psp.h>
#include <linux/psp-hygon.h>
#include <uapi/linux/psp-hygon.h>

#include "csv-dev.h"
#include "psp-dev.h"

/*
 * Hygon CSV build info:
 *    Hygon CSV build info is 32-bit in length other than 8-bit as that
 *    in AMD SEV.
 */
u32 hygon_csv_build;

/*
 * csv_update_api_version used to update the api version of HYGON CSV
 * firmwareat driver side.
 * Currently, we only need to update @hygon_csv_build.
 */
void csv_update_api_version(struct sev_user_data_status *status)
{
	if (status) {
		hygon_csv_build = (status->flags >> 9) |
				   ((u32)status->build << 23);
	}
}

int csv_cmd_buffer_len(int cmd)
{
	switch (cmd) {
	case CSV_CMD_HGSC_CERT_IMPORT:		return sizeof(struct csv_data_hgsc_cert_import);
	case CSV_CMD_RING_BUFFER:		return sizeof(struct csv_data_ring_buffer);
	case CSV3_CMD_LAUNCH_ENCRYPT_DATA:	return sizeof(struct csv3_data_launch_encrypt_data);
	case CSV3_CMD_LAUNCH_ENCRYPT_VMCB:	return sizeof(struct csv3_data_launch_encrypt_vmcb);
	case CSV3_CMD_UPDATE_NPT:		return sizeof(struct csv3_data_update_npt);
	case CSV3_CMD_SET_SMR:			return sizeof(struct csv3_data_set_smr);
	case CSV3_CMD_SET_SMCR:			return sizeof(struct csv3_data_set_smcr);
	case CSV3_CMD_SET_GUEST_PRIVATE_MEMORY:
					return sizeof(struct csv3_data_set_guest_private_memory);
	case CSV3_CMD_DBG_READ_VMSA:		return sizeof(struct csv3_data_dbg_read_vmsa);
	case CSV3_CMD_DBG_READ_MEM:		return sizeof(struct csv3_data_dbg_read_mem);
	case CSV3_CMD_SEND_ENCRYPT_DATA:	return sizeof(struct csv3_data_send_encrypt_data);
	case CSV3_CMD_SEND_ENCRYPT_CONTEXT:
					return sizeof(struct csv3_data_send_encrypt_context);
	case CSV3_CMD_RECEIVE_ENCRYPT_DATA:
					return sizeof(struct csv3_data_receive_encrypt_data);
	case CSV3_CMD_RECEIVE_ENCRYPT_CONTEXT:
					return sizeof(struct csv3_data_receive_encrypt_context);
	default:				return 0;
	}
}

int csv_ioctl_do_hgsc_import(struct sev_issue_cmd *argp)
{
	struct csv_user_data_hgsc_cert_import input;
	struct csv_data_hgsc_cert_import *data;
	void *hgscsk_blob, *hgsc_blob;
	int ret;

	if (!hygon_psp_hooks.sev_dev_hooks_installed)
		return -ENODEV;

	if (copy_from_user(&input, (void __user *)argp->data, sizeof(input)))
		return -EFAULT;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	/* copy HGSCSK certificate blobs from userspace */
	hgscsk_blob = psp_copy_user_blob(input.hgscsk_cert_address, input.hgscsk_cert_len);
	if (IS_ERR(hgscsk_blob)) {
		ret = PTR_ERR(hgscsk_blob);
		goto e_free;
	}

	data->hgscsk_cert_address = __psp_pa(hgscsk_blob);
	data->hgscsk_cert_len = input.hgscsk_cert_len;

	/* copy HGSC certificate blobs from userspace */
	hgsc_blob = psp_copy_user_blob(input.hgsc_cert_address, input.hgsc_cert_len);
	if (IS_ERR(hgsc_blob)) {
		ret = PTR_ERR(hgsc_blob);
		goto e_free_hgscsk;
	}

	data->hgsc_cert_address = __psp_pa(hgsc_blob);
	data->hgsc_cert_len = input.hgsc_cert_len;

	ret = hygon_psp_hooks.__sev_do_cmd_locked(CSV_CMD_HGSC_CERT_IMPORT,
						  data, &argp->error);

	kfree(hgsc_blob);
e_free_hgscsk:
	kfree(hgscsk_blob);
e_free:
	kfree(data);
	return ret;
}
