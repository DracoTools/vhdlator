-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- "mod" takes the sign of its right operand (9.2.7). It was worked out as
-- "((a rem b) + b) rem b", whose "+ b" overflows INTEGER when both are
-- large: VUnit's get_uniform_seed (vunit/vhdl/run/src/run.vhd), "1 +
-- (to_integer(hash(62 downto 32)) mod 2147483562)", gave a negative seed.
entity t_vhdl_modlarge is
end entity;

architecture a of t_vhdl_modlarge is
begin
  process
    variable a, b : integer;
    procedure check(x, y, expected : integer) is
    begin
      assert x mod y = expected
        report integer'image(x) & " mod " & integer'image(y) & " = " & integer'image(x mod y)
        severity failure;
    end procedure;
  begin
    check(7, 3, 1);
    check(-7, 3, 2);
    check(7, -3, -2);
    check(-7, -3, -1);
    check(6, 3, 0);
    check(-6, 3, 0);
    check(6, -3, 0);
    check(1289994363, 2147483562, 1289994363);
    check(2147483647, 2147483562, 85);
    check(-2147483000, 2147483562, 562);
    check(2147483000, -2147483562, -562);
    a := 1289994363;
    b := 2147483562;
    assert 1 + (a mod b) = 1289994364 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
