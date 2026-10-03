-- DESCRIPTION: Verilator: VHDL block ports whose elements take their bounds
-- from the actuals (6.5.6.3)
--
-- A block port of an array type whose index range is given but whose
-- element is left open, or of a record type with an element left open,
-- takes those bounds from what it is associated with: an aggregate, or the
-- actuals of its elements or record elements associated one at a time.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_blockopen_pkg is
    type matrix_t is array (natural range <>) of bit_vector;

    type addr_t is record
        valid : bit;
        address : bit_vector;
    end record;
    type data_t is record
        valid : bit;
        data : bit_vector;
    end record;
    type bus_t is record
        aw : addr_t;
        w : data_t;
    end record;
    subtype bus32_t is bus_t(aw(address(31 downto 0)), w(data(15 downto 0)));
end package;

use work.t_vhdl_blockopen_pkg.all;

entity t_vhdl_blockopen is
end entity;

architecture test of t_vhdl_blockopen is
    signal x : bit_vector(0 to 2) := "011";
    signal y : bit_vector(0 to 2) := "100";
    signal b : bus32_t;
begin

    b0 : block is
        port (m : in matrix_t(0 to 1));
        port map (m => (x, y));
    begin
        process is
        begin
            wait for 1 ns;
            assert m(0) = "011" severity failure;
            assert m(1) = "100" severity failure;
            assert m(0)'length = 3 severity failure;
            wait for 2 ns;
            assert m(1) = "111" severity failure;
            wait;
        end process;
    end block;

    b1 : block is
        port (m : in matrix_t(0 to 1));
        port map (m(0) => x, m(1) => y);
    begin
        process is
        begin
            wait for 1 ns;
            assert m(0) = "011" severity failure;
            assert m(1) = "100" severity failure;
            assert m(1)'length = 3 severity failure;
            wait for 2 ns;
            assert m(1) = "111" severity failure;
            wait;
        end process;
    end block;

    b2 : block is
        port (p : in bus_t);
        port map (p.aw => b.aw, p.w => b.w);
    begin
        process is
        begin
            assert p.aw.address'length = 32 severity failure;
            assert p.w.data'length = 16 severity failure;
            wait for 3 ns;
            assert p.w.data = x"abcd" severity failure;
            assert p.aw.valid = '1' severity failure;
            wait;
        end process;
    end block;

    process is
    begin
        wait for 2 ns;
        y <= "111";
        b.w.data <= x"abcd";
        b.aw.valid <= '1';
        wait for 5 ns;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
