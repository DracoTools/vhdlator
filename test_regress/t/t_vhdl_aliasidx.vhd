-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_aliasidx is
end entity;

architecture a of t_vhdl_aliasidx is
    type bv is array (natural range <>) of integer;

    -- An alias that numbers its elements the other way about, which is how
    -- OSVVM walks a string backwards to hash it
    function backwards (s : string) return string is
        constant len : integer := s'length;
        alias revs : string(len downto 1) is s;
        variable out_s : string(1 to len);
    begin
        for i in 1 to len loop
            out_s(i) := revs(i);
        end loop;
        return out_s;
    end function;

    function total (s : string) return integer is
        constant len : integer := s'length;
        alias revs : string(len downto 1) is s;
        variable t : integer := 0;
    begin
        for i in 1 to len loop
            t := t * 10 + character'pos(revs(i)) - character'pos('0');
        end loop;
        return t;
    end function;

    -- The same over an array that is not text, and one starting elsewhere
    function pick (v : bv) return integer is
        alias av : bv(v'length downto 1) is v;
    begin
        return av(1) * 100 + av(v'length);
    end function;
begin
    process
        constant src : string(1 to 5) := "abcde";
        alias shifted : string(11 to 15) is src;
        variable arr : bv(0 to 3) := (7, 8, 9, 10);
    begin
        assert backwards("abc") = "cba"
            report "backwards = " & backwards("abc") severity failure;
        assert backwards("z") = "z" report "one" severity failure;

        assert total("123") = 321 report "total = " & integer'image(total("123"))
            severity failure;

        assert pick(arr) = 1007 report "pick = " & integer'image(pick(arr)) severity failure;

        -- An alias that only renumbers, with no reversal
        assert shifted(11) = 'a' report "shifted low" severity failure;
        assert shifted(15) = 'e' report "shifted high" severity failure;
        assert shifted(12 to 14) = "bcd" report "shifted slice" severity failure;

        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
