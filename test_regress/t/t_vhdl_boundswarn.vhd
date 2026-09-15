-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

entity t_vhdl_boundswarn is
end entity;

architecture test of t_vhdl_boundswarn is
    subtype small is integer range 0 to 7;
    constant never : boolean := false;
begin

    main : process is
        variable v : small := 1;
    begin
        -- 1076-2008 makes each of these an error of execution, not of
        -- analysis: the statement raises CONSTRAINT_ERROR when it is
        -- reached, and none of these is reached. A design like this
        -- analyzes in GHDL 7.0.0-dev, nvc 1.22.1 and Questa 2025.2.
        if never then
            v := 400;
            wait for -1 ns;
        end if;

        assert v = 1 report "the reachable statements ran" severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
    end process;
end architecture;
