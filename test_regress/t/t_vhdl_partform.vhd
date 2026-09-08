-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- The formal part of an association element may name part of the formal
-- rather than the whole of it: an element of a record by its name, or an
-- element of an array by its index (6.5.7.1). Such a formal is given no
-- object of its own -- each part is a signal of the design in its own right,
-- and a reference to a part lowers to that signal -- so a reference to the
-- whole of it is every part at once, which is a concatenation.
--
-- Two things were missing. The array form was never read as a formal part at
-- all, and a formal bound part by part could not be referred to as a whole,
-- which is what a body does when it writes "d <= ones".
--
-- VUnit's AXI stream protocol checker writes "pass_stable_test(d(0) => tlast)"
-- against "procedure pass_stable_test (signal d : out std_logic_vector)",
-- whose body assigns the whole of d and sizes constants from d'range.
--
-- Checked against GHDL 7.0.0-dev, which runs all of this and agrees. nvc
-- 1.22.1 runs the one-element form but refuses more than one -- "sorry, this
-- form of parameter is not yet supported" -- so it is no oracle for the rest.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_partform is
end entity;

architecture a of t_vhdl_partform is

    type pair is record
        x : std_logic;
        y : std_logic;
    end record;

    -- Written against an unconstrained array, and referring to the whole of
    -- its formal as well as to its bounds
    procedure drive_vec (signal d : out std_logic_vector; v : std_logic) is
        constant fill : std_logic_vector (d'range) := (others => '0');
    begin
        assert d'length = 2 report "formal length" severity failure;
        d <= fill;
        wait for 1 ns;
        d <= (d'range => v);
        wait for 1 ns;
    end procedure;

    -- One element only, which is the shape VUnit writes
    procedure drive_one (signal d : out std_logic_vector; v : std_logic) is
    begin
        assert d'length = 1 report "one-element formal length" severity failure;
        d (d'low) <= v;
        wait for 1 ns;
    end procedure;

    -- And the record form, whole and by part
    procedure drive_rec (signal d : out pair; v : std_logic) is
    begin
        d <= (x => v, y => not v);
        wait for 1 ns;
        d.x <= not v;
        wait for 1 ns;
    end procedure;

    signal s0, s1, s2 : std_logic := 'U';
    signal rx, ry     : std_logic := 'U';

begin

    main: process is
    begin
        -- Two elements of one formal, named by index and out of order
        drive_vec (d(1) => s1, d(0) => s0, v => '1');
        assert s0 = '1' and s1 = '1'
            report "vec: " & to_string (s0) & to_string (s1) severity failure;

        -- One element, from a scalar signal
        drive_one (d(0) => s2, v => '1');
        assert s2 = '1' report "one: " & to_string (s2) severity failure;

        -- The record form, referred to whole and by part
        drive_rec (d.x => rx, d.y => ry, v => '1');
        assert rx = '0' and ry = '0'
            report "rec: " & to_string (rx) & to_string (ry) severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
        wait;
    end process;

end architecture;
