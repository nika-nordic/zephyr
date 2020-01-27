/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EXAMPLE_CONFIG_H__
#define EXAMPLE_CONFIG_H__

#define SHARED_MEM_START DT_IPC_SHM_BASE_ADDRESS
#define SHARED_MEM_SIZE  0x7c00

#define MASTER_ADDR 0x01
#define REMOTE_ADDR 0x02

#endif
