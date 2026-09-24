# Симулятор экрана Wack-OS для ноутбука.
#
#   make && ./wackos-sim
#
# Ничего ставить не нужно: только gcc и libX11, которые уже есть в
# системе. ESP-IDF для симулятора не требуется — недостающие куски
# ESP-API подменены заглушками в compat/.
#
# Собирается ровно тот же код, что уезжает в прошивку: весь UI
# (master/main/ui*.c, screens/, astolfo.c, theme.c), логика, общий
# wackos_gfx и wackos_protocol, а также настоящий диспетчер slave.
# Подменены только нижние слои — экран, энкодер, звук и транспорт.

# Правка флагов в этом файле должна пересобирать объектники — поэтому
# ниже он стоит в зависимостях правил компиляции.
ROOT     := ..
OBJDIR   := build
BIN      := wackos-sim

CC       := gcc
# -MMD -MP: заголовки попадают в зависимости. Без этого правка .h не
# пересобирает объектники, и в бинарь приезжает смесь старых и новых
# структур — ловится потом как случайный SIGSEGV.
CFLAGS   := -std=gnu11 -O2 -g -Wall -Wextra -Wno-unused-parameter -MMD -MP
LDLIBS   := -lX11 -lm

COMMON_INC := -I. -Icompat \
              -I$(ROOT)/components/wackos_protocol/include \
              -I$(ROOT)/components/wackos_gfx/include \
              -I$(ROOT)/components/wackos_encoder/include

MASTER_INC := $(COMMON_INC) -I$(ROOT)/master/main -I$(ROOT)/master/main/screens

# slave/main идёт первым: board_pins.h есть у обеих сторон, и для
# диспетчера нужен именно slave-овский.
SLAVE_INC  := $(COMMON_INC) -I$(ROOT)/slave/main -I$(ROOT)/master/main \
              -I$(ROOT)/master/main/screens

# Слой протокола хранит состояние (seq, транспорт) в статических
# переменных, а в симуляторе в одном процессе живут обе стороны сразу.
# Поэтому слой собирается дважды, и у копии для slave все имена
# переименованы — это честнее, чем подделывать вторую сторону вручную.
SLAVE_DEFS := \
  -Dwackos_proto_init=slv_proto_init \
  -Dwackos_proto_set_address=slv_proto_set_address \
  -Dwackos_proto_request=slv_proto_request \
  -Dwackos_proto_poll_event=slv_proto_poll_event \
  -Dwackos_proto_receive_request=slv_proto_receive_request \
  -Dwackos_proto_send_ok=slv_proto_send_ok \
  -Dwackos_proto_send_err=slv_proto_send_err \
  -Dwackos_proto_send_event=slv_proto_send_event \
  -Dwackos_frame_build=slv_frame_build \
  -Dwackos_frame_parse=slv_frame_parse

# Весь код прошивки, который не привязан к железу.
UI_SRCS := \
  $(ROOT)/master/main/ui.c \
  $(ROOT)/master/main/ui_widgets.c \
  $(ROOT)/master/main/app_logic.c \
  $(ROOT)/master/main/theme.c \
  $(ROOT)/master/main/settings.c \
  $(ROOT)/master/main/astolfo.c \
  $(ROOT)/master/main/screens/screen_boot.c \
  $(ROOT)/master/main/screens/screen_menu.c \
  $(ROOT)/master/main/screens/screen_modules.c \
  $(ROOT)/master/main/screens/screen_module.c \
  $(ROOT)/master/main/screens/screen_slave.c \
  $(ROOT)/master/main/screens/screen_settings.c \
  $(ROOT)/master/main/screens/screen_about.c \
  $(ROOT)/components/wackos_protocol/wackos_protocol.c \
  $(ROOT)/components/wackos_encoder/wackos_encoder.c \
  $(ROOT)/components/wackos_gfx/wackos_gfx.c \
  $(ROOT)/components/wackos_gfx/wackos_font.c \
  $(ROOT)/components/wackos_gfx/wackos_font_data.c

# Окно + клавиатура + петлевой провод.
MASTER_SRCS := $(UI_SRCS) \
  $(ROOT)/sim/sim_main.c \
  $(ROOT)/sim/sim_window.c \
  $(ROOT)/sim/sim_display.c \
  $(ROOT)/sim/sim_encoder.c \
  $(ROOT)/sim/sim_sound.c \
  $(ROOT)/sim/sim_transport.c \
  $(ROOT)/sim/sim_serial.c \
  $(ROOT)/sim/sim_platform.c

# То же самое, но экран/энкодер/окно подменены на запись кадров в файл.
HEADLESS_SRCS := $(UI_SRCS) \
  $(ROOT)/sim/sim_headless.c \
  $(ROOT)/sim/sim_sound.c \
  $(ROOT)/sim/sim_transport.c \
  $(ROOT)/sim/sim_serial.c \
  $(ROOT)/sim/sim_platform.c

SHOT_SRCS := $(HEADLESS_SRCS) $(ROOT)/sim/sim_shot.c
TEST_SRCS := $(HEADLESS_SRCS) $(ROOT)/sim/sim_test.c

# Лист персонажа — только рисование, без интерфейса и протокола.
SHEET_SRCS := \
  $(ROOT)/master/main/theme.c \
  $(ROOT)/master/main/astolfo.c \
  $(ROOT)/components/wackos_encoder/wackos_encoder.c \
  $(ROOT)/components/wackos_gfx/wackos_gfx.c \
  $(ROOT)/components/wackos_gfx/wackos_font.c \
  $(ROOT)/components/wackos_gfx/wackos_font_data.c \
  $(ROOT)/sim/tool_sheet.c

SLAVE_SRCS := \
  $(ROOT)/slave/main/dispatcher.c \
  $(ROOT)/slave/main/modules/module_encoder.c \
  $(ROOT)/slave/main/modules/module_radio.c \
  $(ROOT)/slave/main/modules/module_hid.c \
  $(ROOT)/slave/main/modules/module_sensor.c \
  $(ROOT)/components/wackos_protocol/wackos_protocol.c \
  $(ROOT)/sim/sim_slave.c

MASTER_OBJS := $(patsubst $(ROOT)/%,$(OBJDIR)/m/%,$(MASTER_SRCS:.c=.o))
SHOT_OBJS   := $(patsubst $(ROOT)/%,$(OBJDIR)/h/%,$(SHOT_SRCS:.c=.o))
TEST_OBJS   := $(patsubst $(ROOT)/%,$(OBJDIR)/h/%,$(TEST_SRCS:.c=.o))
SHEET_OBJS  := $(patsubst $(ROOT)/%,$(OBJDIR)/m/%,$(SHEET_SRCS:.c=.o))
SLAVE_OBJS  := $(patsubst $(ROOT)/%,$(OBJDIR)/s/%,$(SLAVE_SRCS:.c=.o))

all: $(BIN)

$(BIN): $(MASTER_OBJS) $(SLAVE_OBJS)
	$(CC) $^ -o $@ $(LDLIBS)

wackos-shot: $(SHOT_OBJS) $(SLAVE_OBJS)
	$(CC) $^ -o $@ $(LDLIBS)

wackos-test: $(TEST_OBJS) $(SLAVE_OBJS)
	$(CC) $^ -o $@ $(LDLIBS)

astolfo-sheet: $(SHEET_OBJS)
	$(CC) $^ -o $@ $(LDLIBS)

$(OBJDIR)/m/%.o: $(ROOT)/%.c Makefile
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(MASTER_INC) -c $< -o $@

$(OBJDIR)/h/%.o: $(ROOT)/%.c Makefile
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(MASTER_INC) -c $< -o $@

$(OBJDIR)/s/%.o: $(ROOT)/%.c Makefile
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(SLAVE_INC) $(SLAVE_DEFS) -c $< -o $@

run: $(BIN)
	./$(BIN)

# Снимки всех экранов без окна: shots/*.ppm (+ .png, если есть ImageMagick).
shots: wackos-shot
	./wackos-shot
	@command -v convert >/dev/null && for f in shots/*.ppm; do convert "$$f" "$${f%.ppm}.png"; done || true

# Прогон по живой плате: make test PORT=/dev/ttyUSB0
# Без PORT гоняется по фальшивому slave — удобно как быстрая проверка.
test: wackos-test
	./wackos-test $(if $(PORT),-p $(PORT))

# Лист персонажа: все настроения Астольфо в двух размерах.
sheet: astolfo-sheet
	./astolfo-sheet

# Перегенерировать растровый шрифт из TTF (нужен python3 + Pillow).
font:
	python3 $(ROOT)/tools/gen_font.py

clean:
	rm -rf $(OBJDIR) $(BIN) wackos-shot wackos-test astolfo-sheet shots

-include $(shell find $(OBJDIR) -name '*.d' 2>/dev/null)

.PHONY: all run clean font shots sheet test
