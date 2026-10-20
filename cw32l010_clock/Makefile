.DEFAULT_GOAL := all
PREFIX ?= arm-none-eabi-
CC = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
SIZE = $(PREFIX)size
HOST_CC ?= cc
PYTHON ?= python3
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

.PHONY: all clean fonts theme-assets
fonts: user/inc/ui_font.h
theme-assets: user/inc/ui_font.h
	$(PYTHON) tools/make_theme_assets.py
user/inc/ui_font.h: tools/make_fonts.py user/src/clock_render.c user/src/clock_theme.c
	$(PYTHON) tools/make_fonts.py
$(BUILD)/user/src/clock_theme.o: user/inc/theme_assets.h
$(OBJECTS): | user/inc/ui_font.h

RENDER_TESTS = $(addprefix $(BUILD)/,test_clock test_rtc test_settings test_desktop test_spi test_labels test_epd test_epd_window test_epd_quality test_epd_quality47 test_epd_fast47 test_window test_main test_focus_alert test_theme_render)
$(RENDER_TESTS): user/src/clock_theme.c user/inc/clock_theme.h user/inc/theme_assets.h


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
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all -Iuser/inc tests/test_clock.c user/src/clock_core.c user/src/clock_render.c user/src/clock_theme.c user/src/epd.c -o $@
test: test-rtc
.PHONY: test-rtc
test-rtc: $(BUILD)/test_rtc
	./$(BUILD)/test_rtc
$(BUILD)/test_rtc: tests/test_rtc.c tests/fake/main.h user/src/rtc_clock.c user/src/clock_core.c user/src/clock_render.c
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all -Itests/fake -Iuser/inc tests/test_rtc.c user/src/rtc_clock.c user/src/clock_core.c user/src/clock_render.c user/src/clock_theme.c -o $@

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
$(BUILD)/test_spi: tests/test_spi.c tests/fake_spi/main.h user/src/board.c user/src/clock_core.c user/src/clock_render.c user/inc/ui_font.h user/inc/board.h
	@mkdir -p $(BUILD)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all -Itests/fake_spi -Iuser/inc $(filter %.c,$^) -o $@

.PHONY: test-labels
test: test-labels
test-labels: $(BUILD)/test_labels
	./$(BUILD)/test_labels
$(BUILD)/test_labels: tests/test_labels.c user/src/clock_core.c user/src/clock_render.c user/inc/clock_core.h user/inc/ui_font.h
	@mkdir -p $(BUILD)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all -Iuser/inc $(filter %.c,$^) -o $@

.PHONY: test-epd test-epd-window
test: test-epd
test: test-epd-window
test-epd: $(BUILD)/test_epd
	./$(BUILD)/test_epd
test-epd-window: $(BUILD)/test_epd_window
	./$(BUILD)/test_epd_window
$(BUILD)/test_epd: tests/test_epd.c user/src/epd.c user/src/clock_core.c user/src/clock_render.c user/inc/epd.h user/inc/epd_lut.h user/inc/ui_font.h user/inc/clock_core.h
	@mkdir -p $(BUILD)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all -Iuser/inc $(filter %.c,$^) -o $@
$(BUILD)/test_epd_window: tests/test_epd.c user/src/epd.c user/src/clock_core.c user/src/clock_render.c user/inc/epd.h user/inc/epd_lut.h user/inc/ui_font.h user/inc/clock_core.h
	@mkdir -p $(BUILD)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all -DEPD_USE_PARTIAL_WINDOW=1 -Iuser/inc $(filter %.c,$^) -o $@

.PHONY: test-epd-quality
test: test-epd-quality
test-epd-quality: $(BUILD)/test_epd_quality
	./$(BUILD)/test_epd_quality
$(BUILD)/test_epd_quality: tests/test_epd.c user/src/epd.c user/src/clock_core.c user/src/clock_render.c user/inc/epd.h user/inc/epd_lut.h user/inc/ui_font.h user/inc/clock_core.h
	@mkdir -p $(BUILD)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all -DEPD_USE_FAST_LUT=0 -DEPD_FULL_BORDER_DATA=0x87u -Iuser/inc $(filter %.c,$^) -o $@

.PHONY: test-epd-quality47
test: test-epd-quality47
test-epd-quality47: $(BUILD)/test_epd_quality47
	./$(BUILD)/test_epd_quality47
$(BUILD)/test_epd_quality47: tests/test_epd.c user/src/epd.c user/src/clock_core.c user/src/clock_render.c user/inc/epd.h user/inc/epd_lut.h user/inc/ui_font.h user/inc/clock_core.h
	@mkdir -p $(BUILD)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all -DEPD_USE_FAST_LUT=0 -DEPD_FULL_BORDER_DATA=0x47u -Iuser/inc $(filter %.c,$^) -o $@

.PHONY: test-epd-fast47
test: test-epd-fast47
test-epd-fast47: $(BUILD)/test_epd_fast47
	./$(BUILD)/test_epd_fast47
$(BUILD)/test_epd_fast47: tests/test_epd.c user/src/epd.c user/src/clock_core.c user/src/clock_render.c user/inc/epd.h user/inc/epd_lut.h user/inc/ui_font.h user/inc/clock_core.h
	@mkdir -p $(BUILD)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all -DEPD_USE_FAST_LUT=1 -DEPD_USE_PARTIAL_WINDOW=0 -DEPD_FAST_BORDER_DATA=0x47u -DEPD_FULL_BORDER_DATA=0x47u -Iuser/inc $(filter %.c,$^) -o $@

.PHONY: test-window
test: test-window
test-window: $(BUILD)/test_window
	./$(BUILD)/test_window
$(BUILD)/test_window: tests/test_window.c user/src/clock_core.c user/src/clock_render.c user/inc/clock_core.h user/inc/ui_font.h
	@mkdir -p $(BUILD)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all -Iuser/inc $(filter %.c,$^) -o $@

.PHONY: test-main
test: test-main
test-main: $(BUILD)/test_main
	./$(BUILD)/test_main
$(BUILD)/test_main: tests/test_main.c tests/fake_main/main.h user/src/main.c user/src/clock_core.c user/src/clock_render.c user/inc/board.h user/inc/epd.h user/inc/clock_core.h user/inc/settings.h user/inc/temperature.h user/inc/ui_font.h
	@mkdir -p $(BUILD)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all -Itests/fake_main -Iuser/inc tests/test_main.c user/src/clock_core.c user/src/clock_render.c user/src/clock_theme.c -o $@

.PHONY: test-focus-alert
test: test-focus-alert
test-focus-alert: $(BUILD)/test_focus_alert
	./$(BUILD)/test_focus_alert
$(BUILD)/test_focus_alert: tests/test_focus_alert.c tests/fake_main/main.h user/src/main.c user/src/clock_core.c user/src/clock_render.c user/inc/board.h user/inc/epd.h user/inc/clock_core.h user/inc/settings.h user/inc/temperature.h user/inc/ui_font.h
	@mkdir -p $(BUILD)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all -Itests/fake_main -Iuser/inc tests/test_focus_alert.c user/src/clock_core.c user/src/clock_render.c user/src/clock_theme.c -o $@

.PHONY: test-theme-render
test: test-theme-render
test-theme-render: $(BUILD)/test_theme_render
	./$(BUILD)/test_theme_render
$(BUILD)/test_theme_render: tests/test_theme_render.c user/src/clock_core.c user/src/clock_render.c
	@mkdir -p $(BUILD)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all -DCLOCK_THEME_TEST -Iuser/inc $(filter %.c,$^) -o $@

.PHONY: test-menu-transition
test: test-menu-transition
test-menu-transition: $(BUILD)/test_menu_transition
	./$(BUILD)/test_menu_transition
$(BUILD)/test_menu_transition: tests/test_menu_transition.c tests/fake_main/main.h user/src/main.c user/src/epd.c user/src/clock_core.c user/src/clock_render.c user/src/clock_theme.c user/inc/theme_assets.h user/inc/clock_core.h user/inc/epd.h user/inc/ui_font.h
	@mkdir -p $(BUILD)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all -DEPD_USE_FAST_LUT=1 -DEPD_USE_PARTIAL_WINDOW=0 -DEPD_FAST_BORDER_DATA=0x47u -DEPD_FULL_BORDER_DATA=0x47u -Itests/fake_main -Iuser/inc tests/test_menu_transition.c user/src/epd.c user/src/clock_core.c user/src/clock_render.c user/src/clock_theme.c -o $@

.PHONY: test-theme-profile
test: test-theme-profile
test-theme-profile: $(BUILD)/test_theme_profile
	./$(BUILD)/test_theme_profile
$(BUILD)/test_theme_profile: tools/profile_render.c user/src/clock_core.c user/src/clock_render.c user/src/clock_theme.c user/inc/theme_assets.h user/inc/clock_core.h user/inc/clock_theme.h user/inc/ui_font.h
	@mkdir -p $(BUILD)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all -DCLOCK_THEME_PROFILE -DCLOCK_THEME_PROFILE_TEST -Iuser/inc $(filter %.c,$^) -o $@
