-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- What an access to an unconstrained array designates, assigned to a
-- variable of a constrained subtype: the value's length is only known when it
-- runs, and must be the variable's (10.6.2.1). The assignment was refused as
-- one from something that is not an array (VESTS tc987).
entity t_vhdl_accfix is
end entity;

architecture test of t_vhdl_accfix is
    type int_vec is array (natural range <>) of integer;
    type int_vec_ptr is access int_vec;
    type real_vec is array (natural range <>) of real;
    type real_vec_ptr is access real_vec;
    type bool_vec is array (natural range <>) of boolean;
    type bool_vec_ptr is access bool_vec;
    type str_ptr is access string;
begin
    process
        variable pi : int_vec_ptr;
        variable up : int_vec(0 to 3);
        variable dn : int_vec(3 downto 0);
        variable pr : real_vec_ptr;
        variable fr : real_vec(1 to 2);
        variable pb : bool_vec_ptr;
        variable fb : bool_vec(0 to 1);
        variable ps : str_ptr;
        variable fs : string(1 to 5);
    begin
        pi := new int_vec'(1, 2, 3, 4);
        up := pi.all;
        assert up = (1, 2, 3, 4) report "ascending" severity failure;
        dn := pi.all;  -- Paired from the left
        assert dn(3) = 1 and dn(0) = 4 report "descending" severity failure;
        pi.all(2) := 30;
        up := pi.all;
        assert up(2) = 30 report "after a change" severity failure;

        pr := new real_vec'(0.5, 1.5);
        fr := pr.all;
        assert fr(1) = 0.5 and fr(2) = 1.5 report "reals" severity failure;
        pb := new bool_vec'(true, false);
        fb := pb.all;
        assert fb(0) and not fb(1) report "booleans" severity failure;
        ps := new string'("hello");
        fs := ps.all;
        assert fs = "hello" report "a string" severity failure;

        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
