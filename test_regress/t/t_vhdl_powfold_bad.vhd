-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- "2 ** 31" of INTEGER has no value (5.2.3): an overflow, which a fold must
-- not hide by working in sixty-four bits (GHDL gna issue683; nvc stops on the
-- overflow, and so does GHDL 7.0.0-dev; Questa fails the assertion).

entity t_vhdl_powfold_bad is
end entity;

architecture behav of t_vhdl_powfold_bad is
  function exp2 (b : integer) return integer is
  begin
    return 2 ** b;
  end function;
begin
  assert exp2(3) = 8 severity failure;
  assert exp2(31) > 0 report "2 ** 31 is not positive" severity failure;
end architecture;
