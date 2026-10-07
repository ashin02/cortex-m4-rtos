# AeroRT

AeroRT is a custom deterministic, preemptive real-time operating system for a flight
controller. It favors bounded worst-case behavior, static resource allocation, and
explicit timing and fault behavior over general-purpose features or average-case
performance. It is not a FreeRTOS or Zephyr derivative.

## Target hardware

- Board: ST NUCLEO-F446RE
- MCU: STM32F446RET6
- Core: ARM Cortex-M4F

The intended implementation is primarily C, with ARM assembly where required by the
architecture.

## Current status

**Scaffolded / architecture phase.** Source files currently contain design contracts
and TODOs; no RTOS, driver, flight-control, benchmark, or hardware-validation claims
are made yet.

## Documentation

- [Documentation index](docs/INDEX.md) — entry point, document authority, and reading order
- [Project specification](docs/PROJECT_SPEC.md) — canonical current architecture and requirements
- [Development roadmap](docs/ROADMAP.md) — implementation phases and exit criteria
- [Testing strategy](docs/TESTING.md) — planned verification and evidence standards
- [Architecture decisions](docs/DECISIONS.md) — accepted decisions and their rationale
