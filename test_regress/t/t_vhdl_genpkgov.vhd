-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package store_pkg is
    generic (type t_element);

    -- These two differ only in the generic type. Where an instantiation
    -- binds it to INTEGER they have the same profile, and only the
    -- uninstantiated package can tell them apart (4.9, 6.5.5).
    function tag_of (e : t_element; base : integer) return integer;
    function tag_of (idx : integer; base : integer) return integer;
    function tag_of (base : integer) return integer;
end package;

package body store_pkg is
    function tag_of (e : t_element; base : integer) return integer is
    begin
        return base;
    end function;
    function tag_of (idx : integer; base : integer) return integer is
    begin
        return base + 100 * idx;
    end function;
    function tag_of (base : integer) return integer is
    begin
        -- Means the "idx" one here, whatever an instantiation binds
        return tag_of(2, base);
    end function;
end package body;

package int_store_pkg is new work.store_pkg generic map (t_element => integer);
package bit_store_pkg is new work.store_pkg generic map (t_element => bit);

use work.int_store_pkg.all;

entity t_vhdl_genpkgov is
end entity;

architecture test of t_vhdl_genpkgov is
begin
    check : process is
    begin
        assert tag_of(7) = 207 report "got " & integer'image(tag_of(7)) severity failure;
        assert work.bit_store_pkg.tag_of(7) = 207
            report "bit: " & integer'image(work.bit_store_pkg.tag_of(7)) severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
