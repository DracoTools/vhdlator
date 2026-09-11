-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- The generic map of an interface package names generics of the package it
-- is new of (6.5.7.2). References: nvc 1.22.1, GHDL 7.0.0-dev, Questa
-- 2025.2.

package pack1 is
  generic (x : integer := 5; y : integer := 2);
end package;

package pack3 is
  component comp1 is
    generic (
      package p is new work.pack1 generic map (x => 2));
  end component;
  component comp2 is generic (package p is new work.pack1 generic map (z => 2)); end component;  -- Bad
end package;
