-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- The package declaration, given last but analyzed first.

package helper is
  constant K : integer := 4;
  function twice(x : integer) return integer;
end package;
