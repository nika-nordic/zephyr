/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

static uint8_t tx_buf[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

int main(void)
{
	int err;

	printf("Hello World! %s\n", CONFIG_BOARD_TARGET);

	const struct device *const uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart1));

	err = uart_tx(uart_dev, tx_buf, sizeof(tx_buf), SYS_FOREVER_US);
	if (err < 0) {
		printf("uart_tx failed: %d\n", err);
	}

	return 0;
}
