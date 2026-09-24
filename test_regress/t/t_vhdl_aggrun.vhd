-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- An aggregate of one value over one range that only running settles, with
-- no subtype of its own to fix its length: it is as long as its range
-- (9.3.3.3), a value that carries its own bounds. Taken for a replication,
-- its count was not a constant (GHDL's issue3007 and issue2581, NVC's
-- issue825).
entity t_vhdl_aggrun is
end entity;

architecture test of t_vhdl_aggrun is
    function zeros(size : natural) return bit_vector is
    begin
        return (size - 1 downto 0 => '0');
    end function;

    function onehot(index, width : natural) return bit_vector is
        variable r : bit_vector(width - 1 downto 0);
    begin
        r := (width - 1 downto index + 1 => '0') & '1' & (index - 1 downto 0 => '0');
        return r;
    end function;

    signal i : integer range 1 to 3 := 1;
    signal s : bit_vector(1 to 4);
begin
    s <= (1 to i => '1') & (i + 1 to 4 => '0');

    process
        variable z : bit_vector(4 downto 0);
    begin
        z := zeros(5);
        assert z = "00000" report "returned" severity failure;
        assert onehot(2, 6) = "000100" report "joined" severity failure;
        assert onehot(0, 3) = "001" report "an end null" severity failure;
        assert onehot(2, 3) = "100" report "the other end null" severity failure;

        wait for 1 ns;
        assert s = "1000" report "one" severity failure;
        i <= 3;
        wait for 1 ns;
        assert s = "1110" report "three" severity failure;

        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
