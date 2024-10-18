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
extern int csv_comm_mode;

void csv_update_api_version(struct sev_user_data_status *status);
int csv_cmd_buffer_len(int cmd);
void csv_restore_mailbox_mode_postprocess(void);
int csv_do_ringbuf_cmds(int *psp_ret);
int csv_ioctl_do_hgsc_import(struct sev_issue_cmd *argp);
int csv_ioctl_do_download_firmware(struct sev_issue_cmd *argp);

static inline bool csv_version_greater_or_equal(u32 build)
{
	return hygon_csv_build >= build;
}

static inline bool csv_in_ring_buffer_mode(void)
{
	return csv_comm_mode == CSV_COMM_RINGBUFFER_ON;
}

#endif	/* __CCP_HYGON_CSV_DEV_H__ */
