-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- An allocator of an array whose length only its allocation decides, as an
-- element of a record another allocator makes. Its "new" had no assignment
-- to learn its class from, and the back end stopped on an internal error
-- (VESTS ch_17_fg_17_11, the bounded buffer of Ashenden's chapter 17).
entity t_vhdl_allocnest is
end entity;

architecture test of t_vhdl_allocnest is
    subtype byte is bit_vector(0 to 7);
    type store_array is array (natural range <>) of byte;
    type store_ptr is access store_array;
    type buffer_object is record
        count : natural;
        store : store_ptr;
    end record;
    type buffer_ptr is access buffer_object;

    impure function new_buffer(size : positive) return buffer_ptr is
    begin
        return new buffer_object'(count => 0, store => new store_array(0 to size - 1));
    end function;
begin
    process
        variable b : buffer_ptr := new_buffer(16);
        variable c : buffer_ptr;
    begin
        assert b.store'length = 16 report "as long as asked" severity failure;
        b.store(3) := x"a5";
        b.count := 1;
        assert b.store(3) = x"a5" and b.count = 1 report "written" severity failure;
        c := new_buffer(4);
        assert c.store'length = 4 and c.store(0) = x"00" report "another" severity failure;
        assert b.store'length = 16 report "the first kept" severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
