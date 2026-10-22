#ifndef SHARP68000_LIBRARY_H
#define SHARP68000_LIBRARY_H

#include <stddef.h>
#include <stdint.h>

#define X68000_LIBRARY_MAX_GAMES 256u
#define X68000_GAME_TITLE_MAX 96u
#define X68000_GAME_PATH_MAX 256u
#define X68000_COVER_PATH_MAX 256u

/* Default storage layout. A later filesystem backend must verify these paths
 * against the mounted Xbox 360 device before attempting I/O. */
#define X68000_PATH_ROMS   "Hdd1:/Emulators/Sharp68000Box360/ROMs"
#define X68000_PATH_COVERS "Hdd1:/Emulators/Sharp68000Box360/Covers"
#define X68000_PATH_STATES "Hdd1:/Emulators/Sharp68000Box360/States"
#define X68000_PATH_SAVES  "Hdd1:/Emulators/Sharp68000Box360/Saves"

typedef struct {
    char title[X68000_GAME_TITLE_MAX];
    char game_path[X68000_GAME_PATH_MAX];
    char cover_path[X68000_COVER_PATH_MAX];
    uint8_t favorite;
} x68000_game_entry_t;

typedef struct {
    x68000_game_entry_t entries[X68000_LIBRARY_MAX_GAMES];
    size_t count;
} x68000_library_t;

void x68000_library_init(x68000_library_t *library);
int x68000_library_add(x68000_library_t *library, const char *title,
                       const char *game_path, const char *cover_path);
int x68000_library_remove(x68000_library_t *library, const char *game_path);
int x68000_library_set_favorite(x68000_library_t *library,
                                const char *game_path, int favorite);
int x68000_library_toggle_favorite(x68000_library_t *library,
                                   const char *game_path);
const x68000_game_entry_t *x68000_library_find(const x68000_library_t *library,
                                                const char *game_path);
size_t x68000_library_favorite_count(const x68000_library_t *library);

#endif
