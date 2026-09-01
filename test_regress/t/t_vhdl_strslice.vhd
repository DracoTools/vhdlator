-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_strslice is
end entity;

architecture test of t_vhdl_strslice is
begin

    main : process is
        variable s : string(1 to 8) := "abcdefgh";
        variable n : integer := 3;
    begin
        -- A slice whose ends only running settles, filled with an aggregate
        -- that says no length of its own: the target says it (9.3.3.3)
        s(n to s'right) := (others => '0');
        assert s = "ab000000" report "fill " & s severity failure;

        -- The value is read before any of it is written, so a slice may be
        -- copied over itself (10.6.2)
        s := "abcdefgh";
        s(n + 1 to s'right) := s(n to s'right - 1);
        assert s = "abccdefg" report "shift " & s severity failure;

        -- A width the text does settle still works the way it did
        s(2 to 3) := "XY";
        assert s = "aXYcdefg" report "static " & s severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
