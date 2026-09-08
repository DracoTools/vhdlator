-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Where the condition operator leaves more than one reading, the expression
-- is still ambiguous: applying "??" settles a condition only when exactly one
-- reading of it has one (9.2.9, and note 1 of 12.5). A bare '1' is one such
-- case -- BIT has a "??" and so does the type declared here -- and nvc 1.22.1
-- refuses the same shape over BIT and STD_ULOGIC. See t_vhdl_condlit for the
-- half that is legal.
--
-- And outside a condition the operator is not applied at all, so a character
-- literal given to a BOOLEAN object stays unresolved.
--
-- Declared here rather than taken from ieee.std_logic_1164 so that the
-- candidates the message lists are all of this file, and the golden says the
-- same thing wherever it is run.

entity t_vhdl_condlit_bad is
end entity;

architecture a of t_vhdl_condlit_bad is

    type tri is ('0', '1', 'H');

    function "??" (l : tri) return boolean is
    begin
        return l = '1' or l = 'H';
    end function;

begin

    main: process is
        variable n : integer := 0;
        variable b : boolean := 'H';
    begin
        if '1' then n := n + 1; end if;
        wait;
    end process;

end architecture;
