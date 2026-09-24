-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- What an access to an unconstrained array points at, read as a value: it
-- carries its own bounds like any other value whose length only running
-- decides, so it can be joined, sliced, compared and handed to a subprogram.
-- A concatenation of two was refused (VESTS tc527), and so was a slice read
-- through the access value (tc537).
entity t_vhdl_accval is
end entity;

architecture test of t_vhdl_accval is
    type bv_ptr is access bit_vector;
    type int_vec is array (natural range <>) of integer;
    type iv_ptr is access int_vec;

    function ones(v : bit_vector) return natural is
        variable n : natural := 0;
    begin
        for i in v'range loop
            if v(i) = '1' then
                n := n + 1;
            end if;
        end loop;
        return n;
    end function;
begin
    process
        variable p : bv_ptr := new bit_vector'("1111");
        variable q : bv_ptr := new bit_vector(1 to 4);
        variable d : bv_ptr := new bit_vector'(x"a5");  -- 0 to 7
        variable r : bit_vector(0 to 7);
        variable ip : iv_ptr := new int_vec'(10, 20, 30, 40);
        variable ok : natural := 0;
    begin
        assert (p.all & q.all) = "11110000" report "two joined" severity failure;
        assert (p.all & "01") = "111101" report "with a literal" severity failure;
        assert ("0" & p.all) = "01111" report "a literal first" severity failure;
        r := p.all & q.all;
        assert r = "11110000" report "joined into a variable" severity failure;
        assert p.all /= q.all report "unequal" severity failure;
        q.all := p.all;
        assert p.all = q.all report "equal" severity failure;

        -- Slices, through the access value and through its dereference
        assert p(0 to 1) = "11" report "implicit slice" severity failure;
        assert d.all(2 to 5) = "1001" report "explicit slice" severity failure;
        assert d(4 to 7) = x"5" report "second half" severity failure;
        assert ones(d.all) = 4 report "handed to a function" severity failure;
        assert ones(d(0 to 3)) = 2 report "a slice handed on" severity failure;

        assert ip(1 to 2) = (20, 30) report "integer slice" severity failure;
        assert (ip.all & 50) = (10, 20, 30, 40, 50) report "integer joined" severity failure;

        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
