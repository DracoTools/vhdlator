-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An assignment's target is an object, or an aggregate naming objects each
-- once by a locally static name. A port of mode linkage is read only by
-- being given to another of mode linkage. GUARD is its block's guard
-- expression and nothing else assigns it. An actual is of its formal's
-- type. References: nvc 1.22.1, GHDL 7.0.0-dev, Questa 2025.2.

entity t_vhdl_targets_sub is
  port (i : in bit; l : linkage bit);
end entity;

architecture a of t_vhdl_targets_sub is
  signal s : bit;
begin
  -- Illegal
  s <= l;
end architecture;

entity t_vhdl_targets_bad is
end entity;

architecture a of t_vhdl_targets_bad is
  type bits is array (1 to 2) of bit;
  function f (x : integer) return integer is
  begin
    return x;
  end function;
  signal sb : bit;
  signal sv : bit_vector(1 to 2);
begin
  -- Legal
  ok : entity work.t_vhdl_targets_sub port map (i => sb, l => sb);
  blk : block (sb = '1')
  begin
    sb <= guarded '1';
  end block;
  process
    variable x, y : integer;
    variable n : integer := 1;
    variable v : bits;
  begin
    (x, y) := integer_vector'(1, 2);
    (v(1), v(2)) := bits'("10");
    -- Illegal
    f(x) := 1;
    (x + y) := 2;
    ns := 3 ns;
    (v(n), v(2)) := bits'("01");
    (x, x) := integer_vector'(3, 4);
    wait;
  end process;
  -- Illegal
  blk2 : block (sb = '1') begin guard <= false; end block;
  bad : entity work.t_vhdl_targets_sub port map (i => sv, l => sb);
end architecture;
