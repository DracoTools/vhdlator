-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-License-Identifier: CC0-1.0

package pack is
    type rec_t is record
        x : integer_vector;
    end record;
    type vov_t is array (natural range <>) of bit_vector;
end package;

-------------------------------------------------------------------------------
-- A port left unconstrained takes the bounds of the actual, all the way
-- down to the element of an array of arrays (5.3.2.2)

use work.pack.all;

entity sub_vov is
    port ( v : in vov_t; n : out integer );
end entity;

architecture test of sub_vov is
begin
    n <= v'length * 100 + v'element'length;
end architecture;

-------------------------------------------------------------------------------
-- A declaration constrained by "x'range" takes the range x was given, even
-- where x is a port whose own bounds the instance settled (16.2.4)

entity sub_range is
    port ( addr : in bit_vector; q : out bit_vector );
end entity;

architecture test of sub_range is
    signal held : bit_vector(addr'range);
begin
    held <= addr;
    q <= held;

    chk : process (addr) is
        variable local : bit_vector(addr'range);
    begin
        local := addr;
        assert local'left = 7 report "range left" severity failure;
        assert local'right = 0 report "range right" severity failure;
    end process;
end architecture;

-------------------------------------------------------------------------------
-- A formal that names a slice of an unconstrained port says how long the
-- port is (6.5.7.1)

entity sub_slice is
    port ( value : in bit_vector );
end entity;

architecture test of sub_slice is
begin
    process is
    begin
        assert value'left = 2 report "slice left" severity failure;
        assert value'right = 9 report "slice right" severity failure;
        assert value = "10110011" report "slice value" severity failure;
        wait;
    end process;
end architecture;

-------------------------------------------------------------------------------

use work.pack.all;

entity t_vhdl_openelem is
end entity;

architecture test of t_vhdl_openelem is
    signal r : rec_t(x(1 to 5)) := (x => (1, 2, 3, 0, 40));
    signal vov : vov_t(0 to 2)(1 to 7) := (others => (others => '0'));
    signal n : integer;
    signal d : bit_vector(2 to 9) := "10110011";
    signal a : bit_vector(7 downto 0) := "10110011";
    signal qa : bit_vector(7 downto 0);
begin
    u2 : entity work.sub_vov port map (vov, n);
    u3 : entity work.sub_slice port map (value(d'range) => d);
    u4 : entity work.sub_range port map (a, qa);

    check : process is
    begin
        wait for 1 ns;
        assert r.x'left = 1 report "own left" severity failure;
        assert r.x'right = 5 report "own right" severity failure;
        assert r.x(1) = 1 report "own first" severity failure;
        assert r.x(5) = 40 report "own last" severity failure;
        assert d(d'range) = "10110011" report "attr slice" severity failure;
        assert n = 307 report "array of array port" severity failure;
        assert qa = "10110011" report "range from a port" severity failure;
        report "*-* All Finished *-*";
        std.env.stop;
    end process;
end architecture;
