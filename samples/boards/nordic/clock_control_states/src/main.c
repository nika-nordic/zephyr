/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <soc.h>

/* The clock consumer is selected per case with the clk-consumer alias. The sample only reads the
 * node's clock state(s) (its `clocks` property): for each it learns the frequency the consumer is
 * clocked at and, when the state names a producer, requests that producer through the clock
 * control API. It never operates the peripheral itself.
 *
 * A consumer may reference one or two states. Two states model a peripheral with two independent
 * clock signals, such as the TDM SCK (index 0) and MCK (index 1); a single state applies to the
 * whole consumer. This mirrors how the driver derives the mux position and the producer(s) to
 * request from the very same states.
 */
#define CLK_CONSUMER_NODE DT_ALIAS(clk_consumer)

/* Request the producer of a selected state and immediately release it. A real driver holds the
 * request for as long as the peripheral is clocked.
 */
static void demo_producer(const char *role, const struct device *clk)
{
	int err;

	if (!device_is_ready(clk)) {
		printf("  %s: producer %s is not ready\n", role, clk->name);
		return;
	}

	/* NULL specification leaves every clock attribute unconstrained, so the producer is simply
	 * started. request_sync() blocks until it is running.
	 */
	err = nrf_clock_control_request_sync(clk, NULL, K_FOREVER);
	if (err < 0) {
		printf("  %s: failed to request %s: %d\n", role, clk->name, err);
		return;
	}
	printf("  %s: requested producer %s\n", role, clk->name);

	err = nrf_clock_control_release(clk, NULL);
	if (err < 0) {
		printf("  %s: failed to release %s: %d\n", role, clk->name, err);
		return;
	}
	printf("  %s: released producer %s\n", role, clk->name);
}

#define REPORT(role, idx)                                                                          \
	printf("  %s (clocks[%d]): %u Hz, %s\n", role, idx,                                         \
	       (unsigned int)NRF_PERIPH_GET_FREQUENCY_BY_IDX(CLK_CONSUMER_NODE, idx),               \
	       NRF_DT_CLK_PRESENT_BY_IDX(CLK_CONSUMER_NODE, idx) ? "has producer" : "no producer")

int main(void)
{
	printf("Clock control producer sample on %s\n", CONFIG_BOARD_TARGET);
	printf("Consumer %s\n", DT_NODE_FULL_NAME(CLK_CONSUMER_NODE));

#if DT_CLOCKS_HAS_IDX(CLK_CONSUMER_NODE, 1)
	/* Two clock states: index 0 is SCK, index 1 is MCK. */
	REPORT("SCK", 0);
	REPORT("MCK", 1);
#if NRF_DT_CLK_PRESENT_BY_IDX(CLK_CONSUMER_NODE, 0)
	demo_producer("SCK", NRF_DT_CLK_DEV_BY_IDX(CLK_CONSUMER_NODE, 0));
#endif
#if NRF_DT_CLK_PRESENT_BY_IDX(CLK_CONSUMER_NODE, 1)
	demo_producer("MCK", NRF_DT_CLK_DEV_BY_IDX(CLK_CONSUMER_NODE, 1));
#endif
#else
	/* A single state (or a plain clock) applies to the whole consumer. */
	REPORT("clock", 0);
#if NRF_DT_CLK_PRESENT(CLK_CONSUMER_NODE)
	demo_producer("clock", NRF_DT_CLK_DEV(CLK_CONSUMER_NODE));
#endif
#endif

	return 0;
}
