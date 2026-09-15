-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

entity t_vhdl_typestmt_bad is
end entity;

architecture test of t_vhdl_typestmt_bad is
    type small is range 0 to 7;
    type pair is record
        a : small;
        b : small;
    end record;
begin

    main : process is
    begin
        -- 10.7: a statement of its own is a procedure call. A type mark
        -- here reads as a conversion with no operand, which is what an
        -- object declaration left among the statements looks like to the
        -- parser: the label is taken for a statement label.
        small;
        v : pair;
        wait;
    end process;
end architecture;
