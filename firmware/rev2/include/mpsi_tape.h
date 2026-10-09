#ifndef MPSI_TAPE_H
#define MPSI_TAPE_H
#include "mpsi_storage.h"
// A cell is one byte plus its control marker in bit 8. Backend callbacks must be
// bounded/nonblocking and return BUSY on a sector-cache miss. No FatFs calls here.
typedef struct {
    void *ctx;
    uint32_t cells;
    bool present, writable;
    mpsi_result (*read_cell)(void *, uint32_t, uint16_t *);
    mpsi_result (*write_cell)(void *, uint32_t, uint16_t);
} mpsi_tape_media;
typedef struct {
    int32_t position, scan;
    uint8_t command, byte;
    bool control, active;
    mpsi_tape_media media;
    mpsi_storage *owner;
} mpsi_tape;
// Media count must be <= INT32_MAX; prototype writes use a preallocated image.
void mpsi_tape_init(mpsi_tape *, mpsi_storage *, mpsi_tape_media);
bool mpsi_tape_begin(mpsi_tape *, uint16_t request);
mpsi_result mpsi_tape_poll(mpsi_tape *, uint16_t *response);
#endif
