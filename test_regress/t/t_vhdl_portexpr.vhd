-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

entity t_vhdl_portexpr is
end entity;

architecture test of t_vhdl_portexpr is
    signal s : integer := 1000;
    signal c : character := 'a';

    function twice (v : integer) return integer is
    begin
        return 2 * v;
    end function;
begin

    s <= 1 after 1 ns;

    b : block is
        -- 6.5.7.1: a port whose actual is an expression is a signal with a
        -- driver of its own, so it holds its default until the association
        -- delivers, and every delivery after that arrives a delta later
        port ( p : in integer := 0;
               q : in integer;
               r : in integer );
        port map ( p => inertial s,
                   q => character'pos(c),
                   r => twice(s) );
    begin
        check : process is
        begin
            assert p = 0 report "the port's own default" severity failure;
            assert q = integer'left report "the type's default" severity failure;
            wait for 0 ns;
            assert p = 1000 report "delivered a delta later" severity failure;
            assert q = 97 report "the expression delivered" severity failure;
            -- A conversion applied to a signal is still an association with
            -- that signal, so it follows with nothing in between
            assert r = 2000 report "a call of a signal" severity failure;

            wait for 1 ns;
            assert p = 1000 report "still one behind" severity failure;
            wait for 0 ns;
            assert p = 1 report "caught up" severity failure;

            report "*-* All Finished *-*";
            std.env.stop;
        end process;
    end block;
end architecture;
