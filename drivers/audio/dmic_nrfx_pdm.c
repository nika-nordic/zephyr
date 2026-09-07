/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nordic_nrf_pdm

#include <zephyr/audio/dmic.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/dt-bindings/clock/nrf-auxpll.h>
#include <soc.h>
#include <dmm.h>
#include <nrfx_pdm.h>

#include <zephyr/logging/log.h>
#include <zephyr/irq.h>
LOG_MODULE_REGISTER(dmic_nrfx_pdm, CONFIG_AUDIO_DMIC_LOG_LEVEL);

#define NODE_AUDIO_AUXPLL DT_NODELABEL(audio_auxpll)

/*
 * Clocking scheme is selected at build time, not per call. A node opts into the clock-state
 * model by pointing its `clocks` property at a nordic,clock-state; the two schemes are mutually
 * exclusive in a given build, so the state-specific code below is compiled in wholesale instead
 * of being chosen at runtime (mirrors UARTE_ANY_CLK in the UARTE shim). When set, the producer
 * to request is a compile-time constant taken from the state and lives in the const config;
 * otherwise the legacy path discovers it at runtime.
 */
#define PDM_INST_IS_STATE(inst) +NRF_DT_CLK_IS_STATE_BY_IDX(DT_DRV_INST(inst), 0)
#if (0 DT_INST_FOREACH_STATUS_OKAY(PDM_INST_IS_STATE)) > 0
#define PDM_ANY_CLK 1
#else
#define PDM_ANY_CLK 0
#endif

#if !PDM_ANY_CLK
/*
 * Legacy (pre-clock-state) audio-clock discovery. Everything in this block - the base-clock
 * frequency, the per-SoC/per-binding ACLK frequency, and the AUDIO_* helpers consumed by the
 * ACLK BUILD_ASSERTs - exists only for nodes that have NOT moved to the clock-state model. Once
 * every node uses nordic,clock-state this whole block (and the asserts that consume it) is
 * deleted; the state path derives all of it from the selected nordic,clock-state instead.
 */
#define DMIC_NRFX_CLOCK_FREQ MHZ(32)

#if defined(CONFIG_SOC_SERIES_NRF54H) || defined(CONFIG_SOC_SERIES_NRF92)
#undef DMIC_NRFX_CLOCK_FREQ
#define DMIC_NRFX_CLOCK_FREQ       MHZ(16)
#define DMIC_NRFX_AUDIO_CLOCK_FREQ DT_PROP_OR(DT_NODELABEL(audiopll), frequency, 0)
#define AUDIO_ASSERT_MSG                                                                           \
	"Clock source ACLK requires frequency property to be set in the audiopll node."
#define AUDIO_FREQUENCY_DEFINED DT_NODE_HAS_PROP(DT_NODELABEL(audiopll), frequency)

#elif DT_NODE_HAS_STATUS_OKAY(NODE_AUDIO_AUXPLL)
/* Target output frequency of the AUXPLL (the driver rounds it to the nearest
 * achievable divider); used as the audio base clock for the PDM ratio.
 */
#define DMIC_NRFX_AUDIO_CLOCK_FREQ DT_PROP_OR(NODE_AUDIO_AUXPLL, clock_frequency, 0)
#define AUDIO_ASSERT_MSG                                                                           \
	"Clock source ACLK requires the clock-frequency property to be set in the audio_auxpll "  \
	"node."
#define AUDIO_FREQUENCY_DEFINED DT_NODE_HAS_PROP(NODE_AUDIO_AUXPLL, clock_frequency)

#elif CONFIG_CLOCK_CONTROL_NRF
#define DMIC_NRFX_AUDIO_CLOCK_FREQ                                                                 \
	DT_PROP_OR(DT_NODELABEL(aclk), clock_frequency,                                            \
		   DT_PROP_OR(DT_NODELABEL(clock), hfclkaudio_frequency, 0))
#define AUDIO_ASSERT_MSG                                                                           \
	"Clock source ACLK requires hfclkaudio_frequency property to be set in the clock node"     \
	"or clock_frequency property to be set in the aclk node."
#define AUDIO_FREQUENCY_DEFINED                                                                    \
	(DT_NODE_HAS_PROP(DT_NODELABEL(clock), hfclkaudio_frequency) ||                            \
	 DT_NODE_HAS_PROP(DT_NODELABEL(aclk), clock_frequency))

#elif defined(CONFIG_CLOCK_CONTROL_NRF_HFCLKAUDIO)
#define DMIC_NRFX_AUDIO_CLOCK_FREQ                                                                 \
	DT_PROP_OR(DT_NODELABEL(aclk), clock_frequency,                                            \
		   DT_PROP_OR(DT_COMPAT_GET_ANY_STATUS_OKAY(nordic_nrf_clock_hfclkaudio),          \
			      clock_frequency,                                                     \
			      DT_PROP_OR(DT_COMPAT_GET_ANY_STATUS_OKAY(                             \
						 nordic_nrf_clock_hfclkaudio),                       \
					 hfclkaudio_frequency, 0)))
#define AUDIO_ASSERT_MSG                                                                           \
	"Clock source ACLK requires the clock-frequency property (or the deprecated "              \
	"hfclkaudio-frequency) to be set in the hfclkaudio node."
#define AUDIO_FREQUENCY_DEFINED                                                                    \
	(DT_NODE_HAS_PROP(DT_COMPAT_GET_ANY_STATUS_OKAY(nordic_nrf_clock_hfclkaudio),              \
			  clock_frequency) ||                                                      \
	 DT_NODE_HAS_PROP(DT_COMPAT_GET_ANY_STATUS_OKAY(nordic_nrf_clock_hfclkaudio),              \
			  hfclkaudio_frequency))

#elif defined(CONFIG_CLOCK_CONTROL_NRF_XO24M)
#define DMIC_NRFX_AUDIO_CLOCK_FREQ                                                                 \
	DT_PROP_OR(DT_NODELABEL(aclk), clock_frequency,                                            \
		   DT_PROP_OR(DT_COMPAT_GET_ANY_STATUS_OKAY(nordic_nrf_clock_xo24m),               \
			      clock_frequency, 0))
#define AUDIO_ASSERT_MSG                                                                           \
	"Clock source ACLK requires clock_frequency property to be set in the xo24m node."
#define AUDIO_FREQUENCY_DEFINED                                                                    \
	DT_NODE_HAS_PROP(DT_COMPAT_GET_ANY_STATUS_OKAY(nordic_nrf_clock_xo24m), clock_frequency)

#elif defined(CONFIG_CLOCK_CONTROL_NRF_XO)
#define DMIC_NRFX_AUDIO_CLOCK_FREQ                                                                 \
	DT_PROP_OR(DT_NODELABEL(aclk), clock_frequency,                                            \
		   DT_PROP_OR(DT_COMPAT_GET_ANY_STATUS_OKAY(nordic_nrf_clock_xo), clock_frequency, \
			      0))
#define AUDIO_ASSERT_MSG                                                                           \
	"Clock source ACLK requires clock_frequency property to be set in the xo node."
#define AUDIO_FREQUENCY_DEFINED                                                                    \
	DT_NODE_HAS_PROP(DT_COMPAT_GET_ANY_STATUS_OKAY(nordic_nrf_clock_xo), clock_frequency)

#else
#define DMIC_NRFX_AUDIO_CLOCK_FREQ 0
#define AUDIO_ASSERT_MSG           "ACLK clock source not available use another clock source."
#define AUDIO_FREQUENCY_DEFINED    0

#endif
#endif /* !PDM_ANY_CLK */

struct dmic_nrfx_pdm_drv_data {
	nrfx_pdm_t pdm;
#if PDM_ANY_CLK
	/* Producer is a compile-time constant in the config (see clk_dev there); nothing about the
	 * clock is kept in runtime data.
	 */
#elif CONFIG_CLOCK_CONTROL_NRFS_AUDIOPLL || DT_NODE_HAS_STATUS_OKAY(NODE_AUDIO_AUXPLL)
	const struct device *audiopll_dev;
#elif CONFIG_CLOCK_CONTROL_NRF
	struct onoff_manager *clk_mgr;
#elif defined(CONFIG_CLOCK_CONTROL_NRF_HFCLKAUDIO) || defined(CONFIG_CLOCK_CONTROL_NRF_HFCLK) ||   \
	defined(CONFIG_CLOCK_CONTROL_NRF_XO) || defined(CONFIG_CLOCK_CONTROL_NRF_XO24M)
	const struct device *clk_dev;
#endif
	struct onoff_client clk_cli;
	struct k_mem_slab *mem_slab;
	uint32_t block_size;
	struct k_msgq mem_slab_queue;
	struct k_msgq rx_queue;
#if !PDM_ANY_CLK
	/* Legacy path only: whether a producer has to be requested (state path uses clk_dev). */
	bool request_clock : 1;
#endif
	bool configured    : 1;
	volatile bool active;
	volatile bool stopping;
};

struct dmic_nrfx_pdm_drv_cfg {
	nrfx_pdm_event_handler_t event_handler;
	nrfx_pdm_config_t nrfx_def_cfg;
	const struct pinctrl_dev_config *pcfg;
	enum clock_source {
		PCLK32M,
		PCLK32M_HFXO,
		ACLK
	} clk_src;
#if PDM_ANY_CLK
	/*
	 * Clock-state prototype: clk_src (the mux), clk_dev (producer to request, NULL for none)
	 * and clk_state_base_freq (signal frequency) are all derived at build time from the
	 * nordic,clock-state selected by the node's `clocks` property.
	 */
	const struct device *clk_dev;
	uint32_t clk_state_base_freq;
#endif
	/* Per-instance completion handler, bound to the device at compile time (see the trampoline
	 * in PDM_NRFX_DEVICE). It lets the shared clock-started logic reach the const config
	 * without any runtime back-reference, while the driver still requests and releases the
	 * producer exclusively through the nrf_clock_control API.
	 */
	onoff_client_callback clk_started_cb;
	void *mem_reg;
};

static void free_buffer(struct dmic_nrfx_pdm_drv_data *drv_data, void *buffer)
{
	k_mem_slab_free(drv_data->mem_slab, buffer);
	LOG_DBG("Freed buffer %p", buffer);
}

static void stop_pdm(struct dmic_nrfx_pdm_drv_data *drv_data)
{
	drv_data->stopping = true;
	nrfx_pdm_stop(&drv_data->pdm);
}

static int request_clock(const struct device *dev)
{
	struct dmic_nrfx_pdm_drv_data *drv_data = dev->data;
#if PDM_ANY_CLK
	const struct dmic_nrfx_pdm_drv_cfg *drv_cfg = dev->config;

	if (drv_cfg->clk_dev == NULL) {
		return 0;
	}
	return nrf_clock_control_request(drv_cfg->clk_dev, NULL, &drv_data->clk_cli);
#else
	if (!drv_data->request_clock) {
		return 0;
	}
#if CONFIG_CLOCK_CONTROL_NRFS_AUDIOPLL || DT_NODE_HAS_STATUS_OKAY(NODE_AUDIO_AUXPLL)
	return nrf_clock_control_request(drv_data->audiopll_dev, NULL, &drv_data->clk_cli);
#elif CONFIG_CLOCK_CONTROL_NRF
	return onoff_request(drv_data->clk_mgr, &drv_data->clk_cli);
#elif defined(CONFIG_CLOCK_CONTROL_NRF_HFCLKAUDIO) || defined(CONFIG_CLOCK_CONTROL_NRF_HFCLK) ||   \
	defined(CONFIG_CLOCK_CONTROL_NRF_XO) || defined(CONFIG_CLOCK_CONTROL_NRF_XO24M)
	return nrf_clock_control_request(drv_data->clk_dev, NULL, &drv_data->clk_cli);
#else
	return -ENOTSUP;
#endif
#endif /* PDM_ANY_CLK */
}

static int release_clock(const struct device *dev)
{
#if PDM_ANY_CLK
	const struct dmic_nrfx_pdm_drv_cfg *drv_cfg = dev->config;

	if (drv_cfg->clk_dev == NULL) {
		return 0;
	}
	return nrf_clock_control_release(drv_cfg->clk_dev, NULL);
#else
	struct dmic_nrfx_pdm_drv_data *drv_data = dev->data;

	if (!drv_data->request_clock) {
		return 0;
	}
#if CONFIG_CLOCK_CONTROL_NRFS_AUDIOPLL || DT_NODE_HAS_STATUS_OKAY(NODE_AUDIO_AUXPLL)
	return nrf_clock_control_release(drv_data->audiopll_dev, NULL);
#elif CONFIG_CLOCK_CONTROL_NRF
	return onoff_release(drv_data->clk_mgr);
#elif defined(CONFIG_CLOCK_CONTROL_NRF_HFCLKAUDIO) || defined(CONFIG_CLOCK_CONTROL_NRF_HFCLK) ||   \
	defined(CONFIG_CLOCK_CONTROL_NRF_XO) || defined(CONFIG_CLOCK_CONTROL_NRF_XO24M)
	return nrf_clock_control_release(drv_data->clk_dev, NULL);
#else
	return -ENOTSUP;
#endif
#endif /* PDM_ANY_CLK */
}

static void event_handler(const struct device *dev, const nrfx_pdm_evt_t *evt)
{
	struct dmic_nrfx_pdm_drv_data *drv_data = dev->data;
	const struct dmic_nrfx_pdm_drv_cfg *drv_cfg = dev->config;
	int ret;
	bool stop = false;
	void *mem_slab_buffer;

	if (evt->buffer_requested) {
		void *buffer;
		int err;

		ret = k_mem_slab_alloc(drv_data->mem_slab, &mem_slab_buffer, K_NO_WAIT);
		if (ret < 0) {
			LOG_ERR("Failed to allocate buffer: %d", ret);
			stop = true;
		} else {
			ret = dmm_buffer_in_prepare(drv_cfg->mem_reg, mem_slab_buffer,
						    drv_data->block_size, &buffer);
			if (ret < 0) {
				LOG_ERR("Failed to prepare buffer: %d", ret);
				free_buffer(drv_data, mem_slab_buffer);
				stop_pdm(drv_data);
				return;
			}
			ret = k_msgq_put(&drv_data->mem_slab_queue, &mem_slab_buffer, K_NO_WAIT);
			if (ret < 0) {
				LOG_ERR("Unable to put mem slab in queue");
				free_buffer(drv_data, mem_slab_buffer);
				stop_pdm(drv_data);
				return;
			}
			err = nrfx_pdm_buffer_set(&drv_data->pdm, buffer, drv_data->block_size / 2);
			if (err != 0) {
				LOG_ERR("Failed to set buffer: %d", err);
				stop = true;
			}
		}
	}

	if (drv_data->stopping) {
		if (evt->buffer_released) {
			ret = k_msgq_get(&drv_data->mem_slab_queue, &mem_slab_buffer, K_NO_WAIT);
			if (ret < 0) {
				LOG_ERR("No buffers to free");
				return;
			}
			ret = dmm_buffer_in_release(drv_cfg->mem_reg, mem_slab_buffer,
						    drv_data->block_size, evt->buffer_released);
			if (ret < 0) {
				LOG_ERR("Failed to release buffer: %d", ret);
				free_buffer(drv_data, mem_slab_buffer);
				return;
			}
			free_buffer(drv_data, mem_slab_buffer);
		}

		if (drv_data->active) {
			drv_data->active = false;
			ret = release_clock(dev);
			if (ret < 0) {
				LOG_ERR("Failed to release clock: %d", ret);
				return;
			}
		}
	} else if (evt->buffer_released) {
		ret = k_msgq_get(&drv_data->mem_slab_queue, &mem_slab_buffer, K_NO_WAIT);
		if (ret < 0) {
			LOG_ERR("No buffers to free");
			stop_pdm(drv_data);
			return;
		}
		ret = dmm_buffer_in_release(drv_cfg->mem_reg, mem_slab_buffer,
					    drv_data->block_size, evt->buffer_released);
		if (ret < 0) {
			LOG_ERR("Failed to release buffer: %d", ret);
			free_buffer(drv_data, mem_slab_buffer);
			stop_pdm(drv_data);
			return;
		}
		ret = k_msgq_put(&drv_data->rx_queue,
				 &mem_slab_buffer,
				 K_NO_WAIT);
		if (ret < 0) {
			LOG_ERR("No room in RX queue");
			stop = true;
			free_buffer(drv_data, mem_slab_buffer);
		} else {
			LOG_DBG("Queued buffer %p", evt->buffer_released);
		}
	}
	if (stop) {
		stop_pdm(drv_data);
	}
}

static int dmic_nrfx_pdm_configure(const struct device *dev,
				   struct dmic_cfg *config)
{
	struct dmic_nrfx_pdm_drv_data *drv_data = dev->data;
	const struct dmic_nrfx_pdm_drv_cfg *drv_cfg = dev->config;
	struct pdm_chan_cfg *channel = &config->channel;
	struct pcm_stream_cfg *stream = &config->streams[0];
	uint32_t def_map, alt_map;
	nrfx_pdm_config_t nrfx_cfg;
	int8_t gain_limit;
	int err;

	if (drv_data->active) {
		LOG_ERR("Cannot configure device while it is active");
		return -EBUSY;
	}

	/*
	 * This device supports only one stream and can be configured to return
	 * 16-bit samples for two channels (Left+Right samples) or one channel
	 * (only Left samples). Left and Right samples can be optionally swapped
	 * by changing the PDM_CLK edge on which the sampling is done
	 * Provide the valid channel maps for both the above configurations
	 * (to inform the requester what is available) and check if what is
	 * requested can be actually configured.
	 */
	if (channel->req_num_chan == 1) {
		def_map = dmic_build_channel_map(0, 0, PDM_CHAN_LEFT);
		alt_map = dmic_build_channel_map(0, 0, PDM_CHAN_RIGHT);

		channel->act_num_chan = 1;
	} else {
		def_map = dmic_build_channel_map(0, 0, PDM_CHAN_LEFT)
			| dmic_build_channel_map(1, 0, PDM_CHAN_RIGHT);
		alt_map = dmic_build_channel_map(0, 0, PDM_CHAN_RIGHT)
			| dmic_build_channel_map(1, 0, PDM_CHAN_LEFT);

		channel->act_num_chan = 2;
	}

	channel->act_num_streams = 1;
	channel->act_chan_map_hi = 0;

	if (channel->req_num_streams != 1 ||
	    channel->req_num_chan > 2 ||
	    channel->req_num_chan < 1 ||
	    (channel->req_chan_map_lo != def_map &&
	     channel->req_chan_map_lo != alt_map) ||
	    channel->req_chan_map_hi != channel->act_chan_map_hi) {
		LOG_ERR("Requested configuration is not supported");
		return -EINVAL;
	}

	/* If either rate or width is 0, the stream is to be disabled. */
	if (stream->pcm_rate == 0 || stream->pcm_width == 0) {
		if (drv_data->configured) {
			nrfx_pdm_uninit(&drv_data->pdm);
			drv_data->configured = false;
		}

		return 0;
	}

	if (stream->pcm_width != 16) {
		LOG_ERR("Only 16-bit samples are supported");
		return -EINVAL;
	}

	nrfx_cfg = drv_cfg->nrfx_def_cfg;
	nrfx_cfg.mode = channel->req_num_chan == 1
		      ? NRF_PDM_MODE_MONO
		      : NRF_PDM_MODE_STEREO;
	if (channel->req_chan_map_lo == def_map) {
		nrfx_cfg.edge = NRF_PDM_EDGE_LEFTFALLING;
		channel->act_chan_map_lo = def_map;
	} else {
		nrfx_cfg.edge = NRF_PDM_EDGE_LEFTRISING;
		channel->act_chan_map_lo = alt_map;
	}

	/* Convert requested gain to 0.5 dB steps limited by defined bounds. */
	gain_limit = CLAMP((2 * stream->gain_db + NRF_PDM_GAIN_DEFAULT),
			   NRF_PDM_GAIN_MINIMUM,
			   NRF_PDM_GAIN_MAXIMUM);
	nrfx_cfg.gain_l = gain_limit;
	nrfx_cfg.gain_r = gain_limit;

#if NRF_PDM_HAS_SELECTABLE_CLOCK
	nrfx_cfg.mclksrc = drv_cfg->clk_src == ACLK
			 ? NRF_PDM_MCLKSRC_ACLK
			 : NRF_PDM_MCLKSRC_PCLK32M;
#endif
	nrfx_pdm_output_t output_config = {
#if PDM_ANY_CLK
		.base_clock_freq = drv_cfg->clk_state_base_freq,
#else
		.base_clock_freq = (NRF_PDM_HAS_SELECTABLE_CLOCK && drv_cfg->clk_src == ACLK)
					   ? DMIC_NRFX_AUDIO_CLOCK_FREQ
					   : DMIC_NRFX_CLOCK_FREQ,
#endif
		.sampling_rate = config->streams[0].pcm_rate,
		.output_freq_min = config->io.min_pdm_clk_freq,
		.output_freq_max = config->io.max_pdm_clk_freq
	};

	if (nrfx_pdm_prescalers_calc(&output_config, &nrfx_cfg.prescalers) != 0) {
		LOG_ERR("Cannot find suitable PDM clock configuration.");
		return -EINVAL;
	}

	if (drv_data->configured) {
		nrfx_pdm_uninit(&drv_data->pdm);
		drv_data->configured = false;
	}

	err = nrfx_pdm_init(&drv_data->pdm, &nrfx_cfg, drv_cfg->event_handler);
	if (err != 0) {
		LOG_ERR("Failed to initialize PDM: %d", err);
		return -EIO;
	}

	drv_data->block_size = stream->block_size;
	drv_data->mem_slab   = stream->mem_slab;

	/* Unless the PCLK32M source is used with the HFINT oscillator
	 * (which is always available without any additional actions),
	 * it is required to request the proper clock to be running
	 * before starting the transfer itself.
	 */
#if !PDM_ANY_CLK
	drv_data->request_clock =
		(drv_cfg->clk_src != PCLK32M && (IS_ENABLED(CONFIG_CLOCK_CONTROL_NRF) ||
						 (IS_ENABLED(CONFIG_CLOCK_CONTROL_NRF_COMMON) &&
						  !(IS_ENABLED(CONFIG_SOC_SERIES_NRF54H) ||
						    IS_ENABLED(CONFIG_SOC_SERIES_NRF92)))));
#endif
	drv_data->configured = true;
	return 0;
}

static int start_transfer(const struct device *dev)
{
	struct dmic_nrfx_pdm_drv_data *drv_data = dev->data;
	int err;
	int ret;

	err = nrfx_pdm_start(&drv_data->pdm);
	if (err == 0) {
		return 0;
	}

	LOG_ERR("Failed to start PDM: %d", err);

	ret = release_clock(dev);
	if (ret < 0) {
		LOG_ERR("Failed to release clock: %d", ret);
	}

	drv_data->active = false;
	return -EIO;
}

static void clock_started(const struct device *dev)
{
	struct dmic_nrfx_pdm_drv_data *drv_data = dev->data;

	/* The driver can turn out to be inactive at this point if the STOP command was triggered
	 * before the clock has started; do not start the transfer in such case, just release.
	 */
	if (!drv_data->active) {
		int ret = release_clock(dev);

		if (ret < 0) {
			LOG_ERR("Failed to release clock: %d", ret);
		}
	} else {
		(void)start_transfer(dev);
	}
}

static int trigger_start(const struct device *dev)
{
	struct dmic_nrfx_pdm_drv_data *drv_data = dev->data;
	const struct dmic_nrfx_pdm_drv_cfg *drv_cfg = dev->config;
	int ret;

	drv_data->active = true;

	/* If it is required to use certain HF clock, request it to be running
	 * first. If not, start the transfer directly.
	 */
#if PDM_ANY_CLK
	if (drv_cfg->clk_dev != NULL) {
#else
	if (drv_data->request_clock) {
#endif
		sys_notify_init_callback(&drv_data->clk_cli.notify, drv_cfg->clk_started_cb);
		ret = request_clock(dev);
		if (ret < 0) {
			drv_data->active = false;

			LOG_ERR("Failed to request clock: %d", ret);
			return -EIO;
		}
	} else {
		ret = start_transfer(dev);
		if (ret < 0) {
			return ret;
		}
	}

	return 0;
}

static int dmic_nrfx_pdm_trigger(const struct device *dev,
				 enum dmic_trigger cmd)
{
	struct dmic_nrfx_pdm_drv_data *drv_data = dev->data;

	switch (cmd) {
	case DMIC_TRIGGER_PAUSE:
	case DMIC_TRIGGER_STOP:
		if (drv_data->active) {
			drv_data->stopping = true;
			nrfx_pdm_stop(&drv_data->pdm);
		}
		break;

	case DMIC_TRIGGER_RELEASE:
	case DMIC_TRIGGER_START:
		if (!drv_data->configured) {
			LOG_ERR("Device is not configured");
			return -EIO;
		} else if (!drv_data->active) {
			drv_data->stopping = false;
			return trigger_start(dev);
		}
		break;

	default:
		LOG_ERR("Invalid command: %d", cmd);
		return -EINVAL;
	}

	return 0;
}

static int dmic_nrfx_pdm_read(const struct device *dev,
			      uint8_t stream,
			      void **buffer, size_t *size, int32_t timeout)
{
	struct dmic_nrfx_pdm_drv_data *drv_data = dev->data;
	int ret;

	ARG_UNUSED(stream);

	if (!drv_data->configured) {
		LOG_ERR("Device is not configured");
		return -EIO;
	}

	ret = k_msgq_get(&drv_data->rx_queue, buffer, SYS_TIMEOUT_MS(timeout));
	if (ret != 0) {
		LOG_DBG("No audio data to be read");
	} else {
		LOG_DBG("Released buffer %p", *buffer);

		*size = drv_data->block_size;
	}

	return ret;
}

static void init_clock_manager(const struct device *dev)
{
#if PDM_ANY_CLK
	/* The selected clock state names the producer (if any) directly in the config, so there is
	 * no backend discovery to do and nothing to record in runtime data.
	 */
	ARG_UNUSED(dev);
#elif DT_NODE_HAS_STATUS_OKAY(NODE_AUDIO_AUXPLL)
	struct dmic_nrfx_pdm_drv_data *drv_data = dev->data;
	drv_data->audiopll_dev = DEVICE_DT_GET(NODE_AUDIO_AUXPLL);
#elif CONFIG_CLOCK_CONTROL_NRF
	clock_control_subsys_t subsys;
	struct dmic_nrfx_pdm_drv_data *drv_data = dev->data;
#if NRF_CLOCK_HAS_HFCLKAUDIO || NRF_CLOCK_HAS_HFCLK24M
	const struct dmic_nrfx_pdm_drv_cfg *drv_cfg = dev->config;

	if (drv_cfg->clk_src == ACLK) {
		subsys = COND_CODE_1(NRF_CLOCK_HAS_HFCLK24M, (CLOCK_CONTROL_NRF_SUBSYS_HF24M),
			(CLOCK_CONTROL_NRF_SUBSYS_HFAUDIO));
	} else {
		subsys = CLOCK_CONTROL_NRF_SUBSYS_HF;
	}
#else
	subsys = CLOCK_CONTROL_NRF_SUBSYS_HF;
#endif

	drv_data->clk_mgr = z_nrf_clock_control_get_onoff(subsys);
	__ASSERT_NO_MSG(drv_data->clk_mgr != NULL);
#elif defined(CONFIG_CLOCK_CONTROL_NRF_HFCLKAUDIO) || defined(CONFIG_CLOCK_CONTROL_NRF_HFCLK) ||   \
	defined(CONFIG_CLOCK_CONTROL_NRF_XO) || defined(CONFIG_CLOCK_CONTROL_NRF_XO24M)
	struct dmic_nrfx_pdm_drv_data *drv_data = dev->data;
#if NRF_CLOCK_HAS_HFCLKAUDIO || NRF_CLOCK_HAS_HFCLK24M
	const struct dmic_nrfx_pdm_drv_cfg *drv_cfg = dev->config;

	if (drv_cfg->clk_src == ACLK) {
		drv_data->clk_dev = DEVICE_DT_GET_ONE(
			COND_CODE_1(NRF_CLOCK_HAS_HFCLK24M,
			(nordic_nrf_clock_xo24m), (nordic_nrf_clock_hfclkaudio)));
	} else {
		drv_data->clk_dev = DEVICE_DT_GET_ONE(
			COND_CODE_1(NRF_CLOCK_HAS_HFCLK,
			(nordic_nrf_clock_hfclk), (nordic_nrf_clock_xo)));
	}
#else
	drv_data->clk_dev = DEVICE_DT_GET_ONE(
		COND_CODE_1(NRF_CLOCK_HAS_HFCLK, (nordic_nrf_clock_hfclk), (nordic_nrf_clock_xo)));
#endif

	__ASSERT_NO_MSG(drv_data->clk_dev != NULL);
#elif CONFIG_CLOCK_CONTROL_NRFS_AUDIOPLL
	struct dmic_nrfx_pdm_drv_data *drv_data = dev->data;

	drv_data->audiopll_dev = DEVICE_DT_GET(DT_NODELABEL(audiopll));
#endif
}

static DEVICE_API(dmic, dmic_ops) = {
	.configure = dmic_nrfx_pdm_configure,
	.trigger = dmic_nrfx_pdm_trigger,
	.read = dmic_nrfx_pdm_read,
};

#define PDM_CLK_SRC(inst) DT_STRING_TOKEN(DT_DRV_INST(inst), clock_source)

/* Clock-state prototype helpers. A node opts in by pointing `clocks` at a nordic,clock-state. */
#if DT_NODE_EXISTS(DT_NODELABEL(aclk))
#define PDM_ACLK_NODE DT_NODELABEL(aclk)
#else
/* No audio-clock signal on this SoC; DT_SAME_NODE below then always resolves to PCLK32M. */
#define PDM_ACLK_NODE DT_ROOT
#endif

/* Mux position inferred from the state's signal identity. */
#define PDM_STATE_MUX(inst)                                                                        \
	COND_CODE_1(DT_SAME_NODE(NRF_DT_CLK_OUTPUT_BY_IDX(DT_DRV_INST(inst), 0), PDM_ACLK_NODE),    \
		    (ACLK), (PCLK32M))

/* Producer device to request for the selected state, or NULL when it names none. */
#define PDM_STATE_DEV(inst)                                                                        \
	COND_CODE_1(NRF_DT_CLK_PRESENT_BY_IDX(DT_DRV_INST(inst), 0),                                \
		    (NRF_DT_CLK_DEV_BY_IDX(DT_DRV_INST(inst), 0)), (NULL))

#define PDM_CLK_CFG_FIELDS(inst)                                                                   \
	COND_CODE_1(PDM_ANY_CLK,                                                                    \
		    (.clk_src = PDM_STATE_MUX(inst),                                                \
		     .clk_dev = PDM_STATE_DEV(inst),                                                \
		     .clk_state_base_freq = NRF_PERIPH_GET_FREQUENCY_BY_IDX(DT_DRV_INST(inst), 0),),\
		    (.clk_src = PDM_CLK_SRC(inst),))

#if !PDM_ANY_CLK
/*
 * Legacy ACLK sanity checks. They rely on the legacy clock_source token and the AUDIO_* helpers
 * defined at the top of the file, so they are compiled only on the pre-clock-state path and are
 * removed together with that block once every node uses nordic,clock-state.
 */
#define PDM_LEGACY_ACLK_ASSERTS(inst)                                                              \
	BUILD_ASSERT(PDM_CLK_SRC(inst) != ACLK || NRF_PDM_HAS_SELECTABLE_CLOCK,                    \
		     "Clock source ACLK is not available.");                                       \
	BUILD_ASSERT(PDM_CLK_SRC(inst) != ACLK || AUDIO_FREQUENCY_DEFINED, AUDIO_ASSERT_MSG);
#else
#define PDM_LEGACY_ACLK_ASSERTS(inst)
#endif

#define PDM_NRFX_DEVICE(inst)                                                                      \
	static void *rx_msgs##inst[DT_INST_PROP(inst, queue_size)];                                \
	static void *mem_slab_msgs##inst[DT_INST_PROP(inst, queue_size)];                          \
	static struct dmic_nrfx_pdm_drv_data dmic_nrfx_pdm_data##inst = {                          \
		.pdm = NRFX_PDM_INSTANCE(DT_INST_REG_ADDR(inst)),                                  \
	};                                                                                         \
	static int pdm_nrfx_init##inst(const struct device *dev)                                   \
	{                                                                                          \
		IRQ_CONNECT(DT_INST_IRQN(inst), DT_INST_IRQ(inst, priority), nrfx_pdm_irq_handler, \
			    &dmic_nrfx_pdm_data##inst.pdm, 0);                                     \
		const struct dmic_nrfx_pdm_drv_cfg *drv_cfg = dev->config;                         \
		int err = pinctrl_apply_state(drv_cfg->pcfg, PINCTRL_STATE_DEFAULT);               \
		if (err < 0) {                                                                     \
			return err;                                                                \
		}                                                                                  \
		k_msgq_init(&dmic_nrfx_pdm_data##inst.rx_queue, (char *)rx_msgs##inst,             \
			    sizeof(void *), ARRAY_SIZE(rx_msgs##inst));                            \
		k_msgq_init(&dmic_nrfx_pdm_data##inst.mem_slab_queue, (char *)mem_slab_msgs##inst, \
			    sizeof(void *), ARRAY_SIZE(mem_slab_msgs##inst));                      \
		init_clock_manager(dev);                                                           \
		return 0;                                                                          \
	}                                                                                          \
	static void event_handler##inst(const nrfx_pdm_evt_t *evt)                                 \
	{                                                                                          \
		event_handler(DEVICE_DT_INST_GET(inst), evt);                                      \
	}                                                                                          \
	static void clock_started##inst(struct onoff_manager *mgr, struct onoff_client *cli,       \
					uint32_t state, int res)                                   \
	{                                                                                          \
		ARG_UNUSED(mgr);                                                                    \
		ARG_UNUSED(cli);                                                                    \
		ARG_UNUSED(state);                                                                  \
		ARG_UNUSED(res);                                                                    \
		clock_started(DEVICE_DT_INST_GET(inst));                                           \
	}                                                                                          \
	PINCTRL_DT_INST_DEFINE(inst);                                                              \
	static const struct dmic_nrfx_pdm_drv_cfg dmic_nrfx_pdm_cfg##inst = {                      \
		.event_handler = event_handler##inst,                                              \
		.nrfx_def_cfg = NRFX_PDM_DEFAULT_CONFIG(0, 0),                                     \
		.nrfx_def_cfg.skip_gpio_cfg = true,                                                \
		.nrfx_def_cfg.skip_psel_cfg = true,                                                \
		.pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(inst),                                      \
		.clk_started_cb = clock_started##inst,                                             \
		PDM_CLK_CFG_FIELDS(inst)                                                           \
		.mem_reg = DMM_DEV_TO_REG(DT_DRV_INST(inst)),                                      \
	};                                                                                         \
	NRF_DT_CHECK_NODE_HAS_REQUIRED_MEMORY_REGIONS(DT_DRV_INST(inst));                          \
	PDM_LEGACY_ACLK_ASSERTS(inst)                                                              \
	DEVICE_DT_INST_DEFINE(inst, pdm_nrfx_init##inst, NULL, &dmic_nrfx_pdm_data##inst,          \
			      &dmic_nrfx_pdm_cfg##inst, POST_KERNEL,                               \
			      CONFIG_AUDIO_DMIC_INIT_PRIORITY, &dmic_ops);

DT_INST_FOREACH_STATUS_OKAY(PDM_NRFX_DEVICE)
