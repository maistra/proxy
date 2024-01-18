/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright(c) 2018-2020 Intel Corporation
 */

#ifndef __DLB2_VDCM_H
#define __DLB2_VDCM_H

#ifdef CONFIG_INTEL_DLB2_SIOV

#include <linux/pci.h>

#define DLB2_SIOV_IMS_WORKAROUND

int dlb2_vdcm_init(struct dlb2 *dlb2);
void dlb2_vdcm_exit(struct pci_dev *pdev);

#endif /* CONFIG_INTEL_DLB2_SIOV */
#endif /* __DLB2_VDCM_H */
