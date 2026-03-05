/*
 * Copyright (c) 2026 Nordic Semiconductor
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_DT_BINDINGS_CLOCK_NRF_CLOCKS_H_
#define ZEPHYR_INCLUDE_DT_BINDINGS_CLOCK_NRF_CLOCKS_H_

#define NRF_DT_CLK_DEFAULT	(32768) /* All peripherals should have this by default in `clocks` property */
#define NRF_DT_CLK_INT		(32767) /* If internal oscillator is to be used */
#define NRF_DT_CLK_XTAL		(128)   /* If crystal is to be used */
#define NRF_DT_CLK_XTAL_TUNED	(127)   /* If tuned crystal is to be used (XOTUNED) */

#endif /* ZEPHYR_INCLUDE_DT_BINDINGS_CLOCK_NRF_CLOCKS_H_ */
