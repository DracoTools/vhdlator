-- DESCRIPTION: Verilator: VHDL external name through one iteration of a for generate
--
-- "<< constant g(2).uut.k : integer >>" names the object k of the
-- instance uut in the iteration of generate g where the parameter is 2
-- (8.7).  A generate whose iterations each drive a resolved signal is
-- written out one iteration at a time, and is reached the same way.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_extnamegen_bot is
    generic (N : integer);
end entity;

architecture test of t_vhdl_extnamegen_bot is
    constant k : integer := N * 10;
    signal x : integer := N;
begin
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_extnamegen is
end entity;

architecture test of t_vhdl_extnamegen is
    signal bus_line : std_logic;
begin

    g : for i in 1 to 3 generate
        uut : entity work.t_vhdl_extnamegen_bot generic map (N => i);
    end generate;

    -- Each iteration drives bus_line, so each is a driver of its own
    w : for i in 4 to 5 generate
        uut : entity work.t_vhdl_extnamegen_bot generic map (N => i);
        bus_line <= 'Z';
    end generate;

    p1 : process is
    begin
        assert << constant g(1).uut.k : integer >> = 10 severity failure;
        assert << constant g(2).uut.k : integer >> = 20 severity failure;
        assert << constant g(3).uut.k : integer >> = 30 severity failure;
        assert << signal g(2).uut.x : integer >> = 2 severity failure;
        assert << constant w(4).uut.k : integer >> = 40 severity failure;
        assert << constant w(5).uut.k : integer >> = 50 severity failure;
        << signal g(1).uut.x : integer >> <= force 7;
        wait for 1 ns;
        assert << signal g(1).uut.x : integer >> = 7 severity failure;
        assert << signal g(3).uut.x : integer >> = 3 severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
