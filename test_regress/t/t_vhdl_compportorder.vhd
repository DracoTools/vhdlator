-- DESCRIPTION: Verilator: VHDL positional port map of a component whose ports are in another order
--
-- A positional association names the port at that place of the component
-- (6.5.7.1), and the component is bound to an entity that declares the same
-- ports in a different order.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_compportorder_sub is
    port (c : out integer := 0;
          a, b : in integer);
end entity;

architecture impl of t_vhdl_compportorder_sub is
begin
    c <= a - 2 * b;
end architecture;

entity t_vhdl_compportorder is
end entity;

architecture test of t_vhdl_compportorder is

    component comp is
        port (b : in integer;
              c : out integer;
              a : in integer);
    end component;

    signal x, y1, y2, y3, y4, z : integer := 0;

    for all : comp use entity work.t_vhdl_compportorder_sub;

    function negate (x : in integer) return integer is
    begin
        return -x;
    end function;
begin

    -- All by position: b, c, a
    uut1 : component comp port map (x, y1, z);

    -- One by position, then a conversion of what the port drives
    uut2 : component comp
        port map (x, negate(c) => y2, a => z);

    -- Position then names
    uut3 : component comp port map (x, a => z, c => y3);

    -- The entity itself, by position in its own order: c, a, b
    uut4 : entity work.t_vhdl_compportorder_sub port map (y4, z, x);

    p1 : process is
    begin
        x <= 1;
        z <= 5;
        wait for 1 ns;
        assert y1 = 3 report integer'image(y1) severity failure;
        assert y2 = -3 report integer'image(y2) severity failure;
        assert y3 = 3 report integer'image(y3) severity failure;
        assert y4 = 3 report integer'image(y4) severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
