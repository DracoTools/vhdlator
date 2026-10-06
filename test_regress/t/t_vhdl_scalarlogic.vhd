-- DESCRIPTION: Verilator: VHDL scalar-array logical operators with the scalar on the left
--
-- "e and a" with e a BIT and a a BIT_VECTOR is VHDL-2008's scalar-array
-- operator, element by element (9.2.2). With the BIT on the left it was
-- taken as the short-circuit operator of two BITs (GHDL synth issue2327).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_scalarlogic is
end entity;

architecture a of t_vhdl_scalarlogic is
  signal a : bit_vector(7 downto 0) := b"01000111";
  signal e : bit := '1';
begin
  process
  begin
    wait for 1 ns;
    assert (e and a) = b"01000111" severity failure;
    assert (e nand a) = b"10111000" severity failure;
    assert (e or a) = b"11111111" severity failure;
    assert (e nor a) = b"00000000" severity failure;
    assert ('0' or a) = b"01000111" severity failure;
    assert (a and e) = (e and a) severity failure;
    report "scalarlogic done";
    std.env.finish;
  end process;
end architecture;
