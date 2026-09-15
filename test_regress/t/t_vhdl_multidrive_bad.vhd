-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

entity t_vhdl_multidrive_bad is
end entity;

architecture test of t_vhdl_multidrive_bad is
    signal i : integer;
    signal b : bit;
begin

    -- 14.7.2: a signal with more than one source whose subtype names no
    -- resolution function. The language makes this an error of elaboration
    -- rather than of analysis, and GHDL 7.0.0-dev and nvc 1.22.1 both
    -- analyze such a design without complaint, so it is reported the way
    -- Verilator reports the same situation in Verilog.
    i <= 1;
    i <= 2;

    b <= '1';
    b <= '0';
    b <= '1';

end architecture;
