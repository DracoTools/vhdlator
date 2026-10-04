-- DESCRIPTION: Verilator: VHDL operators a design declares over unconstrained arrays
--
-- "+" (a, b : tv) gives a result as long as its longer operand, as its body
-- makes it, and "+" (a : tv; b : trit) returns "a + tv'(1 => b)". The
-- result of a design's own operator is what its body gives, not an
-- operand's length: it was taken for one, so the second came out one
-- element long, and its value was mistaken for a carried one or not (GHDL
-- gna issue2536).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_useropres_pkg is
  type trit is (N, O, P);
  type tv is array (integer range <>) of trit;
  function "+" (a, b: trit) return trit;
  function "+" (a, b: tv) return tv;
  function "+" (a: tv; b: trit) return tv;
end package;
package body t_vhdl_useropres_pkg is
  function "+" (a, b: trit) return trit is
  begin
    if a = O then return b; end if;
    return a;
  end function;
  function "+" (a, b: tv) return tv is
    constant width: natural := maximum(a'length, b'length);
    variable x, y : tv(width-1 downto 0) := (others => O);
  begin
    x(a'length-1 downto 0) := a;
    y(b'length-1 downto 0) := b;
    for i in x'range loop
      x(i) := x(i) + y(i);
    end loop;
    return x;
  end function;
  function "+" (a: tv; b: trit) return tv is
  begin
    return a + tv'(1 => b);
  end function;
end package body;
use work.t_vhdl_useropres_pkg.all;

entity t_vhdl_useropres is
    generic (width : positive := 3);
end entity;

architecture test of t_vhdl_useropres is
    signal x, y, z : tv(width - 1 downto 0) := (others => O);
    signal c : trit;
begin
    -- As long as the generic makes it, so spread through an expression
    (c, z) <= (O & x) + y + O;

    process is
        variable v : tv(width downto 0);
        variable w : tv(width - 1 downto 0);
    begin
        x <= (O, P, N);
        wait for 1 ns;
        assert z = (O, P, N) and c = O severity failure;
        v := (O & x) + y + O;
        assert v = (O, O, P, N) severity failure;
        w := x + P;
        assert w = (O, P, N) severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
