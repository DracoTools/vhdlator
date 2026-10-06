-- DESCRIPTION: Verilator: VHDL record formal associated element by element, an element open
--
-- "is_0(v.a => '1', v.w => a)" with w of an open array type: the record the
-- function is handed holds w as a carrier, so the vector given for it is
-- put into one (GHDL synth func03, func06).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_recaggarg is
end entity;

architecture behav of t_vhdl_recaggarg is
  type my_rec is record
    a : bit;
    w : bit_vector;
  end record;

  function is_0 (v : my_rec) return bit is
  begin
    if v.w = "0011" then
      return v.a;
    else
      return '0';
    end if;
  end function;

  signal a : bit_vector(3 downto 0) := "0011";
  signal o : bit;
begin
  o <= is_0(v.a => '1', v.w => a);

  process
  begin
    wait for 1 ns;
    assert o = '1' severity failure;
    a <= "0111";
    wait for 1 ns;
    assert o = '0' severity failure;
    report "recaggarg done";
    std.env.finish;
  end process;
end architecture;
