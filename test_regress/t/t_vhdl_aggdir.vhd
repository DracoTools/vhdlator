-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_aggdir is
end entity;

architecture test of t_vhdl_aggdir is
    signal a : std_logic_vector(11 downto 0) := x"FAE";
    signal b : std_logic_vector(11 downto 0) := x"182";
    signal r : std_logic_vector(31 downto 0);
begin

    -- Each aggregate has no subtype of its own here, so its choices say
    -- which way its index runs (9.3.3.3): "15 downto 12" puts 15 at the left
    r <= (15 downto 12 => a(11), 11 downto 0 => a)
         & (15 downto 12 => b(11), 11 downto 0 => b);

    process is
    begin
        wait for 1 ns;
        assert r = x"FFAE0182" report "sign extended pair" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
