-- DESCRIPTION: Verilator: VHDL allocator of a record whose element carries
-- its own bounds
--
-- A record with an element of an unconstrained array type, allocated with
-- a value and with a constraint (9.3.7). Such an object holds the element
-- in a carrier, and neither form is turned into that shape yet, so both
-- are refused where they are written rather than handed to the C++
-- compiler.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_accrecopen_unsup is
end entity;

architecture test of t_vhdl_accrecopen_unsup is
    type rec_t is record
        n : integer;
        data : bit_vector;
    end record;
    type rec_ptr is access rec_t;
begin
    process is
        variable p : rec_ptr;
    begin
        p := new rec_t'(n => 3, data => "1010");
        assert p.data'length = 4 severity failure;
        p.data(2) := '0';
        assert p.data = "1000" and p.n = 3 severity failure;
        p := new rec_t(data(7 downto 0));
        assert p.data'length = 8 severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
