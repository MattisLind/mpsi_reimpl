`timescale 1ns/1ps
// Behavioural stimulus follows Brent Hilpert's General/Fast Mode sequences:
// https://madrona.ca/e/HP9830/machine.html#sc
// Gating/flag resets follow https://madrona.ca/e/HP9830/mpsi/index.html#technical
// Numeric stimulus delays are prototype assumptions, not measured HP bus minima.
module tb_mpsi;
    reg reset_n = 1, hp_run = 0, mck = 0;
    reg [3:0] co = 0, so = 0;
    reg [7:0] dout = 0;
    reg nceo = 1, nsih = 0;
    reg spi_sck = 1, spi_mosi = 0, spi_cs_n = 1, cfg_frame = 0, commit = 0;
    wire input_pl_n, request_n, overrun, miso;
    tri1 [11:0] hp_n_di;
    tri1 hp_n_cfi, hp_n_ssi;
    integer captures = 0, checks = 0;
    realtime pl_started;
    reg [15:0] received;
    mpsi_cpld dut(.*);
    hct165_pair inputs(input_pl_n, spi_sck, {co, so, dout}, miso);
    always #62.5 mck = ~mck;
    always @(negedge input_pl_n) begin
        captures = captures + 1;
        pl_started = $realtime;
    end
    always @(posedge input_pl_n)
        if (reset_n && hp_run && captures > 0)
            check($realtime - pl_started == 250.0, "capture pulse is two MCK cycles");
    always @(negedge request_n) begin
        #1;
        check(input_pl_n, "request indication follows capture close");
    end

    task check(input bit condition, input string message);
        checks = checks + 1;
        if (!condition) $fatal(1, "%s at %0t", message, $time);
    endtask

    task serial_frame(input bit config_write, input [15:0] word,
                      input integer clocks, output [15:0] rx);
        integer i;
        begin
            cfg_frame = config_write;
            check(input_pl_n, "SPI must not overlap PL");
            spi_cs_n = 0; rx = 0;
            #125;
            for (i = 0; i < clocks; i = i + 1) begin
                spi_mosi = i < 16 ? word[15-i] : 0;
                #62.5; spi_sck = 0;
                #1; rx = {rx[14:0], miso};
                #61.5; spi_sck = 1; #1;
            end
            #125; commit = 1;
            #125; commit = 0;
            // Keep CS low until the consume toggle crosses into MCK.
            #500; spi_cs_n = 1;
            #250;
        end
    endtask
    task exchange(input [15:0] word);
        serial_frame(0, word, 16, received);
    endtask
    task response(input [15:0] word);
        exchange(word & 16'h1fff); // stage data before announcing it ready
        exchange(word);
    endtask
    task configure(input [15:0] word);
        hp_run = 0; #750;
        serial_frame(1, word, 16, received);
        hp_run = 1; #750;
    endtask
    task pulse_general(input [3:0] address, input bit input_op, input [7:0] data);
        co = address; so = input_op ? 8 : 0; dout = data;
        #750; nceo = 0; #2000; nceo = 1; #750;
    endtask

    integer a, old_captures;
    initial begin
        #100; reset_n = 0; #500; reset_n = 1; #500;
        check(hp_n_di === 12'hfff && hp_n_cfi && hp_n_ssi, "reset bus release");
        hp_run = 1; co = 8; nceo = 0; #1000;
        check(request_n && hp_n_di === 12'hfff, "unconfigured device cannot respond");
        nceo = 1;
        configure(16'h0b98); // GP=8, TP=9, PR enabled, IRQ selector=5 -> DI9
        response(16'h2000);
        for (a = 0; a < 16; a = a + 1) begin
            co = a; #100;
            check(dut.nGP === (a != 8), "GP address decode");
            check(dut.nTP === (a != 9), "TP address decode");
            check(dut.nPR === (a != 15), "SC15 address decode");
        end

        // General Mode output: CEO pulse clears ACK; firmware restores ACK.
        pulse_general(8, 0, 8'h58);
        check(!request_n && hp_n_di[8] && hp_n_cfi, "General output busy");
        exchange(16'h0000);
        check(received === 16'h8058, "165 capture of General output");
        check(request_n, "request consumed after exact frame");
        response(16'h2000);
        check(!hp_n_di[8] && hp_n_cfi, "General output acknowledged on SI0");

        // General Mode input: SO3=1, data readable only when service inhibited.
        pulse_general(8, 1, 0);
        exchange(0);
        check(received === 16'h8800, "General input direction bit");
        exchange(16'h005a);
        check(hp_n_di[7:0] === ~8'h5a && hp_n_di[8], "data precedes General ACK");
        exchange(16'h205a);
        check(hp_n_di === ~12'h15a, "General data and ready");
        nsih = 1; #100;
        check(hp_n_di[7:0] === 8'hff, "General data released during service");
        nsih = 0;

        pulse_general(15, 0, 8'h41);
        exchange(0);
        check(received === 16'hf041, "SC15 printer capture");
        response(16'h2000);
        configure(16'h0a98); // printer disabled
        pulse_general(15, 0, 0);
        check(request_n && hp_n_di === 12'hfff, "SC15 disable");
        configure(16'h0b98);

        // Fast Mode: CEO remains low for an arbitrarily long service interval.
        response(16'h2e00); co = 9; so = 0; dout = 0; #750;
        check(hp_n_di[11:8] === ~4'hf, "Fast idle status");
        old_captures = captures;
        nceo = 0; #2000;
        check(!request_n && input_pl_n, "Fast request latched with finite PL");
        check(hp_n_di[11:8] === 4'hf, "Fast status released while enabled");
        exchange(16'h00a5);
        check(received === 16'h9000, "Fast command capture");
        #10000;
        check(captures == old_captures + 1, "held CEO must not reload 165s");
        check(hp_n_cfi && hp_n_di[7:0] === ~8'ha5, "Fast data before CFI");
        exchange(16'h60a5);
        check(!hp_n_cfi && hp_n_di[7:0] === ~8'ha5, "Fast CFI acknowledgment");
        nceo = 1; #100;
        check(hp_n_cfi && hp_n_di[7:0] === 8'hff, "CEO feedback clears CFI/releases data");
        check(!hp_n_di[8], "tape response leaves General ACK ready");
        #750; so = 4; dout = 8'h33; nceo = 0; #2000;
        exchange(0);
        check(received === 16'h9433, "Fast write command and data");
        response(16'h6000); check(!hp_n_cfi, "Fast write acknowledged");
        nceo = 1; #750;

        // Interrupt identification is independent of current device selection.
        for (a = 0; a < 8; a = a + 1) begin
            configure(16'h0198 | (a << 9));
            co = 0; nsih = 1; response(16'h8000);
            check(!hp_n_ssi && hp_n_di === ~(12'b1 << (a + 4)), "interrupt line mapping");
            nsih = 0; #100;
            check(hp_n_ssi && hp_n_di === 12'hfff, "service inhibit masks interrupt/ID");
            nsih = 1; co = 9; #750; nceo = 0; #100;
            check(hp_n_ssi, "device enable clears interrupt");
            #1900; exchange(0); nceo = 1; #750;
        end

        configure(16'h0b98); co = 8; nsih = 0;
        response(16'h205a);
        serial_frame(0, 16'hffff, 15, received);
        check(hp_n_di === ~12'h15a, "short frame cannot change outputs");
        serial_frame(0, 16'hffff, 49, received);
        check(hp_n_di === ~12'h15a, "long frame cannot wrap count and commit");
        serial_frame(1, 16'h0176, 16, received);
        check(dut.cfg_register === 12'hb98, "live configuration forbidden");
        configure(16'h0199);
        check(dut.cfg_register === 12'hb98, "duplicate address configuration rejected");

        // Single mailbox: preserve the first word and report the lost request.
        pulse_general(8, 0, 8'h11);
        pulse_general(8, 0, 8'h22);
        check(overrun, "unread request overrun detected");
        exchange(0);
        check(received === 16'h8011, "unread request preserved");
        hp_run = 0; #750; hp_run = 1; #750;
        check(!overrun, "disable clears overrun");

        // Request during a serial transfer must not alter the input latch.
        co = 9; so = 5; dout = 0;
        fork
            exchange(0);
            begin #500; nceo = 0; #1500; nceo = 1; end
        join
        check(overrun && request_n, "busy SPI request reported and not captured");

        // MSC/reset use HP_RUN as immediate electrical release, including flags.
        co = 0; nsih = 1; response(16'h8000);
        hp_run = 0; #1;
        check(hp_n_di === 12'hfff && hp_n_ssi && hp_n_cfi, "MSC bus release");
        #750; pulse_general(8, 0, 8'h44);
        check(request_n, "no HP service during MSC");
        hp_run = 1; #750;
        pulse_general(8, 0, 8'h44); reset_n = 0; #1;
        check(request_n && hp_n_di === 12'hfff, "reset discards pending and releases bus");
        $display("PASS: %0d assertions; General/Fast Mode, SPI, interrupts, capture and MSC release", checks);
        $finish;
    end
    initial begin #1000000; $fatal(1, "test timeout"); end
endmodule
