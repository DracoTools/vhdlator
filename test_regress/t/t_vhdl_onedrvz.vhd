-- DESCRIPTION: Verilator: VHDL 'Z' on a signal with one driver
--
-- A signal of a nine-value type with a single driver holds whatever that
-- driver gives it, 'Z' included (14.7.3.2): a tri-state buffer written as one
-- conditional assignment, and a process that floats a vector in part.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_onedrvz is
end entity;

architecture test of t_vhdl_onedrvz is
    signal en, d : std_logic := '0';
    signal bus_o : std_logic;
    signal a : std_logic;
    signal v : std_logic_vector(7 downto 0);
begin

    -- A tri-state buffer
    bus_o <= d when en = '1' else 'Z';

    process is
    begin
        a <= 'Z';
        v <= X"5Z";
        wait for 1 ns;
        assert a = 'Z' severity failure;
        assert v = X"5Z" severity failure;
        assert v(3 downto 0) = "ZZZZ" and v(7 downto 4) = "0101" severity failure;
        assert bus_o = 'Z' severity failure;

        a <= '1';
        v <= X"A5";
        en <= '1';
        d <= '1';
        wait for 1 ns;
        assert a = '1' severity failure;
        assert v = X"A5" severity failure;
        assert bus_o = '1' severity failure;

        en <= '0';
        wait for 1 ns;
        assert bus_o = 'Z' severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
