.. zephyr:code-sample:: nrf_clock_control_producer
   :name: Clock control producer

   Request a clock producer for a peripheral with the nordic,clock-producer property.

Overview
********

This sample shows the clock-consumer scheme for Nordic nRF peripherals: a UARTE names a
clock producer through the ``nordic,clock-producer`` devicetree property, and the driver
requests that producer according to the ``CONFIG_UART_NRFX_UARTE_CLOCK_MGMT_*`` scheme (here
``ON_PM``, so the producer is held while the device is resumed).

The property is independent of ``clocks``: ``clocks`` describes the frequency the peripheral
is clocked at, while ``nordic,clock-producer`` names a producer to turn on (for example the
HFXO) to improve clock accuracy. Removing the property makes the instance keep the clock the
hardware requests automatically.

The showcase UARTE is selected per board with the ``clk-showcase-uart`` alias, so each board
overlay points it at a producer that exists on that SoC:

* nRF54L15 DK: ``uart30`` requests ``&xo`` (the driver-backed HFXO producer).
* nRF5340 DK: ``uart1`` requests ``&hfclk`` (HFCLK, and thus HFXO).

Building and Running
********************

.. zephyr-app-commands::
   :zephyr-app: samples/boards/nordic/clock_control_producer
   :board: nrf54l15dk/nrf54l15/cpuapp
   :goals: build flash
   :compact:
