-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- The prefix of a selected name is a name or a function call (8.3), so a
-- value written out in place is not one however plain it is.

entity t_vhdl_selprefix_bad is
end entity;

architecture tb of t_vhdl_selprefix_bad is
  type r1 is record
    re1 : boolean;
    re2 : integer;
  end record;
begin

  process is
    variable v1 : boolean;
    variable n : integer;
    variable r : r1;
  begin
    -- A qualified expression is not a name
    v1 := r1'(re1 => true, re2 => 2).re1;

    -- Nor is an aggregate
    n := r1'(re1 => true, re2 => 2).re2;

    -- What is legal, for contrast
    r := (re1 => true, re2 => 2);
    v1 := r.re1;
    n := r.re2;
    wait;
  end process;

end architecture;
