-- DESCRIPTION: Verilator: VHDL pulse rejection limit longer than the delay
--
-- The pulse rejection limit of an inertial assignment may not be longer than
-- the delay of the first waveform element (10.5.2.1).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_reject_bad is
end entity;

architecture test of t_vhdl_reject_bad is
    signal x, y, z : bit;
begin
    y <= reject 2 ns inertial x after 1 ns;

    process is
    begin
        z <= reject 500 ps inertial x after 1 ns;  -- fine
        z <= reject 3 ns inertial x after 2 ns, not x after 5 ns;
        wait;
    end process;
end architecture;
