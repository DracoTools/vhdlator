-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use std.textio.all;

entity t_vhdl_dyntext is
end entity;

architecture test of t_vhdl_dyntext is
    signal n   : integer                      := 7;
    signal buf : std_logic_vector(7 downto 0) := "10101010";
begin

    main : process is
        variable l : line;
    begin
        wait for 1 ns;

        -- How long the value is only running decides, so its text is one
        -- character per element rather than a width the model was built
        -- with (16.3)
        assert to_string(buf(n - 1 downto 0)) = "0101010"
            report "to_string" severity failure;

        -- And the same value written onto a line
        write(l, buf(n - 1 downto 0));
        assert l.all = "0101010" report "write" severity failure;
        deallocate(l);

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
