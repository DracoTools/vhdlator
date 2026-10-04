-- DESCRIPTION: Verilator: VHDL "others" over an array of generic length, wide elements
--
-- "(0 => x, 1 => y, others => z)" and "(x, y, others => z)" for an array
-- of vectors or integers whose length names a generic of the top entity:
-- each element at its index, and the rest the "others" value (9.3.3.2).
-- GHDL gna issue635 initializes a RAM this way, and issue1323 a constant.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_wideothers is
    generic (w : natural := 4; n : natural := 3);
end entity;

architecture test of t_vhdl_wideothers is
    type arr is array (natural range <>) of bit_vector(w - 1 downto 0);
    type iarr is array (natural range <>) of integer;

    signal ram : arr(0 to n) := (0 => "0001", 1 => "0010", others => "1111");
    constant c : arr(0 to n) := (2 => (others => '1'), others => (others => '0'));
    constant ci : iarr(n downto 0) := (0 => 5, others => 7);
    signal d : arr(n downto 1) := (3 => "1000", 2 => "0100", others => "0001");
    signal p : arr(1 to n) := ("1010", "0101", others => "1111");
    -- Elements whose width is a number: once taken for elements of one bit
    type sarr is array (natural range <>) of bit_vector(7 downto 0);
    constant cs : sarr(0 to n) := (0 => x"81", others => x"0F");
    constant ps : sarr(0 to n) := (x"42", others => x"F0");
begin
    process is
        variable vi : iarr(1 to n);
    begin
        assert ram(0) = "0001" and ram(1) = "0010" severity failure;
        assert ram(2) = "1111" and ram(n) = "1111" severity failure;
        assert c(2) = "1111" and c(1) = "0000" and c(n) = "0000" severity failure;
        assert ci(0) = 5 and ci(1) = 7 and ci(n) = 7 severity failure;
        assert d(3) = "1000" and d(2) = "0100" and d(1) = "0001" severity failure;
        assert d(n) = "0001" severity failure;
        assert p(1) = "1010" and p(2) = "0101" and p(n) = "1111" severity failure;
        assert cs(0) = x"81" and cs(1) = x"0F" and cs(n) = x"0F" severity failure;
        assert ps(0) = x"42" and ps(n) = x"F0" severity failure;
        vi := (10, others => 0);
        assert vi(1) = 10 and vi(2) = 0 and vi(n) = 0 severity failure;
        vi := (2 => 4, others => 1);
        assert vi(2) = 4 and vi(1) = 1 and vi(n) = 1 severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
