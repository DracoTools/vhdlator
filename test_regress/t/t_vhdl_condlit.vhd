-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A condition that is not already a BOOLEAN has the condition operator "??"
-- applied to it, and the expression is legal where exactly one reading of it
-- is a BOOLEAN once it is (9.2.9, and note 1 of 12.5).
--
-- So a bare 'H' as a condition is the STD_ULOGIC one: CHARACTER also has that
-- literal, but no "??" takes a CHARACTER, so only one reading survives. A
-- bare '1' stays ambiguous, because BIT has a "??" as well as STD_ULOGIC --
-- nvc 1.22.1 and GHDL 7.0.0-dev both draw the line in the same place, and
-- t_vhdl_condlit_bad holds that half.
--
-- Only in a condition: "variable b : boolean := 'H';" is still an error, and
-- the operands of "a = b" in a condition are not themselves conditions.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_condlit is
end entity;

architecture a of t_vhdl_condlit is
begin

    main: process is
        variable n : integer := 0;
        variable s : std_logic := 'H';
    begin
        -- The metavalues, whose literals only STD_ULOGIC and CHARACTER have
        if 'H' then n := n + 1; end if;
        if 'L' then n := n + 2; end if;
        if 'W' then n := n + 4; end if;
        if 'X' then n := n + 8; end if;
        if 'U' then n := n + 16; end if;
        if '-' then n := n + 32; end if;
        if 'Z' then n := n + 64; end if;

        -- 'H' is the only one of those that is true, and this runtime maps it
        -- onto one; the rest are false here
        assert n = 1 report "n " & integer'image (n) severity failure;

        -- A while loop's condition is a condition too
        n := 0;
        while 'H' loop
            n := n + 1;
            exit when n = 3;
        end loop;
        assert n = 3 report "while" severity failure;

        -- The operands of a comparison are not themselves conditions, so
        -- this resolves the ordinary way and stays right
        if s = 'H' then n := 100; end if;
        assert n = 100 report "operand" severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
        wait;
    end process;

end architecture;
