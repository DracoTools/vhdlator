-- DESCRIPTION: Verilator: VHDL conversion written on part of an output port
--
-- "signed(res(7 downto 0)) => a, signed(res(15 downto 8)) => b": each
-- association converts its own slice of what the port drives on the way
-- out to its actual (6.5.7.1). The port is associated once, and each
-- conversion reads its part (GHDL gna issue530).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_convpart_sub is
    port (
        arg : in std_logic_vector(15 downto 0);
        res : out std_logic_vector(15 downto 0);
        q   : out std_logic_vector(1 downto 0)
    );
end entity;

architecture behav of t_vhdl_convpart_sub is
begin
    res <= arg(7 downto 0) & arg(15 downto 8);
    q <= arg(1 downto 0);
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_convpart is
end entity;

architecture test of t_vhdl_convpart is
    signal arg : std_logic_vector(15 downto 0) := x"1234";
    signal res_a, res_b : signed(7 downto 0);
    signal b0, b1 : bit;

    function tb (s : std_ulogic) return bit is
    begin
        if s = '1' then
            return '1';
        end if;
        return '0';
    end function;
begin
    u : entity work.t_vhdl_convpart_sub
        port map (
            arg => arg,
            signed(res(7 downto 0)) => res_a,
            signed(res(15 downto 8)) => res_b,
            tb(q(0)) => b0,
            tb(q(1)) => b1
        );

    process is
    begin
        wait for 1 ns;
        assert res_a = x"12" and res_b = x"34" severity failure;
        assert b0 = '0' and b1 = '0' severity failure;
        arg <= x"A5C3";
        wait for 1 ns;
        assert res_a = signed'(x"A5") and res_b = signed'(x"C3") severity failure;
        assert b0 = '1' and b1 = '1' severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
