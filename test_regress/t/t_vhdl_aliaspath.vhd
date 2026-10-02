-- DESCRIPTION: Verilator: VHDL path attributes of an alias
--
-- An alias is a named entity of its own (6.6.1): its 'PATH_NAME and
-- 'INSTANCE_NAME end in its own name, as its 'SIMPLE_NAME does.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_aliaspath is
end entity;

architecture arch of t_vhdl_aliaspath is
    signal clock : bit;
    signal data : bit_vector(7 downto 0);
    alias aclock is clock;
    alias aclock2 is aclock;
    alias low is data(3 downto 0);
begin
    process is
    begin
        assert clock'simple_name = "clock" severity failure;
        assert aclock'simple_name = "aclock" severity failure;
        assert aclock2'simple_name = "aclock2" severity failure;
        assert clock'instance_name = ":t_vhdl_aliaspath(arch):clock" severity failure;
        assert aclock'instance_name = ":t_vhdl_aliaspath(arch):aclock" severity failure;
        assert aclock2'instance_name = ":t_vhdl_aliaspath(arch):aclock2" severity failure;
        assert low'instance_name = ":t_vhdl_aliaspath(arch):low" severity failure;
        assert clock'path_name = ":t_vhdl_aliaspath:clock" severity failure;
        assert aclock'path_name = ":t_vhdl_aliaspath:aclock" severity failure;
        assert aclock2'path_name = ":t_vhdl_aliaspath:aclock2" severity failure;
        assert low'path_name = ":t_vhdl_aliaspath:low" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
