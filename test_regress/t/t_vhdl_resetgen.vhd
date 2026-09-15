-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- A register whose reset polarity and kind are generics: the ordinary way
-- to write one piece of RTL for more than one target
entity resetgen_reg is
    generic (RST_POLAR : std_ulogic := '1';
             SYNC_RST  : boolean    := false);
    port (clk : in std_ulogic;
          rst : in std_ulogic;
          q   : out unsigned(3 downto 0));
end entity;

architecture rtl of resetgen_reg is
    signal cnt : unsigned(3 downto 0) := x"9";
begin
    process (clk, rst) is
    begin
        if not SYNC_RST and rst = RST_POLAR then
            cnt <= (others => '0');
        elsif rising_edge(clk) then
            if SYNC_RST and rst = RST_POLAR then
                cnt <= (others => '0');
            else
                cnt <= cnt + 1;
            end if;
        end if;
    end process;
    q <= cnt;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- The same with the reset tested as a plain comparison with the generic: a
-- polarity worked out from the generic's default would be wrong for every
-- instance that gives it another value
entity resetgen_plain is
    generic (RST_POLAR : std_ulogic := '1');
    port (clk : in std_ulogic;
          rst : in std_ulogic;
          q   : out unsigned(3 downto 0));
end entity;

architecture rtl of resetgen_plain is
    signal cnt : unsigned(3 downto 0) := x"9";
begin
    process (clk, rst) is
    begin
        if rst = RST_POLAR then
            cnt <= (others => '0');
        elsif rising_edge(clk) then
            cnt <= cnt + 1;
        end if;
    end process;
    q <= cnt;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_resetgen is
end entity;

architecture test of t_vhdl_resetgen is
    signal clk  : std_ulogic := '0';
    signal rst  : std_ulogic := '0';
    signal nrst : std_ulogic := '1';
    signal q_ah, q_al, q_sy, q_pl : unsigned(3 downto 0);
begin
    -- Asynchronous, asserted high
    u_ah : entity work.resetgen_reg generic map ('1', false) port map (clk, rst, q_ah);
    -- Asynchronous, asserted low
    u_al : entity work.resetgen_reg generic map ('0', false) port map (clk, nrst, q_al);
    -- Synchronous: the reset waits for the clock
    u_sy : entity work.resetgen_reg generic map ('1', true) port map (clk, rst, q_sy);
    -- Asserted low, where the generic's default says high
    u_pl : entity work.resetgen_plain generic map ('0') port map (clk, nrst, q_pl);

    main : process is
    begin
        wait for 1 ns;
        assert q_ah = 9 and q_al = 9 and q_sy = 9 and q_pl = 9
            report "nothing yet" severity failure;

        rst <= '1'; nrst <= '0';
        wait for 1 ns;
        assert q_ah = 0 report "an asynchronous reset asserted high" severity failure;
        assert q_al = 0 report "an asynchronous reset asserted low" severity failure;
        assert q_pl = 0 report "and one the generic's default would call high" severity failure;
        assert q_sy = 9 report "a synchronous one waits for the clock" severity failure;

        clk <= '1';
        wait for 1 ns;
        assert q_sy = 0 report "and takes effect on it" severity failure;
        clk <= '0';
        wait for 1 ns;

        rst <= '0'; nrst <= '1';
        wait for 1 ns;
        clk <= '1';
        wait for 1 ns;
        assert q_ah = 1 and q_al = 1 and q_sy = 1 and q_pl = 1
            report "all count again" severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
    end process;
end architecture;
