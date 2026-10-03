PREFIX ?= arm-none-eabi-
CC = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
SIZE = $(PREFIX)size
HOST_CC ?= cc
BUILD = build/clock
CPPFLAGS = -Iuser/inc -Ilib/inc -I.cmsis/include
CFLAGS = -mcpu=cortex-m0plus -mthumb -std=c11 -Os -g3 -ffreestanding \
         -ffunction-sections -fdata-sections -fno-common -fstack-usage \
         -Wall -Wextra -Werror
LDFLAGS = -mcpu=cortex-m0plus -mthumb -nostdlib \
          -Tlink/CW32L010_FLASH.ld -Wl,--gc-sections,-Map=$(BUILD)/clock.map
SOURCES = $(wildcard user/src/*.c) lib/src/system_cw32l010.c \
          lib/src/cw32l010_gpio.c lib/src/cw32l010_spi.c \
          lib/src/cw32l010_sysctrl.c
OBJECTS = $(patsubst %.c,$(BUILD)/%.o,$(SOURCES)) $(BUILD)/startup.o

.PHONY: all clean
all: $(BUILD)/clock.bin $(BUILD)/clock.hex
$(BUILD)/clock.elf: $(OBJECTS) link/CW32L010_FLASH.ld
	$(CC) $(LDFLAGS) $(OBJECTS) -lgcc -o $@
	$(SIZE) $@
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/lib/src/cw32l010_spi.o: CFLAGS += -Wno-error=type-limits -Wno-error=unused-parameter
$(BUILD)/startup.o: user/startup_cw32l010.s
	@mkdir -p $(dir $@)
	$(CC) -mcpu=cortex-m0plus -mthumb -c $< -o $@
$(BUILD)/clock.bin: $(BUILD)/clock.elf
	$(OBJCOPY) -O binary $< $@
$(BUILD)/clock.hex: $(BUILD)/clock.elf
	$(OBJCOPY) -O ihex $< $@
clean:
	rm -rf $(BUILD)
-include $(OBJECTS:.o=.d)

.PHONY: test
test: $(BUILD)/test_clock
	./$(BUILD)/test_clock
$(BUILD)/test_clock: tests/test_clock.c user/src/clock_core.c user/src/clock_render.c user/src/epd.c user/inc/epd_lut.h
	@mkdir -p $(BUILD)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all -Iuser/inc tests/test_clock.c user/src/clock_core.c user/src/clock_render.c user/src/epd.c -o $@
test: test-rtc
.PHONY: test-rtc
test-rtc: $(BUILD)/test_rtc
	./$(BUILD)/test_rtc
$(BUILD)/test_rtc: tests/test_rtc.c tests/fake/main.h user/src/rtc_clock.c user/src/clock_core.c user/src/clock_render.c
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all -Itests/fake -Iuser/inc tests/test_rtc.c user/src/rtc_clock.c user/src/clock_core.c user/src/clock_render.c -o $@

test: test-settings
.PHONY: test-settings
test-settings: $(BUILD)/test_settings
	./$(BUILD)/test_settings
$(BUILD)/test_settings: tests/test_settings.c user/src/settings.c user/src/clock_core.c user/src/clock_render.c
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all -Iuser/inc $^ -o $@
