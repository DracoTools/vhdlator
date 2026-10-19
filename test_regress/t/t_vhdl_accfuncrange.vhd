-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- "f'range" where f returns an access value is the range of the array it
-- designates (16.2.3; GHDL gna issue2147): elaboration runs f, and the
-- constant declared with it is as long as that array.

entity t_vhdl_accfuncrange is
end entity;

architecture behav of t_vhdl_accfuncrange is
  type bv_acc is access bit_vector;
  impure function f return bv_acc is
  begin
    return new bit_vector'("011");
  end function;
  constant c : bit_vector(f'range) := (others => '1');
begin
  process begin
    assert c'length = 3 and c'left = 0 and c'right = 2
      report integer'image(c'left) & " to " & integer'image(c'right) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
