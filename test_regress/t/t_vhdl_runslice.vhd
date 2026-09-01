-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_runslice is
end entity;

architecture test of t_vhdl_runslice is
    signal n    : integer                      := 7;
    signal buf  : std_logic_vector(7 downto 0) := "10101010";
    signal out8 : std_logic_vector(7 downto 0);
begin

    main : process is
        variable m  : integer                      := 5;
        variable vb : std_logic_vector(7 downto 0) := "11001100";
        variable v6 : std_logic_vector(5 downto 0);
        variable vw : std_logic_vector(7 downto 0);
    begin
        wait for 1 ns;
        -- The two ends of the slice do not keep a fixed distance apart, so
        -- how long it is only running decides (8.5). What it is worth is
        -- still an ordinary value, and the length has to match where it is
        -- put (10.5.2.1)
        out8 <= ("0" & buf(n - 1 downto 0));
        v6  := vb(m downto 0);

        assert buf(n - 1 downto 0) = "0101010" report "read" severity failure;

        -- And written into, which puts the elements back where the object
        -- keeps them
        vw                     := (others => '0');
        vw(m downto 0)         := "110011";
        assert vw = "00110011" report "written " & to_string(vw) severity failure;

        wait for 1 ns;
        assert out8 = "00101010" report "signal " & to_string(out8) severity failure;
        assert v6 = "001100" report "variable " & to_string(v6) severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
