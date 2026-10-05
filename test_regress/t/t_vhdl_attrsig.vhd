-- DESCRIPTION: Verilator: VHDL attribute specification of one overload,
-- chosen by its signature
--
-- "attribute built_in of increment [bit_vector return bit_vector] :
-- function is bv_incr" decorates that increment and not the other, and the
-- other may be given a value of its own; so may two enumeration literals
-- named the same, by the type a signature names (7.2; VESTS ashenden
-- ch_20_ch_20_08).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_attrsig is
end entity;

architecture a of t_vhdl_attrsig is
  type built_in_type is (none, bv_incr, int_incr);
  attribute built_in : built_in_type;
  attribute representation : string;
  type speed_range is (stop, slow, high);
  type coolant_level is (empty, low, high);
  function increment(v : bit_vector) return bit_vector is
  begin
    return not v;
  end function;
  function increment(v : integer) return integer is
  begin
    return v + 1;
  end function;
  attribute built_in of increment [bit_vector return bit_vector] : function is bv_incr;
  attribute built_in of increment [integer return integer] : function is int_incr;
  attribute representation of high [return speed_range] : literal is "byte";
  attribute representation of high [return coolant_level] : literal is "word";
begin
  process is
  begin
    assert increment [bit_vector return bit_vector]'built_in = bv_incr severity failure;
    assert increment [integer return integer]'built_in = int_incr severity failure;
    assert high [return speed_range]'representation = "byte" severity failure;
    assert high [return coolant_level]'representation = "word" severity failure;
    assert increment(3) = 4 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
