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
LDFLAGS := $(CPU_FLAGS) -nostdlib -T linker/stm32f446re.ld \
	-Wl,--gc-sections -Wl,--build-id=none -Wl,--fatal-warnings \
	-Wl,-z,noexecstack \
	-Wl,-Map=$(BUILD_DIR)/$(PROJECT).map -Wl,--print-memory-usage

C_SOURCES := \
	app/main.c \
	arch/cortex_m4/exception.c \
	arch/cortex_m4/port.c \
	bsp/stm32f446/clock.c \
	bsp/stm32f446/gpio.c \
	kernel/task.c
ASM_SOURCES := startup/startup_stm32f446re.S
OBJECTS := $(addprefix $(BUILD_DIR)/,$(C_SOURCES:.c=.o)) \
	$(addprefix $(BUILD_DIR)/,$(ASM_SOURCES:.S=.o))
DEPS := $(OBJECTS:.o=.d)
HOST_TEST := $(BUILD_DIR)/host/task_test
HOST_SANITIZED_TEST := $(BUILD_DIR)/host/task_test_sanitized
HOST_TEST_SOURCES := tests/task_test.c tests/host/platform_stub.c \
	kernel/task.c arch/cortex_m4/port.c
HOST_CFLAGS := -Iinclude -std=c11 -O0 -g3 -fno-builtin \
	-Wall -Wextra -Werror -Wshadow -Wconversion -Wdouble-promotion \
	-Wformat=2 -Wundef -Wstrict-prototypes -Wmissing-prototypes

.PHONY: all clean test test-sanitize verify flash openocd gdb

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

$(HOST_TEST): $(HOST_TEST_SOURCES) include/aerortos/task.h \
	include/aerortos/port.h include/aerortos/tcb_offsets.h \
	include/aerortos/platform.h
	@mkdir -p $(@D)
	$(HOST_CC) $(HOST_CFLAGS) $(HOST_TEST_SOURCES) -o $@

test: $(HOST_TEST)
	$(HOST_TEST)

$(HOST_SANITIZED_TEST): $(HOST_TEST_SOURCES) include/aerortos/task.h \
	include/aerortos/port.h include/aerortos/tcb_offsets.h \
	include/aerortos/platform.h
	@mkdir -p $(@D)
	$(HOST_CC) $(HOST_CFLAGS) -O1 -fno-omit-frame-pointer -fno-pie -no-pie \
		-fsanitize=address,undefined $(HOST_TEST_SOURCES) -o $@

test-sanitize: $(HOST_SANITIZED_TEST)
	ASAN_OPTIONS=detect_leaks=0 $(HOST_SANITIZED_TEST)

verify: all test
	$(READELF) -h -S -l -A $(BUILD_DIR)/$(PROJECT).elf
	$(OBJDUMP) -h -s -j .isr_vector $(BUILD_DIR)/$(PROJECT).elf
	$(OBJDUMP) -d --disassemble=Reset_Handler $(BUILD_DIR)/$(PROJECT).elf
	$(NM) -n $(BUILD_DIR)/$(PROJECT).elf | grep -E '(_vector_table|Reset_Handler|main|_sidata|_sdata|_edata|_sbss|_ebss|_estack)$$'
	$(NM) -n $(BUILD_DIR)/$(PROJECT).elf | grep -E '(aerortos_m2_smoke_stack|aerortos_m2_smoke_task|aerortos_port_build_initial_frame|aerortos_task_create)$$'
	test -z "$$($(NM) -u $(BUILD_DIR)/$(PROJECT).elf)"
	! $(NM) -u $(BUILD_DIR)/kernel/task.o | grep -E '(malloc|calloc|realloc|free|memcpy|memset)'

flash: all
	$(OPENOCD) -f interface/stlink.cfg -f target/stm32f4x.cfg \
		-c "program $(BUILD_DIR)/$(PROJECT).elf verify reset exit"

openocd:
	$(OPENOCD) -f interface/stlink.cfg -f target/stm32f4x.cfg

gdb: all
	$(GDB) $(BUILD_DIR)/$(PROJECT).elf \
		-ex "target extended-remote :3333" \
		-ex "monitor reset halt" \
		-ex "break Reset_Handler"

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
