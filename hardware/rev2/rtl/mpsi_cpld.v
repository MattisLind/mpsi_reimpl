`timescale 1ns/1ps
// Rev2 functional prototype. See requirements/03_interface.md for the wire contract.
// No fitter/pin/timing sign-off has been performed. Bus outputs drive only 0 or Z.
module mpsi_cpld #(
    // Three-bit selector maps to eight bus lines. Confirm against HP firmware.
    parameter [3:0] IRQ0 = 4, IRQ1 = 5, IRQ2 = 6, IRQ3 = 7,
                    IRQ4 = 8, IRQ5 = 9, IRQ6 = 10, IRQ7 = 11
) (
    input wire reset_n, hp_run, mck,
    input wire [3:0] co,
    input wire nceo, nsih,
    input wire spi_sck, spi_mosi, spi_cs_n, cfg_frame, commit,
    output wire input_pl_n, request_n,
    output reg overrun,
    output wire [11:0] hp_n_di,
    output wire hp_n_cfi, hp_n_ssi
);
    reg [15:0] shift;
    reg [4:0] bit_count;
    reg [11:0] cfg_register;
    reg configured;
    reg [10:0] response;
    reg ack, cfi, ssi;
    reg consume_toggle;
    reg consume_meta, consume_sync, consume_seen;
    reg enable_meta, enable_sync, enable_prev;
    reg [1:0] capture_phase;
    reg pending;

    wire nGP = !(configured && hp_run && co == cfg_register[3:0]);
    wire nTP = !(configured && hp_run && co == cfg_register[7:4]);
    wire nPR = !(configured && hp_run && cfg_register[8] && co == 4'd15);
    wire general_selected = !nGP || !nPR;
    wire tape_selected = !nTP;
    wire device_enabled = (general_selected || tape_selected) && !nceo;
    wire frame_valid = !spi_cs_n && bit_count == 5'd16;
    wire flags_reset_n = reset_n && hp_run;
    wire ack_reset_n = flags_reset_n && !(general_selected && !nceo);
    wire cfi_reset_n = flags_reset_n && tape_selected && !nceo;
    wire ssi_reset_n = flags_reset_n && !device_enabled;

    // SPI mode 2: MCU samples the existing 165 Q7 on the falling edge;
    // both the 165 chain and this register shift on the rising edge.
    always @(posedge spi_sck or negedge reset_n)
        if (!reset_n) shift <= 16'b0;
        else if (!spi_cs_n) shift <= {shift[14:0], spi_mosi};

    // Saturate at 17, so an overlong frame cannot wrap to a valid length.
    // COMMIT is a separate edge while CS is still LOW; do not commit on CS rise.
    always @(posedge spi_sck or posedge spi_cs_n or negedge reset_n)
        if (!reset_n || spi_cs_n) bit_count <= 5'b0;
        else if (bit_count < 5'd17) bit_count <= bit_count + 1'b1;

    always @(posedge commit or negedge reset_n) begin
        if (!reset_n) begin
            cfg_register <= 12'b0;
            configured <= 1'b0;
            response <= 11'b0;
            consume_toggle <= 1'b0;
        end else if (frame_valid) begin
            if (cfg_frame) begin
                // Configuration changes only when the MCU has released the HP bus.
                if (!hp_run && shift[15:12] == 0 &&
                    shift[3:0] != shift[7:4] &&
                    shift[3:0] != 0 && shift[7:4] != 0 &&
                    shift[3:0] != 10 && shift[7:4] != 10 &&
                    (!shift[8] || (shift[3:0] != 15 && shift[7:4] != 15))) begin
                    cfg_register <= shift[11:0];
                    configured <= 1'b1;
                end
            end else begin
                response <= {shift[11:9], shift[7:0]};
                consume_toggle <= !consume_toggle;
            end
        end
    end

    // Preserve the reference's separate ACK, CFI and SSI reset semantics.
    // Firmware first commits data with flags clear, then repeats it with flags set.
    always @(posedge commit or negedge ack_reset_n)
        if (!ack_reset_n) ack <= 1'b0;
        else if (frame_valid && !cfg_frame) ack <= shift[13];
    always @(posedge commit or negedge cfi_reset_n)
        if (!cfi_reset_n) cfi <= 1'b0;
        else if (frame_valid && !cfg_frame) cfi <= shift[14];
    always @(posedge commit or negedge ssi_reset_n)
        if (!ssi_reset_n) ssi <= 1'b0;
        else if (frame_valid && !cfg_frame) ssi <= shift[15];

    // MCK must be a continuous 8 MHz clock. Bus data must remain stable until
    // PL rises (worst nominal latency 625 ns, excluding propagation/metastability).
    // The 165s are transparent while PL is low; IRQ is asserted after PL closes.
    always @(posedge mck or negedge reset_n) begin
        if (!reset_n) begin
            consume_meta <= 0; consume_sync <= 0; consume_seen <= 0;
            enable_meta <= 0; enable_sync <= 0; enable_prev <= 0;
            capture_phase <= 0; pending <= 0; overrun <= 0;
        end else begin
            consume_meta <= consume_toggle;
            consume_sync <= consume_meta;
            consume_seen <= consume_sync;
            enable_meta <= device_enabled;
            enable_sync <= enable_meta;
            enable_prev <= enable_sync;
            if (!hp_run) begin
                capture_phase <= 0; pending <= 0; overrun <= 0;
                enable_meta <= 0; enable_sync <= 0; enable_prev <= 0;
            end else begin
                if (consume_sync != consume_seen) pending <= 0;
                case (capture_phase)
                    1: capture_phase <= 2;
                    2: begin capture_phase <= 3; pending <= 1; end
                    3: capture_phase <= 0;
                    default: begin end
                endcase
                if (enable_sync && !enable_prev) begin
                    if (pending || !spi_cs_n || capture_phase != 0)
                        overrun <= 1;
                    else capture_phase <= 1;
                end
            end
        end
    end
    assign input_pl_n = capture_phase != 1 && capture_phase != 2;
    assign request_n = !(reset_n && hp_run && pending);

    reg [3:0] irq_line;
    always @* begin
        case (cfg_register[11:9])
            0: irq_line = IRQ0; 1: irq_line = IRQ1;
            2: irq_line = IRQ2; 3: irq_line = IRQ3;
            4: irq_line = IRQ4; 5: irq_line = IRQ5;
            6: irq_line = IRQ6; 7: irq_line = IRQ7;
        endcase
    end
    wire interrupt_active = configured && hp_run && ssi && nsih;
    wire data_enable = tape_selected ? !nceo : general_selected && !nsih;
    wire status_enable = tape_selected ? nceo : general_selected;
    wire [11:0] interrupt_mask = 12'b1 << irq_line;
    wire [11:0] value = {response[10:8], ack, response[7:0]};
    wire [11:0] gate_mask = {{4{status_enable}}, {8{data_enable}}};
    // During interrupt identification, expose only the selected identifying bit.
    wire [11:0] sink = reset_n && hp_run ?
        (interrupt_active ? interrupt_mask : value & gate_mask) : 12'b0;
    genvar i;
    generate for (i = 0; i < 12; i = i + 1) begin : open_collector
        assign hp_n_di[i] = sink[i] ? 1'b0 : 1'bz;
    end endgenerate
    assign hp_n_ssi = reset_n && interrupt_active ? 1'b0 : 1'bz;
    assign hp_n_cfi = reset_n && hp_run && tape_selected && !nceo && cfi
                     ? 1'b0 : 1'bz;
endmodule
