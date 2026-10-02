-- DESCRIPTION: Verilator: VHDL force and release through another name
--
-- A force or a release written on an alias of a signal, or on a signal
-- parameter of a procedure, is one of the signal itself (6.6.2, 4.2.2.3):
-- the release hands it back to its drivers.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_forcealias is
end entity;

architecture test of t_vhdl_forcealias is
    signal sig : std_logic := '0';
    signal vec : std_logic_vector(3 downto 0) := "0101";
    alias sig_a is sig;
    alias sig_b is sig_a;
    alias vec_a is vec;
begin

    process is
        procedure force_pulse (signal target : out std_logic) is
        begin
            target <= force '1';
            wait for 1 ps;
            assert sig = '1' report "forced in the procedure" severity failure;
            target <= release;
            wait for 1 ps;
        end procedure;
    begin
        sig <= force '1';
        wait for 1 ps;
        assert sig = '1' report "forced by name" severity failure;
        sig <= release;
        wait for 1 ps;
        assert sig = '0' report "released by name" severity failure;

        sig_a <= force '1';
        wait for 1 ps;
        assert sig = '1' and sig_a = '1' report "forced by alias" severity failure;
        sig_a <= release;
        wait for 1 ps;
        assert sig = '0' report "released by alias" severity failure;

        sig_b <= force '1';
        wait for 1 ps;
        assert sig = '1' report "forced by an alias of an alias" severity failure;
        sig <= release;         -- by the other name
        wait for 1 ps;
        assert sig_b = '0' report "released by name after a force by alias" severity failure;

        vec_a <= force "1111";
        wait for 1 ps;
        assert vec = "1111" report "vector forced by alias" severity failure;
        vec_a <= release;
        wait for 1 ps;
        assert vec = "0101" report "vector released by alias" severity failure;

        force_pulse(sig_a);
        assert sig = '0' report "released in the procedure" severity failure;
        force_pulse(sig);
        assert sig = '0' report "released in the procedure, by name" severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
