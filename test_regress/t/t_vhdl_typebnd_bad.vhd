-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- What a numeric type definition declares follows from its bounds: two
-- integers make an integer type and two reals a floating point type, so a
-- bound of any other type, or one of each, says nothing coherent. A
-- physical type counts in its primary unit, so its bounds are integers.

entity t_vhdl_typebnd_bad is
end entity;

architecture a of t_vhdl_typebnd_bad is
  constant k : integer := 7;
  type ok1 is range 0 to k;
  type ok2 is range -1.0 to 1.0e3;
  type bad1 is range 1 to 9.0;
  type bad2 is range false to true;
  type bad3 is range 0.0 to "999";
  type bad4 is range 0.0 to 100.0
    units
      u;
    end units;
begin
end architecture;
