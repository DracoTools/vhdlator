-- DESCRIPTION: Verilator: VHDL conversion to an array type whose element is open
--
-- "slv_array_t(a)" with both index and element left open takes both from
-- the operand (9.3.6), so a constant declared with only the type mark is
-- as long as a, and so is each element (GHDL gna issue2657).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_convelem is
end entity;

architecture test of t_vhdl_convelem is
    type slv_array_t is array (integer range <>) of std_logic_vector;
    type uns_array_t is array (integer range <>) of unsigned;

    constant a : uns_array_t(0 to 1)(2 downto 0) := ("101", "011");
    constant b : slv_array_t := slv_array_t(a);
begin
    process is
        constant c : slv_array_t := slv_array_t(a);
    begin
        assert b'length = 2 and b'left = 0 severity failure;
        assert b(0)'length = 3 and b(0)'left = 2 severity failure;
        assert b(0) = "101" and b(1) = "011" severity failure;
        assert c(1) = "011" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
