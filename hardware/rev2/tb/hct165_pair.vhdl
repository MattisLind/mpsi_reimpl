-- Simulation model of the two HCT165s; non-inverting Q7 feeds MISO.
library ieee;
use ieee.std_logic_1164.all;
entity hct165_pair is
    port(pl_n, cp : in std_logic; parallel_data : in std_logic_vector(15 downto 0);
         q7 : out std_logic);
end entity;
architecture model of hct165_pair is
    signal contents : std_logic_vector(15 downto 0) := (others => '0');
begin
    process(pl_n, cp)
    begin
        if rising_edge(pl_n) then contents <= parallel_data;
        elsif rising_edge(cp) and pl_n = '1' then
            contents <= contents(14 downto 0) & '0';
        end if;
    end process;
    q7 <= contents(15) when pl_n = '1' else parallel_data(15);
end architecture;
