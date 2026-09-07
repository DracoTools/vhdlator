-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- The frontend carries out the arithmetic of the IEEE packages itself rather
-- than compiling their bodies. Which subprograms those are is a question
-- about the declaration, not about the name: IEEE's own float_generic_pkg
-- declares a RESIZE that converts between floating point formats, a MINIMUM
-- over two floats and a TO_01 of its own, and every design is free to do the
-- same. A subprogram declared anywhere else means what its body says.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package ownname_pkg is
    -- A record standing in for a format, so these cannot be confused with
    -- numeric_std's operations by their argument types either
    type pair is record
        hi : integer;
        lo : integer;
    end record;

    -- Named exactly as numeric_std's, and meaning something else: this one
    -- reads its second argument as a bound rather than as a length, which is
    -- what ieee.fixed_generic_pkg's RESIZE does
    function resize (arg : unsigned; bounds : pair) return unsigned;
    function minimum (l, r : pair) return pair;
    function to_01 (arg : pair) return pair;
    -- One whose result is as long as an argument says, so that a copy has to
    -- be made for the call site before it can be emitted at all
    function to_unsigned (p : pair; v : unsigned) return unsigned;
end package;

package body ownname_pkg is

    function resize (arg : unsigned; bounds : pair) return unsigned is
        variable v : unsigned (bounds.hi downto 0) := (others => '0');
    begin
        for i in 0 to bounds.hi loop
            if i <= arg'high - arg'low then
                v (i) := arg (arg'low + i);
            end if;
        end loop;
        return v;
    end function;

    function minimum (l, r : pair) return pair is
    begin
        if l.hi + l.lo <= r.hi + r.lo then
            return l;
        end if;
        return r;
    end function;

    function to_01 (arg : pair) return pair is
        variable v : pair;
    begin
        v.hi := arg.hi + 1;
        v.lo := arg.lo + 1;
        return v;
    end function;

    function to_unsigned (p : pair; v : unsigned) return unsigned is
        variable r : unsigned (p.hi - p.lo downto 0) := (others => '1');
    begin
        r (0) := v (v'low);
        return r;
    end function;

end package body;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.ownname_pkg.all;

entity t_vhdl_ownname is
end entity;

architecture a of t_vhdl_ownname is
    constant PR : pair := (hi => 9, lo => 3);
begin

    main: process is
        variable u : unsigned (3 downto 0) := "1011";
        variable w : unsigned (5 downto 0);
        variable q : pair;
    begin
        -- The design's RESIZE counts a bound, not a length, so this is
        -- seven elements and not five
        w := resize (u, pair'(hi => 5, lo => 0));
        assert w'length = 6 report "bad own resize length" severity failure;
        assert w = "001011" report "bad own resize value" severity failure;

        -- numeric_std's own is still what a call over its types means
        assert resize (u, 6)'length = 6 report "bad numeric resize" severity failure;

        q := minimum (PR, pair'(hi => 2, lo => 2));
        assert q.hi = 2 and q.lo = 2 report "bad own minimum" severity failure;

        q := to_01 (PR);
        assert q.hi = 10 and q.lo = 4 report "bad own to_01" severity failure;

        -- Its result is as long as the record says, which only a copy made
        -- for this call site can know
        assert to_unsigned (PR, u)'length = 7 report "bad own to_unsigned"
            severity failure;
        assert to_unsigned (PR, u) = "1111111" report "bad own to_unsigned value"
            severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
        wait;
    end process;

end architecture;
