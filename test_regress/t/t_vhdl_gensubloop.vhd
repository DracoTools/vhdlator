-- DESCRIPTION: Verilator: VHDL loop over a subtype whose range names a generic
--
-- "for k in st" runs over the range st is declared with (10.10), which here
-- names a generic of the top entity, so it is a number only once the run
-- gives the generic its value (-Gw=5; str keeps its default).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_gensubloop is
    generic (str : string := "1234"; w : natural := 4);
end entity;

architecture test of t_vhdl_gensubloop is
    subtype up_t is natural range 1 to w;
    subtype down_t is natural range w downto 2;
begin
    p : process is
        subtype chars_t is natural range str'range;
        variable n     : natural;
        variable first : integer;
    begin
        n := 0;
        for k in up_t loop
            n := n + k;
        end loop;
        assert n = 15 severity failure;

        first := -1;
        n := 0;
        for k in down_t loop
            if first < 0 then
                first := k;
            end if;
            n := n + 1;
        end loop;
        assert first = 5 and n = 4 severity failure;

        n := 0;
        for k in chars_t loop
            n := n + k;
        end loop;
        assert n = 10 severity failure;

        -- The same subtype named through the label
        n := 0;
        for k in p.chars_t loop
            n := n + k;
        end loop;
        assert n = 10 severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
