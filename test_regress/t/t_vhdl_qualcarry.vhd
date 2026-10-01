-- DESCRIPTION: Verilator: VHDL qualified call as an unconstrained actual
--
-- A qualified expression leaves its operand's bounds as they are (9.3.5), so
-- "ufixed'(f(x))" handed to an unconstrained formal is as long as the call
-- made it.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.fixed_pkg.all;

entity t_vhdl_qualcarry is
end entity;

architecture test of t_vhdl_qualcarry is

    procedure chk (got : ufixed; expected : ufixed; l, r : integer) is
    begin
        assert got = expected severity failure;
        assert got'left = l and got'right = r severity failure;
        assert got'length = l - r + 1 severity failure;
    end procedure;

    procedure chks (got : sfixed; v : real; l, r : integer) is
    begin
        assert to_real(got) = v severity failure;
        assert got'left = l and got'right = r severity failure;
    end procedure;

begin

    process is
    begin
        chk(ufixed'(from_hstring("A5", 3, -4)), ufixed'(from_hstring("A5", 3, -4)), 3, -4);
        chk(ufixed'(from_hstring("A5A5A5A5A", 31, -4)),
            ufixed'(from_hstring("A5A5A5A5A", 31, -4)), 31, -4);
        chk(ufixed'(from_hstring("A5", 3, -4)), to_ufixed(10.3125, 3, -4), 3, -4);
        chks(sfixed'(from_hstring("A5", 3, -4)), -5.6875, 3, -4);
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
