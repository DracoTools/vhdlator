-- DESCRIPTION: Verilator: VHDL unconstrained port whose actual is a literal
--
-- An unconstrained port takes its bounds from its actual (6.5.6.3), and the
-- actual may be a literal or an aggregate: a literal's range starts at the
-- left of its index subtype and runs upward (9.3.2), and an aggregate
-- written over a range has its ends, in the index subtype's direction.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_openportlit_sub is
    generic (width : integer;
             want_left, want_right : integer;
             want_asc : boolean);
    port (x : in bit_vector(width - 1 downto 0);
          y : in bit_vector;
          z : out bit_vector(width - 1 downto 0));
end entity;

architecture test of t_vhdl_openportlit_sub is
begin
    z <= x and y;

    process is
    begin
        assert y'left = want_left report "left" severity failure;
        assert y'right = want_right report "right" severity failure;
        assert y'ascending = want_asc report "ascending" severity failure;
        assert y'length = width report "length" severity failure;
        wait;
    end process;
end architecture;

entity t_vhdl_openportlit is
end entity;

architecture test of t_vhdl_openportlit is
    signal z1, z2, z3 : bit_vector(7 downto 0);
    signal z4 : bit_vector(3 downto 0);
begin

    -- A bit string literal: 0 to 7
    u1 : entity work.t_vhdl_openportlit_sub
        generic map (width => 8, want_left => 0, want_right => 7, want_asc => true)
        port map (x => X"ab", y => X"cd", z => z1);

    -- An aggregate over a range: its ends, in the direction of the index
    -- subtype, whichever way the range was written
    u2 : entity work.t_vhdl_openportlit_sub
        generic map (width => 8, want_left => 3, want_right => 10, want_asc => true)
        port map (x => X"ff", y => (10 downto 3 => '1'), z => z2);

    -- A string literal, and a positional aggregate
    u3 : entity work.t_vhdl_openportlit_sub
        generic map (width => 8, want_left => 0, want_right => 7, want_asc => true)
        port map (x => X"0f", y => "00111100", z => z3);

    u4 : entity work.t_vhdl_openportlit_sub
        generic map (width => 4, want_left => 0, want_right => 3, want_asc => true)
        port map (x => "1111", y => ('1', '0', '1', '0'), z => z4);

    process is
    begin
        wait for 1 ns;
        assert z1 = "10001001" severity failure;
        assert z2 = X"ff" severity failure;
        assert z3 = "00001100" severity failure;
        assert z4 = "1010" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
