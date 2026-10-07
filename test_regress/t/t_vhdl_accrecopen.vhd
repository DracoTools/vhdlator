-- DESCRIPTION: Verilator: VHDL allocator of a record whose element carries
-- its own bounds
--
-- A record with an element of an unconstrained array type, allocated with
-- an aggregate, with a record object of a constrained subtype, and with a
-- constraint fixed or settled only while running (9.3.7), in a process and
-- in a package's functions. The object keeps the bounds it was made with
-- when it is written whole (10.6.2.1).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_accrecopen_pkg is
    type rec_t is record
        n : integer;
        data : bit_vector;
    end record;
    type rec_ptr is access rec_t;
    impure function mk(k : natural) return rec_ptr;
    impure function mk8 return rec_ptr;
    impure function mkv return rec_ptr;
end package;

package body t_vhdl_accrecopen_pkg is
    impure function mk(k : natural) return rec_ptr is
    begin
        return new rec_t(data(1 to k));
    end;
    impure function mk8 return rec_ptr is
    begin
        return new rec_t(data(7 downto 0));
    end;
    impure function mkv return rec_ptr is
    begin
        return new rec_t'(5, "01");
    end;
end package body;

use work.t_vhdl_accrecopen_pkg.all;

entity t_vhdl_accrecopen is
end entity;

architecture test of t_vhdl_accrecopen is
begin
    process is
        variable p, q : rec_ptr;
        variable r : rec_t(data(2 downto 0)) := (n => 9, data => "110");
        variable k : natural := 6;
        procedure fill(m : natural) is
            variable w : rec_t(data(1 to m));
        begin
            w := (n => 2, data => "111001");
            assert w.n = 2 and w.data = "111001" and w.data'left = 1 severity failure;
        end procedure;
    begin
        -- An aggregate, positional and named
        p := new rec_t'(3, "1010");
        assert p.n = 3 and p.data'length = 4 severity failure;
        assert p.data'left = 0 and p.data'right = 3 severity failure;
        p.data(2) := '0';
        assert p.data = "1000" severity failure;
        q := new rec_t'(data => "11", n => 4);
        assert q.data = "11" and q.n = 4 severity failure;
        -- A record object of a constrained subtype
        p := new rec_t'(r);
        assert p.n = 9 and p.data = "110" severity failure;
        assert p.data'left = 2 and p.data'right = 0 severity failure;
        -- A constraint, fixed and settled only while running
        p := new rec_t(data(7 downto 0));
        assert p.data'length = 8 and p.data = x"00" and p.n = integer'left severity failure;
        p.data := x"a5";
        assert p.data(7) = '1' and p.data(0) = '1' and p.data(1) = '0' severity failure;
        q := new rec_t(data(1 to k));
        assert q.data'length = 6 and q.data'left = 1 severity failure;
        q.data(6) := '1';
        assert q.data = "000001" severity failure;
        -- Written whole, the object keeps the bounds it was made with
        q.all := (n => 1, data => "101011");
        assert q.n = 1 and q.data = "101011" severity failure;
        assert q.data'left = 1 and q.data'right = 6 severity failure;
        fill(6);
        p := new rec_t'(7, "011000");
        p.all := q.all;
        assert p.n = 1 and p.data = "101011" severity failure;
        assert p.data'left = 0 and p.data'right = 5 severity failure;
        -- Made by a package's functions
        p := mk(3);
        assert p.data'length = 3 and p.data'left = 1 severity failure;
        p := mk8;
        assert p.data'length = 8 and p.data'left = 7 severity failure;
        p := mkv;
        assert p.data = "01" and p.n = 5 severity failure;
        deallocate(p);
        assert p = null severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
