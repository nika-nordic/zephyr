/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <nrfx_gpiote.h>
#include "gpiote_nrfx.h"

#define GPIOTE_NRFX_NATIVE_INST_DEF(instname) \
	nrfx_gpiote_t instname;
#define GPIOTE_NRFX_NATIVE_INST_DEFINE(node_id) \
	GPIOTE_NRFX_NATIVE_INST_DEF(GPIOTE_NRFX_INST_BY_NODE(node_id))

DT_FOREACH_STATUS_OKAY(nordic_nrf_gpiote, GPIOTE_NRFX_NATIVE_INST_DEFINE)

#define GPIOTE_NRFX_INST_IDX(idx) _CONCAT(NRFX_GPIOTE, idx, _INST_IDX)
#define GPIOTE_INST_IDX(node_id) DT_PROP(node_id, instance)
#define GPIOTE_INST_AND_COMMA(node_id) \
	[GPIOTE_INST_IDX(node_id)] = &GPIOTE_NRFX_INST_BY_NODE(node_id),

static int gpiote_nrfx_native_init(void) {
	nrfx_gpiote_t *gpiote_instances[] = {
		DT_FOREACH_STATUS_OKAY(nordic_nrf_gpiote, GPIOTE_INST_AND_COMMA)
	};

	for (int inst = 0; inst < ARRAY_SIZE(gpiote_instances); inst++) {
		gpiote_instances[inst]->p_reg = &NRF_GPIOTE_regs[inst];
		gpiote_instances[inst]->cb.drv_inst_idx = NRFX_REG_TO_INSTANCE(GPIOTE, &NRF_GPIOTE_regs[inst]);
	}

	return 0;
}

SYS_INIT(gpiote_nrfx_native_init, PRE_KERNEL_1, 0);
