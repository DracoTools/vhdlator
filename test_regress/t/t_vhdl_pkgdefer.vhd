-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

package inner_gen is
    generic ( lo : integer := -23 );
    type u_arr is array (integer range <>) of bit;
    subtype arr8 is u_arr (8 downto -23);
    function width return integer;
end package;

package body inner_gen is
    function width return integer is
    begin
        -- 16.2.4: of an array these ask about an index range, not about the
        -- array, and that has to hold where overload resolution asks too
        return arr8'high - arr8'low + 1;
    end function;
end package body;

package inner_pkg is new work.inner_gen generic map ( lo => -23 );

-------------------------------------------------------------------------------
-- A generic package given another package by name, the way
-- IEEE.float_generic_pkg is given IEEE.fixed_pkg (4.9, 6.5.5)

package outer_gen is
    generic ( package p is new work.inner_gen generic map (<>) );
    function twice return integer;
end package;

package body outer_gen is
    use p.all;
    function twice return integer is
    begin
        return 2 * (arr8'high - arr8'low + 1);
    end function;
end package body;

package outer_pkg is new work.outer_gen generic map ( p => work.inner_pkg );

-------------------------------------------------------------------------------
-- 6.4.2.2: a constant declared with no value is deferred, and the body
-- gives it one

package defer_pkg is
    constant deferred_width : integer;
    function held return integer;
end package;

package body defer_pkg is
    constant deferred_width : integer := 32;
    function held return integer is
    begin
        return deferred_width;
    end function;
end package body;

-------------------------------------------------------------------------------

entity t_vhdl_pkgdefer is
end entity;

use work.outer_pkg.all;
use work.defer_pkg.all;

architecture test of t_vhdl_pkgdefer is
begin
    main : process is
    begin
        assert work.inner_pkg.width = 32 report "attribute in a package body"
            severity failure;
        assert twice = 64 report "through a generic package" severity failure;
        assert deferred_width = 32 report "deferred constant" severity failure;
        assert held = 32 report "deferred constant read in the body" severity failure;
        report "*-* All Finished *-*";
        std.env.stop;
    end process;
end architecture;
