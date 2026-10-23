#include "x68000_frontend.h"
#include <string.h>

void x68000_frontend_init(x68000_frontend_t *ui, uint32_t splash_frames)
{
    if (!ui) return;
    memset(ui, 0, sizeof(*ui));
    ui->screen = X68000_SCREEN_SPLASH;
    ui->splash_frames_remaining = splash_frames;
}

void x68000_frontend_tick(x68000_frontend_t *ui)
{
    if (!ui || ui->screen != X68000_SCREEN_SPLASH) return;
    if (ui->splash_frames_remaining > 0u) --ui->splash_frames_remaining;
    if (ui->splash_frames_remaining == 0u) ui->screen = X68000_SCREEN_LIBRARY;
}

void x68000_frontend_open_menu(x68000_frontend_t *ui)
{
    if (!ui) return;
    ui->selected_menu_item = X68000_MENU_LIBRARY;
    ui->screen = X68000_SCREEN_LIBRARY;
    ui->show_favorites_only = 0u;
}

void x68000_frontend_move_menu(x68000_frontend_t *ui, int direction)
{
    if (!ui || direction == 0) return;
    if (direction > 0)
        ui->selected_menu_item = (ui->selected_menu_item + 1u) % X68000_MENU_COUNT;
    else
        ui->selected_menu_item = (ui->selected_menu_item + X68000_MENU_COUNT - 1u) % X68000_MENU_COUNT;
}

void x68000_frontend_activate_menu(x68000_frontend_t *ui)
{
    if (!ui) return;
    switch (ui->selected_menu_item) {
    case X68000_MENU_LIBRARY:
        ui->screen = X68000_SCREEN_LIBRARY;
        ui->show_favorites_only = 0u;
        break;
    case X68000_MENU_FAVORITES:
        ui->screen = X68000_SCREEN_FAVORITES;
        ui->show_favorites_only = 1u;
        break;
    case X68000_MENU_SETTINGS:
        ui->screen = X68000_SCREEN_SETTINGS;
        break;
    case X68000_MENU_QUIT:
    default:
        /* The platform-specific shell owns process exit. */
        break;
    }
}

void x68000_frontend_move_game(x68000_frontend_t *ui,
                               const x68000_library_t *library, int direction)
{
    size_t count;
    if (!ui || !library || direction == 0) return;
    count = library->count;
    if (ui->show_favorites_only) {
        count = x68000_library_favorite_count(library);
        if (count == 0u) { ui->selected_game = 0u; return; }
        ui->selected_game = direction > 0
            ? (ui->selected_game + 1u) % count
            : (ui->selected_game + count - 1u) % count;
    } else {
        if (count == 0u) { ui->selected_game = 0u; return; }
        ui->selected_game = direction > 0
            ? (ui->selected_game + 1u) % count
            : (ui->selected_game + count - 1u) % count;
    }
}

const x68000_game_entry_t *x68000_frontend_selected_game(
    const x68000_frontend_t *ui, const x68000_library_t *library)
{
    size_t i, visible = 0u;
    if (!ui || !library) return 0;
    for (i = 0; i < library->count; ++i) {
        if (ui->show_favorites_only && !library->entries[i].favorite) continue;
        if (visible == ui->selected_game) return &library->entries[i];
        ++visible;
    }
    return 0;
}
