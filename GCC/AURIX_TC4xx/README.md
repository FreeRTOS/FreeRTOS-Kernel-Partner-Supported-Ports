# Overview

This directory contains the FreeRTOS port for the Infineon AURIX™ TC4xx family of MCUs equipped with the TriCore™ core.

The port is placed under the `GCC/` folder because the compiler used — **tricore-gcc**, bundled with [AURIX™ Development Studio Limited (ADS-L)](https://softwaretools.infineon.com/tools/com.ifx.tb.tool.aurixide) — is a GCC-based toolchain and exposes the standard `__GNUC__` preprocessor macro.

## Software & Tool Dependencies

- [FreeRTOS Kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel) **v11.1.0** or later
- [AURIX™ Development Studio Limited (ADS-L)](https://softwaretools.infineon.com/tools/com.ifx.tb.tool.aurixide) — includes **tricore-gcc** and an integrated debugger. To request access, send an email to `ads@infineon.com` or install via the [Infineon Developer Center Launcher](https://www.infineon.com/cms/en/design-support/tools/utilities/infineon-developer-center-idc-launcher/)
- [Infineon Low Level Drivers (iLLD)](https://www.infineon.com/cms/en/tools/aurix-embedded-sw/aurix-illd-drivers/) — bundled with ADS-L; no separate download required

## Test Project

Demo projects for validating port functionality can be found in the [Partner-Supported Demos](https://github.com/FreeRTOS/FreeRTOS-Partner-Supported-Demos/tree/main/AURIX_TC4D7_ADS/) repository, targeting the [AURIX™ TC4D7 Lite Kit](https://www.infineon.com/evaluation-board/KIT-A3G-TC4D7-LITE).

## Support

- For support queries, please open an issue on the [FreeRTOS-Kernel-Partner-Supported-Ports](https://github.com/FreeRTOS/FreeRTOS-Kernel-Partner-Supported-Ports/issues) repository
