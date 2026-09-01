-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

package trig_pkg is
    type t_grid is array (0 to 1, 0 to 3) of std_logic;
    -- A signal a package declares, which every design unit shares
    signal g_trig : t_grid := (others => (others => '0'));
    procedure bump (signal t : inout std_logic);
    procedure clear (signal t : inout std_logic);
end package;

package body trig_pkg is
    procedure bump (signal t : inout std_logic) is
    begin
        t <= not t;
    end procedure;
    procedure clear (signal t : inout std_logic) is
    begin
        t <= '0';
    end procedure;
end package body;

-------------------------------------------------------------------------------

library ieee;
use ieee.std_logic_1164.all;
use work.trig_pkg.all;

entity t_vhdl_sigelem2d is
end entity;

architecture test of t_vhdl_sigelem2d is

    -- One element of a two-dimensional signal, named through an alias whose
    -- indices only elaboration settles
    constant C_CHANNEL : natural := 1;
    constant C_INDEX   : natural := 2;
    alias my_trig : std_logic is g_trig(C_CHANNEL, C_INDEX);

begin

    main : process is
    begin
        wait for 1 ns;
        bump(my_trig);                  -- through the alias
        wait for 1 ns;
        assert g_trig(1, 2) = '1' report "bumped" severity failure;
        assert g_trig(0, 2) = '0' report "only that one" severity failure;

        bump(g_trig(0, 3));             -- and named outright
        wait for 1 ns;
        assert g_trig(0, 3) = '1' report "second" severity failure;

        clear(my_trig);
        wait for 1 ns;
        assert g_trig(1, 2) = '0' report "cleared" severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
