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

#define LIBRARY_MAGIC 0x5836384Cu /* X68L */
#define LIBRARY_VERSION 1u
#define LIBRARY_HEADER_SIZE 8u
#define LIBRARY_CRC_SIZE 4u
#define LIBRARY_MAX_ENCODED (LIBRARY_HEADER_SIZE + X68000_LIBRARY_MAX_GAMES * (5u + X68000_GAME_TITLE_MAX + X68000_GAME_PATH_MAX + X68000_COVER_PATH_MAX) + LIBRARY_CRC_SIZE)

static void lib_put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v;
}
static uint16_t lib_get16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
static void lib_put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}
static uint32_t lib_get32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}
static uint32_t lib_crc32(const uint8_t *p, size_t n)
{
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;
    for (i = 0; i < n; ++i) {
        unsigned bit;
        crc ^= p[i];
        for (bit = 0; bit < 8u; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
    }
    return ~crc;
}
size_t x68000_library_encoded_max_size(void) { return (size_t)LIBRARY_MAX_ENCODED; }

int x68000_library_encode(const x68000_library_t *library, uint8_t *out, size_t capacity)
{
    size_t at = LIBRARY_HEADER_SIZE, i;
    if (!library || !out || library->count > X68000_LIBRARY_MAX_GAMES ||
        capacity < LIBRARY_HEADER_SIZE + LIBRARY_CRC_SIZE) return -1;
    lib_put32(out, LIBRARY_MAGIC);
    out[4] = 0u; out[5] = LIBRARY_VERSION;
    lib_put16(out + 6, (uint16_t)library->count);
    for (i = 0; i < library->count; ++i) {
        const x68000_game_entry_t *e = &library->entries[i];
        size_t title_len = strlen(e->title), path_len = strlen(e->game_path);
        size_t cover_len = strlen(e->cover_path);
        size_t need = 5u + title_len + path_len + cover_len;
        if (title_len == 0u || title_len >= X68000_GAME_TITLE_MAX ||
            path_len == 0u || path_len >= X68000_GAME_PATH_MAX ||
            cover_len >= X68000_COVER_PATH_MAX || e->favorite > 1u ||
            at > capacity || capacity - at < LIBRARY_CRC_SIZE || need > capacity - at - LIBRARY_CRC_SIZE) return -2;
        out[at++] = (uint8_t)title_len;
        out[at++] = (uint8_t)(path_len >> 8);
        out[at++] = (uint8_t)path_len;
        out[at++] = (uint8_t)cover_len;
        out[at++] = e->favorite;
        memcpy(out + at, e->title, title_len); at += title_len;
        memcpy(out + at, e->game_path, path_len); at += path_len;
        memcpy(out + at, e->cover_path, cover_len); at += cover_len;
    }
    if (at + LIBRARY_CRC_SIZE > capacity) return -2;
    lib_put32(out + at, lib_crc32(out, at));
    return (int)(at + LIBRARY_CRC_SIZE);
}

int x68000_library_decode(x68000_library_t *library, const uint8_t *in, size_t length)
{
    size_t at = LIBRARY_HEADER_SIZE, i, count;
    if (!library || !in || length < LIBRARY_HEADER_SIZE + LIBRARY_CRC_SIZE ||
        length > LIBRARY_MAX_ENCODED) return -1;
    if (lib_get32(in) != LIBRARY_MAGIC || in[4] != 0u || in[5] != LIBRARY_VERSION)
        return -2;
    count = lib_get16(in + 6);
    if (count > X68000_LIBRARY_MAX_GAMES ||
        lib_get32(in + length - LIBRARY_CRC_SIZE) != lib_crc32(in, length - LIBRARY_CRC_SIZE))
        return -3;
    /* First pass validates every record and duplicate path without changing library. */
    for (i = 0; i < count; ++i) {
        size_t title_len, path_len, cover_len, j, prior = LIBRARY_HEADER_SIZE;
        if (at + 5u > length - LIBRARY_CRC_SIZE) return -4;
        title_len = in[at++];
        path_len = ((size_t)in[at] << 8) | in[at + 1u]; at += 2u;
        cover_len = in[at++];
        if (in[at++] > 1u || title_len == 0u || title_len >= X68000_GAME_TITLE_MAX ||
            path_len == 0u || path_len >= X68000_GAME_PATH_MAX ||
            cover_len >= X68000_COVER_PATH_MAX ||
            title_len + path_len + cover_len > length - LIBRARY_CRC_SIZE - at) return -4;
        for (j = 0; j < title_len; ++j) if (in[at + j] == 0u) return -4;
        for (j = 0; j < path_len; ++j) if (in[at + title_len + j] == 0u) return -4;
        for (j = 0; j < cover_len; ++j) if (in[at + title_len + path_len + j] == 0u) return -4;
        /* Compare path bytes against each earlier record to reject duplicate entries. */
        {
            size_t k;
            for (k = 0; k < i; ++k) {
                size_t old_title = in[prior++], old_path = ((size_t)in[prior] << 8) | in[prior + 1u];
                size_t old_cover;
                prior += 2u; old_cover = in[prior++];
                prior++; /* favorite */
                if (old_path == path_len &&
                    memcmp(in + prior + old_title, in + at + title_len, path_len) == 0) return -5;
                prior += old_title + old_path + old_cover;
            }
        }
        at += title_len + path_len + cover_len;
    }
    if (at != length - LIBRARY_CRC_SIZE) return -4;
    /* Validation passed; now rebuild destination without further failure points. */
    x68000_library_init(library);
    at = LIBRARY_HEADER_SIZE;
    for (i = 0; i < count; ++i) {
        size_t title_len = in[at++], path_len = ((size_t)in[at] << 8) | in[at + 1u];
        size_t cover_len;
        at += 2u; cover_len = in[at++];
        library->entries[i].favorite = in[at++];
        memcpy(library->entries[i].title, in + at, title_len); at += title_len;
        library->entries[i].title[title_len] = '\\0';
        memcpy(library->entries[i].game_path, in + at, path_len); at += path_len;
        library->entries[i].game_path[path_len] = '\\0';
        memcpy(library->entries[i].cover_path, in + at, cover_len); at += cover_len;
        library->entries[i].cover_path[cover_len] = '\\0';
    }
    library->count = count;
    return 0;
}
