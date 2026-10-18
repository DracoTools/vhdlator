-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A signal of a BIT subtype that names a resolution function of the design's
-- own is resolved by calling it, side effects and all, not as a wired net
-- (NVC driver15): it reports what it resolves to

entity t_vhdl_userres_bit is
end entity;

architecture test of t_vhdl_userres_bit is
  function resolved (x : bit_vector) return bit is
    variable r : bit := '0';
  begin
    for i in x'range loop
      r := r or x(i);
    end loop;
    report "result is " & bit'image(r);
    return r;
  end function;

  subtype rbit is resolved bit;

  signal s : rbit;
begin
  p1 : s <= '0';

  p2 : process is
  begin
    wait for 1 ns;
    s <= '1';
    wait for 1 ns;
    assert s = '1' severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
