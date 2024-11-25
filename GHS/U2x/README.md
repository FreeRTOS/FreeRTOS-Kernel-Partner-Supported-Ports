# RH850/U2x FreeRTOS Port with GHS Compiler

## Introduction

This repository contains the port of FreeRTOS for Renesas RH850/U2x microcontrollers using the GHS compiler. The following sections provide instructions on how to use this port, a link to the test project, and other relevant information.

## Prerequisites
- Compiler: GHS
- FreeRTOS version 11.1.0

| Device   | FPU | FXU | SMP |
|----------|-----|-----|-----|
| U2A6     | Yes | No  | Yes |
| U2A8     | Yes | No  | Yes |
| U2A16    | Yes | No  | Yes |
| U2B6     | Yes | Yes | Yes |
| U2B10    | Yes | Yes | Yes |

## Link to Test Project

The test project can be found in [RH850_U2Ax_GHS](https://github.com/renesas/FreeRTOS-Partner-Supported-Demos/tree/u2x_port/) and [RH850_U2Bx_GHS](https://github.com/renesas/FreeRTOS-Partner-Supported-Demos/tree/u2x_port/). This project contains example tasks and configurations to help you get started with FreeRTOS on the RH850/U2Ax and U2Bx.

## Note
   1. The minimal stack size `configMINIMAL_STACK_SIZE` must be included the reserved memory for nested interrupt. This formula can be referred: `(task_context_size) * (2 + configMAX_INT_NESTING) + Stack_depth_of_taskcode`
   In which, `task_context_size` is calculated as `36*4bytes = 144bytes` (when FPU enabled) or `34*4bytes = 136` (when FPU disabled), configMAX_INT_NESTING is `02` as default (Note that a value of `0` is not allowed).
   2. Users need to create a memory section named `mev_address` in `CRAM` for Exclusive Control functionality. Users should initialize the `mev_address` section in the startup file.

Example:
   ```
  -- .mev_address section in CRAM is used for Sync flags
  mov     ___ghsbegin_mev_address, r20
  st.w    r0, 0[r20]
   ```
   3. The `FXU unit` is only available on `core 0`. Users must ensure that FXU operations are restricted to `core 0` by using the `vTaskCoreAffinitySet` function provided by FreeRTOS SMP.
   4. Set the macro `configENABLE_FXU` to `1` to enable the `FXU unit`; otherwise set `0` to disable `FXU unit`.
   5. This port supports both U2Ax and U2Bx devices. The user must configure `configDEVICE_NAME` with the value `U2Bx_DEVICES` or `U2Ax_DEVICES` to specify which device is being used.
   6. The User can configure the interrupt priority of the OSTM Timer using `configTIMER_INT_PRIORITY`, with 16 levels available (0 being the highest priority and 15 the lowest).
   7. This port also supports the configuration of contiguous CPU cores in FreeRTOS, allowing the user to set task affinity for execution on specific cores or subsets of cores.

## Other Relevant Information

- **Documentation:**
  - Refer to the official [FreeRTOS documentation](https://www.freertos.org/Documentation/RTOS_book.html) for detailed information on configuring and using FreeRTOS.
  - Consult the [RH850 U2A group user manual hardware manual](https://www.renesas.com/en/document/mah/rh850u2a-eva-group-users-manual-hardware-0?r=1546621) for specific details about the microcontroller.
  - Consult the [RH850 U2B group user manual hardware manual](https://www.renesas.com/en/document/mah/rh850u2b-flash-memory-users-manual-r01uh0927ej0100?srsltid=AfmBOooTd6sk-_EsdVV0L_Rw0vGMsaoCQ0SSzpmtoScRcxgcwVkLrQwY) for specific details about the microcontroller.
  - For more information about Renesas RH850 microcontrolers, please visit [this website](https://www.renesas.com/us/en/products/microcontrollers-microprocessors/rh850-automotive-mcus)

- **Support:**
  - If you encounter any issues or have questions about this port, please open an issue in this repository or contact the maintainer.

- **Contributing:**
  - Contributions to improve this port are welcome. Please fork the repository, make your changes, and submit a pull request.