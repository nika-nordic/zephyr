/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_ADC_ADC_NRF_H
#define ZEPHYR_INCLUDE_DRIVERS_ADC_ADC_NRF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/drivers/adc.h>
#include <nrfx_saadc.h>

typedef nrfx_saadc_event_handler_t adc_nrf_event_handler_t;

typedef nrfx_saadc_adv_config_t adc_nrf_read_config_t;

int adc_nrf_provide_buffer(const struct device *dev, void * buffer, size_t sample_cnt);

int adc_nrf_read_start(const struct device *dev, uint32_t channels,
                       const adc_nrf_read_config_t * config, adc_nrf_event_handler_t handler);

int adc_nrf_read_stop_async(const struct device *dev);

// Optional helper API for common use-cases.
// Could be left as application responsibility
int adc_nrf_ppi_setup_start_on_end(const struct device *dev);

// Optional helper API for common use-cases
// Could be left as application responsibility
int adc_nrf_ppi_setup_sample_on_event(const struct device *dev, uint32_t event_addr);

static inline uint32_t adc_nrf_ppi_task_get(void * nrf_dev, nrf_saadc_task_t task)
{
	// We could assume p_regs is always first in each of nordic device static struct.
	// Otherwise we need explicit device base address as parameter

	NRF_SAADC_Type * p_reg = (NRF_SAADC_Type *)nrf_dev;
	return nrf_saadc_task_address_get(p_reg, task);
}

static inline uint32_t adc_nrf_ppi_event_get(void * nrf_dev, nrf_saadc_event_t event)
{
	// We could assume p_regs is always first in each of nordic device structures
	// Otherwise we need explicit device base address as parameter

	NRF_SAADC_Type * p_reg = (NRF_SAADC_Type *)nrf_dev;
	return nrf_saadc_event_address_get(p_reg, event);
}


#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_DRIVERS_ADC_ADC_NRF_H */
