/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>

#include <nrfx_gpiote.h>
#include <helpers/nrfx_gppi.h>
#include <hal/nrf_timer.h>
#include <hal/nrf_saadc.h>

#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/adc/adc_nrf.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/counter/counter_nrf.h>
#include <zephyr/drivers/misc/ppi/nrfx_dppi.h>

#include <zephyr/logging/log.h>
#include <zephyr/irq.h>
LOG_MODULE_REGISTER(nrfx_gppi, LOG_LEVEL_INF);

#define DEV_REG(node) ((void*)DT_REG_ADDR(DT_NODELABEL(node)))

#define DT_SPEC_AND_COMMA(node_id, prop, idx) ADC_DT_SPEC_GET_BY_IDX(node_id, idx),
static const struct adc_dt_spec adc_channels[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels, DT_SPEC_AND_COMMA)
};

static int16_t saadc_buf1[9];
static int16_t saadc_buf2[9];
static int16_t * p_curr_buf;

static void adc_nrf_event_handler(nrfx_saadc_evt_t const * p_event)
{
    int err;
    const struct device * timer = DEVICE_DT_GET(DT_NODELABEL(timer20));
    const struct device * saadc = DEVICE_DT_GET(DT_NODELABEL(adc));

    LOG_INF("adc_nrf evt=%d", p_event->type);

    switch (p_event->type)
    {
        case NRFX_SAADC_EVT_DONE:
            break;

        case NRFX_SAADC_EVT_BUF_REQ:
            if (p_curr_buf == saadc_buf1) {
                p_curr_buf = saadc_buf2;
            } else { /* saadc_buf2 or NULL */
                p_curr_buf = saadc_buf1;
            }

            err = adc_nrf_provide_buffer(saadc, p_curr_buf, ARRAY_SIZE(saadc_buf1));
            __ASSERT_NO_MSG(err == 0);
            break;

        case NRFX_SAADC_EVT_READY:
            err = counter_start(timer);
            __ASSERT_NO_MSG(err == 0);
            break;

        case NRFX_SAADC_EVT_FINISHED:
            err = counter_stop(timer);
            __ASSERT_NO_MSG(err == 0);
            break;

        case NRFX_SAADC_EVT_CALIBRATEDONE:
            break;

        case NRFX_SAADC_EVT_LIMIT:
            break;

        default:
            LOG_ERR("Not supported event type");
            break;
    }
}

void setup_saadc_on_timer(void)
{
}

int main(void)
{
	int err;

	LOG_INF("TIMER-GPPI-SAADC sample on %s", CONFIG_BOARD);

	/* --- Configuration --- */

	/* Initialize TIMER */
	const struct device * timer = DEVICE_DT_GET(DT_NODELABEL(timer20));

	struct counter_top_cfg cfg;
	cfg.ticks = counter_us_to_ticks(timer, 500 * 1000UL) - 1;
	cfg.callback = NULL;

	err = counter_set_top_value(timer, &cfg);
	__ASSERT_NO_MSG(err == 0);

	/* Initialize SAADC */
	memset(saadc_buf1, 0xFF, sizeof(saadc_buf1));
	memset(saadc_buf2, 0xFF, sizeof(saadc_buf2));

	for (int i = 0; i < ARRAY_SIZE(adc_channels); i++) {
		err = adc_channel_setup_dt(&adc_channels[i]);
		__ASSERT_NO_MSG(err == 0);
	}

	struct adc_sequence sequence;
	(void)adc_sequence_init_dt(&adc_channels[0], &sequence);
	for (int i = 1; i < ARRAY_SIZE(adc_channels); i++) {
		sequence.channels |= BIT(adc_channels[i].channel_id);
	}

	static const adc_nrf_read_config_t adc_nrf_config_const =
	{
		.oversampling = NRF_SAADC_OVERSAMPLE_DISABLED,
		.burst = NRF_SAADC_BURST_DISABLED,
		.internal_timer_cc = 0,
		.start_on_end = true,
	};

	/* Allocate the interconnect channel. */
	uint32_t handle;
	uint32_t eep = counter_nrf_ppi_event_get(DEV_REG(timer20), NRF_TIMER_EVENT_COMPARE0);
	uint32_t tep = adc_nrf_ppi_task_get(DEV_REG(adc), NRF_SAADC_TASK_SAMPLE);
	err = nrf_dppi_conn_alloc(eep, tep, &handle);
	if (err < 0) {
		LOG_ERR("Failed to setup DPPI.");
	}

	/* Enable the interconnect channel. */
	nrf_dppi_conn_ctrl(handle, true);

	/* --- Operation --- */

    const struct device * saadc = DEVICE_DT_GET(DT_NODELABEL(adc));
	err = adc_nrf_read_start(saadc, 0x7, &adc_nrf_config_const, adc_nrf_event_handler);
	if (err < 0) {
		LOG_ERR("Failed to start ADC: %d", err);
	}

	return 0;
}
