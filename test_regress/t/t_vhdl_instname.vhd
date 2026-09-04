-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_instname is
end entity;

architecture tb of t_vhdl_instname is
    -- The LRM's own examples write a profile with no space after the comma,
    -- and GHDL 7.0.0-dev does the same; nvc 1.22.1 puts one there. Neither
    -- spelling is what this is about, so the spaces come out first.
    function squeeze (constant s : string) return string is
        variable r : string(1 to s'length);
        variable n : natural := 0;
    begin
        for i in s'range loop
            if s(i) /= ' ' then
                n    := n + 1;
                r(n) := s(i);
            end if;
        end loop;
        return r(1 to n);
    end function;

    -- A path names a subprogram with its profile, so that two of the same
    -- name are told apart (16.2.7)
    procedure show (constant n : integer; variable ok : inout boolean) is
        variable v : integer := n;
    begin
        ok := ok and squeeze(v'instance_name)
                     = ":t_vhdl_instname(tb):show[integer,boolean]:v";
        ok := ok and squeeze(v'path_name) = ":t_vhdl_instname:show[integer,boolean]:v";
        ok := ok and v'simple_name = "v";
    end procedure;

    -- Unconstrained, so each call site gets a copy of its own -- whose
    -- made-up name is no part of what the design was written as
    function width (constant s : string) return integer is
        variable w : integer := s'length;
    begin
        assert squeeze(w'instance_name)
               = ":t_vhdl_instname(tb):width[stringreturninteger]:w"
            report "width inst = " & w'instance_name severity failure;
        return w;
    end function;
begin
    p_main : process is
        variable q  : integer := 0;
        variable ok : boolean := true;
    begin
        assert squeeze(q'instance_name) = ":t_vhdl_instname(tb):p_main:q"
            report "q inst = " & q'instance_name severity failure;

        show(3, ok);
        assert ok report "show paths wrong" severity failure;

        assert width("abc") = 3 report "width(3) wrong" severity failure;
        assert width("abcdefgh") = 8 report "width(8) wrong" severity failure;

        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
