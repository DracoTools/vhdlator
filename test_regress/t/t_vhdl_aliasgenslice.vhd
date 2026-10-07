-- DESCRIPTION: Verilator: VHDL aliases of slices whose ends a generic gives
--
-- An alias with a subtype of its own, of a slice of a generic array whose
-- ends name the generic, numbers its elements its own way (6.6.2): hi(0)
-- is g(n / 2). The offset was dropped where the ends were not numbers here,
-- so hi(0) read g(0) (NVC issue843's twiddle factors). Ascending,
-- descending and one-based aliases, indexed by constants and by a
-- variable, sliced, and slices of the generic compared with aggregates.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_aliasgenslice_sub is
    generic (n : positive; g : integer_vector(0 to n - 1));
end entity;

architecture a of t_vhdl_aliasgenslice_sub is
    subtype half_t is integer_vector(0 to n / 2 - 1);
    alias lo : half_t is g(0 to n / 2 - 1);
    alias hi : half_t is g(n / 2 to n - 1);
    alias hid : integer_vector(n / 2 - 1 downto 0) is g(n / 2 to n - 1);
    alias h1 : integer_vector(1 to n / 2) is g(n / 2 to n - 1);
begin
    process is
        variable k : natural := 1;
    begin
        assert lo(0) = g(0) and lo(1) = g(1) severity failure;
        assert hi(0) = g(n / 2) and hi(1) = g(n / 2 + 1) severity failure;
        assert hid(n / 2 - 1) = g(n / 2) and hid(0) = g(n - 1) severity failure;
        assert h1(1) = g(n / 2) and h1(n / 2) = g(n - 1) severity failure;
        assert hi(k) = g(n / 2 + 1) and h1(k) = g(n / 2) severity failure;
        assert hi(0 to 1) = g(n / 2 to n / 2 + 1) severity failure;
        assert h1(1 to 2) = g(n / 2 to n / 2 + 1) severity failure;
        assert g(n / 2 to n / 2 + 1) = (g(n / 2), g(n / 2 + 1)) severity failure;
        assert g(0 to 1) /= g(n / 2 to n / 2 + 1) severity failure;
        wait;
    end process;
end architecture;

entity t_vhdl_aliasgenslice is
end entity;

architecture a of t_vhdl_aliasgenslice is
begin
    u4 : entity work.t_vhdl_aliasgenslice_sub generic map (n => 4, g => (10, 11, 12, 13));
    u8 : entity work.t_vhdl_aliasgenslice_sub
        generic map (n => 8, g => (20, 21, 22, 23, 24, 25, 26, 27));
    process is
    begin
        wait for 1 ns;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
