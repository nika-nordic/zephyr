.. zephyr:code-sample:: nrf_clock_control_states
   :name: Clock control states

   Request the clock producer of a peripheral's clock state through the clock control API.

Overview
********

This sample shows the clock-consumer scheme for Nordic nRF peripherals, using only the clock
control API so that it applies to any peripheral. A consumer node (selected with the
``clk-consumer`` alias) points its generic ``clocks`` property at one of the clock states that
the SoC defines, for example ``clocks = <&pclk16m_hfxo>``. A clock state
(``nordic,clock-state``) names two things:

* a ``clock-output`` that gives the frequency the peripheral is clocked at (read with
  ``NRF_PERIPH_GET_FREQUENCY``, never acted upon), and
* an optional ``clock-producer`` that has to be running for that state.

The sample reads the selected state, prints the frequency, and when the state has a producer
requests it with :c:func:`nrf_clock_control_request_sync` and releases it with
:c:func:`nrf_clock_control_release`. It never operates the peripheral itself, so the same
code applies to a UARTE, I2S, SPIM, TIMER or any other consumer.

Cases
*****

Each case is a separate overlay under ``boards/`` (with a matching conf file where extra
Kconfig is needed), wired as a scenario in ``tests.yaml``. New cases are added as individual
overlays over time.

.. list-table::
   :header-rows: 1

   * - Consumer / state
     - Board
     - What it shows
   * - uart30, ``pclk16m_hfxo``
     - nrf54l15dk/nrf54l15/cpuapp
     - 16 MHz base, requesting the ``xo`` producer for accuracy.
   * - uart00, ``hclkcore_hfint``
     - nrf54l15dk/nrf54l15/cpuapp
     - Core clock (HCLKCORE) from the internal oscillator; no producer.
   * - uart00, ``hclkcore_hfxo``
     - nrf54l15dk/nrf54l15/cpuapp
     - Core clock (HCLKCORE) from the HFXO, requesting ``xo``.
   * - i2s20, ``pclk32m_hfint``
     - nrf54l15dk/nrf54l15/cpuapp
     - 32 MHz clock from the internal oscillator; no producer.
   * - i2s20, ``pclk32m_hfxo``
     - nrf54l15dk/nrf54l15/cpuapp
     - 32 MHz clock from the HFXO, requesting ``xo``.
   * - tdm, ``aclk_xo24m``
     - nrf54lm20dk/nrf54lm20a/cpuapp
     - 24 MHz audio clock from the HFXO, requesting ``xo24m`` (nRF54LM20 has no I2S).
   * - uart1, ``pclk16m_hfxo``
     - nrf5340dk/nrf5340/cpuapp
     - 16 MHz base, requesting HFCLK (and thus the HFXO).
   * - i2s0, ``aclk_hfclkaudio``
     - nrf5340dk/nrf5340/cpuapp
     - Audio clock from the HFXO-backed audio PLL, requesting ``hfclkaudio``.

Building and Running
********************

Select a case with its overlay (and conf, if present):

.. zephyr-app-commands::
   :zephyr-app: samples/boards/nordic/clock_control_states
   :board: nrf54l15dk/nrf54l15/cpuapp
   :gen-args: -DEXTRA_DTC_OVERLAY_FILE="boards/nrf54l15dk_uart00_hclkcore_hfxo.overlay"
   :goals: build flash
   :compact:
