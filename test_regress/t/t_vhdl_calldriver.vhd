-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_calldriver_pkg is
    -- A procedure that drives what it is handed: the driver it writes to is
    -- the one belonging to the process that called it (4.2.2.3, 14.7.2)
    procedure drive (signal s : inout std_logic; constant v : in std_logic);
    procedure release_it (signal s : inout std_logic);
end package;

package body t_vhdl_calldriver_pkg is
    procedure drive (signal s : inout std_logic; constant v : in std_logic) is
    begin
        s <= v;
    end procedure;
    procedure release_it (signal s : inout std_logic) is
    begin
        s <= 'Z';
    end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_calldriver_pkg.all;

entity t_vhdl_calldriver is
end entity;

architecture test of t_vhdl_calldriver is
    signal sig : std_logic;

    -- A record of logic values has a driver per element, and each of those
    -- resolves the way the logic values do -- so the record needs no
    -- resolution function of its own (6.5.2)
    type rec_t is record
        a : std_logic;
        b : std_logic_vector(1 downto 0);
    end record;
    signal rec : rec_t;
begin

    -- Two processes drive one signal through the same procedure. Each has a
    -- driver of its own, so one of them letting go leaves the other's value.
    p1 : process is
    begin
        wait for 1 ns;
        drive(sig, '1');
        wait;
    end process;

    p2 : process is
    begin
        wait for 2 ns;
        drive(sig, '0');
        wait for 1 ns;
        release_it(sig);
        wait;
    end process;

    r1 : process is
    begin
        rec <= ('0', "0Z");
        wait;
    end process;

    r2 : process is
    begin
        rec <= ('Z', "Z0");
        wait;
    end process;

    check : process is
    begin
        wait for 4 ns;
        assert sig = '1' report "sig " & std_logic'image(sig) severity failure;
        assert rec.a = '0' report "rec.a " & std_logic'image(rec.a) severity failure;
        assert rec.b = "00" report "rec.b" severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
