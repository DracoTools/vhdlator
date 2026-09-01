-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package store_pkg is
    generic (type t_element);

    -- Two subprograms that differ only in the generic type: where the
    -- package is written they are told apart, and a call inside it resolves
    -- against the generic type, not against whatever an instantiation later
    -- gives it (4.7)
    function pick (index : integer; tag : string) return integer;
    function pick (element : t_element; tag : string) return integer;
    function inner (index : integer) return integer;
end package;

package body store_pkg is
    function pick (index : integer; tag : string) return integer is
    begin
        return index + tag'length;
    end function;
    function pick (element : t_element; tag : string) return integer is
    begin
        return 100;
    end function;
    function inner (index : integer) return integer is
    begin
        -- Reads as the first of the two wherever the package was written
        return pick(index, "ab");
    end function;
end package body;

-------------------------------------------------------------------------------

package int_store_pkg is new work.store_pkg generic map (t_element => integer);

use work.int_store_pkg.all;

entity t_vhdl_geninst is
end entity;

architecture test of t_vhdl_geninst is
begin

    main : process is
        variable n : integer;
    begin
        n := inner(7);
        assert n = 9 report "inner=" & integer'image(n) severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
