-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- The top of the design has no instantiation to say how long its generics
-- are, and a STRING says its own length, so it needs nothing passed beside it
entity t_vhdl_strgeneric is
    generic (tag : string);
end entity;

architecture test of t_vhdl_strgeneric is
begin
    check : process is
    begin
        assert tag = "hello" report "tag = " & tag severity failure;
        assert tag'length = 5
            report "length = " & integer'image(tag'length) severity failure;
        assert tag(1) = 'h' report "first = " & tag(1) severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
