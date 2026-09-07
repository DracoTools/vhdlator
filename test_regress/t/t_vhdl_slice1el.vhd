-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A slice of one element is written the same way as any other, and where the
-- value comes from something whose length only running decides the elements
-- are copied one at a time. One element is a place, not an array of one, and
-- has no bit range to select from. VUnit's queue tests write "bv(0 to 0) :=
-- pop_bit_vector(queue)".

package t_vhdl_slice1el_pkg is
    function pull (n : natural) return bit_vector;
end package;

package body t_vhdl_slice1el_pkg is
    -- The length is what the call passes, so nothing here says it
    function pull (n : natural) return bit_vector is
        variable v : bit_vector (0 to n) := (others => '1');
    begin
        v (0) := '1';
        return v;
    end function;
end package body;

use work.t_vhdl_slice1el_pkg.all;

entity t_vhdl_slice1el is
end entity;

architecture a of t_vhdl_slice1el is
begin

    main: process is
        variable bv   : bit_vector (0 to 5) := (others => '0');
        variable down : bit_vector (7 downto 0) := (others => '0');
        variable one  : bit_vector (0 to 0) := "0";
        variable n    : natural := 0;
    begin
        -- One element, ascending
        bv (0 to 0) := pull (n);
        assert bv = "100000" report "one element at the left" severity failure;

        bv (4 to 4) := pull (n);
        assert bv = "100010" report "one element in the middle" severity failure;

        -- One element, descending
        down (5 downto 5) := pull (n);
        assert down = "00100000" report "one element descending" severity failure;

        -- A whole object of one element, which is an array and not a place
        one := pull (n);
        assert one = "1" report "whole one-element object" severity failure;

        -- More than one, which already worked
        n := 1;
        bv (2 to 3) := pull (n);
        assert bv = "101110" report "two elements" severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
        wait;
    end process;

end architecture;
