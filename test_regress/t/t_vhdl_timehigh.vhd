-- DESCRIPTION: Verilator: VHDL wait that would end after TIME'HIGH
--
-- Time stops at TIME'HIGH: the simulation is complete once the next thing to
-- happen lies beyond it (14.7.5.1), so a process waiting past it does not
-- resume, and the clock does not wrap round.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_timehigh is
end entity;

architecture test of t_vhdl_timehigh is
    signal x : bit;
begin

    main : process is
        variable n : natural := 0;
        variable step : time := 100 ms;
    begin
        x <= not x;
        n := n + 1;
        -- TIME'HIGH is some 92,233 steps of 100 ms away with a femtosecond
        -- resolution; a clock that wrapped would count past that
        assert n <= 100000 report "time wrapped round" severity failure;
        if n mod 2 = 0 then
            wait for 100 ms;             -- a delay the text gives
        else
            wait for step;               -- and one worked out as it runs
        end if;
    end process;

    -- A wait that is past the end from the start
    never : process is
    begin
        wait for time'high;
        wait for 1 fs;
        report "resumed after TIME'HIGH" severity failure;
        wait;
    end process;

end architecture;
