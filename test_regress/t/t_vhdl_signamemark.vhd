-- DESCRIPTION: Verilator: VHDL 'PATH_NAME of a subprogram whose parameter
-- is declared with a constrained subtype indication
--
-- The signature in a subprogram's path name lists the type marks of its
-- parameters (16.2.5): "t : std.standard.natural range 0 to 4" is NATURAL,
-- though the subtype the constraint makes has no name of its own (GHDL gna
-- issue1268).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_signamemark is
end entity;

architecture behav of t_vhdl_signamemark is
  subtype byte is bit_vector(7 downto 0);
  procedure p3(a, b : byte) is
  begin
    null;
  end procedure;
  procedure p4(t : std.standard.natural range 0 to 4) is
  begin
    null;
  end procedure;
  function f5(x : integer range 1 to 3; v : bit_vector(1 to 2)) return boolean is
  begin
    return true;
  end function;
begin
  process is
  begin
    assert p3'path_name = ":t_vhdl_signamemark:p3[byte,byte]:" severity failure;
    assert p4'path_name = ":t_vhdl_signamemark:p4[natural]:" severity failure;
    assert f5'path_name = ":t_vhdl_signamemark:f5[integer,bit_vector return boolean]:"
      severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
