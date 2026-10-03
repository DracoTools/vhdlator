-- DESCRIPTION: Verilator: VHDL array-of-records port with an open element
--
-- An array of records whose element is left open, constrained in one go --
-- "rec_array(1 to 2)(y(1 to 3))" -- and a port of the unconstrained type
-- that takes both the array's and the element's bounds from its actual
-- (5.3.3, 6.5.6.3), here walked by a generate whose stride is the length of
-- one element.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_recarrport_pkg is
    type rec is record
        x : integer;
        y : bit_vector;
    end record;
    type rec_array is array (natural range <>) of rec;
end package;

use work.t_vhdl_recarrport_pkg.all;

entity t_vhdl_recarrport_sub is
    port ( i : in rec_array;
           o1 : out integer_vector;
           o2 : out bit_vector );
end entity;

architecture test of t_vhdl_recarrport_sub is
begin
    g: for n in i'range generate
        constant stride : natural := i(n).y'length;
    begin
        o1(n) <= i(n).x;
        o2(1 + (n - 1) * stride to n * stride) <= i(n).y;
    end generate;
end architecture;

use work.t_vhdl_recarrport_pkg.all;

entity t_vhdl_recarrport is
end entity;

architecture test of t_vhdl_recarrport is
    signal a : rec_array(1 to 2)(y(1 to 3));
    signal b : integer_vector(1 to 2);
    signal c : bit_vector(1 to 6);
    signal d : rec_array(1 to 3)(y(1 to 2));
    signal e : integer_vector(1 to 3);
    signal f : bit_vector(1 to 6);
begin

    u1 : entity work.t_vhdl_recarrport_sub port map ( a, b, c );
    u2 : entity work.t_vhdl_recarrport_sub port map ( d, e, f );

    process is
    begin
        assert a(1).y'length = 3 and d(3).y'length = 2 severity failure;
        a(1) <= (y => "101", x => 2);
        d(2) <= (y => "11", x => 7);
        wait for 1 ns;
        assert b = (2, integer'left) severity failure;
        assert c = "101000" severity failure;
        assert e(2) = 7 severity failure;
        assert f = "001100" severity failure;
        a(2).x <= 5;
        a(2).y <= "110";
        wait for 1 ns;
        assert b = (2, 5) severity failure;
        assert c = "101110" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
