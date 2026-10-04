-- DESCRIPTION: Verilator: VHDL tables of widths of an unconstrained type,
-- computed by functions from a generic
--
-- NVC issue820's shape: constants of an array type the text leaves open,
-- each given what a function returns -- the widths from "num_arr(G)", whose
-- result is a local "t_arr(0 to n - 1)", and where each field starts and
-- ends from functions over those widths, whose results are as long as the
-- parameter. Each constant is as long as the call's result says, a length
-- the instance settles, and a generate reads the tables in its widths and
-- in the bounds of its slices, plainly and through a slice of a slice.
-- An instance with a generic of its own checks the lengths follow it.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_widthtableopen_sub is
    generic (G_NUM : integer := 4);
    port (dout, dout2 : out std_logic_vector(8 * G_NUM - 1 downto 0));
end entity;

architecture rtl of t_vhdl_widthtableopen_sub is
    type t_arr is array (natural range <>) of natural;

    function num_arr (n : natural) return t_arr is
        variable w : t_arr(0 to n - 1) := (others => 8);
    begin
        return w;
    end function;

    function bit_low (w : t_arr) return t_arr is
        variable r : t_arr(w'range) := (others => 0);
    begin
        for i in w'range loop
            if i /= w'low then
                r(i) := r(i - 1) + w(i - 1);
            end if;
        end loop;
        return r;
    end function;

    function bit_high (w : t_arr) return t_arr is
        variable r : t_arr(w'range) := (others => 0);
    begin
        for i in w'range loop
            if i = w'low then
                r(i) := w(i) - 1;
            else
                r(i) := r(i - 1) + w(i);
            end if;
        end loop;
        return r;
    end function;

    constant C_W : t_arr := num_arr(G_NUM);
    constant C_L : t_arr := bit_low(C_W);
    constant C_H : t_arr := bit_high(C_W);
begin
    assert C_W'length = G_NUM and C_L'length = G_NUM and C_H(C_H'right) = 8 * G_NUM - 1
        report "table lengths" severity failure;

    g : for i in 0 to G_NUM - 1 generate
        signal d : unsigned(C_W(i) - 1 downto 0) := to_unsigned(i + 1, C_W(i));
    begin
        dout(C_H(i) downto C_L(i)) <= std_logic_vector(d);
        dout2(C_H(i to i)(i) downto C_L(i to i)(i)) <= std_logic_vector(d + 16);
    end generate;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_widthtableopen is
end entity;

architecture test of t_vhdl_widthtableopen is
    signal a, a2 : std_logic_vector(31 downto 0);
    signal b, b2 : std_logic_vector(15 downto 0);
begin
    u4 : entity work.t_vhdl_widthtableopen_sub port map (a, a2);
    u2 : entity work.t_vhdl_widthtableopen_sub generic map (G_NUM => 2) port map (b, b2);

    process is
    begin
        wait for 1 ns;
        assert a = x"04030201" report "a" severity failure;
        assert a2 = x"14131211" report "a2" severity failure;
        assert b = x"0201" report "b" severity failure;
        assert b2 = x"1211" report "b2" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
