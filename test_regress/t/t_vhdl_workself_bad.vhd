-- DESCRIPTION: Verilator: VHDL package naming itself through its library
--
-- A unit is put in its library when its analysis ends (13.1), so inside
-- the package itself "work.p.c" names nothing; "p.c" is the expanded name
-- to write there (8.3).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_workself_pkg is
    constant c : integer := 3;
    constant d : integer := work.t_vhdl_workself_pkg.c + 1;
end package;

entity t_vhdl_workself_bad is
end entity;

architecture test of t_vhdl_workself_bad is
begin
end architecture;
