-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_dyncall is
end entity;

library ieee;
use ieee.std_logic_1164.all;

architecture test of t_vhdl_dyncall is
    signal rd : std_logic_vector(7 downto 0) := X"5a";

    -- A parameter the body writes and whose bounds it reads carries them
    -- with it, and the procedure also takes a signal, so the call is one
    -- whose signal parameters are bound to the actuals
    procedure take (variable data_value : out std_logic_vector;
                    signal   s : in std_logic_vector) is
        variable v : std_logic_vector(data_value'range);
    begin
        for i in v'range loop v(i) := s(i); end loop;
        data_value := v;
    end procedure;

begin

    chk : process is
        variable buf : std_logic_vector(7 downto 0) := (others => '0');
        variable whole : std_logic_vector(7 downto 0) := (others => '0');
        variable n : natural := 4;
    begin
        -- The actual is a slice whose ends only running settles, so nothing
        -- about it has a length until the call is made
        take(buf(n - 1 downto 0), rd);
        assert buf = X"0a" report "slice: " & to_string(buf) severity failure;

        take(whole, rd);
        assert whole = X"5a" report "whole: " & to_string(whole) severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
