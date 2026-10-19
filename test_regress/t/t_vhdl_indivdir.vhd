-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An unconstrained formal associated element by element takes the range its
-- choices name, running as its index subtype does: NATURAL's, ascending, for a
-- BIT_VECTOR (6.5.7.1; GHDL gna issue2346, as GHDL and Questa run it)

entity t_vhdl_indivdir is
end entity;

architecture a of t_vhdl_indivdir is
  signal a1, a0 : bit;
  function f return bit_vector is
  begin
    return "01";
  end function;
  procedure asgn (signal o : out bit_vector) is
  begin
    assert o'ascending and o'left = 0 and o'right = 1 severity failure;
    o <= f after 1 ns;
  end procedure;
begin
  process is
  begin
    asgn(o(1) => a1, o(0) => a0);
    wait for 2 ns;
    assert a1 = '1' severity failure;
    assert a0 = '0' severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
