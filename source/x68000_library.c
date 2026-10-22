#include "x68000_library.h"
#include <string.h>

static int copy_text(char *dst, size_t capacity, const char *src)
{
    size_t n;
    if (!dst || !src || capacity == 0u) return -1;
    n = strlen(src);
    if (n == 0u || n >= capacity) return -1;
    memcpy(dst, src, n + 1u);
    return 0;
}

static size_t find_index(const x68000_library_t *library, const char *path)
{
    size_t i;
    if (!library || !path) return X68000_LIBRARY_MAX_GAMES;
    for (i = 0; i < library->count; ++i)
        if (strcmp(library->entries[i].game_path, path) == 0) return i;
    return X68000_LIBRARY_MAX_GAMES;
}

void x68000_library_init(x68000_library_t *library)
{
    if (library) memset(library, 0, sizeof(*library));
}

int x68000_library_add(x68000_library_t *library, const char *title,
                       const char *game_path, const char *cover_path)
{
    x68000_game_entry_t entry;
    if (!library || !title || !game_path || library->count >= X68000_LIBRARY_MAX_GAMES)
        return -1;
    if (find_index(library, game_path) != X68000_LIBRARY_MAX_GAMES) return -2;
    memset(&entry, 0, sizeof(entry));
    if (copy_text(entry.title, sizeof(entry.title), title) != 0 ||
        copy_text(entry.game_path, sizeof(entry.game_path), game_path) != 0)
        return -3;
    if (cover_path && cover_path[0] != '\0' &&
        copy_text(entry.cover_path, sizeof(entry.cover_path), cover_path) != 0)
        return -3;
    library->entries[library->count++] = entry;
    return 0;
}

int x68000_library_remove(x68000_library_t *library, const char *game_path)
{
    size_t i = find_index(library, game_path);
    if (!library || i == X68000_LIBRARY_MAX_GAMES) return -1;
    if (i + 1u < library->count)
        memmove(&library->entries[i], &library->entries[i + 1u],
                (library->count - i - 1u) * sizeof(library->entries[0]));
    --library->count;
    memset(&library->entries[library->count], 0, sizeof(library->entries[0]));
    return 0;
}

int x68000_library_set_favorite(x68000_library_t *library,
                                const char *game_path, int favorite)
{
    size_t i = find_index(library, game_path);
    if (!library || i == X68000_LIBRARY_MAX_GAMES) return -1;
    library->entries[i].favorite = favorite ? 1u : 0u;
    return 0;
}

int x68000_library_toggle_favorite(x68000_library_t *library, const char *game_path)
{
    size_t i = find_index(library, game_path);
    if (!library || i == X68000_LIBRARY_MAX_GAMES) return -1;
    library->entries[i].favorite = library->entries[i].favorite ? 0u : 1u;
    return (int)library->entries[i].favorite;
}

const x68000_game_entry_t *x68000_library_find(const x68000_library_t *library,
                                                const char *game_path)
{
    size_t i = find_index(library, game_path);
    return i == X68000_LIBRARY_MAX_GAMES ? 0 : &library->entries[i];
}

size_t x68000_library_favorite_count(const x68000_library_t *library)
{
    size_t i, count = 0u;
    if (!library) return 0u;
    for (i = 0; i < library->count; ++i)
        if (library->entries[i].favorite) ++count;
    return count;
}
