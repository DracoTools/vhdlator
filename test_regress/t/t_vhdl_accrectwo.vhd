-- DESCRIPTION: Verilator: VHDL record holding an array of arrays open at
-- both levels, through an access value
--
-- "data : data_t" with data_t "array (natural range <>) of bit_vector": an
-- object of the record has the bounds its subtype or its value gives both
-- levels (5.3.3), held two levels deep where nothing here fixes them.
-- Allocated with a constraint, fixed and settled only while running, and
-- with an aggregate; elements written, read and compared, the element
-- written whole, and its attributes (NVC issue1576).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_accrectwo is
end entity;

architecture test of t_vhdl_accrectwo is
    type data_t is array (natural range <>) of bit_vector;
    type container_t is record
        n : integer;
        data : data_t;
    end record;
    type container_ptr_t is access container_t;
begin
    process is
        variable p, q : container_ptr_t;
        variable size : natural := 0;
        variable k : natural := 3;
        subtype c4x8_t is container_t(data(0 to 3)(7 downto 0));

        procedure append(d : bit_vector(7 downto 0)) is
        begin
            p.data(size) := d;
            size := size + 1;
        end procedure;
    begin
        -- A constraint written with a subtype
        p := new c4x8_t;
        assert p.data'length = 4 and p.data(0)'length = 8 severity failure;
        assert p.data(3) = x"00" and p.n = integer'left severity failure;
        p.data := (0 to 3 => (0 to 7 => '1'));
        assert p.data(2) = x"ff" severity failure;
        append(x"01");
        append(x"02");
        append(x"04");
        append(x"08");
        assert p.data(0) = x"01" and p.data(3) = x"08" severity failure;
        assert to_string(p.data(1)) = "00000010" severity failure;
        p.data(2)(0) := '1';
        assert p.data(2) = x"05" severity failure;
        -- Bounds settled only while running
        q := new container_t(data(1 to k)(3 downto 0));
        assert q.data'length = 3 and q.data'left = 1 and q.data(1)'length = 4 severity failure;
        q.data(3) := x"a";
        q.n := 7;
        assert q.data(3) = x"a" and q.data(2) = x"0" and q.n = 7 severity failure;
        -- An aggregate
        q := new container_t'(n => 2, data => (x"12", x"34"));
        assert q.data'length = 2 and q.data(1) = x"34" and q.n = 2 severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
