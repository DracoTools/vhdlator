-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

entity t_vhdl_procall93_bad is
end entity;

architecture test of t_vhdl_procall93_bad is
    signal s : bit;
begin
    -- "process (all)" is VHDL-2008 (11.3); before it, a process names the
    -- signals it is sensitive to
    p : process (all) is
    begin
        report "s changed";
    end process;
end architecture;
