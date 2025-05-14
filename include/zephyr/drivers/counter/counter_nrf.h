/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_COUNTER_COUNTER_NRF_H
#define ZEPHYR_INCLUDE_DRIVERS_COUNTER_COUNTER_NRF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/drivers/counter.h>
#include <nrfx_timer.h>

static inline uint32_t counter_nrf_ppi_task_get(void * nrf_dev, nrf_timer_task_t task)
{
	// We could assume p_regs is always first in each of nordic device static struct.
	// Otherwise we need explicit device base address as parameter
	// TODO: fix for rtc

	NRF_TIMER_Type * p_reg = (NRF_TIMER_Type *)nrf_dev;
	return nrf_timer_task_address_get(p_reg, task);
}

static inline uint32_t counter_nrf_ppi_event_get(void * nrf_dev, nrf_timer_event_t event)
{
	// We could assume p_regs is always first in each of nordic device structures
	// Otherwise we need explicit device base address as parameter
	// TODO: fix for rtc

	NRF_TIMER_Type * p_reg = (NRF_TIMER_Type *)nrf_dev;
	return nrf_timer_event_address_get(p_reg, event);
}

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_DRIVERS_COUNTER_COUNTER_NRF_H */
