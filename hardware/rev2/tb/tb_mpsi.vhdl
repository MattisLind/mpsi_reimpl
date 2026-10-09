-- General/standard and Fast Mode stimulus from Hilpert's descriptions:
-- https://madrona.ca/e/HP9830/machine.html#sc
-- https://madrona.ca/e/HP9830/mpsi/index.html#technical
-- Stimulus CEO durations are test choices, not measured HP minimum timing.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use std.env.all;

entity tb_mpsi is
    generic(SPI_HZ : positive := 9_000_000);
end entity;
architecture test of tb_mpsi is
    constant PERIOD : time := 1 sec / 24_000_000;
    constant HIGH_TIME : time := PERIOD / 3; -- TIM1 PSC=0, ARR=2, CCR1=1
    constant SPI_PERIOD : time := 1 sec / SPI_HZ;
    signal reset_n : std_logic := '0';
    signal hp_run, mck : std_logic := '0';
    signal co, so : std_logic_vector(3 downto 0) := x"0";
    signal dout : std_logic_vector(7 downto 0) := x"00";
    signal nceo : std_logic := '1';
    signal nsih : std_logic := '0';
    signal spi_sck, spi_cs_n : std_logic := '1';
    signal spi_mosi, cfg_frame : std_logic := '0';
    signal input_pl_n, request_n, miso : std_logic;
    signal hp_n_di : std_logic_vector(11 downto 0);
    signal hp_n_cfi, hp_n_ssi : std_logic;
    signal captures : natural := 0;
begin
    hp_n_di <= (others => 'H'); hp_n_cfi <= 'H'; hp_n_ssi <= 'H';
    dut: entity work.mpsi_cpld port map
        (reset_n, hp_run, mck, co, nceo, nsih, spi_sck, spi_mosi, spi_cs_n,
         cfg_frame, input_pl_n, request_n, hp_n_di, hp_n_cfi, hp_n_ssi);
    input_chain: entity work.hct165_pair
        port map(input_pl_n, spi_sck, co & so & dout, miso);
    clock: process
    begin
        mck <= '1'; wait for HIGH_TIME;
        mck <= '0'; wait for PERIOD - HIGH_TIME;
    end process;
    pulse_monitor: process
        variable started : time;
    begin
        wait until falling_edge(input_pl_n);
        started := now; captures <= captures + 1;
        wait until rising_edge(input_pl_n);
        if reset_n = '1' and hp_run = '1' and cfg_frame = '0' then
            assert now - started = 2 * PERIOD report "PL must be two 24 MHz cycles" severity failure;
        end if;
    end process;
    request_monitor: process
    begin
        wait until falling_edge(request_n); wait for 1 ps;
        assert input_pl_n = '1' report "request precedes capture close" severity failure;
    end process;
    timeout: process
    begin
        wait for 2 ms; assert false report "test timeout" severity failure;
    end process;
    stimulus: process
        variable checks : natural := 0;
        variable received : std_logic_vector(15 downto 0);
        variable old_captures : natural;
        type irq_array is array(0 to 4) of natural;
        constant IRQ_BITS : irq_array := (0, 7, 8, 9, 10);
        procedure check(condition : boolean; message : string) is
        begin
            checks := checks + 1;
            assert condition report message severity failure;
        end procedure;
        procedure check_bus(sinks : natural; message : string) is
        begin
            check(to_x01(hp_n_di) = not std_logic_vector(to_unsigned(sinks, 12)), message);
        end procedure;
        procedure serial_frame(config_write : std_logic; word : std_logic_vector(15 downto 0);
                               inject_request : boolean := false; watch_flags : boolean := false;
                               watch_data_shift : boolean := false) is
            variable old_flags : std_logic_vector(2 downto 0);
            variable old_data : std_logic_vector(7 downto 0);
            variable data_changed : boolean;
        begin
            cfg_frame <= config_write;
            check(input_pl_n = '1', "SPI overlaps PL");
            old_flags := to_x01(hp_n_di(8) & hp_n_cfi & hp_n_ssi);
            old_data := to_x01(hp_n_di(7 downto 0)); data_changed := false;
            spi_cs_n <= '0'; received := (others => '0'); wait for 125 ns;
            check(request_n = '1', "CS low must clear request");
            for i in 0 to 15 loop
                spi_mosi <= word(15-i);
                if inject_request and i = 3 then nceo <= '0'; end if;
                if inject_request and i = 12 then nceo <= '1'; end if;
                if config_write = '1' then
                    -- Try every address with CEO active while intermediate
                    -- configuration values pass through the register.
                    co <= std_logic_vector(to_unsigned(i, 4)); nceo <= '0';
                end if;
                wait for SPI_PERIOD / 2; spi_sck <= '0'; wait for 1 ps;
                received := received(14 downto 0) & miso;
                wait for SPI_PERIOD / 2 - 1 ps; spi_sck <= '1'; wait for 1 ps;
                check(input_pl_n = '1' and request_n = '1', "CS low must inhibit input loading");
                if config_write = '1' then
                    check_bus(0, "configuration shifting must release bus");
                    check(to_x01(hp_n_cfi) = '1' and to_x01(hp_n_ssi) = '1',
                          "configuration shifting must release flags");
                    check(input_pl_n = '1' and request_n = '1',
                          "configuration shifting must inhibit capture");
                elsif watch_flags then
                    check(to_x01(hp_n_di(8) & hp_n_cfi & hp_n_ssi) = old_flags,
                          "shift bits changed flags before CS rising");
                end if;
                if to_x01(hp_n_di(7 downto 0)) /= old_data then data_changed := true; end if;
            end loop;
            if watch_data_shift then
                check(data_changed, "data must come directly from shift register");
                check_bus(16#05A#, "complete data must precede ACK");
            end if;
            if config_write = '1' then
                -- Configuration takes effect directly; CS must not load flags.
                nceo <= '1'; co <= x"0"; wait for 250 ns;
            end if;
            wait for 125 ns; spi_cs_n <= '1'; wait for 250 ns;
            cfg_frame <= '0'; wait for 125 ns;
        end procedure;
        procedure exchange(word : std_logic_vector(15 downto 0)) is
        begin serial_frame('0', word); end procedure;
        procedure response(word : std_logic_vector(15 downto 0)) is
        begin exchange(word); end procedure;
        procedure configure(word : std_logic_vector(15 downto 0)) is
        begin
            hp_run <= '0'; wait for 750 ns; serial_frame('1', word);
            hp_run <= '1'; wait for 750 ns;
        end procedure;
        procedure pulse_general(address : natural; input_op : boolean; data : natural) is
        begin
            co <= std_logic_vector(to_unsigned(address, 4));
            if input_op then so <= x"8"; else so <= x"0"; end if;
            dout <= std_logic_vector(to_unsigned(data, 8));
            wait for 750 ns; nceo <= '0'; wait for 2 us; nceo <= '1'; wait for 750 ns;
        end procedure;
        procedure check_decode is
        begin
            response(x"2000");
            for address in 0 to 15 loop
                co <= std_logic_vector(to_unsigned(address, 4)); wait for 100 ns;
                if address = 8 or address = 9 or address = 15 then
                    check_bus(16#100#, "selected port must expose ACK");
                else check_bus(0, "unselected port must release bus"); end if;
            end loop;
        end procedure;
    begin
        wait for 500 ns; reset_n <= '1'; wait for 500 ns;
        check_bus(0, "reset bus release");
        check(to_x01(hp_n_cfi) = '1' and to_x01(hp_n_ssi) = '1', "reset flags release");
        hp_run <= '1'; co <= x"8"; nceo <= '0'; wait for 1 us;
        check(request_n = '1', "unconfigured device captures"); check_bus(0, "unconfigured outputs");
        nceo <= '1'; configure(x"0798"); -- GP8 TP9 printer, selector3 -> nSI1
        check_decode;

        pulse_general(8, false, 16#58#);
        check(request_n = '0' and to_x01(hp_n_di(8)) = '1', "General output busy");
        exchange(x"0000"); check(received = x"8058", "General output capture");
        check(request_n = '1', "SPI consumes request");
        response(x"2000"); check_bus(16#100#, "General ACK on SI0");
        pulse_general(8, true, 0); exchange(x"0000");
        check(received = x"8800", "General input direction");
        exchange(x"005A"); check_bus(16#05A#, "data before ACK");
        serial_frame('0', x"205A", watch_flags => true, watch_data_shift => true);
        check_bus(16#15A#, "CS rising sets General ACK after data");
        nsih <= '1'; wait for 100 ns; check_bus(16#100#, "SIH inhibits General data"); nsih <= '0';
        pulse_general(15, false, 16#41#); exchange(x"0000");
        check(received = x"F041", "printer capture");
        configure(x"0698"); pulse_general(15, false, 0);
        check(request_n = '1', "disabled printer captures"); check_bus(0, "disabled printer drives");
        configure(x"0798");

        response(x"2E00"); co <= x"9"; so <= x"0"; dout <= x"00"; wait for 750 ns;
        check_bus(16#F00#, "Fast idle status"); old_captures := captures;
        nceo <= '0'; wait for 2 us;
        check(request_n = '0' and input_pl_n = '1', "Fast finite capture");
        check_bus(0, "Fast active status inhibited"); exchange(x"00A5");
        check(received = x"9000", "Fast command capture"); wait for 10 us;
        check(captures = old_captures + 1, "held CEO recaptures");
        check_bus(16#A5#, "Fast data precedes CFI");
        check(to_x01(hp_n_cfi) = '1', "premature CFI");
        serial_frame('0', x"60A5", watch_flags => true);
        check(to_x01(hp_n_cfi) = '0', "CS rising sets Fast CFI after data");
        nceo <= '1'; wait for 100 ns;
        check(to_x01(hp_n_cfi) = '1', "CEO feedback clears CFI");
        check_bus(16#100#, "Fast data released, ACK preserved");
        wait for 750 ns; so <= x"4"; dout <= x"33"; nceo <= '0'; wait for 2 us;
        exchange(x"0000"); check(received = x"9433", "Fast write capture");
        response(x"6000"); check(to_x01(hp_n_cfi) = '0', "Fast write CFI");
        nceo <= '1'; wait for 750 ns;

        for selector in 0 to 7 loop
            configure(std_logic_vector(to_unsigned(16#198# + selector * 512, 16)));
            co <= x"0"; nsih <= '1'; response(x"8000");
            if selector < 5 then
                check_bus(2 ** IRQ_BITS(selector), "five interrupt ID choices");
                check(to_x01(hp_n_ssi) = '0', "SSI missing");
            else
                check_bus(0, "reserved selector drives ID");
                check(to_x01(hp_n_ssi) = '1', "reserved selector drives SSI");
            end if;
            nsih <= '0'; wait for 100 ns; check_bus(0, "SIH masks interrupt ID");
            check(to_x01(hp_n_ssi) = '1', "SIH masks SSI");
            nsih <= '1'; co <= x"9"; wait for 750 ns; nceo <= '0'; wait for 2 us;
            check(to_x01(hp_n_ssi) = '1', "device enable clears SSI");
            exchange(x"0000"); nceo <= '1'; wait for 750 ns;
        end loop;
        configure(x"0198"); co <= x"8"; nsih <= '1'; response(x"AA55");
        check_bus(16#B01#, "normal GP status remains beside interrupt ID");
        co <= x"0"; wait for 100 ns; check_bus(1, "unselected interrupt ID only");

        configure(x"0798"); co <= x"8"; nsih <= '0'; response(x"205A");
        -- Even with HP_RUN high, CFG_FRAME prevents intermediate selections.
        serial_frame('1', x"0176"); response(x"2000");
        for address in 0 to 15 loop
            co <= std_logic_vector(to_unsigned(address, 4)); wait for 100 ns;
            if address = 6 or address = 7 or address = 15 then
                check_bus(16#100#, "direct configuration did not change decode");
            else check_bus(0, "old or intermediate address still selected"); end if;
        end loop;
        configure(x"0798"); check_decode;

        -- Controller: GP8 enabled, TP disabled by code zero, no printer.
        configure(x"0608"); nsih <= '0';
        pulse_general(9, false, 0); check(request_n = '1', "Controller TP captures");
        pulse_general(10, false, 0); check(request_n = '1', "Controller internal SC10 captures");
        pulse_general(15, false, 0); check(request_n = '1', "Controller printer captures");
        pulse_general(0, false, 0); check(request_n = '1', "zero disable code captures SC0");
        pulse_general(8, true, 16#3C#); exchange(x"0000");
        check(received = x"883C", "Controller GP remains available");
        -- TP-only decode is also possible.
        configure(x"0690"); pulse_general(8, false, 0);
        check(request_n = '1', "disabled GP captures");
        configure(x"0798");

        pulse_general(8, false, 16#11#); pulse_general(8, false, 16#22#);
        exchange(x"0000"); check(received = x"8022", "capture must not add an unread-word buffer");
        co <= x"9"; so <= x"5"; dout <= x"00";
        serial_frame('0', x"0000", inject_request => true);
        check(request_n = '1', "request during SPI must not reload inputs");

        co <= x"0"; nsih <= '1'; response(x"8000"); hp_run <= '0'; wait for 1 ns;
        check_bus(0, "MSC releases data/status");
        check(to_x01(hp_n_ssi) = '1' and to_x01(hp_n_cfi) = '1', "MSC releases flags");
        wait for 750 ns; pulse_general(8, false, 16#44#);
        check(request_n = '1', "MSC captures");
        hp_run <= '1'; wait for 750 ns; pulse_general(8, false, 16#44#);
        reset_n <= '0'; wait for 1 ns;
        check(request_n = '1', "reset leaves pending request"); check_bus(0, "reset bus release");
        report "PASS: " & natural'image(checks) & " checks; 24 MHz MCK, SPI " & positive'image(SPI_HZ)
            & " Hz; General/Fast, five IRQs, Controller, direct data, CS flags, capture, MSC";
        stop; wait;
    end process;
end architecture;
