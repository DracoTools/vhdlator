-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_shiftmatch is
end entity;

architecture test of t_vhdl_shiftmatch is
    type my_bit_vector is array (integer range <>) of bit;

    -- "case?" matches with "?=", so a '-' in a choice stands for any value
    function classify (x : std_logic_vector(2 downto 0)) return integer is
    begin
        case? x is
            when "1--" => return 1;
            when "01-" => return 2;
            when "001" => return 3;
            when others => return 0;
        end case?;
    end function;

    signal sel : std_logic_vector(1 downto 0) := "00";
    signal picked : integer;
begin

    -- The matching form of a selected assignment reads the same way
    with sel select? picked <=
        10 when "1-",
        20 when "01",
        30 when others;

    main : process is
        variable b : my_bit_vector(3 downto 0);
        variable c : my_bit_vector(0 to 3);
        variable slv : std_logic_vector(7 downto 0);
    begin
        b := "1011";
        c := "1011";

        -- 9.2.4: sll and srl bring in the element type's left bound, sla
        -- repeats the rightmost element and sra the leftmost
        assert (b sll 1) = "0110" report "sll" severity failure;
        assert (c sll 1) = "0110" report "sll ascending" severity failure;
        assert (b srl 1) = "0101" report "srl" severity failure;
        assert (b sla 1) = "0111" report "sla" severity failure;
        assert (c sla 1) = "0111" report "sla ascending" severity failure;
        assert (b sra 1) = "1101" report "sra" severity failure;
        assert (c sra 1) = "1101" report "sra ascending" severity failure;
        -- A count that is negative shifts the other way
        assert (b sla -1) = "1101" report "sla by a negative count" severity failure;
        assert (b sra -1) = "0111" report "sra by a negative count" severity failure;
        -- Shifting by the whole length leaves only what was brought in
        assert (b sla 4) = "1111" report "sla by the whole length" severity failure;
        assert (b sll 4) = "0000" report "sll by the whole length" severity failure;

        assert classify("110") = 1 report "case? wildcard" severity failure;
        assert classify("010") = 2 report "case? second" severity failure;
        assert classify("001") = 3 report "case? exact" severity failure;
        assert classify("000") = 0 report "case? others" severity failure;

        -- 9.3.3.3: a choice that is a range may be given the whole run at
        -- once, and then the value is of the aggregate's own type
        slv := (3 downto 0 => X"C", others => '0');
        assert slv = X"0C" report "run given at once: " & to_hstring(slv) severity failure;
        slv := (7 downto 4 => '0') & X"C";
        assert slv = X"0C" report "run of one element" severity failure;

        wait for 1 ns;
        assert picked = 30 report "select? others" severity failure;
        sel <= "10";
        wait for 1 ns;
        assert picked = 10 report "select? wildcard" severity failure;
        sel <= "01";
        wait for 1 ns;
        assert picked = 20 report "select? exact" severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
    end process;
end architecture;
