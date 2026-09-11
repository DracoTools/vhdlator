-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An index an array type leaves open is written "type_mark range <>"
-- (5.3.2.1), and a literal names no type. References: nvc 1.22.1, GHDL
-- 7.0.0-dev, Questa 2025.2.

package t is
  type ok_t is array (natural range <>) of bit;
  type bad_t is array (1 tnatural range <>) of bit;
end package;
