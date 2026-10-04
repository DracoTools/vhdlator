-- DESCRIPTION: Verilator: VHDL open port converted in the formal part
--
-- "w(d_out) => s" converts an open output port on its way to the actual
-- (6.5.7.1). The port takes its bounds from the actual, as GHDL 7.0.0-dev
-- and Questa 2025.2 give it ("9 downto 2" here); nvc 1.22.1 takes those of
-- the conversion's type mark instead (VESTS ashenden ch_21_fg_21_04).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_convopen_rom is
    port (d_out : out std_ulogic_vector; len : out natural; lft : out integer);
end entity;

architecture a of t_vhdl_convopen_rom is
begin
    d_out <= x"0F";
    len <= d_out'length;
    lft <= d_out'left;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_convopen is
end entity;

architecture test of t_vhdl_convopen is
    subtype w is std_logic_vector(0 to 7);
    signal s : std_logic_vector(9 downto 2);
    signal len : natural;
    signal lft : integer;
begin
    u : entity work.t_vhdl_convopen_rom port map (w(d_out) => s, len => len, lft => lft);

    process is
    begin
        wait for 1 ns;
        assert s = x"0F" report "s " & to_string(s) severity failure;
        assert len = 8 report "len" severity failure;
        assert lft = 9 report "lft " & integer'image(lft) severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
