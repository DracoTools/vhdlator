-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

entity t_vhdl_noloop_bad is
end entity;

architecture test of t_vhdl_noloop_bad is
    procedure note is
    begin
        report "not waiting";
    end procedure;
begin

    -- 11.3: a process with no sensitivity list and no wait loops for ever at
    -- the start of the simulation, directly or through what it calls
    loops : process is
    begin
        report "no wait";
    end process;

    loops_too : process is
    begin
        note;
    end process;

    -- One that ends the run does not loop
    ends : process is
    begin
        std.env.finish;
    end process;
end architecture;
