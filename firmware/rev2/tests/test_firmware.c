#include "mpsi_protocol.h"
#include "mpsi_storage.h"
#include "mpsi_tape.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    bool hp, fs, usb, idle, released, eof;
    char fail_at, log[128];
    unsigned log_len, reads, writes, files;
} mock;
static bool event(mock *m, char c)
{
    assert(m->log_len + 1 < sizeof m->log);
    m->log[m->log_len++] = c; m->log[m->log_len] = 0;
    return m->fail_at != c;
}
static void hp_run(void *ctx, bool run)
{
    mock *m = ctx;
    if (run) assert(m->fs && !m->usb);
    m->hp = run; (void)event(m, run ? '1' : '0');
}
static bool quiet(void *ctx) { return ((mock *)ctx)->idle; }
static bool prepare(void *ctx) { return event(ctx, 'P'); }
static bool ready(void *ctx)
{
    mock *m = ctx; assert(m->hp && m->fs && !m->usb);
    return event(m, 'A');
}
static bool mount_fs(void *ctx)
{
    mock *m = ctx; assert(!m->usb && !m->hp);
    if (!event(m, 'M')) return false;
    m->fs = true; return true;
}
static bool flush_close(void *ctx)
{
    mock *m = ctx; assert(m->fs && !m->usb && !m->hp);
    return event(m, 'F');
}
static bool sync_fs(void *ctx)
{
    mock *m = ctx; assert(m->fs && !m->usb && !m->hp);
    return event(m, 'S');
}
static bool unmount_fs(void *ctx)
{
    mock *m = ctx; assert(m->fs && !m->usb && !m->hp);
    if (!event(m, 'U')) return false;
    m->fs = false; return true;
}
static bool usb_start(void *ctx)
{
    mock *m = ctx; assert(!m->fs && !m->hp);
    m->usb = true; return event(m, 'X');
}
static bool host_released(void *ctx) { return ((mock *)ctx)->released; }
static bool usb_stop(void *ctx)
{
    mock *m = ctx; assert(!m->fs && !m->hp);
    if (!event(m, 'T')) return false;
    m->usb = false; return true;
}
static bool block_read(void *ctx, uint32_t lba, uint8_t *data, uint32_t count)
{
    mock *m = ctx; assert(m->usb && !m->fs && !m->hp);
    (void)lba; (void)count; data[0] = 0x5a; ++m->reads; return true;
}
static bool block_write(void *ctx, uint32_t lba, const uint8_t *data, uint32_t count)
{
    mock *m = ctx; assert(m->usb && !m->fs && !m->hp);
    (void)lba; (void)data; (void)count; ++m->writes; return true;
}
static bool file_read(void *ctx, uint8_t *byte, bool *eof)
{
    mock *m = ctx; assert(m->fs && !m->usb && m->hp);
    ++m->files; *byte = 'A'; *eof = m->eof; return event(m, 'R');
}
static bool file_append(void *ctx, uint8_t byte)
{
    mock *m = ctx; assert(m->fs && !m->usb && m->hp);
    assert(byte == 'B'); ++m->files; return event(m, 'W');
}
static mpsi_storage_port port(mock *m)
{
    return (mpsi_storage_port){m, hp_run, quiet, prepare, ready, mount_fs, flush_close,
        sync_fs, unmount_fs, usb_start, host_released, usb_stop,
        block_read, block_write, file_read, file_append};
}
static void clear_log(mock *m) { m->log_len = 0; m->log[0] = 0; }

typedef struct { uint16_t cell[80]; unsigned busy; } image;
static mpsi_result cell_read(void *ctx, uint32_t index, uint16_t *value)
{
    image *i = ctx; assert(index < 80);
    if (i->busy) { --i->busy; return MPSI_BUSY; }
    *value = i->cell[index]; return MPSI_OK;
}
static mpsi_result cell_write(void *ctx, uint32_t index, uint16_t value)
{
    image *i = ctx; assert(index < 80);
    if (i->busy) { --i->busy; return MPSI_BUSY; }
    i->cell[index] = value; return MPSI_OK;
}
static uint16_t tape_command(mpsi_tape *t, uint16_t request)
{
    uint16_t response = 0;
    assert(mpsi_tape_begin(t, request));
    assert(mpsi_tape_poll(t, &response) == MPSI_OK);
    return response;
}
static void test_tape(mpsi_storage *s)
{
    image image_data = {.cell = {0x11, 0x13c, 0x22, 0x13c, 0x33}};
    mpsi_tape_media media = { &image_data, 5, true, true, cell_read, cell_write };
    mpsi_tape t;
    uint16_t response;
    mpsi_tape_init(&t, s, media);
    assert(tape_command(&t, 0x9000) == (MPSI_ACK | MPSI_CFI | 0x11));
    assert(tape_command(&t, 0x9700) == (MPSI_ACK | MPSI_CFI | 0x3c));
    assert(tape_command(&t, 0x9800) == (MPSI_ACK | MPSI_CFI | MPSI_SSI | 0x3c));
    assert(t.position == 4);
    assert(tape_command(&t, 0x9a00) == (MPSI_ACK | MPSI_CFI | MPSI_SSI | 0x3c));
    assert(t.position == 2);
    assert(tape_command(&t, 0x9b00) == (MPSI_ACK | MPSI_CFI | MPSI_SSI | 0x3c));
    assert(t.position == 0);
    assert(tape_command(&t, 0x9600) == (MPSI_ACK | MPSI_CFI | MPSI_SSI | MPSI_LEADER));
    assert(tape_command(&t, 0x9500) == (MPSI_ACK | MPSI_LEADER));
    assert(tape_command(&t, 0x9300) == (MPSI_ACK | MPSI_LEADER));
    image_data.busy = 1;
    assert(mpsi_tape_begin(&t, 0x9c3c));
    assert(mpsi_tape_poll(&t, &response) == MPSI_BUSY && t.position == -1);
    assert(!mpsi_tape_begin(&t, 0x9500));
    assert(mpsi_tape_poll(&t, &response) == MPSI_OK && image_data.cell[0] == 0x13c);
    assert(t.position == 1);
    t.media.writable = false;
    assert(tape_command(&t, 0x9444) == (MPSI_ACK | MPSI_CFI | MPSI_SSI | MPSI_WRITE_PROTECTED));
    t.media.present = false;
    assert(tape_command(&t, 0x9000) == (MPSI_ACK | MPSI_CFI | MPSI_SSI |
                                       MPSI_LEADER | MPSI_WRITE_PROTECTED | MPSI_NO_CASSETTE));
    memset(&image_data, 0, sizeof image_data);
    media.cells = 80; image_data.cell[70] = 0x13c;
    mpsi_tape_init(&t, s, media);
    assert(mpsi_tape_begin(&t, 0x9800));
    assert(mpsi_tape_poll(&t, &response) == MPSI_BUSY && t.scan == 32);
    assert(mpsi_tape_poll(&t, &response) == MPSI_BUSY && t.scan == 64);
    assert(mpsi_tape_poll(&t, &response) == MPSI_OK && t.position == 71);
    s->mode = MPSI_MSC;
    assert(!mpsi_tape_begin(&t, 0x9000));
    assert(mpsi_tape_poll(&t, &response) == MPSI_ERROR);
    s->mode = MPSI_SERVING;
}
int main(void)
{
    mock m = {.idle = true};
    mpsi_storage s;
    uint8_t buffer[512] = {0};
    uint16_t response;
    mpsi_config config = {8, 9, 3, true};
    assert(mpsi_config_valid(config) && mpsi_config_word(config) == 0x0798);
    config.tp_address = 8; assert(!mpsi_config_valid(config));
    config.tp_address = 10; assert(!mpsi_config_valid(config));
    config.tp_address = 15; assert(!mpsi_config_valid(config));
    config.tp_address = 0; config.printer_enabled = false;
    assert(mpsi_config_valid(config) && mpsi_config_word(config) == 0x0608);
    config.gp_address = 0; config.tp_address = 9;
    assert(mpsi_config_valid(config) && mpsi_config_word(config) == 0x0690);
    config.irq_selector = 5; assert(!mpsi_config_valid(config));
    assert(mpsi_request_address(0x985a) == 9 && mpsi_request_command(0x985a) == 8);
    assert(mpsi_general_input(0x885a) && !mpsi_general_input(0x805a));
    assert(mpsi_storage_init(&s, port(&m), 100) == MPSI_OK && m.hp);
    assert(!mpsi_msc_read(&s, 0, buffer, 1));
    assert(mpsi_general_read(&s, &response) && response == (MPSI_ACK | 'A'));
    m.eof = true; assert(mpsi_general_read(&s, &response) && response == MPSI_ACK);
    assert(mpsi_general_write(&s, 'B', &response) && response == MPSI_ACK);
    test_tape(&s);
    clear_log(&m); m.idle = false;
    assert(mpsi_enter_msc(&s) == MPSI_BUSY && m.hp && m.log_len == 0);
    m.idle = true; assert(mpsi_enter_msc(&s) == MPSI_OK);
    assert(strcmp(m.log, "0FSUX") == 0 && !m.hp && !s.mounted && s.usb_owned);
    unsigned files = m.files;
    assert(!mpsi_general_read(&s, &response) && !mpsi_general_write(&s, 'B', &response));
    assert(m.files == files);
    assert(mpsi_msc_read(&s, 99, buffer, 1) && buffer[0] == 0x5a);
    assert(mpsi_msc_write(&s, 0, buffer, 1));
    assert(!mpsi_msc_read(&s, 100, buffer, 1));
    assert(!mpsi_msc_read(&s, 99, buffer, 2));
    assert(!mpsi_msc_write(&s, 1, buffer, UINT32_MAX));
    assert(!mpsi_msc_read(&s, 0, buffer, 0));
    assert(!mpsi_msc_read(&s, 0, NULL, 1));
    assert(mpsi_leave_msc(&s) == MPSI_BUSY && s.mode == MPSI_MSC);
    clear_log(&m); m.released = true;
    assert(mpsi_leave_msc(&s) == MPSI_OK && strcmp(m.log, "TMP1A") == 0);
    assert(m.hp && s.mounted && !s.usb_owned && !m.usb);
    assert(!mpsi_msc_write(&s, 0, buffer, 1));

    // Inject failure at every storage/USB handover step. HP stays disabled.
    const char *entry_failures = "FSUX";
    for (const char *p = entry_failures; *p; ++p) {
        m = (mock){.idle = true};
        assert(mpsi_storage_init(&s, port(&m), 100) == MPSI_OK);
        m.fail_at = *p;
        assert(mpsi_enter_msc(&s) == MPSI_ERROR && s.mode == MPSI_FAULT && !m.hp);
        assert(!mpsi_msc_read(&s, 0, buffer, 1));
        assert(!mpsi_general_read(&s, &response));
        assert(!m.usb);
    }
    const char *exit_failures = "TMPA";
    for (const char *p = exit_failures; *p; ++p) {
        m = (mock){.idle = true, .released = true};
        assert(mpsi_storage_init(&s, port(&m), 100) == MPSI_OK);
        assert(mpsi_enter_msc(&s) == MPSI_OK);
        m.fail_at = *p;
        assert(mpsi_leave_msc(&s) == MPSI_ERROR && s.mode == MPSI_FAULT && !m.hp);
        assert(!mpsi_msc_read(&s, 0, buffer, 1));
    }
    m = (mock){.idle = true};
    assert(mpsi_storage_init(&s, port(&m), 100) == MPSI_OK);
    m.fail_at = 'W';
    assert(!mpsi_general_write(&s, 'B', &response) && s.mode == MPSI_FAULT && !m.hp);
    puts("PASS: firmware protocol, tape commands/cache retries, MSC ownership and failure paths");
    return 0;
}
