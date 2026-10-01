-- DESCRIPTION: Verilator: VHDL subprogram nested in one that takes signals
--
-- A subprogram declared inside a procedure sees that procedure's signal
-- parameters (12.3), which name the signals each call passed (4.2.2.3): two
-- calls of the procedure, two sets of signals.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_signest is
end entity;

architecture test of t_vhdl_signest is
    signal clk : std_logic := '0';
    signal a, b : std_logic := '0';
    signal seen_a, seen_b : std_logic_vector(1 to 2) := "00";
    signal cnt_a, cnt_b : natural := 0;

    procedure watch (signal clock, expr : in std_logic;
                     signal seen : out std_logic_vector(1 to 2);
                     signal cnt : inout natural) is
        impure function peek return std_logic is
        begin
            return expr;
        end function;
        procedure note is
        begin
            -- What the procedure itself sees, and what its function does
            seen <= expr & peek;
            if expr = '1' then
                cnt <= cnt + 1;
            end if;
        end procedure;
    begin
        loop
            wait until rising_edge(clock);
            note;
        end loop;
    end procedure;

begin

    clk <= not clk after 5 ns;

    -- The same procedure, with nothing but its signals to tell the two apart
    wa : watch(clk, a, seen_a, cnt_a);
    wb : watch(clk, b, seen_b, cnt_b);

    process is
    begin
        a <= '1';
        b <= '0';
        wait until rising_edge(clk);
        wait for 1 ns;
        assert seen_a = "11" report "a, first edge" severity failure;
        assert seen_b = "00" report "b, first edge" severity failure;
        a <= '0';
        b <= '1';
        wait until rising_edge(clk);
        wait for 1 ns;
        assert seen_a = "00" report "a, second edge" severity failure;
        assert seen_b = "11" report "b, second edge" severity failure;
        wait until rising_edge(clk);
        wait for 1 ns;
        assert cnt_a = 1 report "count of a" severity failure;
        assert cnt_b = 2 report "count of b" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
