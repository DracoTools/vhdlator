-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity sub is
    port (x : in bit_vector;
          y : out bit_vector);
begin
    -- An entity may have passive statements of its own (3.2.4)
    assert x'length = 2 report "x length" severity failure;
end entity;

architecture rtl of sub is
begin
    y <= not x;
end architecture;

entity t_vhdl_partformal is
end entity;

architecture test of t_vhdl_partformal is
    signal b, c, d, e : bit;
begin

    -- Two blocks each labelling an instance the same way
    ba : block is
        signal q : bit;
    begin
        u : entity work.sub port map (x(0) => b, x(1) => c, y(0) => d, y(1) => q);
    end block;

    bb : block is
        signal q : bit;
    begin
        u : entity work.sub port map (x(0) => c, x(1) => b, y(0) => q, y(1) => e);
    end block;

    process is
    begin
        b <= '1';
        c <= '0';
        wait for 1 ns;
        assert d = '0' report "d" severity failure;
        assert e = '0' report "e" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
