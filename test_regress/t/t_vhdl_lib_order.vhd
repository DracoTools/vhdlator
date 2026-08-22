-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- The design unit that depends on everything else, given to Verilator first.
-- IEEE 1076-2008 clause 13.5 requires it to be analyzed last.

use work.helper.all;

entity t is
end entity;

architecture rtl of t is
  constant c : integer := K;
begin
end architecture;
