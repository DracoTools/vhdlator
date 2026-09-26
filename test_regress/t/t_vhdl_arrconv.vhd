-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A conversion between two array types whose elements are integers --
-- "integer_vector(val)" over a t_natural_vector, as UVVM's to_string and
-- random generator write it (uvvm_util/src/string_methods_pkg.vhd) -- was
-- taken for a reinterpretation of bits and made one number, which the
-- subprogram it was handed to refused.
entity t_vhdl_arrconv is end entity;
architecture a of t_vhdl_arrconv is
  type t_natural_vector is array (natural range <>) of natural;
  function sum(v : integer_vector) return integer is
    variable s : integer := 0;
  begin
    for i in v'range loop
      s := s + v(i);
    end loop;
    return s;
  end function;
  function nsum(val : t_natural_vector) return integer is
  begin
    return sum(integer_vector(val));
  end function;
begin
  process
    variable one : t_natural_vector(0 to 0) := (0 => 7);
    variable three : t_natural_vector(1 to 3) := (1, 2, 3);
  begin
    assert nsum(one) = 7 report integer'image(nsum(one)) severity failure;
    assert nsum(three) = 6 report integer'image(nsum(three)) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
