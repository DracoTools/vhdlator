-- DESCRIPTION: Verilator: VHDL unit named through the working library
--
-- Every unit sees library WORK without a library clause (13.2), so
-- "work.nosuch_pkg.k" needs work.nosuch_pkg as a use clause would, and is
-- reported as the missing unit it is rather than as a name that denotes
-- nothing.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_workdep_bad is
end entity;

architecture test of t_vhdl_workdep_bad is
    constant c : integer := work.nosuch_pkg.k;
begin
end architecture;
