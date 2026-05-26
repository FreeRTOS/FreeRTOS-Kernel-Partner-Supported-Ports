# Overview

This directory contains the FreeRTOS port for the Infineon AURIX™ TC4xx family of MCUs equipped with the TriCore™ core.

The port is placed under the `GCC/` folder because the compiler used — **tricore-gcc**, bundled with [AURIX™ Development Studio Limited (ADS-L)](https://softwaretools.infineon.com/tools/com.ifx.tb.tool.aurixide) — is a GCC-based toolchain and exposes the standard `__GNUC__` preprocessor macro.

## Tool Dependencies

- [AURIX™ Development Studio Limited (ADS-L)](https://softwaretools.infineon.com/tools/com.ifx.tb.tool.aurixide) — includes **tricore-gcc** and an integrated debugger. To request access, send an email to `ads@infineon.com` or install via the [Infineon Developer Center Launcher](https://www.infineon.com/cms/en/design-support/tools/utilities/infineon-developer-center-idc-launcher/)

## System Call Trap Integration

`portYIELD()` uses the TriCore `syscall` instruction with `configYIELD_SYSCALL_ID` (default `0`). The syscall enters the TriCore system-call trap class (class 6 / SYS); the trap identification number (TIN) is passed to `vPortSyscallHandler()` and is matched against `configYIELD_SYSCALL_ID`.

The TC4xx GCC port does not emit its own trap-vector entry for `vPortSyscallHandler()`. In an ADS-L/iLLD project, the startup code sets the Base Trap Vector register (BTV) from the linker-provided `__TRAPTAB_CPUx` symbols, for example `Libraries/Infra/Ssw/TC4xx/Tricore/Ifx_Ssw_Tc0.c` writes `CPU_BTV` from `__TRAPTAB(0)`. The linker script places the iLLD trap table sections, for example `.traptab_cpu0`, at those addresses. The system-call trap table entry in `Libraries/iLLD/TC4xx/Tricore/Cpu/Trap/IfxCpu_Trap.c` dispatches class 6 / SYS traps through the `IFX_CFG_CPU_TRAP_SYSCALL_CPUx_HOOK()` macros.

For a CPU0-only FreeRTOS integration using the ADS-L/iLLD trap handler, enable the trap hook extension in `Configurations/Ifx_Cfg.h`:

```c
#define IFX_CFG_EXTEND_TRAP_HOOKS
```

The `Configurations` files used by the companion demo can be found under `/FreeRTOS-Partner-Supported-Demos/AURIX_TC4D7_ADS/Configurations`.

Then provide the CPU0 syscall hook in `Configurations/Ifx_Cfg_Trap.h`:

```c
extern int vPortSyscallHandler( unsigned char id );
#define IFX_CFG_CPU_TRAP_SYSCALL_CPU0_HOOK( t )    vPortSyscallHandler( t.tId )
```

## Support

- For support queries, please open an issue on the [FreeRTOS-Kernel-Partner-Supported-Ports](https://github.com/FreeRTOS/FreeRTOS-Kernel-Partner-Supported-Ports/issues) repository
