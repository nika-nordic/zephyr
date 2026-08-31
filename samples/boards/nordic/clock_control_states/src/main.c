/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <soc.h>

/* The clock consumer is selected per case with the clk-consumer alias. Any peripheral node
 * works: the sample only reads the node's clock state (its `clocks` property) to learn the
 * frequency it is clocked at and, when the state names a producer, requests that producer
 * directly through the clock control API. It never touches the peripheral itself, so the
 * same code applies to a UARTE, SPIM, TIMER or any other consumer.
 */
#define CLK_CONSUMER_NODE DT_ALIAS(clk_consumer)

int main(void)
{
	printf("Clock control producer sample on %s\n", CONFIG_BOARD_TARGET);
	printf("Consumer %s is clocked at %u Hz\n",
	       DT_NODE_FULL_NAME(CLK_CONSUMER_NODE),
	       (unsigned int)NRF_PERIPH_GET_FREQUENCY(CLK_CONSUMER_NODE));

#if NRF_DT_CLK_PRESENT(CLK_CONSUMER_NODE)
	const struct device *const clk = NRF_DT_CLK_DEV(CLK_CONSUMER_NODE);
	int err;

	if (!device_is_ready(clk)) {
		printf("clock producer %s is not ready\n", clk->name);
		return 0;
	}

	/* NULL specification leaves every clock attribute unconstrained, so the producer is
	 * simply started. request_sync() blocks until it is running.
	 */
	err = nrf_clock_control_request_sync(clk, NULL, K_FOREVER);
	if (err < 0) {
		printf("failed to request %s: %d\n", clk->name, err);
		return 0;
	}
	printf("requested clock producer %s\n", clk->name);

	/* The producer is running now; a real peripheral would be operated here. */

	err = nrf_clock_control_release(clk, NULL);
	if (err < 0) {
		printf("failed to release %s: %d\n", clk->name, err);
		return 0;
	}
	printf("released clock producer %s\n", clk->name);
#else
	printf("no clock producer to request; the default clock is used\n");
#endif

	return 0;
}
