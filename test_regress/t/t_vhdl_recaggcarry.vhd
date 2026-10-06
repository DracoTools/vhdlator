-- DESCRIPTION: Verilator: VHDL record aggregate with a fixed value for a carried element
--
-- A function returns a record whose element type is unconstrained, and
-- builds the result from an aggregate whose element value is a vector of
-- fixed length: "data => to_slv(sig.data)". The element is held as a
-- carrier, so the vector is put into one (GHDL gna issue3034).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.fixed_pkg.all;

entity t_vhdl_recaggcarry is
end entity;

architecture arch of t_vhdl_recaggcarry is
  type rec_t is record
    valid : std_logic;
    data : std_logic_vector;
  end record;
  type rec2_t is record
    valid : std_logic;
    data : sfixed;
  end record;
  function f(sig : rec2_t) return rec_t is
  begin
    return (valid => sig.valid, data => to_slv(sig.data));
  end function;
  signal b : rec2_t(data(3 downto -4));
  signal a : rec_t(data(7 downto 0));
begin
  a <= f(b);

  process
  begin
    b <= ('1', to_sfixed(-3, 3, -4));
    wait for 1 ns;
    assert a.valid = '1' severity failure;
    assert a.data = x"d0" severity failure;
    b <= ('0', to_sfixed(2.5, 3, -4));
    wait for 1 ns;
    assert a.valid = '0' severity failure;
    assert a.data = x"28" severity failure;
    report "recaggcarry done";
    std.env.finish;
  end process;
end architecture;
