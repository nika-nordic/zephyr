/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/device.h>

/* The showcase instance is selected per board with the clk-showcase-uart alias, so that
 * each board can point it at a UARTE that references its own clock producer, see the board
 * overlays.
 */
#define CLK_SHOWCASE_NODE DT_ALIAS(clk_showcase_uart)

#if DT_NODE_EXISTS(CLK_SHOWCASE_NODE) && DT_NODE_HAS_STATUS_OKAY(CLK_SHOWCASE_NODE) && \
	defined(CONFIG_UART_ASYNC_API)
#include <zephyr/drivers/uart.h>

static uint8_t tx_buf[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

/* Transmit over an instance that references a clock producer with its
 * nordic,clock-producer property, so that the producer is requested by the shim according
 * to the selected CONFIG_UART_NRFX_UARTE_CLOCK_MGMT_* scheme.
 */
static void clk_showcase(void)
{
	const struct device *const uart_dev = DEVICE_DT_GET(CLK_SHOWCASE_NODE);
	int err;

	err = uart_tx(uart_dev, tx_buf, sizeof(tx_buf), SYS_FOREVER_US);
	if (err < 0) {
		printf("uart_tx failed: %d\n", err);
	}
}
#else
static void clk_showcase(void)
{
}
#endif

int main(void)
{
	printf("Clock control producer sample on %s\n", CONFIG_BOARD_TARGET);

	clk_showcase();

	return 0;
}
