-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A function whose result has no length of its own, written into a slice
-- whose ends name a generate or a loop parameter: neither end is a number,
-- but their distance is. "res_int(op_brev8_c)(i*8+7 downto i*8) <=
-- bit_rev_f(...)", as neorv32's bit-manipulation unit reverses each byte.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_genslicedyn is
end entity;

architecture a of t_vhdl_genslicedyn is
  function bit_rev_f(d : std_ulogic_vector) return std_ulogic_vector is
    variable r : std_ulogic_vector(d'length-1 downto 0);
  begin
    for i in 0 to d'length-1 loop
      r((d'length-1)-i) := d(d'low + i);
    end loop;
    return r;
  end function;
  type res_t is array (2 downto 0) of std_ulogic_vector(31 downto 0);
  signal res_int : res_t;
  signal rs1 : std_ulogic_vector(31 downto 0) := x"1234A6C8";
  signal v2 : std_ulogic_vector(31 downto 0);
  signal up : std_ulogic_vector(0 to 15);
  signal w2 : std_ulogic_vector(0 to 31);
begin
  res_int(0) <= rs1;
  res_int(2) <= (others => '0');
  -- Into a slice of an element, running down
  brev_gen : for i in 0 to 3 generate
    res_int(1)(i*8+7 downto i*8) <= bit_rev_f(rs1(i*8+7 downto i*8));
  end generate;
  -- Into a slice of a signal, running up
  up_gen : for i in 0 to 1 generate
    up(i*8 to i*8+7) <= bit_rev_f(rs1(i*8+7 downto i*8));
  end generate;
  -- Into slices of variables, by a loop parameter
  process (rs1)
    variable v : std_ulogic_vector(31 downto 0);
    variable w : std_ulogic_vector(0 to 31);
  begin
    for k in 0 to 3 loop
      v(k*8+7 downto k*8) := bit_rev_f(rs1(31-k*8 downto 24-k*8));
      w(k*8 to k*8+7) := bit_rev_f(rs1(k*8+7 downto k*8));
    end loop;
    v2 <= v;
    w2 <= w;
  end process;

  process
  begin
    wait for 1 ns;
    assert res_int(1) = x"482C6513" report "res_int(1) " & to_hstring(res_int(1)) severity failure;
    assert v2 = x"13652C48" report "v2 " & to_hstring(v2) severity failure;
    assert up = x"1365" report "up " & to_hstring(up) severity failure;
    assert w2 = x"13652C48" report "w2 " & to_hstring(w2) severity failure;
    rs1 <= x"80000001";
    wait for 1 ns;
    assert res_int(1) = x"01000080" report "res_int(1) " & to_hstring(res_int(1)) severity failure;
    assert v2 = x"80000001" report "v2 " & to_hstring(v2) severity failure;
    assert up = x"8000" report "up " & to_hstring(up) severity failure;
    assert w2 = x"80000001" report "w2 " & to_hstring(w2) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
