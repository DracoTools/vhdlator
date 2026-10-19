-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An attribute of an element of the record a call gives back is a value
-- elaboration folds (GHDL gna issue1138, where nvc and Questa agree and GHDL
-- wants -frelaxed): the element is as long as the call made it.

entity t_vhdl_callelemattr is
end entity;

architecture behav of t_vhdl_callelemattr is
  type rec is record
    bv : bit_vector;
    n : natural;
  end record;

  function get (n : natural) return rec is
  begin
    return (bv => (1 to n => '0'), n => n);
  end function;

  constant l : natural := get(5).bv'length;
  constant r : natural := get(3).bv'right;
  constant k : natural := get(7).n;
  signal s : bit_vector(1 to l);
begin
  process begin
    assert l = 5 report "length " & integer'image(l) severity failure;
    assert r = 3 report "right " & integer'image(r) severity failure;
    assert k = 7 report "n " & integer'image(k) severity failure;
    assert s'length = 5 severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
