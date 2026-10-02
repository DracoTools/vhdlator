-- DESCRIPTION: Verilator: VHDL external name starting one region up, from a unit two units instantiate
--
-- Which region "^" is depends on the instance, and the two are different
-- modules here, so the name cannot be written from either.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_extnameup_bad_leaf is
end entity;

architecture test of t_vhdl_extnameup_bad_leaf is
begin
    process is
    begin
        << signal ^.s : natural >> <= 5;
        wait;
    end process;
end architecture;

entity t_vhdl_extnameup_bad_mid is
end entity;

architecture test of t_vhdl_extnameup_bad_mid is
    signal s : natural;
begin
    u : entity work.t_vhdl_extnameup_bad_leaf;
end architecture;

entity t_vhdl_extnameup_bad is
end entity;

architecture test of t_vhdl_extnameup_bad is
    signal s : natural;
begin
    a : entity work.t_vhdl_extnameup_bad_leaf;
    b : entity work.t_vhdl_extnameup_bad_mid;
end architecture;
