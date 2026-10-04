-- DESCRIPTION: Verilator: VHDL constant of a generate's block given by a
-- function the block declares
--
-- A for generate's block declares a function that reads the generate
-- parameter, and a constant of the block is given what it returns (NVC
-- issue668). The constant is an object given its value, not a parameter:
-- the function lives in the block, and a function under a generate cannot
-- be one the back end runs to settle a parameter. A constant of the block
-- given by the architecture's own function still sizes a signal.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_genlocalfunc is
end entity;

architecture test of t_vhdl_genlocalfunc is
    function width_of (i : integer) return integer is
    begin
        return i + 2;
    end function;

    type int_array is array (0 to 3) of integer;
    signal counts : int_array := (others => -1);
    type bv_array is array (0 to 3) of bit_vector(7 downto 0);
    signal parts : bv_array := (others => (others => '0'));
begin
    g : for channel in 0 to 3 generate
        function popcount_channel return integer is
            constant vec : std_logic_vector(2 downto 0) := std_logic_vector(to_unsigned(channel, 3));
            variable num : integer := 0;
        begin
            for i in vec'range loop
                if vec(i) = '1' then
                    num := num + 1;
                end if;
            end loop;
            return num;
        end function;

        constant k : integer := popcount_channel;
        constant w : integer := width_of(channel);
        signal part : bit_vector(w - 1 downto 0);
    begin
        counts(channel) <= k;
        part <= (others => '1');
        parts(channel)(w - 1 downto 0) <= part;
    end generate;

    process is
    begin
        wait for 1 ns;
        assert counts = (0, 1, 1, 2) severity failure;
        assert parts(0) = "00000011" and parts(3) = "00011111" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
