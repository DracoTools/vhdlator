-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_ptrlen is
end entity;

architecture a of t_vhdl_ptrlen is
    type int_vec is array (natural range <>) of integer;
    type int_vec_ptr is access int_vec;

    function make (n : natural) return int_vec_ptr is
        variable p : int_vec_ptr;
    begin
        p := new int_vec(1 to n);
        for i in p'range loop
            p(i) := i;
        end loop;
        return p;
    end function;

    -- An array as long as an access value points at. The attribute
    -- dereferences the pointer where it stands, so what it answers is
    -- whatever allocating the value made it, and that is not known until
    -- the allocation has happened.
    function lengths (n : natural) return int_vec is
        variable parts : int_vec_ptr := make(n);
        constant len : positive := parts'length + 1;
        variable out_v : int_vec(1 to len);
    begin
        for i in out_v'range loop
            out_v(i) := i * 5;
        end loop;
        deallocate(parts);
        return out_v;
    end function;
begin
    process
        variable three : int_vec(1 to 4);
        variable six : int_vec(1 to 7);
    begin
        three := lengths(3);
        assert three'length = 4 report "three length" severity failure;
        assert three(1) = 5 and three(4) = 20 report "three values" severity failure;

        six := lengths(6);
        assert six'length = 7 report "six length" severity failure;
        assert six(1) = 5 and six(7) = 35 report "six values" severity failure;

        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
