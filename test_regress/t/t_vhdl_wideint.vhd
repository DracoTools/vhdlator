-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_wideint is
end entity;

architecture a of t_vhdl_wideint is

  -- An integer type of the language whose range needs more than the
  -- thirty-two bits the predefined INTEGER has (5.2.3)
  type big_t is range -2**40 to 2**40;

  function twice(v : big_t) return big_t is
  begin
    return v * 2;
  end function;

  signal s : big_t := 0;

begin

  main : process
    variable x : big_t;
    variable y : big_t := 3;
  begin
    x := big_t(integer'high) + 1;
    assert x = 2147483648 report "x is " & big_t'image(x) severity failure;

    x := x * 512;
    assert x = 1099511627776 report "x*512 is " & big_t'image(x) severity failure;

    assert twice(y) = 6 report "twice" severity failure;
    assert big_t'low = -1099511627776 report "low" severity failure;
    assert big_t'high = 1099511627776 report "high" severity failure;

    s <= x;
    wait for 1 ns;
    assert s = 1099511627776 report "s is " & big_t'image(s) severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;

end architecture;
