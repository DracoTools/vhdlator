-- DESCRIPTION: Verilator: VHDL user attribute of a design unit, read from another unit
--
-- An attribute declared in an entity and given to that entity is read
-- back as "work.e'attr" from another design unit, where the attribute's
-- declaration is not visible (7.2, 16.2).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_unitattr_sub is
    constant MIN_DELAY : natural := 42;
    attribute delay : natural;
    attribute delay of t_vhdl_unitattr_sub : entity is MIN_DELAY;
    attribute tag : string;
    attribute tag of t_vhdl_unitattr_sub : entity is "sub";
end entity;

architecture rtl of t_vhdl_unitattr_sub is
begin
end architecture;

entity t_vhdl_unitattr is
end entity;

architecture test of t_vhdl_unitattr is
    constant fumble : natural := work.t_vhdl_unitattr_sub'delay;
begin
    u : entity work.t_vhdl_unitattr_sub;

    process is
    begin
        assert fumble = 42 severity failure;
        assert work.t_vhdl_unitattr_sub'delay = 42 severity failure;
        assert work.t_vhdl_unitattr_sub'tag = "sub" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
