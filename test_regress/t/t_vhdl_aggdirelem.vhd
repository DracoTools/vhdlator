-- DESCRIPTION: Verilator: VHDL direction of a named aggregate of elements
--
-- An aggregate with named associations and no subtype from its context
-- runs the way its index subtype does, whatever direction a range choice
-- is written in (9.3.3.3): "(3 downto 0 => '0')" is 0 to 3. Only where an
-- association gives a slice of the aggregate -- "3 downto 0 => a" with a a
-- BIT_VECTOR -- does the choice's direction become the aggregate's, as
-- t_vhdl_aggdir checks. nvc 1.22.1, GHDL 7.0.0-dev and Questa 2025.2 agree
-- on every value here.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_aggdirelem_sub is
    generic (vec : bit_vector);
    port (lft, rgt : out integer);
end entity;

architecture a of t_vhdl_aggdirelem_sub is
begin
    lft <= vec'left;
    rgt <= vec'right;
end architecture;

entity t_vhdl_aggdirelem is
end entity;

architecture test of t_vhdl_aggdirelem is
    constant a : bit_vector(3 downto 0) := "0011";
    constant c1 : bit_vector := (3 downto 0 => '0');
    constant c2 : bit_vector := (7 downto 4 => '1', 3 downto 0 => '0');
    constant c3 : bit_vector := (7 downto 4 => '1', 3 downto 0 => '0') & "00";
    constant c4 : string := (5 downto 2 => 'x');
    constant c6 : bit_vector := (7 downto 4 => '1', 3 downto 0 => a);
    constant c7 : bit_vector := (7 downto 4 => '1', 3 downto 0 => a) & "00";
    signal gl, gr : integer;
begin
    u : entity work.t_vhdl_aggdirelem_sub
        generic map (vec => (3 downto 0 => '0'))
        port map (gl, gr);

    process is
    begin
        assert c1'left = 0 and c1'right = 3 report "c1" severity failure;
        assert c2'left = 0 and c2'right = 7 and c2 = "00001111" report "c2" severity failure;
        assert c3 = "0000111100" report "c3" severity failure;
        assert c4'left = 2 and c4'right = 5 report "c4" severity failure;
        assert c6'left = 7 and c6'right = 0 and c6 = "11110011" report "c6" severity failure;
        assert c7 = "1111001100" report "c7" severity failure;
        wait for 1 ns;
        assert gl = 0 and gr = 3 report "generic" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
