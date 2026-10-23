#ifndef SHARP68000_FRONTEND_H
#define SHARP68000_FRONTEND_H

#include <stddef.h>
#include <stdint.h>
#include "x68000_library.h"

typedef enum {
    X68000_SCREEN_SPLASH = 0,
    X68000_SCREEN_LIBRARY,
    X68000_SCREEN_FAVORITES,
    X68000_SCREEN_SETTINGS
} x68000_screen_t;

typedef enum {
    X68000_MENU_LIBRARY = 0,
    X68000_MENU_FAVORITES,
    X68000_MENU_SETTINGS,
    X68000_MENU_QUIT,
    X68000_MENU_COUNT
} x68000_menu_item_t;

typedef struct {
    x68000_screen_t screen;
    uint32_t splash_frames_remaining;
    size_t selected_game;
    size_t selected_menu_item;
    uint8_t show_favorites_only;
} x68000_frontend_t;

void x68000_frontend_init(x68000_frontend_t *ui, uint32_t splash_frames);
void x68000_frontend_tick(x68000_frontend_t *ui);
void x68000_frontend_open_menu(x68000_frontend_t *ui);
void x68000_frontend_move_menu(x68000_frontend_t *ui, int direction);
void x68000_frontend_activate_menu(x68000_frontend_t *ui);
void x68000_frontend_move_game(x68000_frontend_t *ui,
                               const x68000_library_t *library, int direction);
const x68000_game_entry_t *x68000_frontend_selected_game(
    const x68000_frontend_t *ui, const x68000_library_t *library);

#endif
