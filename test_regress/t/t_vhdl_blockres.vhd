-- DESCRIPTION: Verilator: VHDL block ports driving a resolved signal
--
-- A port of a block associated with a signal is that signal (6.5.6.3), so a
-- process inside the block that drives the port is one of the signal's
-- drivers, resolved with the others; and its 'DRIVING is its own driver's.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_blockres is
end entity;

architecture test of t_vhdl_blockres is
    type int_vec is array (natural range <>) of integer;

    -- How many drivers are connected
    function count(x : int_vec) return integer is
    begin
        return x'length;
    end function;

    subtype cint is count integer;

    signal s : std_logic;
    signal vbus : cint bus;
begin

    s <= 'H';

    b1 : block is
        port ( p : out std_logic );
        port map ( p => s );
    begin
        process is
        begin
            p <= '0';
            wait for 1 ns;
            p <= 'Z';
            wait;
        end process;
    end block;

    b2 : block is
        port ( pbus : inout cint bus );
        port map ( vbus );
    begin
        process is
        begin
            assert pbus = 1 severity failure;
            assert pbus'driving severity failure;
            pbus <= 100;
            wait for 1 ns;
            assert pbus = 1 severity failure;
            pbus <= null;
            wait for 1 ns;
            assert not pbus'driving severity failure;
            wait;
        end process;
    end block;

    process is
    begin
        wait for 500 ps;
        assert s = '0' severity failure;
        wait for 1 ns;
        assert s = 'H' severity failure;
        wait for 1 ns;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
