#define _DEFAULT_SOURCE
#include "../include/ui.h"
#include "../include/tinyfiledialogs.h"
#include <SDL2/SDL_ttf.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define LOGICAL_W 1920
#define LOGICAL_H 1080
#define ROW_H     52

static const SDL_Rect R_TOPBAR    = {  60,  30, 1800,  78};
static const SDL_Rect B_SEARCH    = { 480,  47,  520,  44};
static const SDL_Rect B_SORT      = {1020,  47,  175,  44};
static const SDL_Rect B_RESCAN    = {1215,  47,  240,  44};
static const SDL_Rect R_TRACKCOUNT= {1475,  47,  345,  44};
static const SDL_Rect R_LISTPANEL = {  60, 126, 1800, 650};
static const SDL_Rect R_LIST      = {  60, 181, 1800, 582};
static const SDL_Rect R_PROGRESS  = { 590, 980,  680,  16};
static const SDL_Rect R_VOLUME    = {1530, 981,  210,  14};
static const SDL_Rect B_PREV      = {  60, 945,   80,  80};
static const SDL_Rect B_PLAY      = { 200, 935,  100, 100};
static const SDL_Rect B_NEXT      = { 360, 945,   80,  80};
static const SDL_Color COL_BG     = { 24,  24,  28, 255};
static const SDL_Color COL_PANEL  = { 36,  36,  42, 255};
static const SDL_Color COL_ROW    = { 52,  52,  62, 255};
static const SDL_Color COL_TRACK  = { 70,  70,  80, 255};
static const SDL_Color COL_TEXT   = {240, 240, 245, 255};
static const SDL_Color COL_DIM    = {150, 150, 160, 255};
static const SDL_Color COL_ACCENT = { 30, 215,  96, 255};
static const SDL_Color COL_ACC_HI = { 90, 240, 140, 255};

static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;
static TTF_Font *font_big   = NULL;
static TTF_Font *font       = NULL;
static TTF_Font *font_small = NULL;

static int mouse_x = 0, mouse_y = 0;
static int drag_seek = 0, drag_volume = 0;
static double seek_preview = 0.0;
static int scroll = 0;
static int last_current = -1;
static int    cache_count    = 0;
static const char *const *cache_tracks = NULL;
static double cache_duration = 0.0;
static float  cache_volume   = 1.0f;

static char search_query[160] = "";
static int search_active = 0;
static int sort_descending = 0;
static int view_dirty = 1;
static int *view_indices = NULL;
static int view_count = 0;
static int view_capacity = 0;
static const char *FONT_PATHS[] = {
    "assets/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/dejavu/DejaVuSans.ttf",
    NULL
};


static int contains_case_insensitive(const char *text, const char *query)
{
    if (query == NULL || query[0] == '\0') return 1;
    if (text == NULL) return 0;

    size_t qlen = strlen(query);
    for (const char *p = text; *p; p++) {
        size_t i = 0;
        while (i < qlen && p[i] != '\0' &&
               tolower((unsigned char)p[i]) == tolower((unsigned char)query[i])) {
            i++;
        }
        if (i == qlen) return 1;
    }
    return 0;
}

static int compare_case_insensitive(const char *a, const char *b)
{
    if (a == NULL) a = "";
    if (b == NULL) b = "";

    while (*a && *b) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++;
        b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

static int compare_view_indices(const void *left, const void *right)
{
    int a = *(const int *)left;
    int b = *(const int *)right;
    int result = compare_case_insensitive(cache_tracks[a], cache_tracks[b]);
    if (result == 0) return a - b;
    return sort_descending ? -result : result;
}

static void rebuild_view(void)
{
    if (!view_dirty) return;

    view_count = 0;
    if (cache_count <= 0 || cache_tracks == NULL) {
        view_dirty = 0;
        return;
    }

    if (view_capacity < cache_count) {
        int *next = realloc(view_indices, (size_t)cache_count * sizeof(*view_indices));
        if (next == NULL) {
            view_dirty = 0;
            return;
        }
        view_indices = next;
        view_capacity = cache_count;
    }

    for (int i = 0; i < cache_count; i++) {
        if (contains_case_insensitive(cache_tracks[i], search_query)) {
            view_indices[view_count++] = i;
        }
    }

    if (view_count > 1) {
        qsort(view_indices, (size_t)view_count, sizeof(*view_indices), compare_view_indices);
    }

    view_dirty = 0;
}

static void set_search_active(int active)
{
    if (search_active == active) return;
    search_active = active;
    if (search_active) SDL_StartTextInput();
    else SDL_StopTextInput();
}

static void erase_last_utf8_character(char *text)
{
    size_t len = strlen(text);
    if (len == 0) return;

    len--;
    while (len > 0 && (((unsigned char)text[len] & 0xC0) == 0x80)) {
        len--;
    }
    text[len] = '\0';
}

static double clampd(double v, double lo, double hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}
static int point_in(const SDL_Rect *r, int x, int y)
{
    return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}
static int point_in_bar(const SDL_Rect *r, int x, int y)
{
    SDL_Rect hit = {r->x - 12, r->y - 20, r->w + 24, r->h + 40};
    return point_in(&hit, x, y);
}
static double bar_fraction(const SDL_Rect *r, int x)
{
    return clampd((double)(x - r->x) / r->w, 0.0, 1.0);
}
static void set_color(SDL_Color c)
{
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
}
static void fill_rect(const SDL_Rect *r, SDL_Color c)
{
    set_color(c);
    SDL_RenderFillRect(renderer, r);
}
static void fill_circle(int cx, int cy, int radius, SDL_Color c)
{
    set_color(c);
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = (int)SDL_sqrt((double)(radius * radius - dy * dy));
        SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}
static void fill_triangle(int x, int y, int w, int h, int right, SDL_Color c)
{
    set_color(c);
    for (int i = 0; i < w; i++) {
        int half = (h * (w - i)) / (2 * w);
        int px = right ? x + i : x + w - 1 - i;
        SDL_RenderDrawLine(renderer, px, y + h / 2 - half, px, y + h / 2 + half);
    }
}
static int text_width(TTF_Font *f, const char *s)
{
    int w = 0, h = 0;
    TTF_SizeUTF8(f, s, &w, &h);
    return w;
}
static void fit_text(TTF_Font *f, const char *text, int max_w,
                     char *out, size_t out_size)
{
    snprintf(out, out_size - 3, "%s", text);
    if (text_width(f, out) <= max_w) {
        return;
    }
    int dots = text_width(f, "...");
    size_t len = strlen(out);
    while (len > 0 && text_width(f, out) + dots > max_w) {
        len--;
        while (len > 0 && ((unsigned char)out[len] & 0xC0) == 0x80) {
            len--;
        }
        out[len] = '\0';
    }
    strcat(out, "...");
}
#define TEXT_CACHE_SIZE 160
typedef struct {
    TTF_Font *font;
    char text[260];
    int max_w;
    SDL_Color color;
    SDL_Texture *tex;
    int w, h;
    unsigned long stamp;
} TextEntry;
static TextEntry text_cache[TEXT_CACHE_SIZE];
static unsigned long text_clock = 0;
static void text_cache_clear(void)
{
    for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
        if (text_cache[i].tex != NULL) {
            SDL_DestroyTexture(text_cache[i].tex);
        }
    }
    memset(text_cache, 0, sizeof(text_cache));
}
static TextEntry *text_lookup(TTF_Font *f, const char *text, int max_w, SDL_Color c){
    TextEntry *oldest = &text_cache[0];
    for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
        TextEntry *e = &text_cache[i];
        if (e->tex != NULL && e->font == f && e->max_w == max_w
            && e->color.r == c.r && e->color.g == c.g && e->color.b == c.b
            && strcmp(e->text, text) == 0) {
            e->stamp = ++text_clock;
            return e;
        }
        if (e->stamp < oldest->stamp) {
            oldest = e;
        }
    }
    char buf[260];
    const char *shown = text;
    if (max_w > 0) {
        fit_text(f, text, max_w, buf, sizeof(buf));
        shown = buf;
    }
    SDL_Surface *surface = TTF_RenderUTF8_Blended(f, shown, c);
    if (surface == NULL) {
        return NULL;
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surface);
    if (tex == NULL) {
        SDL_FreeSurface(surface);
        return NULL;
    }
    if (oldest->tex != NULL) {
        SDL_DestroyTexture(oldest->tex);
    }
    oldest->font = f;
    snprintf(oldest->text, sizeof(oldest->text), "%s", text);
    oldest->max_w = max_w;
    oldest->color = c;
    oldest->tex = tex;
    oldest->w = surface->w;
    oldest->h = surface->h;
    oldest->stamp = ++text_clock;
    SDL_FreeSurface(surface);
    return oldest;
}
static void draw_text_fit(TTF_Font *f, const char *text, int x, int y, int max_w, int align, SDL_Color color){
    if (text == NULL || text[0] == '\0') {
        return;
    }
    TextEntry *e = text_lookup(f, text, max_w, color);
    if (e == NULL) {
        return;
    }
    if (align == 1) x -= e->w / 2;
    if (align == 2) x -= e->w;
    SDL_Rect rect = {x, y, e->w, e->h};
    SDL_RenderCopy(renderer, e->tex, NULL, &rect);
}
static void draw_text(TTF_Font *f, const char *text, int x, int y, SDL_Color color){
    draw_text_fit(f, text, x, y, 0, 0, color);
}
static void format_time(double sec, char *buf, size_t n, int minus){
    int t = (int)(sec < 0 ? 0 : sec);
    snprintf(buf, n, "%s%02d:%02d", minus ? "-" : "", t / 60, t % 60);
}
static TTF_Font *open_font(int size){
    for (int i = 0; FONT_PATHS[i] != NULL; i++) {
        TTF_Font *f = TTF_OpenFont(FONT_PATHS[i], size);
        if (f != NULL) {
            return f;
        }
    }
    return NULL;
}
int ui_init(void)
{
    search_query[0] = '\0';
    search_active = 0;
    sort_descending = 0;
    view_dirty = 1;
    scroll = 0;
    last_current = -1;
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Error: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() < 0) {
        printf("Error: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }
    window = SDL_CreateWindow("Sonora",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              1280, 720,
                              SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (window == NULL) {
        printf("Error: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    renderer = SDL_CreateRenderer(window, -1,
                                  SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        printf("Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        window = NULL;
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    SDL_RenderSetLogicalSize(renderer, LOGICAL_W, LOGICAL_H);
    font_big   = open_font(48);
    font       = open_font(30);
    font_small = open_font(26);
    if (font_big == NULL || font == NULL || font_small == NULL) {
        printf("Error: could not load font: %s\n", TTF_GetError());
        ui_quit();
        return 1;
    }
    return 0;
}
void ui_quit(void)
{
    SDL_StopTextInput();
    free(view_indices);
    view_indices = NULL;
    view_count = 0;
    view_capacity = 0;
    text_cache_clear();
    if (font_big)   { TTF_CloseFont(font_big);   font_big = NULL; }
    if (font)       { TTF_CloseFont(font);       font = NULL; }
    if (font_small) { TTF_CloseFont(font_small); font_small = NULL; }
    if (renderer) { SDL_DestroyRenderer(renderer); renderer = NULL; }
    if (window)   { SDL_DestroyWindow(window);     window = NULL; }
    TTF_Quit();
    SDL_Quit();
}

int ui_choose_folder(char *out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 1;
    }

    out[0] = '\0';

    FILE *dialog = popen(
        "zenity --file-selection --directory "
        "--title='Select music folder'",
        "r"
    );

    if (dialog == NULL) {
        perror("ui_choose_folder: popen");
        return 1;
    }

    char *result = fgets(out, (int)out_size, dialog);
    int status = pclose(dialog);

    if (result == NULL) {
        out[0] = '\0';

        if (status == -1) {
            perror("ui_choose_folder: pclose");
        } else if (WIFEXITED(status)) {
            fprintf(stderr,
                    "[FOLDER] Zenity exited with code %d\n",
                    WEXITSTATUS(status));
        } else {
            fprintf(stderr, "[FOLDER] Zenity terminated unexpectedly\n");
        }

        return 1;
    }

    if (strchr(out, '\n') == NULL && !feof(dialog)) {
        out[0] = '\0';
        fprintf(stderr, "[FOLDER] Selected path is too long\n");
        return 1;
    }

    out[strcspn(out, "\r\n")] = '\0';

    if (status == -1 ||
        !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0 ||
        out[0] == '\0') {
        out[0] = '\0';
        fprintf(stderr, "[FOLDER] Folder selection failed\n");
        return 1;
    }

    return 0;
}

void ui_playlist_changed(void)
{
    cache_tracks = NULL;   
    cache_count  = 0;
    view_count   = 0;
    view_dirty   = 1;
    scroll       = 0;
    last_current = -1;
}

UiAction ui_handle_event(SDL_Event *event, int *running)
{
    UiAction a = {UI_NONE, 0.0, 0};

    switch (event->type) {
    case SDL_QUIT:
        *running = 0;
        break;

    case SDL_TEXTINPUT:
        if (search_active) {
            size_t used = strlen(search_query);
            size_t incoming = strlen(event->text.text);
            if (used + incoming < sizeof(search_query)) {
                memcpy(search_query + used, event->text.text, incoming + 1);
                view_dirty = 1;
                scroll = 0;
            }
        }
        break;

    case SDL_KEYDOWN:
        if (search_active) {
            SDL_Keycode key = event->key.keysym.sym;
            if (key == SDLK_ESCAPE || key == SDLK_RETURN || key == SDLK_KP_ENTER) {
                set_search_active(0);
            } else if (key == SDLK_BACKSPACE) {
                erase_last_utf8_character(search_query);
                view_dirty = 1;
                scroll = 0;
            } else if (key == SDLK_a && (event->key.keysym.mod & KMOD_CTRL)) {
                search_query[0] = '\0';
                view_dirty = 1;
                scroll = 0;
            }
            break;
        }

        switch (event->key.keysym.sym) {
        case SDLK_ESCAPE: *running = 0; break;
        case SDLK_SPACE:  a.type = UI_PLAY_PAUSE; break;
        case SDLK_RIGHT:  a.type = UI_NEXT; break;
        case SDLK_LEFT:   a.type = UI_PREV; break;
        case SDLK_UP:
            a.type = UI_SET_VOLUME;
            a.value = clampd(cache_volume + 0.05, 0.0, 1.0);
            break;
        case SDLK_DOWN:
            a.type = UI_SET_VOLUME;
            a.value = clampd(cache_volume - 0.05, 0.0, 1.0);
            break;
        default: break;
        }
        break;

    case SDL_MOUSEWHEEL:
        if (point_in(&R_LIST, mouse_x, mouse_y)) {
            scroll -= event->wheel.y;
            if (scroll < 0) scroll = 0;
        }
        break;

    case SDL_MOUSEMOTION:
        mouse_x = event->motion.x;
        mouse_y = event->motion.y;
        if (drag_seek) {
            seek_preview = bar_fraction(&R_PROGRESS, mouse_x);
        }
        if (drag_volume) {
            a.type = UI_SET_VOLUME;
            a.value = bar_fraction(&R_VOLUME, mouse_x);
        }
        break;

    case SDL_MOUSEBUTTONDOWN:
        if (event->button.button != SDL_BUTTON_LEFT) break;
        mouse_x = event->button.x;
        mouse_y = event->button.y;

        if (!point_in(&B_SEARCH, mouse_x, mouse_y)) {
            set_search_active(0);
        }

        if (point_in(&B_SEARCH, mouse_x, mouse_y)) {
            set_search_active(1);
        } else if (point_in(&B_SORT, mouse_x, mouse_y)) {
            sort_descending = !sort_descending;
            view_dirty = 1;
            scroll = 0;
        } else if (point_in(&B_RESCAN, mouse_x, mouse_y)) {
            a.type = UI_RESCAN;
        } else if (point_in(&B_PLAY, mouse_x, mouse_y)) {
            a.type = UI_PLAY_PAUSE;
        } else if (point_in(&B_PREV, mouse_x, mouse_y)) {
            a.type = UI_PREV;
        } else if (point_in(&B_NEXT, mouse_x, mouse_y)) {
            a.type = UI_NEXT;
        } else if (cache_duration > 0.0 && point_in_bar(&R_PROGRESS, mouse_x, mouse_y)) {
            drag_seek = 1;
            seek_preview = bar_fraction(&R_PROGRESS, mouse_x);
        } else if (point_in_bar(&R_VOLUME, mouse_x, mouse_y)) {
            drag_volume = 1;
            a.type = UI_SET_VOLUME;
            a.value = bar_fraction(&R_VOLUME, mouse_x);
        } else if (point_in(&R_LIST, mouse_x, mouse_y)
                   && mouse_x < R_LIST.x + R_LIST.w - 18) {
            rebuild_view();
            int shown = scroll + (mouse_y - R_LIST.y) / ROW_H;
            if (shown >= 0 && shown < view_count) {
                a.type = UI_SELECT_TRACK;
                a.index = view_indices[shown];
            }
        }
        break;

    case SDL_MOUSEBUTTONUP:
        if (event->button.button == SDL_BUTTON_LEFT) {
            if (drag_seek) {
                a.type = UI_SEEK;
                a.value = seek_preview * cache_duration;
            }
            drag_seek = 0;
            drag_volume = 0;
        }
        break;

    default:
        break;
    }

    return a;
}

static void draw_playlist(const UiState *s)
{
    char num[16];
    int visible = R_LIST.h / ROW_H;

    fill_rect(&R_LISTPANEL, COL_PANEL);
    draw_text(font, "PLAYLIST", R_LISTPANEL.x + 28, R_LISTPANEL.y + 13, COL_DIM);

    if (view_count == 0) {
        const char *message = cache_count == 0
            ? "No tracks found."
            : "No tracks match your search.";
        draw_text_fit(font, message, R_LIST.x + 28, R_LIST.y + 16,
                      R_LIST.w - 70, 0, COL_DIM);
        return;
    }

    for (int i = 0; i < visible; i++) {
        int view_pos = scroll + i;
        if (view_pos >= view_count) break;

        int idx = view_indices[view_pos];
        SDL_Rect row = {R_LIST.x, R_LIST.y + i * ROW_H, R_LIST.w - 18, ROW_H - 3};
        int is_current = (idx == s->current_track);
        int hover = point_in(&row, mouse_x, mouse_y);

        if (is_current) {
            fill_rect(&row, COL_ROW);
            SDL_Rect marker = {row.x, row.y, 5, row.h};
            fill_rect(&marker, COL_ACCENT);
        } else if (hover) {
            fill_rect(&row, (SDL_Color){44, 44, 52, 255});
        }

        snprintf(num, sizeof(num), "%02d", view_pos + 1);
        draw_text(font_small, num, row.x + 20, row.y + 10, COL_DIM);
        draw_text_fit(font, cache_tracks[idx], row.x + 84, row.y + 8,
                      row.w - 104, 0, is_current ? COL_ACCENT : COL_TEXT);
    }

    if (view_count > visible) {
        SDL_Rect track = {R_LIST.x + R_LIST.w - 10, R_LIST.y, 8, R_LIST.h};
        int thumb_h = R_LIST.h * visible / view_count;
        if (thumb_h < 40) thumb_h = 40;
        int range = R_LIST.h - thumb_h;
        int max_scroll = view_count - visible;
        SDL_Rect thumb = {track.x, R_LIST.y + range * scroll / max_scroll, 8, thumb_h};
        fill_rect(&track, COL_ROW);
        fill_rect(&thumb, COL_TRACK);
    }
}

static void draw_toolbar(const UiState *s)
{
    char count_text[64];
    fill_rect(&R_TOPBAR, COL_PANEL);

    SDL_Rect accent = {R_TOPBAR.x, R_TOPBAR.y, 6, R_TOPBAR.h};
    fill_rect(&accent, COL_ACCENT);
    draw_text(font_small, "MUSIC LIBRARY", 92, 55, COL_DIM);

    fill_rect(&B_SEARCH, COL_BG);
    set_color(search_active ? COL_ACCENT : COL_TRACK);
    SDL_RenderDrawRect(renderer, &B_SEARCH);
    if (search_query[0] == '\0') {
        draw_text(font_small, "Search tracks...", B_SEARCH.x + 15, B_SEARCH.y + 7, COL_DIM);
    } else {
        draw_text_fit(font_small, search_query, B_SEARCH.x + 15, B_SEARCH.y + 7,
                      B_SEARCH.w - 32, 0, COL_TEXT);
    }
    if (search_active) {
        int cursor_x = B_SEARCH.x + 16 + text_width(font_small, search_query);
        if (cursor_x > B_SEARCH.x + B_SEARCH.w - 12) cursor_x = B_SEARCH.x + B_SEARCH.w - 12;
        set_color(COL_ACCENT);
        SDL_RenderDrawLine(renderer, cursor_x, B_SEARCH.y + 9,
                           cursor_x, B_SEARCH.y + B_SEARCH.h - 9);
    }

    int hover_sort = point_in(&B_SORT, mouse_x, mouse_y);

    fill_rect(&B_SORT, hover_sort ? (SDL_Color){58, 58, 68, 255} : COL_ROW);
    set_color(COL_TRACK);
    SDL_RenderDrawRect(renderer, &B_SORT);
    draw_text_fit(font_small, sort_descending ? "Sort: Z-A" : "Sort: A-Z",
                  B_SORT.x + B_SORT.w / 2, B_SORT.y + 7,
                  B_SORT.w - 12, 1, COL_TEXT);

    int hover_rescan = point_in(&B_RESCAN, mouse_x, mouse_y);

    fill_rect(&B_RESCAN, hover_rescan ? (SDL_Color){58, 58, 68, 255} : COL_ROW);
    set_color(COL_TRACK);
    SDL_RenderDrawRect(renderer, &B_RESCAN);
    draw_text_fit(font_small, "Rescan folder",
                  B_RESCAN.x + B_RESCAN.w / 2, B_RESCAN.y + 7,
                  B_RESCAN.w - 12, 1, COL_TEXT);

    fill_rect(&R_TRACKCOUNT, (SDL_Color){30, 30, 35, 255});
    snprintf(count_text, sizeof(count_text), "%d / %d tracks", view_count, s->track_count);
    draw_text_fit(font_small, count_text, R_TRACKCOUNT.x + R_TRACKCOUNT.w / 2,
                  R_TRACKCOUNT.y + 7, R_TRACKCOUNT.w - 18, 1, COL_DIM);
}

static void draw_now_playing(const UiState *s)
{
    const char *title  = (s->title  && s->title[0])  ? s->title  : "No track";
    const char *artist = (s->artist && s->artist[0]) ? s->artist : "Unknown artist";
    draw_text_fit(font_big, title,  LOGICAL_W / 2, 795, 1700, 1, COL_TEXT);
    draw_text_fit(font,     artist, LOGICAL_W / 2, 862, 1700, 1, COL_DIM);
}
static void draw_bar(const SDL_Rect *r, double frac)
{
    SDL_Rect fill = {r->x, r->y, (int)(r->w * frac), r->h};
    fill_rect(r, COL_TRACK);
    fill_rect(&fill, COL_ACCENT);
    fill_circle(r->x + fill.w, r->y + r->h / 2, 12, COL_TEXT);
}
static void draw_controls(const UiState *s)
{
    char buf[32];
    double frac = 0.0;
    if (drag_seek) {
        frac = seek_preview;
    } else if (s->duration > 0.0) {
        frac = clampd(s->position / s->duration, 0.0, 1.0);
    }
    double shown = frac * s->duration;
    draw_bar(&R_PROGRESS, frac);
    format_time(shown, buf, sizeof(buf), 0);
    draw_text(font_small, buf, 480, R_PROGRESS.y - 8, COL_DIM);
    format_time(s->duration - shown, buf, sizeof(buf), 1);
    draw_text_fit(font_small, buf, 1400, R_PROGRESS.y - 8, 100, 2, COL_DIM);
    int hover_prev = point_in(&B_PREV, mouse_x, mouse_y);
    int hover_next = point_in(&B_NEXT, mouse_x, mouse_y);
    int hover_play = point_in(&B_PLAY, mouse_x, mouse_y);
    int cx, cy;
    cx = B_PREV.x + B_PREV.w / 2;
    cy = B_PREV.y + B_PREV.h / 2;
    SDL_Color c = hover_prev ? COL_ACCENT : COL_TEXT;
    SDL_Rect bar_l = {cx - 19, cy - 17, 7, 34};
    fill_rect(&bar_l, c);
    fill_triangle(cx - 9, cy - 17, 28, 34, 0, c);
    cx = B_NEXT.x + B_NEXT.w / 2;
    cy = B_NEXT.y + B_NEXT.h / 2;
    c = hover_next ? COL_ACCENT : COL_TEXT;
    SDL_Rect bar_r = {cx + 12, cy - 17, 7, 34};
    fill_triangle(cx - 19, cy - 17, 28, 34, 1, c);
    fill_rect(&bar_r, c);
    cx = B_PLAY.x + B_PLAY.w / 2;
    cy = B_PLAY.y + B_PLAY.h / 2;
    fill_circle(cx, cy, 48, hover_play ? COL_ACC_HI : COL_ACCENT);
    if (s->is_playing) {
        SDL_Rect p1 = {cx - 16, cy - 22, 11, 44};
        SDL_Rect p2 = {cx + 5,  cy - 22, 11, 44};
        fill_rect(&p1, COL_BG);
        fill_rect(&p2, COL_BG);
    } else {
        fill_triangle(cx - 14, cy - 23, 40, 46, 1, COL_BG);
    }
    double vol = clampd(s->volume, 0.0, 1.0);
    draw_text(font_small, "Vol", 1460, R_VOLUME.y - 8, COL_DIM);
    draw_bar(&R_VOLUME, vol);
    snprintf(buf, sizeof(buf), "%d%%", (int)(vol * 100.0 + 0.5));
    draw_text_fit(font_small, buf, 1860, R_VOLUME.y - 8, 90, 2, COL_DIM);
}
void ui_draw(const UiState *s)
{
    int tracks_changed = (cache_tracks != s->tracks || cache_count != s->track_count);
    cache_tracks = s->tracks;
    cache_count = s->track_count;
    cache_duration = s->duration;
    cache_volume = s->volume;
    if (tracks_changed) view_dirty = 1;
    rebuild_view();

    int visible = R_LIST.h / ROW_H;
    int max_scroll = view_count > visible ? view_count - visible : 0;

    if (s->current_track != last_current) {
        last_current = s->current_track;
        for (int i = 0; i < view_count; i++) {
            if (view_indices[i] == s->current_track) {
                if (i < scroll) scroll = i;
                else if (i >= scroll + visible) scroll = i - visible + 1;
                break;
            }
        }
    }

    if (scroll > max_scroll) scroll = max_scroll;
    if (scroll < 0) scroll = 0;

    set_color(COL_BG);
    SDL_RenderClear(renderer);

    draw_toolbar(s);
    draw_playlist(s);
    draw_now_playing(s);
    draw_controls(s);

    SDL_RenderPresent(renderer);
}