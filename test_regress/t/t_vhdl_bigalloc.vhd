-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Allocating, assigning and comparing designated objects too large to write
-- out element by element, as VESTS tc988 does with 1024 by 64 arrays: each
-- default, each copy and each comparison has to stay one operation.
package bigpk is
    type row_t is range 1 to 128;
    type col_t is range 1 to 64;
    type bool2_t is array (row_t, col_t) of boolean;
    type bit2_t is array (row_t, col_t) of bit;
    type int2_t is array (row_t, col_t) of integer;
    type real2_t is array (row_t, col_t) of real;
    type int16_t is array (15 downto 0) of integer;
    type int16x16_t is array (0 to 15) of int16_t;

    type rec_t is record
        b : bool2_t;
        t : bit2_t;
        i : int2_t;
        r : real2_t;
        s : int16x16_t;
    end record;

    type bool2_ptr is access bool2_t;
    type bit2_ptr is access bit2_t;
    type int2_ptr is access int2_t;
    type real2_ptr is access real2_t;
    type int16x16_ptr is access int16x16_t;
    type rec_ptr is access rec_t;

    constant CBO : bool2_t := (others => (others => true));
    constant CBI : bit2_t := (others => (others => '1'));
    constant CIN : int2_t := (others => (others => 7));
    constant CRE : real2_t := (others => (others => 2.5));
    constant CSM : int16x16_t := (others => (others => 3));
    constant CREC : rec_t := (CBO, CBI, CIN, CRE, CSM);
end package;

entity t_vhdl_bigalloc is
end entity;

use work.bigpk.all;

architecture tb of t_vhdl_bigalloc is
begin
    process
        variable pb : bool2_ptr := new bool2_t;
        variable pt : bit2_ptr := new bit2_t;
        variable pi : int2_ptr := new int2_t;
        variable pr : real2_ptr := new real2_t;
        variable ps : int16x16_ptr := new int16x16_t;
        variable prec : rec_ptr := new rec_t;
    begin
        -- An allocator gives the designated subtype's default: each
        -- element's type's leftmost value
        assert pb.all(128, 64) = false report "bool default" severity failure;
        assert pt.all(1, 1) = '0' report "bit default" severity failure;
        assert pi.all(64, 32) = integer'left report "int default" severity failure;
        assert pr.all(1, 64) = real'left report "real default" severity failure;
        assert ps.all(15)(0) = integer'left report "int16 default" severity failure;
        assert prec.all.i(1, 1) = integer'left report "rec int default" severity failure;
        assert prec.all.t(128, 1) = '0' report "rec bit default" severity failure;
        assert prec.all.s(0)(15) = integer'left report "rec int16 default" severity failure;
        assert pi.all /= CIN report "int differs from constant" severity failure;
        assert prec.all /= CREC report "rec differs from constant" severity failure;

        pb.all := CBO;
        pt.all := CBI;
        pi.all := CIN;
        pr.all := CRE;
        ps.all := CSM;
        prec.all := CREC;
        assert pb.all = CBO report "bool copy" severity failure;
        assert pt.all = CBI report "bit copy" severity failure;
        assert pi.all = CIN report "int copy" severity failure;
        assert pr.all = CRE report "real copy" severity failure;
        assert ps.all = CSM report "int16 copy" severity failure;
        assert prec.all = CREC report "rec copy" severity failure;

        -- One element changed is found by each comparison
        pi.all(128, 64) := 8;
        assert pi.all /= CIN report "int change missed" severity failure;
        pr.all(1, 1) := 0.5;
        assert pr.all /= CRE report "real change missed" severity failure;
        ps.all(15)(0) := 4;
        assert ps.all /= CSM report "int16 change missed" severity failure;
        prec.all.r(64, 32) := 1.0;
        assert prec.all /= CREC report "rec change missed" severity failure;
        assert prec.all.i = CIN report "rec int unchanged" severity failure;

        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
