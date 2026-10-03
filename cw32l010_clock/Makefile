.DEFAULT_GOAL := all
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

.PHONY: all clean fonts
fonts: user/inc/ui_font.h
user/inc/ui_font.h: tools/make_fonts.py user/src/clock_render.c
	python3 tools/make_fonts.py
$(OBJECTS): | user/inc/ui_font.h


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
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all -Iuser/inc $(filter %.c,$^) -o $@

$(BUILD)/test_clock $(BUILD)/test_rtc $(BUILD)/test_settings: user/inc/ui_font.h user/inc/clock_core.h user/inc/epd.h
.PHONY: test-desktop
test: test-desktop
test-desktop: $(BUILD)/test_desktop
	./$(BUILD)/test_desktop
$(BUILD)/test_desktop: tests/test_desktop.c user/src/temperature_math.c user/src/clock_core.c user/src/clock_render.c user/inc/clock_core.h user/inc/ui_font.h
	@mkdir -p $(BUILD)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all -Iuser/inc $(filter %.c,$^) -o $@

.PHONY: test-spi
test: test-spi
test-spi: $(BUILD)/test_spi
	./$(BUILD)/test_spi
$(BUILD)/test_spi: tests/test_spi.c tests/fake_spi/main.h user/src/board.c user/src/clock_core.c user/src/clock_render.c user/inc/ui_font.h
	@mkdir -p $(BUILD)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all -Itests/fake_spi -Iuser/inc $(filter %.c,$^) -o $@
