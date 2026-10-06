-- DESCRIPTION: Verilator: VHDL slice written with a constrained subtype indication
--
-- "msg(positive range 1 to 5)" slices over the range the indication's
-- constraint gives; the type mark only says what it is a range of (8.5,
-- 5.3.2.1). The slice was taken as the whole value (GHDL gna issue877).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_slicerangeind is
end entity;

architecture behav of t_vhdl_slicerangeind is
  constant msg : string := "hello world";
  signal s : string(1 to 11) := "hello world";
  signal v : bit_vector(7 downto 0) := x"a5";
begin
  assert msg(positive range 1 to 5) = "hello" severity failure;

  process
    variable lo : positive := 7;
  begin
    assert msg(positive range 7 to 11) = "world" severity failure;
    assert s(positive range 7 to 11) = "world" severity failure;
    assert s(positive range lo to 11) = "world" severity failure;
    assert v(natural range 7 downto 4) = x"a" severity failure;
    assert v(integer range 3 downto 0) = x"5" severity failure;
    report "slicerangeind done";
    std.env.finish;
  end process;
end architecture;
