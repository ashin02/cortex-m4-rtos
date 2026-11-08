PROJECT := aerortos
BUILD_DIR := build

CROSS_COMPILE ?= arm-none-eabi-
CC := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
READELF := $(CROSS_COMPILE)readelf
NM := $(CROSS_COMPILE)nm
SIZE := $(CROSS_COMPILE)size
GDB ?= $(CROSS_COMPILE)gdb
OPENOCD ?= openocd
HOST_CC ?= cc

CPU_FLAGS := -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
CPPFLAGS := -Iinclude -DSTM32F446xx
CFLAGS := $(CPU_FLAGS) -std=c11 -Og -g3 -ffreestanding -fno-builtin \
	-ffunction-sections -fdata-sections -fno-common -fstack-usage \
	-Wall -Wextra -Werror -Wshadow -Wconversion -Wdouble-promotion \
	-Wformat=2 -Wundef -Wstrict-prototypes -Wmissing-prototypes
ASFLAGS := $(CPU_FLAGS) -x assembler-with-cpp -g3
LDFLAGS_BASE := $(CPU_FLAGS) -nostdlib -T linker/stm32f446re.ld \
	-Wl,--gc-sections -Wl,--build-id=none -Wl,--fatal-warnings \
	-Wl,-z,noexecstack
LDFLAGS := $(LDFLAGS_BASE) \
	-Wl,-Map=$(BUILD_DIR)/$(PROJECT).map -Wl,--print-memory-usage

C_SOURCES := \
	app/main.c \
	arch/cortex_m4/exception.c \
	arch/cortex_m4/critical_section.c \
	arch/cortex_m4/port.c \
	bsp/stm32f446/clock.c \
	bsp/stm32f446/gpio.c \
	bsp/stm32f446/timer.c \
	kernel/task.c \
	kernel/scheduler.c \
	kernel/queue.c \
	kernel/mutex.c \
	kernel/timer.c \
	kernel/deadline.c
ASM_SOURCES := startup/startup_stm32f446re.S arch/cortex_m4/context_switch.S
OBJECTS := $(addprefix $(BUILD_DIR)/,$(C_SOURCES:.c=.o)) \
	$(addprefix $(BUILD_DIR)/,$(ASM_SOURCES:.S=.o))
DEPS := $(OBJECTS:.o=.d)
M5_HW_C_SOURCES := tests/target/m5_context_switch_test.c \
	arch/cortex_m4/exception.c arch/cortex_m4/critical_section.c \
	arch/cortex_m4/port.c kernel/task.c kernel/scheduler.c kernel/timer.c \
	kernel/deadline.c
M5_HW_ASM_SOURCES := startup/startup_stm32f446re.S \
	arch/cortex_m4/context_switch.S tests/target/m5_register_probe.S
M5_HW_OBJECTS := $(addprefix $(BUILD_DIR)/m5-hw/,$(M5_HW_C_SOURCES:.c=.o)) \
	$(addprefix $(BUILD_DIR)/m5-hw/,$(M5_HW_ASM_SOURCES:.S=.o))
M5_HW_DEPS := $(M5_HW_OBJECTS:.o=.d)
M6_HW_C_SOURCES := tests/target/m6_periodic_test.c \
	arch/cortex_m4/exception.c arch/cortex_m4/critical_section.c \
	arch/cortex_m4/port.c bsp/stm32f446/clock.c bsp/stm32f446/timer.c \
	kernel/task.c kernel/scheduler.c kernel/timer.c kernel/deadline.c
M6_HW_ASM_SOURCES := startup/startup_stm32f446re.S \
	arch/cortex_m4/context_switch.S tests/target/m5_register_probe.S
M6_HW_OBJECTS := $(addprefix $(BUILD_DIR)/m6-hw/,$(M6_HW_C_SOURCES:.c=.o)) \
	$(addprefix $(BUILD_DIR)/m6-hw/,$(M6_HW_ASM_SOURCES:.S=.o))
M6_HW_DEPS := $(M6_HW_OBJECTS:.o=.d)
M7_HW_C_SOURCES := tests/target/m7_queue_test.c \
	arch/cortex_m4/exception.c arch/cortex_m4/critical_section.c \
	arch/cortex_m4/port.c bsp/stm32f446/clock.c kernel/task.c \
	kernel/scheduler.c kernel/queue.c kernel/timer.c kernel/deadline.c
M7_HW_ASM_SOURCES := startup/startup_stm32f446re.S \
	arch/cortex_m4/context_switch.S
M7_HW_OBJECTS := $(addprefix $(BUILD_DIR)/m7-hw/,$(M7_HW_C_SOURCES:.c=.o)) \
	$(addprefix $(BUILD_DIR)/m7-hw/,$(M7_HW_ASM_SOURCES:.S=.o))
M7_HW_DEPS := $(M7_HW_OBJECTS:.o=.d)
M8_HW_C_SOURCES := tests/target/m8_mutex_test.c \
	arch/cortex_m4/exception.c arch/cortex_m4/critical_section.c \
	arch/cortex_m4/port.c bsp/stm32f446/clock.c kernel/task.c \
	kernel/scheduler.c kernel/queue.c kernel/mutex.c kernel/timer.c \
	kernel/deadline.c
M8_HW_ASM_SOURCES := startup/startup_stm32f446re.S \
	arch/cortex_m4/context_switch.S
M8_HW_OBJECTS := $(addprefix $(BUILD_DIR)/m8-hw/,$(M8_HW_C_SOURCES:.c=.o)) \
	$(addprefix $(BUILD_DIR)/m8-hw/,$(M8_HW_ASM_SOURCES:.S=.o))
M8_HW_DEPS := $(M8_HW_OBJECTS:.o=.d)
M9_HW_C_SOURCES := tests/target/m9_integration_test.c \
	arch/cortex_m4/exception.c arch/cortex_m4/critical_section.c \
	arch/cortex_m4/port.c bsp/stm32f446/clock.c bsp/stm32f446/timer.c \
	kernel/task.c kernel/scheduler.c kernel/queue.c kernel/mutex.c \
	kernel/timer.c kernel/deadline.c
M9_HW_ASM_SOURCES := startup/startup_stm32f446re.S \
	arch/cortex_m4/context_switch.S
M9_HW_OBJECTS := $(addprefix $(BUILD_DIR)/m9-hw/,$(M9_HW_C_SOURCES:.c=.o)) \
	$(addprefix $(BUILD_DIR)/m9-hw/,$(M9_HW_ASM_SOURCES:.S=.o))
M9_HW_DEPS := $(M9_HW_OBJECTS:.o=.d)
M10_HW_C_SOURCES := tests/target/m10_timing_test.c \
	arch/cortex_m4/exception.c arch/cortex_m4/critical_section.c \
	arch/cortex_m4/cycle_counter.c arch/cortex_m4/port.c \
	bsp/stm32f446/clock.c bsp/stm32f446/timer.c kernel/task.c \
	kernel/scheduler.c kernel/queue.c kernel/mutex.c kernel/timer.c \
	kernel/deadline.c kernel/monitor.c
M10_HW_ASM_SOURCES := startup/startup_stm32f446re.S \
	arch/cortex_m4/context_switch.S
M10_HW_OBJECTS := $(addprefix $(BUILD_DIR)/m10-hw/,$(M10_HW_C_SOURCES:.c=.o)) \
	$(addprefix $(BUILD_DIR)/m10-hw/,$(M10_HW_ASM_SOURCES:.S=.o))
M10_HW_DEPS := $(M10_HW_OBJECTS:.o=.d)
HOST_TESTS := $(BUILD_DIR)/host/task_test $(BUILD_DIR)/host/scheduler_test \
	$(BUILD_DIR)/host/context_switch_test $(BUILD_DIR)/host/deadline_test \
	$(BUILD_DIR)/host/queue_test $(BUILD_DIR)/host/mutex_test \
	$(BUILD_DIR)/host/integration_test $(BUILD_DIR)/host/timing_test
HOST_SANITIZED_TESTS := $(addsuffix _sanitized,$(HOST_TESTS))
HOST_COMMON_SOURCES := tests/host/platform_stub.c kernel/task.c \
	kernel/scheduler.c kernel/queue.c kernel/mutex.c kernel/timer.c kernel/deadline.c \
	kernel/monitor.c arch/cortex_m4/port.c arch/cortex_m4/critical_section.c \
	arch/cortex_m4/cycle_counter.c
HOST_CFLAGS := -Iinclude -DAERORTOS_HOST_TEST -DAERORTOS_ENABLE_TIMING \
	-std=c11 -O0 -g3 -fno-builtin \
	-Wall -Wextra -Werror -Wshadow -Wconversion -Wdouble-promotion \
	-Wformat=2 -Wundef -Wstrict-prototypes -Wmissing-prototypes

.PHONY: all clean test test-sanitize verify verify-critical-assembly \
	m5-hardware-test m6-hardware-test m7-hardware-test m8-hardware-test \
	m9-hardware-test flash flash-m5-test flash-m6-test flash-m7-test \
	flash-m8-test flash-m9-test openocd gdb gdb-m5-test gdb-m6-test \
	gdb-m7-test gdb-m8-test gdb-m9-test m10-hardware-test flash-m10-test \
	gdb-m10-test

all: $(BUILD_DIR)/$(PROJECT).elf $(BUILD_DIR)/$(PROJECT).bin

$(BUILD_DIR)/$(PROJECT).elf: $(OBJECTS) linker/stm32f446re.ld
	@mkdir -p $(@D)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BUILD_DIR)/$(PROJECT).bin: $(BUILD_DIR)/$(PROJECT).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(ASFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/m5-hw/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/m5-hw/%.o: %.S
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(ASFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/m6-hw/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/m6-hw/%.o: %.S
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(ASFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/m7-hw/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/m7-hw/%.o: %.S
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(ASFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/m8-hw/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/m8-hw/%.o: %.S
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(ASFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/m9-hw/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/m9-hw/%.o: %.S
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(ASFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/m10-hw/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -DAERORTOS_ENABLE_TIMING $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/m10-hw/%.o: %.S
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -DAERORTOS_ENABLE_TIMING $(ASFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/m5_hw_test.elf: $(M5_HW_OBJECTS) linker/stm32f446re.ld
	$(CC) $(M5_HW_OBJECTS) $(LDFLAGS_BASE) \
		-Wl,-Map=$(BUILD_DIR)/m5_hw_test.map -Wl,--print-memory-usage -o $@
	$(SIZE) $@

$(BUILD_DIR)/m5_hw_test.bin: $(BUILD_DIR)/m5_hw_test.elf
	$(OBJCOPY) -O binary $< $@

m5-hardware-test: $(BUILD_DIR)/m5_hw_test.elf $(BUILD_DIR)/m5_hw_test.bin
	$(NM) -n $(BUILD_DIR)/m5_hw_test.elf | grep -E \
		'(SVC_Handler|PendSV_Handler|SysTick_Handler|aerortos_m5_hw_(pass_count|fail_count|complete|trace))$$'
	test "$$($(NM) -n $(BUILD_DIR)/m5_hw_test.elf | awk '$$3 == "SysTick_Handler" {print $$1}')" != \
		"$$($(NM) -n $(BUILD_DIR)/m5_hw_test.elf | awk '$$3 == "Default_Handler" {print $$1}')"
	$(OBJDUMP) -d --disassemble=SVC_Handler $(BUILD_DIR)/m5_hw_test.elf
	$(OBJDUMP) -d --disassemble=PendSV_Handler $(BUILD_DIR)/m5_hw_test.elf

$(BUILD_DIR)/m6_hw_test.elf: $(M6_HW_OBJECTS) linker/stm32f446re.ld
	$(CC) $(M6_HW_OBJECTS) $(LDFLAGS_BASE) \
		-Wl,-Map=$(BUILD_DIR)/m6_hw_test.map -Wl,--print-memory-usage -o $@
	$(SIZE) $@

$(BUILD_DIR)/m6_hw_test.bin: $(BUILD_DIR)/m6_hw_test.elf
	$(OBJCOPY) -O binary $< $@

m6-hardware-test: $(BUILD_DIR)/m6_hw_test.elf $(BUILD_DIR)/m6_hw_test.bin
	$(NM) -n $(BUILD_DIR)/m6_hw_test.elf | grep -E \
		'(SVC_Handler|PendSV_Handler|SysTick_Handler|aerortos_m6_hw_(pass_count|fail_count|complete|trace))$$'
	test "$$($(NM) -n $(BUILD_DIR)/m6_hw_test.elf | awk '$$3 == "SysTick_Handler" {print $$1}')" != \
		"$$($(NM) -n $(BUILD_DIR)/m6_hw_test.elf | awk '$$3 == "Default_Handler" {print $$1}')"
	test -z "$$($(NM) -u $(BUILD_DIR)/m6_hw_test.elf)"
	$(OBJDUMP) -d --disassemble=SysTick_Handler $(BUILD_DIR)/m6_hw_test.elf

$(BUILD_DIR)/m7_hw_test.elf: $(M7_HW_OBJECTS) linker/stm32f446re.ld
	$(CC) $(M7_HW_OBJECTS) $(LDFLAGS_BASE) \
		-Wl,-Map=$(BUILD_DIR)/m7_hw_test.map -Wl,--print-memory-usage -o $@
	$(SIZE) $@

$(BUILD_DIR)/m7_hw_test.bin: $(BUILD_DIR)/m7_hw_test.elf
	$(OBJCOPY) -O binary $< $@

m7-hardware-test: $(BUILD_DIR)/m7_hw_test.elf $(BUILD_DIR)/m7_hw_test.bin
	$(NM) -n $(BUILD_DIR)/m7_hw_test.elf | grep -E \
		'(SVC_Handler|PendSV_Handler|SysTick_Handler|aerortos_m7_hw_(pass_count|fail_count|complete|trace))$$'
	test "$$($(NM) -n $(BUILD_DIR)/m7_hw_test.elf | awk '$$3 == "SysTick_Handler" {print $$1}')" != \
		"$$($(NM) -n $(BUILD_DIR)/m7_hw_test.elf | awk '$$3 == "Default_Handler" {print $$1}')"
	test -z "$$($(NM) -u $(BUILD_DIR)/m7_hw_test.elf)"
	! $(NM) -u $(BUILD_DIR)/m7-hw/kernel/queue.o | grep -E \
		'(malloc|calloc|realloc|free|memcpy|memset)'
	$(OBJDUMP) -d --disassemble=aerortos_queue_send \
		$(BUILD_DIR)/m7_hw_test.elf

$(BUILD_DIR)/m8_hw_test.elf: $(M8_HW_OBJECTS) linker/stm32f446re.ld
	$(CC) $(M8_HW_OBJECTS) $(LDFLAGS_BASE) \
		-Wl,-Map=$(BUILD_DIR)/m8_hw_test.map -Wl,--print-memory-usage -o $@
	$(SIZE) $@

$(BUILD_DIR)/m8_hw_test.bin: $(BUILD_DIR)/m8_hw_test.elf
	$(OBJCOPY) -O binary $< $@

m8-hardware-test: $(BUILD_DIR)/m8_hw_test.elf $(BUILD_DIR)/m8_hw_test.bin
	$(NM) -n $(BUILD_DIR)/m8_hw_test.elf | grep -E \
		'(SVC_Handler|PendSV_Handler|SysTick_Handler|aerortos_m8_hw_(pass_count|fail_count|complete|trace))$$'
	test "$$($(NM) -n $(BUILD_DIR)/m8_hw_test.elf | awk '$$3 == "SysTick_Handler" {print $$1}')" != \
		"$$($(NM) -n $(BUILD_DIR)/m8_hw_test.elf | awk '$$3 == "Default_Handler" {print $$1}')"
	test -z "$$($(NM) -u $(BUILD_DIR)/m8_hw_test.elf)"
	! $(NM) -u $(BUILD_DIR)/m8-hw/kernel/mutex.o | grep -E \
		'(malloc|calloc|realloc|free|memcpy|memset)'
	$(OBJDUMP) -d --disassemble=aerortos_mutex_lock \
		$(BUILD_DIR)/m8_hw_test.elf

$(BUILD_DIR)/m9_hw_test.elf: $(M9_HW_OBJECTS) linker/stm32f446re.ld
	$(CC) $(M9_HW_OBJECTS) $(LDFLAGS_BASE) \
		-Wl,-Map=$(BUILD_DIR)/m9_hw_test.map -Wl,--print-memory-usage -o $@
	$(SIZE) $@

$(BUILD_DIR)/m9_hw_test.bin: $(BUILD_DIR)/m9_hw_test.elf
	$(OBJCOPY) -O binary $< $@

m9-hardware-test: $(BUILD_DIR)/m9_hw_test.elf $(BUILD_DIR)/m9_hw_test.bin
	$(NM) -n $(BUILD_DIR)/m9_hw_test.elf | grep -E \
		'(SVC_Handler|PendSV_Handler|SysTick_Handler|aerortos_m9_hw_(pass_count|fail_count|complete|trace))$$'
	test "$$($(NM) -n $(BUILD_DIR)/m9_hw_test.elf | awk '$$3 == "SysTick_Handler" {print $$1}')" != \
		"$$($(NM) -n $(BUILD_DIR)/m9_hw_test.elf | awk '$$3 == "Default_Handler" {print $$1}')"
	test -z "$$($(NM) -u $(BUILD_DIR)/m9_hw_test.elf)"
	! $(NM) -u $(BUILD_DIR)/m9-hw/kernel/queue.o | grep -E \
		'(malloc|calloc|realloc|free|memcpy|memset)'
	! $(NM) -u $(BUILD_DIR)/m9-hw/kernel/mutex.o | grep -E \
		'(malloc|calloc|realloc|free|memcpy|memset)'
	$(OBJDUMP) -d --disassemble=SysTick_Handler \
		$(BUILD_DIR)/m9_hw_test.elf

$(BUILD_DIR)/m10_hw_test.elf: $(M10_HW_OBJECTS) linker/stm32f446re.ld
	$(CC) $(M10_HW_OBJECTS) $(LDFLAGS_BASE) \
		-Wl,-Map=$(BUILD_DIR)/m10_hw_test.map -Wl,--print-memory-usage -o $@
	$(SIZE) $@

$(BUILD_DIR)/m10_hw_test.bin: $(BUILD_DIR)/m10_hw_test.elf
	$(OBJCOPY) -O binary $< $@

m10-hardware-test: $(BUILD_DIR)/m10_hw_test.elf $(BUILD_DIR)/m10_hw_test.bin
	$(NM) -n $(BUILD_DIR)/m10_hw_test.elf | grep -E \
		'(SVC_Handler|PendSV_Handler|SysTick_Handler|aerortos_m10_hw_(pass_count|fail_count|complete)|aerortos_monitor_data)$$'
	test "$$($(NM) -n $(BUILD_DIR)/m10_hw_test.elf | awk '$$3 == "SysTick_Handler" {print $$1}')" != \
		"$$($(NM) -n $(BUILD_DIR)/m10_hw_test.elf | awk '$$3 == "Default_Handler" {print $$1}')"
	test -z "$$($(NM) -u $(BUILD_DIR)/m10_hw_test.elf)"
	! $(NM) -u $(BUILD_DIR)/m10-hw/kernel/monitor.o | grep -E \
		'(malloc|calloc|realloc|free|printf)'
	$(OBJDUMP) -d --disassemble=PendSV_Handler \
		$(BUILD_DIR)/m10_hw_test.elf

$(BUILD_DIR)/host/%_test: tests/%_test.c $(HOST_COMMON_SOURCES) include/aerortos/task.h \
	include/aerortos/port.h include/aerortos/tcb_offsets.h \
	include/aerortos/platform.h include/aerortos/scheduler.h \
	include/aerortos/critical_section.h include/aerortos/timer.h \
	include/aerortos/deadline.h include/aerortos/queue.h \
	include/aerortos/mutex.h include/aerortos/cycle_counter.h \
	include/aerortos/monitor.h
	@mkdir -p $(@D)
	$(HOST_CC) $(HOST_CFLAGS) $< $(HOST_COMMON_SOURCES) -o $@

test: $(HOST_TESTS)
	@set -e; for test_binary in $(HOST_TESTS); do $$test_binary; done

$(BUILD_DIR)/host/%_test_sanitized: tests/%_test.c $(HOST_COMMON_SOURCES) include/aerortos/task.h \
	include/aerortos/port.h include/aerortos/tcb_offsets.h \
	include/aerortos/platform.h include/aerortos/scheduler.h \
	include/aerortos/critical_section.h include/aerortos/timer.h \
	include/aerortos/deadline.h include/aerortos/queue.h \
	include/aerortos/mutex.h include/aerortos/cycle_counter.h \
	include/aerortos/monitor.h
	@mkdir -p $(@D)
	$(HOST_CC) $(HOST_CFLAGS) -O1 -fno-omit-frame-pointer -fno-pie -no-pie \
		-fsanitize=address,undefined $< $(HOST_COMMON_SOURCES) -o $@

test-sanitize: $(HOST_SANITIZED_TESTS)
	@set -e; for test_binary in $(HOST_SANITIZED_TESTS); do \
		ASAN_OPTIONS=detect_leaks=0 $$test_binary; done

$(BUILD_DIR)/critical_section.dis: $(BUILD_DIR)/arch/cortex_m4/critical_section.o \
	tests/verify_critical_assembly.sh
	$(OBJDUMP) -d $< > $@

verify-critical-assembly: $(BUILD_DIR)/critical_section.dis
	sh tests/verify_critical_assembly.sh $< 64

verify: all test verify-critical-assembly
	$(READELF) -h -S -l -A $(BUILD_DIR)/$(PROJECT).elf
	$(OBJDUMP) -h -s -j .isr_vector $(BUILD_DIR)/$(PROJECT).elf
	$(OBJDUMP) -d --disassemble=Reset_Handler $(BUILD_DIR)/$(PROJECT).elf
	$(NM) -n $(BUILD_DIR)/$(PROJECT).elf | grep -E '(_vector_table|Reset_Handler|main|_sidata|_sdata|_edata|_sbss|_ebss|_estack)$$'
	$(NM) -n $(BUILD_DIR)/$(PROJECT).elf | grep -E '(SVC_Handler|PendSV_Handler|SysTick_Handler|aerortos_current_task|aerortos_scheduler_start|aerortos_port_build_initial_frame|aerortos_task_create)$$'
	test "$$($(NM) -n $(BUILD_DIR)/$(PROJECT).elf | awk '$$3 == "SysTick_Handler" {print $$1}')" != \
		"$$($(NM) -n $(BUILD_DIR)/$(PROJECT).elf | awk '$$3 == "Default_Handler" {print $$1}')"
	test -z "$$($(NM) -u $(BUILD_DIR)/$(PROJECT).elf)"
	! $(NM) -u $(BUILD_DIR)/kernel/task.o | grep -E '(malloc|calloc|realloc|free|memcpy|memset)'

flash: all
	$(OPENOCD) -f interface/stlink.cfg -f target/stm32f4x.cfg \
		-c "program $(BUILD_DIR)/$(PROJECT).elf verify reset exit"

flash-m5-test: m5-hardware-test
	$(OPENOCD) -f interface/stlink.cfg -f target/stm32f4x.cfg \
		-c "program $(BUILD_DIR)/m5_hw_test.elf verify reset exit"

flash-m6-test: m6-hardware-test
	$(OPENOCD) -f interface/stlink.cfg -f target/stm32f4x.cfg \
		-c "program $(BUILD_DIR)/m6_hw_test.elf verify reset exit"

flash-m7-test: m7-hardware-test
	$(OPENOCD) -f interface/stlink.cfg -f target/stm32f4x.cfg \
		-c "program $(BUILD_DIR)/m7_hw_test.elf verify reset exit"

flash-m8-test: m8-hardware-test
	$(OPENOCD) -f interface/stlink.cfg -f target/stm32f4x.cfg \
		-c "program $(BUILD_DIR)/m8_hw_test.elf verify reset exit"

flash-m9-test: m9-hardware-test
	$(OPENOCD) -f interface/stlink.cfg -f target/stm32f4x.cfg \
		-c "program $(BUILD_DIR)/m9_hw_test.elf verify reset exit"

flash-m10-test: m10-hardware-test
	$(OPENOCD) -f interface/stlink.cfg -f target/stm32f4x.cfg \
		-c "program $(BUILD_DIR)/m10_hw_test.elf verify reset exit"

openocd:
	$(OPENOCD) -f interface/stlink.cfg -f target/stm32f4x.cfg

gdb: all
	$(GDB) $(BUILD_DIR)/$(PROJECT).elf \
		-ex "target extended-remote :3333" \
		-ex "monitor reset halt" \
		-ex "break Reset_Handler"

gdb-m5-test: m5-hardware-test
	$(GDB) $(BUILD_DIR)/m5_hw_test.elf \
		-ex "target extended-remote :3333" \
		-ex "monitor reset halt" \
		-ex "break aerortos_m5_hw_test_complete_marker"

gdb-m6-test: m6-hardware-test
	$(GDB) $(BUILD_DIR)/m6_hw_test.elf \
		-ex "target extended-remote :3333" \
		-ex "monitor reset halt" \
		-ex "break aerortos_m6_hw_test_complete_marker"

gdb-m7-test: m7-hardware-test
	$(GDB) $(BUILD_DIR)/m7_hw_test.elf \
		-ex "target extended-remote :3333" \
		-ex "monitor reset halt" \
		-ex "break aerortos_m7_hw_test_complete_marker"

gdb-m8-test: m8-hardware-test
	$(GDB) $(BUILD_DIR)/m8_hw_test.elf \
		-ex "target extended-remote :3333" \
		-ex "monitor reset halt" \
		-ex "break aerortos_m8_hw_test_complete_marker"

gdb-m9-test: m9-hardware-test
	$(GDB) $(BUILD_DIR)/m9_hw_test.elf \
		-ex "target extended-remote :3333" \
		-ex "monitor reset halt" \
		-ex "break aerortos_m9_hw_test_complete_marker"

gdb-m10-test: m10-hardware-test
	$(GDB) $(BUILD_DIR)/m10_hw_test.elf \
		-ex "target extended-remote :3333" \
		-ex "monitor reset halt" \
		-ex "break aerortos_m10_hw_test_complete_marker"

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS) $(M5_HW_DEPS) $(M6_HW_DEPS) $(M7_HW_DEPS) $(M8_HW_DEPS) \
	$(M9_HW_DEPS) $(M10_HW_DEPS)
