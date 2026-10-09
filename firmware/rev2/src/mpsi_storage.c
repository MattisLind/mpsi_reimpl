#include "mpsi_storage.h"
#include "mpsi_protocol.h"
#include <stddef.h>

static mpsi_result fail(mpsi_storage *s)
{
    s->port.hp_run(s->port.ctx, false);
    s->mode = MPSI_FAULT;
    return MPSI_ERROR;
}
mpsi_result mpsi_storage_init(mpsi_storage *s, mpsi_storage_port p, uint32_t count)
{
    *s = (mpsi_storage){ .mode = MPSI_OFF, .block_count = count, .port = p };
    p.hp_run(p.ctx, false);
    if (count == 0 || !p.fs_mount(p.ctx)) return fail(s);
    s->mounted = true;
    if (!p.hp_prepare(p.ctx)) return fail(s);
    p.hp_run(p.ctx, true);
    if (!p.hp_ready(p.ctx)) return fail(s);
    s->mode = MPSI_SERVING;
    return MPSI_OK;
}
mpsi_result mpsi_enter_msc(mpsi_storage *s)
{
    if (s->mode != MPSI_SERVING) return MPSI_ERROR;
    if (!s->port.hp_quiescent(s->port.ctx)) return MPSI_BUSY;
    // hp_quiescent check and HP_RUN deassertion must be atomic with respect to
    // the HP request handler in the board port. The HP itself is asynchronous:
    // the user must initiate handover between HP operations (see issue 011).
    s->port.hp_run(s->port.ctx, false);
    s->mode = MPSI_OFF;
    if (!s->port.fs_flush_close(s->port.ctx) || !s->port.fs_sync(s->port.ctx) ||
        !s->port.fs_unmount(s->port.ctx)) return fail(s);
    s->mounted = false;
    s->usb_owned = true;
    s->mode = MPSI_MSC;
    if (!s->port.usb_start(s->port.ctx)) {
        s->mode = MPSI_OFF; // deny block callbacks before attempting detach
        if (s->port.usb_stop(s->port.ctx)) s->usb_owned = false;
        return fail(s);
    }
    return MPSI_OK;
}
mpsi_result mpsi_leave_msc(mpsi_storage *s)
{
    if (s->mode != MPSI_MSC) return MPSI_ERROR;
    if (!s->port.usb_host_released(s->port.ctx)) return MPSI_BUSY;
    s->mode = MPSI_OFF;
    if (!s->port.usb_stop(s->port.ctx)) return fail(s);
    s->usb_owned = false;
    if (!s->port.fs_mount(s->port.ctx)) return fail(s);
    s->mounted = true;
    if (!s->port.hp_prepare(s->port.ctx)) return fail(s);
    s->port.hp_run(s->port.ctx, true);
    if (!s->port.hp_ready(s->port.ctx)) return fail(s);
    s->mode = MPSI_SERVING;
    return MPSI_OK;
}
static bool block_access(const mpsi_storage *s, uint32_t lba, uint32_t blocks)
{
    // Subtraction avoids overflow when a host supplies a very large block count.
    return s->mode == MPSI_MSC && s->usb_owned && !s->mounted && blocks > 0 &&
           lba < s->block_count && blocks <= s->block_count - lba;
}
bool mpsi_msc_read(mpsi_storage *s, uint32_t lba, uint8_t *buffer, uint32_t blocks)
{
    return buffer && block_access(s, lba, blocks) &&
           s->port.block_read(s->port.ctx, lba, buffer, blocks);
}
bool mpsi_msc_write(mpsi_storage *s, uint32_t lba, const uint8_t *buffer, uint32_t blocks)
{
    return buffer && block_access(s, lba, blocks) &&
           s->port.block_write(s->port.ctx, lba, buffer, blocks);
}
bool mpsi_general_read(mpsi_storage *s, uint16_t *response)
{
    uint8_t byte = 0;
    bool eof = false;
    if (s->mode != MPSI_SERVING || !s->mounted || s->usb_owned || !response)
        return false;
    if (!s->port.file_read(s->port.ctx, &byte, &eof)) { fail(s); return false; }
    *response = MPSI_ACK | (eof ? 0 : byte);
    return true;
}
bool mpsi_general_write(mpsi_storage *s, uint8_t byte, uint16_t *response)
{
    if (s->mode != MPSI_SERVING || !s->mounted || s->usb_owned || !response)
        return false;
    if (!s->port.file_append(s->port.ctx, byte)) { fail(s); return false; }
    *response = MPSI_ACK;
    return true;
}
