/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * HYGON CSV driver interface
 *
 * Copyright (C) 2024 Hygon Info Technologies Ltd.
 *
 * Author: Liyang Han <hanliyang@hygon.cn>
 */

#ifndef __CCP_HYGON_CSV_DEV_H__
#define __CCP_HYGON_CSV_DEV_H__

#include <linux/psp-sev.h>

#define CSV_FW_FILE		"hygon/csv.fw"

extern u32 hygon_csv_build;

void csv_update_api_version(struct sev_user_data_status *status);
int csv_cmd_buffer_len(int cmd);
int csv_ioctl_do_hgsc_import(struct sev_issue_cmd *argp);
int csv_ioctl_do_download_firmware(struct sev_issue_cmd *argp);

static inline bool csv_version_greater_or_equal(u32 build)
{
	return hygon_csv_build >= build;
}

#endif	/* __CCP_HYGON_CSV_DEV_H__ */
