/**
 * @file    screen_files.c
 * @author  IAmWack
 * @brief   Обзор файлов на встроенной microSD + просмотр .txt.
 *
 * Навигация по папкам (вход/выход) — сам экран, не стек ui.c: заход в
 * папку и выход из неё просто перезагружают список с новым путём,
 * экран не меняется. "Назад" — последний пункт списка (см. go_back()):
 * жеста кнопкой BOOT под это не выделено (см. boot_button.c), выход —
 * через выбор пункта, как и в остальных списках-экранах. Служебные
 * файлы ФС (System Volume Information, лог fsck и т.п. — см.
 * is_system_entry()) из списка скрыты, это мусор ФС, а не содержимое
 * карты. Реализует контракт ui_screen_t (см. ui_screen.h).
 */
#include "ui.h"
#include "ui_screen.h"
#include "sdcard.h"
#include "settings.h"

#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <strings.h>

/* ============================================================================
 *  Defines / Macros
 * ============================================================================ */

#define SD_ROOT      "/sdcard"
#define MAX_ENTRIES  48
#define NAME_LEN     40

/* Буфер сырого содержимого файла. 8 КБ хватает на подавляющее
 * большинство заметок/логов/README; длиннее — показываем начало и
 * явно помечаем, что файл обрезан (см. s_view_truncated), а не молчим. */
#define VIEW_BUF_LEN 8192

/* Текст режется на строки один раз при открытии файла (см. open_view()),
 * а не на каждый кадр — после этого просмотр просто прокручивает готовый
 * массив. 80 байт на строку хватает с запасом даже под кириллицу
 * (2 байта/символ) на самую широкую раскладку (альбомную, ~300px). */
#define VIEW_LINE_LEN   80
#define MAX_VIEW_LINES  300

/* ============================================================================
 *  Types
 * ============================================================================ */

typedef struct {
    char   name[NAME_LEN];
    bool   is_dir;
    size_t size;
} file_entry_t;

/* ============================================================================
 *  Static state
 * ============================================================================ */

static char s_path[256];
static file_entry_t s_entries[MAX_ENTRIES];
static size_t s_count;
static ui_list_t s_list;

/* Просмотр текста: вместо списка — прокручиваемое содержимое файла. */
static bool s_viewing = false;
static char s_view_name[NAME_LEN];
static char s_view_buf[VIEW_BUF_LEN];
static bool s_view_truncated;   /* файл не влез в s_view_buf целиком */
static bool s_view_landscape;   /* альбомная раскладка (см. settings_reader_landscape()) */

static char s_view_lines[MAX_VIEW_LINES][VIEW_LINE_LEN];
static int  s_view_line_count;
static int  s_view_scroll;      /* индекс первой видимой строки */

/* ============================================================================
 *  Static helpers
 * ============================================================================ */

/**
 * @brief   true, если имя — служебный мусор ФС, а не содержимое карты
 *          (System Volume Information, логи fsck, macOS/Windows артефакты).
 */
static bool is_system_entry(const char *name)
{
    static const char *const JUNK[] = {
        "System Volume Information",
        "RECYCLE.BIN", "$RECYCLE.BIN",
        "Thumbs.db", "desktop.ini",
        ".Trash-1000", ".fseventsd", ".Spotlight-V100",
    };
    if (name[0] == '.') {
        return true; /* dotfiles — тоже служебные (macOS ._*, .Trashes и т.п.) */
    }
    if (strncasecmp(name, "FOUND.", 6) == 0) {
        return true; /* fsck/scandisk "потерянные кластеры" */
    }
    for (size_t i = 0; i < sizeof(JUNK) / sizeof(JUNK[0]); i++) {
        if (strcasecmp(name, JUNK[i]) == 0) {
            return true;
        }
    }
    return false;
}

/** @brief  true, если имя файла оканчивается на ext (без учёта регистра). */
static bool has_ext(const char *name, const char *ext)
{
    size_t nlen = strlen(name), elen = strlen(ext);
    return nlen >= elen && strcasecmp(name + nlen - elen, ext) == 0;
}

/** @brief  (Пере)прочитать список s_path в s_entries и сбросить список/выделение. */
static void load_dir(void)
{
    s_count = 0;

    DIR *d = opendir(s_path);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) != NULL && s_count < MAX_ENTRIES) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0 ||
                is_system_entry(de->d_name)) {
                continue;
            }

            char full[sizeof(s_path) + 256 + 2];
            snprintf(full, sizeof(full), "%s/%s", s_path, de->d_name);

            struct stat st;
            bool is_dir = (stat(full, &st) == 0) && S_ISDIR(st.st_mode);

            file_entry_t *e = &s_entries[s_count++];
            strncpy(e->name, de->d_name, sizeof(e->name) - 1);
            e->name[sizeof(e->name) - 1] = '\0';
            e->is_dir = is_dir;
            e->size = is_dir ? 0 : (size_t)st.st_size;
        }
        closedir(d);
    }

    /* +1 — синтетический последний пункт "Назад" (см. row()/go_back()). */
    ui_list_init(&s_list, s_count + 1, 5, 30);
}

/** @brief  Подняться на уровень вверх по дереву папок, а из корня — уйти с экрана. */
static void go_back(void)
{
    if (strcmp(s_path, SD_ROOT) == 0) {
        ui_back();
        return;
    }
    char *slash = strrchr(s_path, '/');
    if (slash && slash != s_path) {
        *slash = '\0';
    }
    load_dir();
}

/** @brief  Отформатировать размер файла коротко ("345Б"/"12КБ"/"3МБ"). */
static void format_size(size_t bytes, char *out, size_t out_len)
{
    if (bytes < 1024) {
        snprintf(out, out_len, "%uБ", (unsigned)bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(out, out_len, "%uКБ", (unsigned)(bytes / 1024));
    } else {
        snprintf(out, out_len, "%uМБ", (unsigned)(bytes / (1024 * 1024)));
    }
}

/**
 * @brief   Геометрия просмотра текста — портретная (в рамках обычного
 *          контента экрана) либо альбомная (полноэкранная, см.
 *          gfx_set_landscape()/draw_hintbar() в ui.c).
 * @param   out_x        Левый край текста.
 * @param   out_top_y    Y первой строки текста.
 * @param   out_bottom_y Y, ниже которого строки уже не влезают.
 * @param   out_w        Логическая ширина холста (для прижатия к правому краю).
 * @return  Сколько строк текста влезает между out_top_y и out_bottom_y.
 */
static int view_layout(int *out_x, int *out_top_y, int *out_bottom_y, int *out_w)
{
    if (s_view_landscape) {
        *out_x = 6;
        *out_top_y = 20;
        *out_bottom_y = GFX_W - 6;   /* логическая высота холста в альбоме — GFX_W */
        *out_w = GFX_H;              /* логическая ширина холста в альбоме — GFX_H */
    } else {
        *out_x = LIST_X;
        *out_top_y = CONTENT_Y + 16;
        *out_bottom_y = HINT_Y - 4;
        *out_w = GFX_W;
    }
    int pitch = wackos_font_small.line_height + 2;
    int visible = (*out_bottom_y - *out_top_y) / pitch;
    return visible < 1 ? 1 : visible;
}

/**
 * @brief   Открыть файл на просмотр: прочитать в s_view_buf и один раз
 *          разбить на строки в s_view_lines (см. view_layout()/draw()).
 * @param   full_path  Полный путь к файлу на карте.
 * @param   name       Имя файла (для заголовка просмотра).
 */
static void open_view(const char *full_path, const char *name)
{
    FILE *f = fopen(full_path, "r");
    if (!f) {
        ui_toast("не открылся");
        return;
    }

    size_t n = fread(s_view_buf, 1, sizeof(s_view_buf) - 1, f);
    s_view_buf[n] = '\0';
    /* Влезло ровно впритык буфера, и это не совпало с концом файла —
     * значит, хвост обрезан (а не файл случайно был ровно такого размера). */
    s_view_truncated = (n == sizeof(s_view_buf) - 1) && (fgetc(f) != EOF);
    fclose(f);

    strncpy(s_view_name, name, sizeof(s_view_name) - 1);
    s_view_name[sizeof(s_view_name) - 1] = '\0';

    s_view_landscape = settings_reader_landscape();
    int x, top_y, bottom_y, w;
    view_layout(&x, &top_y, &bottom_y, &w);
    s_view_line_count = ui_wrap_lines(&wackos_font_small, s_view_buf, w - 2 * x,
                                      (char *)s_view_lines, VIEW_LINE_LEN, MAX_VIEW_LINES);
    s_view_scroll = 0;
    s_viewing = true;
}

/* ============================================================================
 *  Screen callbacks (ui_screen_t)
 * ============================================================================ */

/** @brief  Открыть экран всегда с корня карты. */
static void enter(void)
{
    strncpy(s_path, SD_ROOT, sizeof(s_path) - 1);
    s_path[sizeof(s_path) - 1] = '\0';
    s_viewing = false;
    load_dir();
}

/**
 * @brief   Заполнить строку списка именем файла/папки и размером —
 *          либо, для последнего индекса (== s_count), пунктом "Назад".
 * @param   index  Индекс записи в s_entries, либо s_count для "Назад".
 * @param   out    Строка списка для заполнения.
 */
static void row(size_t index, ui_row_t *out)
{
    if (index == s_count) {
        strncpy(out->title, "Назад", sizeof(out->title) - 1);
        out->value[0] = '\0';
        out->icon = UI_ICON_PLUG;
        return;
    }

    const file_entry_t *e = &s_entries[index];
    strncpy(out->title, e->name, sizeof(out->title) - 1);
    out->title[sizeof(out->title) - 1] = '\0';

    if (e->is_dir) {
        strncpy(out->value, "папка", sizeof(out->value) - 1);
        out->icon = UI_ICON_BOW;
    } else {
        format_size(e->size, out->value, sizeof(out->value));
        out->icon = UI_ICON_NONE;
    }
}

/**
 * @brief   Отрисовать список и текущий путь (без "/sdcard" — он и так
 *          единственный корень на этой плате).
 * @param   t  Текущее время кадра (мс).
 */
static void draw(uint32_t t)
{
    (void)t;
    if (s_viewing) {
        const wackos_font_t *f = &wackos_font_small;
        int x, top_y, bottom_y, w;
        int visible = view_layout(&x, &top_y, &bottom_y, &w);
        (void)bottom_y;

        int max_scroll = s_view_line_count - visible;
        if (max_scroll < 0) max_scroll = 0;
        if (s_view_scroll > max_scroll) s_view_scroll = max_scroll;
        if (s_view_scroll < 0) s_view_scroll = 0;

        if (s_view_landscape) {
            /* Полноэкранный альбомный заход — см. gfx_set_landscape() и
             * draw_hintbar()/render() в ui.c (прячут портретный "хром"
             * на этот кадр). Заливаем весь физический буфер: поворот
             * на gfx_fill_rect() не влияет, он рисует напрямую в s_fb. */
            gfx_set_landscape(true);
            gfx_fill_rect(0, 0, GFX_W, GFX_H, TH->bg_mid, 255);
        }

        /* Заголовок: имя файла слева, страница справа (если не влезает целиком). */
        if (s_view_line_count > visible) {
            char page[20];
            int pages = (s_view_line_count + visible - 1) / visible;
            int cur = s_view_scroll / visible + 1;
            snprintf(page, sizeof(page), "%d/%d%s", cur, pages, s_view_truncated ? "+" : "");
            int page_w = gfx_text_width(f, page);
            gfx_text(f, x + (w - 2 * x) - page_w, top_y - 16, page, TH->text_dim, 220);
            gfx_text_ellipsis(f, x, top_y - 16, s_view_name, (w - 2 * x) - page_w - 8,
                              TH->text_dim, 220);
        } else {
            gfx_text_ellipsis(f, x, top_y - 16, s_view_name, w - 2 * x, TH->text_dim, 220);
        }

        int pitch = f->line_height + 2;
        for (int i = 0; i < visible && s_view_scroll + i < s_view_line_count; i++) {
            gfx_text(f, x, top_y + i * pitch, s_view_lines[s_view_scroll + i], TH->text, 255);
        }
        return;
    }

    const char *rel = s_path + strlen(SD_ROOT);
    gfx_text_ellipsis(&wackos_font_small, LIST_X, CONTENT_Y - 2,
                      rel[0] ? rel : "/", GFX_W - 2 * LIST_X, TH->text_dim, 220);

    if (s_count == 0) {
        /* Ниже единственной строки списка ("Назад"), чтобы не наезжать. */
        gfx_text_center(&wackos_font_small, GFX_W / 2, CONTENT_Y + 64,
                        sdcard_ready() ? "папка пуста" : "нет карты", TH->text_dim, 220);
    }
    /* Список рисуем всегда — в нём как минимум пункт "Назад" (см. row()),
     * иначе из пустой папки/без карты некуда будет деться. */
    ui_list_draw(&s_list, CONTENT_Y + 14, row, t);
}

/**
 * @brief   Обработать ввод: прокрутка, вход в папку, выход на уровень вверх.
 * @param   in  Событие ввода.
 */
static void input(ui_input_t in)
{
    if (s_viewing) {
        /* NEXT/PREV — листать страницы, PRESS/BACK — закрыть просмотр
         * (как в screen_about.c: экран без списка, "назад" = PRESS,
         * т.к. BACK с кнопкой BOOT этой платы недостижим, см. ui.h). */
        int x, top_y, bottom_y, w;
        int visible = view_layout(&x, &top_y, &bottom_y, &w);
        (void)x; (void)top_y; (void)bottom_y; (void)w;
        switch (in) {
            case UI_IN_NEXT:
                s_view_scroll += visible;
                break;
            case UI_IN_PREV:
                s_view_scroll -= visible;
                break;
            case UI_IN_PRESS:
            case UI_IN_BACK:
                s_viewing = false;
                break;
        }
        int max_scroll = s_view_line_count - visible;
        if (max_scroll < 0) max_scroll = 0;
        if (s_view_scroll > max_scroll) s_view_scroll = max_scroll;
        if (s_view_scroll < 0) s_view_scroll = 0;
        return;
    }

    switch (in) {
        case UI_IN_NEXT: ui_list_move(&s_list, 1); break;
        case UI_IN_PREV: ui_list_move(&s_list, -1); break;

        case UI_IN_PRESS: {
            if (s_list.selected == s_count) {
                go_back();
                break;
            }
            const file_entry_t *e = &s_entries[s_list.selected];
            if (e->is_dir) {
                size_t len = strlen(s_path);
                snprintf(s_path + len, sizeof(s_path) - len, "/%s", e->name);
                load_dir();
            } else if (has_ext(e->name, ".txt") || has_ext(e->name, ".log") ||
                       has_ext(e->name, ".md")) {
                char full[sizeof(s_path) + NAME_LEN + 2];
                snprintf(full, sizeof(full), "%s/%s", s_path, e->name);
                open_view(full, e->name);
            } else {
                /* Любой другой формат (.docx, .jpg, бинарники и т.п.) —
                 * честно сказать "не умеем", а не молча показать размер:
                 * раньше это выглядело как "почему файл не открылся?",
                 * хотя вьювер просто не про этот формат. */
                ui_toast("формат не поддерживается");
            }
            break;
        }

        case UI_IN_BACK:
            go_back();
            break;
    }
}

/* ============================================================================
 *  Public API
 * ============================================================================ */

/** @brief  Таблица экрана файлов для роутера ui.c. */
const ui_screen_t screen_files = {
    .title = "Файлы",
    .enter = enter,
    .draw = draw,
    .input = input,
};
