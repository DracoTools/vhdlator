-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Declarations that analysis must reject: names that denote nothing, names
-- that denote the wrong kind of thing, and a declaration that illegally
-- hides another in the same region (IEEE 1076-2008 clause 12.3).

package p is
  -- No such type
  signal a : no_such_type;
  -- A constant is not a type
  constant K : integer := 1;
  signal b : K;
  -- No such library or package
  signal c : nowhere.something.t;
  -- Two declarations of one name in one region
  constant dup : integer := 1;
  constant dup : integer := 2;
  -- A resolution function that is not a function
  signal d : K bit;
end package;
