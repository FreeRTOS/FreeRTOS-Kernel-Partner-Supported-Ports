# Overview

This directory contains the FreeRTOS port for the Infineon AURIX™ TC3xx family of MCUs equipped with the TriCore™ core.

The port is placed under the `GCC/` folder because the compiler used — **tricore-gcc**, bundled with [AURIX™ Development Studio (ADS)](https://www.infineon.com/design-resources/platforms/aurix-software-tools/aurix-tools/aurix-development-studio) — is a GCC-based toolchain and exposes the standard `__GNUC__` preprocessor macro.

## Tool Dependencies

- [AURIX™ Development Studio (ADS)](https://www.infineon.com/design-resources/platforms/aurix-software-tools/aurix-tools/aurix-development-studio) — includes **tricore-gcc** and an integrated debugger. Please install via the [Infineon Developer Center Launcher](https://www.infineon.com/cms/en/design-support/tools/utilities/infineon-developer-center-idc-launcher/)

## How To Use This Port

Add `port.c` and `portmacro.h` from this directory to the application's FreeRTOS portable layer, and add this directory to the compiler include path so the FreeRTOS kernel can include `portmacro.h`.

The application must provide a `FreeRTOSConfig.h` that defines the TCxxx-specific options listed below. It must also provide the ADS-L/iLLD trap hook configuration described in the System Call Trap Integration section so `portYIELD()` can reach `vPortSyscallHandler()`.

Supported demos for TC375 are available in `/FreeRTOS-Partner-Supported-Demos/AURIX_TC375_ADS`.

## Required FreeRTOSConfig.h Options

In addition to the standard FreeRTOS kernel configuration options such as `configCPU_CLOCK_HZ`, `configTICK_RATE_HZ`, `configMAX_PRIORITIES`, `configMINIMAL_STACK_SIZE`, and `configTOTAL_HEAP_SIZE`, an application that uses this port must define the TCxxx-specific options below in `FreeRTOSConfig.h`. The companion demo provides example values for TC375 CPU0.

| Macro | Purpose |
| --- | --- |
| `configSTM` | Base address of the per-CPU STM register block used for the tick timer. |
| `configSTM_SRC` | Address of the STM service request register used by the tick interrupt. |
| `configSTM_CLOCK_HZ` | STM input clock frequency used to calculate the tick compare interval. |
| `configCONTEXT_SRC` | Address of the service request register used for software-triggered context switches. |
| `configCPU_NR` | TriCore CPU number used in SRC TOS fields and in the generated `.intvec_tc<cpu>_<priority>` section names. |
| `configCONTEXT_INTERRUPT_PRIORITY` | Interrupt priority for the context-switch service request. |
| `configTIMER_INTERRUPT_PRIORITY` | Interrupt priority for the STM tick service request (must not be higher than `configCONTEXT_INTERRUPT_PRIORITY`). |
| `configMAX_API_CALL_INTERRUPT_PRIORITY` | CCPN mask threshold used by critical sections and FromISR API masking. |
| `configSYSCALL_CALL_DEPTH` | Call depth used when saving and restoring context from `vPortSyscallYield()`. |
| `configPROVIDE_SYSCALL_TRAP` | Set to `0` when the ADS-L/iLLD trap table dispatches the syscall trap (see below); set to `1` to let the port emit its own class 6 trap vector. |

The following TCxxx-specific options are optional or have port-provided defaults:

| Macro | Purpose |
| --- | --- |
| `configCPU_STM_DEBUG` | Optional debug assert for missed STM ticks; undefined or `0` disables the check. |
| `configTICK_STM_DEBUG` | Optional STM debug-control setup during tick timer initialization; undefined or `0` disables it. |
| `configYIELD_SYSCALL_ID` | Syscall ID used by `portYIELD()`; defaults to `0` when not defined. |

## System Call Trap Integration

`portYIELD()` uses the TriCore `syscall` instruction with `configYIELD_SYSCALL_ID` (default `0`). The syscall enters the TriCore system-call trap class (class 6 / SYS); the trap identification number (TIN) is passed to `vPortSyscallHandler()` and is matched against `configYIELD_SYSCALL_ID`.

The TC3xx GCC port does not emit its own trap-vector entry for `vPortSyscallHandler()`. In an ADS/iLLD project, the startup code sets the Base Trap Vector register (BTV) from the linker-provided `__TRAPTAB_CPUx` symbols, for example `Libraries/Infra/Ssw/TC3xx/Tricore/Ifx_Ssw_Tc0.c` writes `CPU_BTV` from `__TRAPTAB(0)`. The linker script (`Lcf_Gnuc_Tricore_Tc.lsl`) places the iLLD trap table sections, for example `.traptab_cpu0`, at those addresses. The system-call trap table entry in `Libraries/iLLD/TC3xx/Tricore/Cpu/Trap/IfxCpu_Trap.c` unconditionally invokes the `IFX_CFG_CPU_TRAP_SYSCALL_CPUx_HOOK()` macros; when the application does not override them they resolve to an empty default provided by the iLLD header.

For a CPU0-only FreeRTOS integration using the iLLD trap handler, declare `vPortSyscallHandler()` and route the CPU0 syscall hook to it in `Configurations/Ifx_Cfg.h`:

```c
extern int vPortSyscallHandler( unsigned char id );
#define IFX_CFG_CPU_TRAP_SYSCALL_CPU0_HOOK( t )    vPortSyscallHandler( t.tId )
```

No separate `Configurations/Ifx_Cfg_Trap.h` and no `IFX_CFG_EXTEND_TRAP_HOOKS` define are required on TC3xx — the syscall hook macro is picked up directly by `IfxCpu_Trap.c` regardless.

The `Configurations` files used by the companion demo can be found from [ Infineon AURIX Code Examples - Configurations](https://www.infineon.com/design-resources/platforms/aurix-software-tools/aurix-tools/aurix-development-studio).

## Test Coverage

This port has been verified against the qualification checklist in the [FreeRTOS Third-Party Template README](https://github.com/FreeRTOS/FreeRTOS/blob/main/FreeRTOS/Demo/ThirdParty/Template/README.md). All required tests execute continuously without reporting an error on TC375 CPU0.

## Support

- For support queries, please open an issue on the [FreeRTOS-Kernel-Partner-Supported-Ports](https://github.com/FreeRTOS/FreeRTOS-Kernel-Partner-Supported-Ports/issues) repository
