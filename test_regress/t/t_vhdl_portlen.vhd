-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity sat is
    port (x : in signed;
          y : out signed);
end entity;

architecture rtl of sat is
    -- The length is a port's, which the instance settles: not a number
    -- here, but an expression elaboration folds
    constant hi : signed := to_signed(3, y'length);
    constant lo : signed := to_signed(1, x'length);
    -- An operator that gives a result as long as what it was handed (16.5),
    -- so this constant is as long as those
    constant mid : signed := hi - lo;
begin
    y <= hi when x > mid else mid;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_portlen is
end entity;

architecture test of t_vhdl_portlen is
    signal a : signed(3 downto 0) := "0101";
    signal b : signed(3 downto 0);
    signal c : signed(7 downto 0) := "00000000";
    signal d : signed(7 downto 0);
begin

    u1 : entity work.sat port map (x => a, y => b);
    u2 : entity work.sat port map (x => c, y => d);

    process is
    begin
        wait for 1 ns;
        assert b = "0011" report "b" severity failure;
        assert d = "00000010" report "d" severity failure;
        assert b'length = 4 report "blen" severity failure;
        assert d'length = 8 report "dlen" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
