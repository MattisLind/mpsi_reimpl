#ifndef MPSI_STORAGE_H
#define MPSI_STORAGE_H
#include <stdbool.h>
#include <stdint.h>

typedef enum { MPSI_OFF, MPSI_SERVING, MPSI_MSC, MPSI_FAULT } mpsi_mode;
typedef enum { MPSI_OK, MPSI_BUSY, MPSI_ERROR } mpsi_result;
// These hooks must be implemented by the STM32 board port. All operations are
// serialized in the main loop; USB callbacks use mpsi_msc_read/write exclusively.
typedef struct {
    void *ctx;
    void (*hp_run)(void *, bool);
    bool (*hp_quiescent)(void *); // CEO idle, no request/queued work, no active transfer
    bool (*hp_prepare)(void *);   // reset queues, apply config, stage data (flags clear)
    bool (*hp_ready)(void *);     // after HP_RUN rises, send initial ACK/status; CS rising loads flags
    bool (*fs_mount)(void *);
    bool (*fs_flush_close)(void *); // every open file/cache, including tape writes
    bool (*fs_sync)(void *);
    bool (*fs_unmount)(void *);
    bool (*usb_start)(void *);
    bool (*usb_host_released)(void *); // safe eject/disconnect AND no in-flight blocks
    bool (*usb_stop)(void *); // detach and drain callbacks before returning success
    bool (*block_read)(void *, uint32_t, uint8_t *, uint32_t);
    bool (*block_write)(void *, uint32_t, const uint8_t *, uint32_t);
    bool (*file_read)(void *, uint8_t *, bool *eof); // selected paper tape file
    bool (*file_append)(void *, uint8_t); // selected output/printer capture file
} mpsi_storage_port;
typedef struct {
    mpsi_mode mode;
    bool mounted, usb_owned;
    uint32_t block_count;
    mpsi_storage_port port;
} mpsi_storage;

// Required port hooks and block_count>0 are prerequisites of init.
mpsi_result mpsi_storage_init(mpsi_storage *, mpsi_storage_port, uint32_t block_count);
mpsi_result mpsi_enter_msc(mpsi_storage *);
mpsi_result mpsi_leave_msc(mpsi_storage *);
bool mpsi_msc_read(mpsi_storage *, uint32_t lba, uint8_t *, uint32_t blocks);
bool mpsi_msc_write(mpsi_storage *, uint32_t lba, const uint8_t *, uint32_t blocks);
bool mpsi_general_read(mpsi_storage *, uint16_t *response);
bool mpsi_general_write(mpsi_storage *, uint8_t byte, uint16_t *response);
#endif
