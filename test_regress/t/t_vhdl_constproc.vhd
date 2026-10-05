-- DESCRIPTION: Verilator: VHDL constant set by a function that calls a
-- procedure
--
-- "constant r : range_t := set_range" where set_range calls a procedure,
-- nested in it, that writes an inout record element and the function's
-- own variable. A signal is sized from the constant, and the bounds name a
-- generic, so they fold per instance (nvc simp issue331, issue437).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_constproc_sub is
    generic (bits : positive := 1; second : boolean := false);
    port (len : out natural; hi : out integer);
end entity;

architecture a of t_vhdl_constproc_sub is
    type info_t is record
        lo, hi : integer;
    end record;
    type range_t is record
        lo, hi : integer;
        i0, i1 : info_t;
    end record;

    function set_range return range_t is
        variable pos : integer;
        variable v : range_t;
        procedure take (info : inout info_t; n : in integer) is
        begin
            info.lo := pos;
            info.hi := pos + n - 1;
            pos := pos + n;
        end procedure;
    begin
        pos := 0;
        v.lo := pos;
        take(v.i0, bits);
        if second then
            take(v.i1, bits);
        end if;
        v.hi := pos - 1;
        return v;
    end function;

    constant r : range_t := set_range;
    signal data : bit_vector(r.hi downto r.lo);
begin
    len <= data'length;
    hi <= r.i0.hi;
end architecture;

entity t_vhdl_constproc is
end entity;

architecture test of t_vhdl_constproc is
    signal len1, len2 : natural;
    signal hi1, hi2 : integer;
begin
    u1 : entity work.t_vhdl_constproc_sub generic map (bits => 3) port map (len1, hi1);
    u2 : entity work.t_vhdl_constproc_sub generic map (bits => 4, second => true)
        port map (len2, hi2);

    process is
    begin
        wait for 1 ns;
        assert len1 = 3 and hi1 = 2 report "u1 " & integer'image(len1) severity failure;
        assert len2 = 8 and hi2 = 3 report "u2 " & integer'image(len2) severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
