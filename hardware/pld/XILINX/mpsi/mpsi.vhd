----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date:    19:19:50 01/17/2021 
-- Design Name: 
-- Module Name:    mpsi - Behavioral 
-- Project Name: 
-- Target Devices: 
-- Tool versions: 
-- Description: 
--
-- Dependencies: 
--
-- Revision: 
-- Revision 0.01 - File Created
-- Additional Comments: 
--
----------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
--use IEEE.NUMERIC_STD.ALL;

-- Uncomment the following library declaration if instantiating
-- any Xilinx primitives in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

entity mpsi is
    Port ( CO0 : in  STD_LOGIC;
           CO1 : in  STD_LOGIC;
           CO2 : in  STD_LOGIC;
           CO3 : in  STD_LOGIC;
           nSIH : in  STD_LOGIC;
           nCEO : in  STD_LOGIC;
           GPSEL0 : in  STD_LOGIC;
           GPSEL1 : in  STD_LOGIC;
           GPSEL2 : in  STD_LOGIC;
           GPSEL3 : in  STD_LOGIC;
           TPSEL0 : in  STD_LOGIC;
           TPSEL1 : in  STD_LOGIC;
           TPSEL2 : in  STD_LOGIC;
           TPSEL3 : in  STD_LOGIC;
           PRENABLE : in  STD_LOGIC;
           CTL : in  STD_LOGIC;
           INT : in  STD_LOGIC;
           CFI : out  STD_LOGIC;
			  CFIHOST : in STD_LOGIC;
           GMACK : in  STD_LOGIC;
           SSI : out  STD_LOGIC;
           ACK : out  STD_LOGIC;
           STATUS_ENB : out  STD_LOGIC;
           DATA_ENB : out  STD_LOGIC;
           INT_ID : out  STD_LOGIC;
			  nRESET: in STD_LOGIC;
			  DATACLK: out STD_LOGIC);
end mpsi;

architecture Behavioral of mpsi is
	SIGNAL u11b, u11a, u10a, u10b, u14b : STD_LOGIC;
begin


logic: PROCESS (CO0, CO1, CO2, CO3, GPSEL0, GPSEL1, GPSEL2, GPSEL3, TPSEL0, TPSEL1, TPSEL2, TPSEL3, PRENABLE, CTL, INT, GMACK, nRESET, nCEO, nSIH, u10b, u10a, CFIHOST)
	VARIABLE nCO3, nGP, nTP, nPR, u15e, u15c, u15f, u14a, u15d, u14d, u13b, u13d, u13c, u9b, u14c, u9a, u9c, u9d: STD_LOGIC;
	BEGIN
	  nCO3 := NOT CO3;
	  if (nCO3 = GPSEL3 AND CO2 = GPSEL2 AND CO1 = GPSEL1 AND CO0 = GPSEL0) THEN
		 nTP := '0';
	  else 
		 nTP := '1';
	  end if;
	  if (nCO3 = TPSEL3 AND CO2 = TPSEL2 AND CO1 = TPSEL1 AND CO0 = TPSEL0) THEN
		 nGP := '0';
	  else 
		 nGP := '1';
	  end if;  
	  if (nCO3 = '0' AND CO2 = '1' AND CO1 = '1' AND CO0 = '0' AND PRENABLE = '1') THEN
		 nPR := '0';
	  else 
		 nPR := '1';
	  end if;
	  u14c := nPR NAND nGP;
	  if (nTP = '1') THEN 
		 u13d := u14c;
	  else
		 u13d := '0';
	  end if;
	  u15f := NOT u13d;
	  u15c := NOT nSIH;
	  u15e := NOT nCEO;
	  u14b <= u15e AND u15f;
	  u14a := u15c AND u15f;
	  u14d := nRESET AND u15d;
	  if (nTP = '1') then
		 u13b := u15d;
		 u13c := u14a;
	  else 
		 u13b := u15d;
		 u13c := u14b;
	  end if;
	  STATUS_ENB <= u13b;
	  DATA_ENB <= u13c;
	  u15d := NOT u14b;
-- 7474 u10b
	  if (u14d = '0') then
		 u10b <= '0';
	  elsif rising_edge(CTL) then
	    u10b <= INT;
	  END IF;
-- 7474 u10a	  
	  if (u14b = '0') then 
		 u10a <= '0';
	  elsif rising_edge(CTL) then
	    u10a <= CFIHOST;
	  END IF;
-- 7474 u11a
	  if (u15d = '0') then
		 u11a <= '0';
	  elsif rising_edge(CTL) then
	    u11a <= GMACK;
	  END IF;
-- 7474 u11b
	  if (CTL = '0') then
		 u11b <= '0';
	  elsif rising_edge(u14b) then	
	    u11b <= '1';
	  END IF;	
	  u9d := u10b NAND nSIH;
	  u9a := u14b NAND u10a;
	  u9c := nSIH NAND u10b;
     SSI <= NOT u9d;
     CFI <= NOT u9a;
     INT_ID <= NOT u9c;	
     ACK <= nTP NAND (NOT u11a);
	  DATACLK <= u11b;
END PROCESS logic;
   
	
end Behavioral;

