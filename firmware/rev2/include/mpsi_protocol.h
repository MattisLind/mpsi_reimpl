#ifndef MPSI_PROTOCOL_H
#define MPSI_PROTOCOL_H
#include <stdbool.h>
#include <stdint.h>

enum {
    MPSI_ACK = 0x2000, MPSI_CFI = 0x4000, MPSI_SSI = 0x8000,
    MPSI_NO_CASSETTE = 0x0800, MPSI_LEADER = 0x0400,
    MPSI_WRITE_PROTECTED = 0x0200, MPSI_CONTROL_CELL = 0x0100
};
typedef struct {
    uint8_t gp_address, tp_address, irq_selector;
    bool printer_enabled;
} mpsi_config;

static inline bool mpsi_config_valid(mpsi_config c)
{
    /* Validate in firmware; the CPLD shifts configuration without checking it. */
    /* Select code zero disables a port; it is never an HP SC0 decode. */
    return c.gp_address < 16 && c.gp_address != 10 &&
           c.tp_address < 16 && c.tp_address != 10 &&
           (!c.gp_address || !c.tp_address || c.gp_address != c.tp_address) &&
           c.irq_selector < 5 &&
           (!c.printer_enabled || (c.gp_address != 15 && c.tp_address != 15));
}
static inline uint16_t mpsi_config_word(mpsi_config c)
{
    return (uint16_t)(c.gp_address | (c.tp_address << 4) |
                     (c.printer_enabled << 8) | (c.irq_selector << 9));
}
static inline uint8_t mpsi_request_address(uint16_t request) { return request >> 12; }
static inline uint8_t mpsi_request_command(uint16_t request) { return (request >> 8) & 15; }
static inline bool mpsi_general_input(uint16_t request) { return (request & 0x0800) != 0; }
#endif
