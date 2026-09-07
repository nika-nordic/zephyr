/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nordic_nrf_auxpll

#include <errno.h>
#include <stdint.h>

#include <zephyr/arch/cpu.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/sys/util.h>
#include <zephyr/toolchain.h>
#include <zephyr/dt-bindings/clock/nrf-auxpll.h>

#include <hal/nrf_auxpll.h>


/* Check dt-bindings match MDK frequency division definitions*/
#define CHECK_DTS_BINDING_VS_MDK(dt, mdk) \
	BUILD_ASSERT((mdk) == (dt), \
		"Different " #mdk " definition in MDK and devicetree binding")

CHECK_DTS_BINDING_VS_MDK(NRF_AUXPLL_FREQ_DIV_MIN,	 NRF_AUXPLL_FREQUENCY_DIV_MIN);
CHECK_DTS_BINDING_VS_MDK(NRF_AUXPLL_FREQ_DIV_AUDIO_44K1, NRF_AUXPLL_FREQUENCY_AUDIO_44K1);
CHECK_DTS_BINDING_VS_MDK(NRF_AUXPLL_FREQ_DIV_USB24M,	 NRF_AUXPLL_FREQUENCY_USB_24M);
CHECK_DTS_BINDING_VS_MDK(NRF_AUXPLL_FREQ_DIV_AUDIO_48K,	 NRF_AUXPLL_FREQUENCY_AUDIO_48K);
CHECK_DTS_BINDING_VS_MDK(NRF_AUXPLL_FREQ_DIV_MAX,	 NRF_AUXPLL_FREQUENCY_DIV_MAX);

/* maximum lock time in us, >10x time observed experimentally */
#define AUXPLL_LOCK_TIME_MAX_US  20000
/* lock wait step in us*/
#define AUXPLL_LOCK_WAIT_STEP_US 1000

struct dev_data_auxpll {
	struct onoff_manager mgr;
	onoff_notify_fn notify;
	const struct device *dev;
};

struct clock_control_nrf_auxpll_config {
	NRF_AUXPLL_Type *auxpll;
	uint32_t ref_clk_hz;
	uint32_t ficr_ctune;
	nrf_auxpll_config_t cfg;
	nrf_auxpll_freq_div_ratio_t frequency;
	uint8_t out_div;
};

/* Helper function to convert out_div to register AUXPLLCTRL.OUTSEL value */
static inline void set_out_div(const struct clock_control_nrf_auxpll_config *config)
{
	nrf_auxpll_ctrl_outsel_t out_div_nrfx;
	uint8_t out_div_dts = config->out_div;

	switch (out_div_dts) {
	case NRF_AUXPLL_CTRL_OUTSEL_DIV_6:
		out_div_nrfx = (nrf_auxpll_ctrl_outsel_t)AUXPLL_AUXPLLCTRL_OUTSEL_OUTSEL_Div6;
		break;
	case NRF_AUXPLL_CTRL_OUTSEL_DIV_8:
		out_div_nrfx = (nrf_auxpll_ctrl_outsel_t)AUXPLL_AUXPLLCTRL_OUTSEL_OUTSEL_Div8;
		break;
	case NRF_AUXPLL_CTRL_OUTSEL_DIV_12:
		out_div_nrfx = (nrf_auxpll_ctrl_outsel_t)AUXPLL_AUXPLLCTRL_OUTSEL_OUTSEL_Div12;
		break;
	case NRF_AUXPLL_CTRL_OUTSEL_DIV_16:
		out_div_nrfx = (nrf_auxpll_ctrl_outsel_t)AUXPLL_AUXPLLCTRL_OUTSEL_OUTSEL_Div16;
		break;
	default:
		/* Values less than 5 align with the OUTSEL register value */
		out_div_nrfx = out_div_dts;
		break;
	}

	nrf_auxpll_ctrl_outsel_set(config->auxpll, out_div_nrfx);
}

static int clock_control_nrf_auxpll_on(struct dev_data_auxpll *dev_data)
{
	const struct clock_control_nrf_auxpll_config *config = dev_data->dev->config;
	bool locked;

	nrf_auxpll_task_trigger(config->auxpll, NRF_AUXPLL_TASK_START);

	NRFX_WAIT_FOR(nrf_auxpll_mode_locked_check(config->auxpll),
					AUXPLL_LOCK_TIME_MAX_US / AUXPLL_LOCK_WAIT_STEP_US,
					AUXPLL_LOCK_WAIT_STEP_US, locked);

	return locked ? 0 : -ETIMEDOUT;
}

static int clock_control_nrf_auxpll_off(struct dev_data_auxpll *dev_data)
{
	const struct clock_control_nrf_auxpll_config *config = dev_data->dev->config;

	nrf_auxpll_task_trigger(config->auxpll, NRF_AUXPLL_TASK_STOP);

	while (nrf_auxpll_running_check(config->auxpll)) {
	}

	return 0;
}

static void onoff_start_auxpll(struct onoff_manager *mgr, onoff_notify_fn notify)
{
	struct dev_data_auxpll *dev_data =
		CONTAINER_OF(mgr, struct dev_data_auxpll, mgr);

	int ret = clock_control_nrf_auxpll_on(dev_data);

	notify(&dev_data->mgr, ret);

}

static void onoff_stop_auxpll(struct onoff_manager *mgr, onoff_notify_fn notify)
{
	struct dev_data_auxpll *dev_data =
		CONTAINER_OF(mgr, struct dev_data_auxpll, mgr);

	clock_control_nrf_auxpll_off(dev_data);
	notify(mgr, 0);
}

static int api_nosys_on_off(const struct device *dev, clock_control_subsys_t sys)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(sys);

	return -ENOSYS;
}

static int api_request_auxpll(const struct device *dev,
			      const struct nrf_clock_spec *spec,
			      struct onoff_client *cli)
{
	struct dev_data_auxpll *dev_data = dev->data;

	ARG_UNUSED(spec);

	return onoff_request(&dev_data->mgr, cli);
}

static int api_release_auxpll(const struct device *dev,
			      const struct nrf_clock_spec *spec)
{
	struct dev_data_auxpll *dev_data = dev->data;

	ARG_UNUSED(spec);

	return onoff_release(&dev_data->mgr);
}

static int api_cancel_or_release_auxpll(const struct device *dev,
					const struct nrf_clock_spec *spec,
					struct onoff_client *cli)
{
	struct dev_data_auxpll *dev_data = dev->data;

	ARG_UNUSED(spec);

	return onoff_cancel_or_release(&dev_data->mgr, cli);
}

static int clock_control_nrf_auxpll_get_rate(const struct device *dev, clock_control_subsys_t sys,
					     uint32_t *rate)
{
	const struct clock_control_nrf_auxpll_config *config = dev->config;
	uint8_t ratio;

	ARG_UNUSED(sys);

	ratio = nrf_auxpll_static_ratio_get(config->auxpll);

	*rate = (ratio * config->ref_clk_hz +
		 (config->ref_clk_hz * (uint64_t)config->frequency) /
			 (AUXPLL_AUXPLLCTRL_FREQUENCY_FREQUENCY_MaximumDiv + 1U)) /
		config->out_div;

	return 0;
}

static enum clock_control_status clock_control_nrf_auxpll_get_status(const struct device *dev,
								     clock_control_subsys_t sys)
{
	const struct clock_control_nrf_auxpll_config *config = dev->config;

	ARG_UNUSED(sys);

	if (nrf_auxpll_mode_locked_check(config->auxpll)) {
		return CLOCK_CONTROL_STATUS_ON;
	}

	return CLOCK_CONTROL_STATUS_OFF;
}

static const struct onoff_transitions transitions = {
	.start = onoff_start_auxpll,
	.stop = onoff_stop_auxpll
};

static int clock_control_nrf_auxpll_init(const struct device *dev)
{
	struct dev_data_auxpll *dev_data = dev->data;
	const struct clock_control_nrf_auxpll_config *config = dev->config;
	int rc;

	rc = onoff_manager_init(&dev_data->mgr, &transitions);
	if (rc < 0) {
		return rc;
	}

	nrf_auxpll_ctrl_frequency_set(config->auxpll, config->frequency);

	nrf_auxpll_lock(config->auxpll);
	nrf_auxpll_trim_ctune_set(config->auxpll, sys_read8(config->ficr_ctune));
	nrf_auxpll_config_set(config->auxpll, &config->cfg);
	set_out_div(config);
	nrf_auxpll_unlock(config->auxpll);

	nrf_auxpll_ctrl_mode_set(config->auxpll, NRF_AUXPLL_CTRL_MODE_LOCKED);

	return 0;
}

static DEVICE_API(nrf_clock_control, drv_api_auxpll) = {
	.std_api = {
		.on = api_nosys_on_off,
		.off = api_nosys_on_off,
		.get_rate = clock_control_nrf_auxpll_get_rate,
		.get_status = clock_control_nrf_auxpll_get_status,
	},
	.request = api_request_auxpll,
	.release = api_release_auxpll,
	.cancel_or_release = api_cancel_or_release_auxpll,
};

/*
 * Frequency <-> divider-token helpers. From the binding:
 *
 *   f_out = (R * f_ref + (f_ref * A) / 2^16) / B,   R = nordic,range idx + 3
 *
 * where A is the fractional PLL divider (register FREQUENCY field). A may be
 * supplied directly through the deprecated nordic,frequency, or derived from
 * the requested output clock-frequency (Hz). All terms are DeviceTree values,
 * so the derivation and its cross-checks happen entirely at build time.
 */
#define AUXPLL_DIV_SCALE (AUXPLL_AUXPLLCTRL_FREQUENCY_FREQUENCY_MaximumDiv + 1U) /* 2^16 */
#define AUXPLL_FREF(n)   DT_PROP(DT_INST_CLOCKS_CTLR(n), clock_frequency)
#define AUXPLL_R(n)      (DT_INST_ENUM_IDX(n, nordic_range) + 3U)
#define AUXPLL_B(n)      DT_INST_PROP(n, nordic_out_div)

/* Forward: output Hz produced by divider token a (mirrors get_rate, truncating). */
#define AUXPLL_HZ_FROM_A(n, a)                                                                     \
	((uint32_t)((AUXPLL_R(n) * (uint64_t)AUXPLL_FREF(n) +                                      \
		     ((uint64_t)AUXPLL_FREF(n) * (a)) / AUXPLL_DIV_SCALE) /                        \
		    AUXPLL_B(n)))

/* A = round( (f_out * B - R * f_ref) * 2^16 / f_ref ). The FREQUENCY field is a
 * continuous 16-bit fractional divider (ratios 4..5), so the target clock-frequency
 * is rounded to the nearest achievable divider - the same scheme as the audiopll
 * shim - which minimises the quantisation error. For example 12288000 rounds to
 * 39846 (real 12288004, +0.37 ppm) rather than snapping to the AUDIO_48K token 39845
 * (real 12287963, -2.94 ppm), and it matches what audiopll produces on nRF54H20.
 */
#define AUXPLL_A_FROM_HZ(n)                                                                        \
	((uint32_t)(((((uint64_t)DT_INST_PROP_OR(n, clock_frequency, 0) * AUXPLL_B(n)) -           \
		      ((uint64_t)AUXPLL_R(n) * AUXPLL_FREF(n))) *                                  \
			     AUXPLL_DIV_SCALE +                                                    \
		     (AUXPLL_FREF(n) / 2U)) /                                                     \
		    AUXPLL_FREF(n)))

/* Effective divider: explicit legacy token wins, else derived from the target Hz. */
#define AUXPLL_A(n)                                                                                \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(n, nordic_frequency),                                    \
		    (DT_INST_PROP_OR(n, nordic_frequency, 0)), (AUXPLL_A_FROM_HZ(n)))

/* Producible band for this node's range/out-div/source clock: A in [0, 2^16 - 1]. */
#define AUXPLL_FMIN(n) AUXPLL_HZ_FROM_A(n, 0)
#define AUXPLL_FMAX(n) AUXPLL_HZ_FROM_A(n, AUXPLL_AUXPLLCTRL_FREQUENCY_FREQUENCY_MaximumDiv)

/* Legacy nordic,frequency must still be one of Nordic's named divider tokens. */
#define AUXPLL_LEGACY_TOKEN_OK(n)                                                                  \
	(!DT_INST_NODE_HAS_PROP(n, nordic_frequency) ||                                            \
	 DT_INST_PROP_OR(n, nordic_frequency, 0) == NRF_AUXPLL_FREQUENCY_DIV_MIN ||                \
	 DT_INST_PROP_OR(n, nordic_frequency, 0) == NRF_AUXPLL_FREQUENCY_AUDIO_44K1 ||             \
	 DT_INST_PROP_OR(n, nordic_frequency, 0) == NRF_AUXPLL_FREQUENCY_USB_24M ||                \
	 DT_INST_PROP_OR(n, nordic_frequency, 0) == NRF_AUXPLL_FREQUENCY_AUDIO_48K ||              \
	 DT_INST_PROP_OR(n, nordic_frequency, 0) == NRF_AUXPLL_FREQUENCY_DIV_MAX)

/* Derived target (clock-frequency) must lie within the producible band. */
#define AUXPLL_TARGET_IN_BAND(n)                                                                   \
	(!DT_INST_NODE_HAS_PROP(n, clock_frequency) ||                                             \
	 (DT_INST_PROP_OR(n, clock_frequency, 0) >= AUXPLL_FMIN(n) &&                              \
	  DT_INST_PROP_OR(n, clock_frequency, 0) <= AUXPLL_FMAX(n)))

#define CLOCK_CONTROL_NRF_AUXPLL_DEFINE(n)                                                         \
	BUILD_ASSERT(DT_INST_NODE_HAS_PROP(n, clock_frequency) ||                                  \
			     DT_INST_NODE_HAS_PROP(n, nordic_frequency),                          \
		"AUXPLL instance " #n " needs clock-frequency (or legacy nordic,frequency)");      \
	BUILD_ASSERT(AUXPLL_LEGACY_TOKEN_OK(n),                                                    \
		"Invalid nordic,frequency value in DeviceTree for AUXPLL instance " #n);           \
	BUILD_ASSERT(AUXPLL_TARGET_IN_BAND(n),                                                     \
		"clock-frequency is outside the AUXPLL producible band for instance " #n);         \
	BUILD_ASSERT(DT_INST_PROP(n, nordic_out_div) > 0,                                          \
		"nordic,out_div must be greater than 0 for AUXPLL instance " #n);                  \
	static struct dev_data_auxpll data_auxpll##n    = {                                        \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
	};                                                                                         \
	static const struct clock_control_nrf_auxpll_config config##n = {                          \
		.auxpll = (NRF_AUXPLL_Type *)DT_INST_REG_ADDR(n),                                  \
		.ref_clk_hz = DT_PROP(DT_INST_CLOCKS_CTLR(n), clock_frequency),                    \
		.ficr_ctune = DT_REG_ADDR(DT_INST_PHANDLE(n, nordic_ficrs)) +                      \
			      DT_INST_PHA(n, nordic_ficrs, offset),                                \
		.cfg =                                                                             \
			{                                                                          \
				.outdrive = DT_INST_PROP(n, nordic_out_drive),                     \
				.current_tune = DT_INST_PROP(n, nordic_current_tune),              \
				.sdm_off = DT_INST_PROP(n, nordic_sdm_disable),                    \
				.dither_off = DT_INST_PROP(n, nordic_dither_disable),              \
				.range = DT_INST_ENUM_IDX(n, nordic_range),                        \
			},                                                                         \
		.frequency = AUXPLL_A(n),                                                          \
		.out_div = DT_INST_PROP(n, nordic_out_div),                                        \
	};                                                                                         \
	                                                                                           \
	DEVICE_DT_INST_DEFINE(n, clock_control_nrf_auxpll_init, NULL, &data_auxpll##n, &config##n, \
			      PRE_KERNEL_1, CONFIG_CLOCK_CONTROL_INIT_PRIORITY,                    \
			      &drv_api_auxpll);

DT_INST_FOREACH_STATUS_OKAY(CLOCK_CONTROL_NRF_AUXPLL_DEFINE)
