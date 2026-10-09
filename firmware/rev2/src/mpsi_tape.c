#include "mpsi_tape.h"
#include "mpsi_protocol.h"
#include <limits.h>

static bool accessible(const mpsi_tape *t)
{
    return t->owner && t->owner->mode == MPSI_SERVING && t->owner->mounted &&
           !t->owner->usb_owned;
}
void mpsi_tape_init(mpsi_tape *t, mpsi_storage *owner, mpsi_tape_media media)
{
    *t = (mpsi_tape){ .position = -1, .command = 5, .media = media, .owner = owner };
    if (media.cells > INT32_MAX) t->media.present = false;
}
bool mpsi_tape_begin(mpsi_tape *t, uint16_t request)
{
    uint8_t command = mpsi_request_command(request) & 7;
    if (!accessible(t) || t->active) return false;
    if (command < 6) {
        t->command = command;
        t->control = (request & 0x0800) != 0;
    }
    t->byte = request & 255;
    t->active = true;
    if (t->command < 2) t->scan = t->position < 0 ? 0 : t->position;
    else t->scan = t->position >= (int32_t)t->media.cells ?
                   (int32_t)t->media.cells - 1 : t->position;
    return true;
}
static uint16_t status(const mpsi_tape *t)
{
    return MPSI_ACK | (!t->media.present ? MPSI_NO_CASSETTE : 0) |
           (!t->media.writable ? MPSI_WRITE_PROTECTED : 0);
}
static mpsi_result finish(mpsi_tape *t, uint16_t word, uint16_t *response)
{
    t->active = false;
    *response = status(t) | word;
    return MPSI_OK;
}
mpsi_result mpsi_tape_poll(mpsi_tape *t, uint16_t *response)
{
    uint16_t cell;
    mpsi_result result;
    if (!accessible(t) || !t->active || !response) return MPSI_ERROR;
    if (t->command == 5 || (t->command == 3 && !t->control)) {
        if (t->command == 3) t->position = -1; // reverse high speed -> rewind
        if (t->command == 5) t->control = false;
        return finish(t, (t->position < 0 || t->position >= (int32_t)t->media.cells)
                      ? MPSI_LEADER : 0, response);
    }
    if (!t->media.present)
        return finish(t, MPSI_CFI | MPSI_SSI | MPSI_LEADER, response);
    if (t->command == 4) {
        int32_t position = t->position < 0 ? 0 : t->position;
        if (!t->media.writable || position >= (int32_t)t->media.cells)
            return finish(t, MPSI_CFI | MPSI_SSI |
                          (position >= (int32_t)t->media.cells ? MPSI_LEADER : 0), response);
        result = t->media.write_cell(t->media.ctx, (uint32_t)position,
                                    t->byte | (t->control ? MPSI_CONTROL_CELL : 0));
        if (result != MPSI_OK) return result;
        t->position = position + 1;
        return finish(t, MPSI_CFI | t->byte, response);
    }
    // Bound control-mode searches to 32 cached cells per main-loop iteration.
    // BUSY retries retain the current scan position without advancing the tape.
    for (unsigned budget = 0; budget < 32; ++budget) {
        if (t->scan < 0 || t->scan >= (int32_t)t->media.cells) {
            t->position = t->scan;
            return finish(t, MPSI_CFI | MPSI_SSI | MPSI_LEADER, response);
        }
        result = t->media.read_cell(t->media.ctx, (uint32_t)t->scan, &cell);
        if (result != MPSI_OK) return result;
        if (!t->control || (cell & MPSI_CONTROL_CELL)) {
            t->position = t->scan + (t->command < 2 ? 1 : -1);
            return finish(t, MPSI_CFI | (cell & 255) |
                          (t->control && (cell & 255) == 0x3c ? MPSI_SSI : 0), response);
        }
        t->scan += t->command < 2 ? 1 : -1;
    }
    return MPSI_BUSY;
}
