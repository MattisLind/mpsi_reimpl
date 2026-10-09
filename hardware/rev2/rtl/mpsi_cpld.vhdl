-- HP9830 MPSI Rev2. All HP outputs sink or release; never drive a HIGH.
-- Protocol: requirements/03_interface.md. 24 MHz capture clock, SPI mode 2.
-- Pin syntax follows DECPROMEM and the selected run_fitter.sh.
--PIN: CHIP "mpsi_cpld" ASSIGNED TO AN PLCC44
--PIN: reset_n : 1
--PIN: spi_sck : 2
--PIN: hp_n_cfi : 4
--PIN: co_0 : 5
--PIN: co_1 : 6
--PIN: co_2 : 8
--PIN: co_3 : 9
--PIN: nceo : 11
--PIN: nsih : 12
--PIN: hp_n_ssi : 14
--PIN: hp_n_di_11 : 16
--PIN: hp_n_di_10 : 17
--PIN: hp_n_di_9 : 18
--PIN: hp_n_di_8 : 19
--PIN: hp_n_di_7 : 20
--PIN: hp_n_di_6 : 21
--PIN: hp_n_di_5 : 24
--PIN: hp_n_di_4 : 25
--PIN: hp_n_di_3 : 26
--PIN: hp_n_di_2 : 27
--PIN: hp_n_di_1 : 28
--PIN: hp_n_di_0 : 29
--PIN: input_pl_n : 31
--PIN: request_n : 33
--PIN: spi_mosi : 36
--PIN: cfg_frame : 37
--PIN: spi_cs_n : 41
--PIN: mck : 43
--PIN: hp_run : 44
--PIN: TDI : 7
--PIN: TMS : 13
--PIN: TCK : 32
--PIN: TDO : 38
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity mpsi_cpld is
    port (
        reset_n, hp_run, mck : in std_logic;
        co : in std_logic_vector(3 downto 0);
        nceo, nsih : in std_logic;
        spi_sck, spi_mosi, spi_cs_n, cfg_frame : in std_logic;
        input_pl_n, request_n : out std_logic;
        hp_n_di : out std_logic_vector(11 downto 0);
        hp_n_cfi, hp_n_ssi : out std_logic
    );
end entity;

architecture rtl of mpsi_cpld is
    signal shift : std_logic_vector(15 downto 0);
    signal cfg : std_logic_vector(11 downto 0);
    signal configured, hp_active : std_logic;
    signal ack, cfi, ssi : std_logic;
    signal enable_meta, enable_sync, enable_prev : std_logic;
    constant IDLE        : unsigned(1 downto 0) := "00";
    constant FIRST_LOAD  : unsigned(1 downto 0) := "01";
    constant SECOND_LOAD : unsigned(1 downto 0) := "11";
    constant CAPTURED    : unsigned(1 downto 0) := "10";
    -- Bit 0 stays high through both
    -- load states, so PL needs no combinational multi-bit state decoding.
    -- The word lives in the external HCT165s. Each CEO assertion reloads it;
    -- firmware handles requests promptly using the HP handshake, as in MPSI.
    signal capture_state : unsigned(1 downto 0);
    signal capture_reset_n : std_logic;
    signal gp_selected, tape_selected, printer_selected, general_selected : std_logic;
    signal device_enabled, flags_reset_n : std_logic;
    signal ack_reset_n, cfi_reset_n, ssi_reset_n : std_logic;
    signal data_enable, status_enable, interrupt_active, valid_irq : std_logic;
    signal irq_mask, value, gate_mask, sink : std_logic_vector(11 downto 0);

begin
    -- CFG_FRAME blanks HP selection/outputs throughout direct config shifting.
    -- Firmware normally also holds HP_RUN low during startup/configuration.
    hp_active <= hp_run and not cfg_frame;
    configured <= '1' when cfg(8 downto 0) /= "000000000" else '0';
    gp_selected <= '1' when configured = '1' and hp_active = '1'
        and cfg(3 downto 0) /= "0000" and co = cfg(3 downto 0) else '0';
    tape_selected <= '1' when configured = '1' and hp_active = '1'
        and cfg(7 downto 4) /= "0000" and co = cfg(7 downto 4) else '0';
    printer_selected <= '1' when configured = '1' and hp_active = '1'
        and cfg(8) = '1' and co = "1111" else '0';
    general_selected <= gp_selected or printer_selected;
    device_enabled <= (general_selected or tape_selected) and not nceo;
    flags_reset_n <= reset_n and hp_active;
    ack_reset_n <= flags_reset_n and not (general_selected and not nceo);
    cfi_reset_n <= flags_reset_n and tape_selected and not nceo;
    ssi_reset_n <= flags_reset_n and not device_enabled;

    -- MCU samples HCT165 Q7 on falling SCK; both registers shift on rising SCK.
    -- STM32 hardware supplies exactly 16 clocks; no bit counter/length check.
    -- Config is itself a shift register: the final 12 bits are retained. It
    -- cannot select the HP while CFG_FRAME is high. Data/status go directly
    -- from the data shift register to the bus gates: there is no output latch.
    shift_registers: process(spi_sck, reset_n)
    begin
        if reset_n = '0' then
            shift <= (others => '0');
            cfg <= (others => '0');
        elsif rising_edge(spi_sck) then
            if spi_cs_n = '0' then
                if cfg_frame = '1' then
                    cfg <= cfg(10 downto 0) & spi_mosi;
                else
                    shift <= shift(14 downto 0) & spi_mosi;
                end if;
            end if;
        end if;
    end process;

    -- Hilpert's CTL loads the three flags only after shifting is complete.
    -- CS rising performs that function; there is no separate COMMIT signal.
    -- Firmware shifts only when the HP handshake permits changing data.
    -- The separate flag FFs prevent intermediate shift bits asserting ready.
    latch_ack: process(spi_cs_n, ack_reset_n)
    begin
        if ack_reset_n = '0' then ack <= '0';
        elsif rising_edge(spi_cs_n) then
            if cfg_frame = '0' then ack <= shift(13); end if;
        end if;
    end process;
    latch_cfi: process(spi_cs_n, cfi_reset_n)
    begin
        if cfi_reset_n = '0' then cfi <= '0';
        elsif rising_edge(spi_cs_n) then
            if cfg_frame = '0' then cfi <= shift(14); end if;
        end if;
    end process;
    latch_ssi: process(spi_cs_n, ssi_reset_n)
    begin
        if ssi_reset_n = '0' then ssi <= '0';
        elsif rising_edge(spi_cs_n) then
            if cfg_frame = '0' then ssi <= shift(15); end if;
        end if;
    end process;

    -- Two-stage CDC, finite two-cycle PL, one capture per selected CEO assertion.
    -- PL is nominally 83.33 ns at 24 MHz; HP data must remain stable until close.
    -- CS low clears the request and inhibits HCT165 loading during SPI,
    -- matching the role of CTL low in Hilpert's design. No unread protection
    -- or overrun checking: a new CEO edge can replace the previous request.
    capture_reset_n <= reset_n and hp_active and spi_cs_n;
    capture_input: process(mck, capture_reset_n)
    begin
        if capture_reset_n = '0' then
            capture_state <= IDLE;
        elsif rising_edge(mck) then
            if enable_sync = '1' and enable_prev = '0' then capture_state <= FIRST_LOAD;
            elsif capture_state = FIRST_LOAD then capture_state <= SECOND_LOAD;
            elsif capture_state = SECOND_LOAD then capture_state <= CAPTURED;
            end if;
        end if;
    end process;
    -- HP CEO is asynchronous to MCK. The first two registers reduce
    -- metastability risk; the third detects its assertion once, even if Fast
    -- Mode holds CEO active. They are not an overrun monitor or a data buffer.
    synchronize_hp_request: process(mck, flags_reset_n)
    begin
        if flags_reset_n = '0' then
            enable_meta <= '0'; enable_sync <= '0'; enable_prev <= '0';
        elsif rising_edge(mck) then
            enable_meta <= device_enabled;
            enable_sync <= enable_meta;
            enable_prev <= enable_sync;
        end if;
    end process;
    input_pl_n <= not capture_state(0);
    request_n <= '0' when reset_n = '1' and hp_active = '1' and capture_state = CAPTURED else '1';

    valid_irq <= '1' when unsigned(cfg(11 downto 9)) < 5 else '0';
    interrupt_active <= configured and hp_active and ssi and nsih and valid_irq;
    with cfg(11 downto 9) select irq_mask <=
        x"001" when "000", x"080" when "001", x"100" when "010",
        x"200" when "011", x"400" when "100", x"000" when others;
    data_enable <= (general_selected and not nsih) or (tape_selected and not nceo);
    status_enable <= general_selected or (tape_selected and nceo);
    value <= shift(11 downto 9) & ack & shift(7 downto 0);
    gate_mask <= (11 downto 8 => status_enable, 7 downto 0 => data_enable);
    -- Interrupt ID is an additional sink. Normally gated status remains visible.
    sink <= (value and gate_mask) or irq_mask when reset_n = '1'
        and hp_active = '1' and interrupt_active = '1' else
        value and gate_mask when reset_n = '1' and hp_active = '1' else x"000";
    outputs: for i in hp_n_di'range generate
        hp_n_di(i) <= '0' when sink(i) = '1' else 'Z';
    end generate;
    hp_n_ssi <= '0' when reset_n = '1' and interrupt_active = '1' else 'Z';
    hp_n_cfi <= '0' when reset_n = '1' and hp_active = '1' and tape_selected = '1'
        and nceo = '0' and cfi = '1' else 'Z';
end architecture;
