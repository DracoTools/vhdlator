-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- The entity the configuration configures.

entity tb is
end entity;

architecture rtl of tb is
    component worker is
        port ( d : in bit; q : out bit );
    end component;
    signal d, q : bit := '0';
begin
    u : worker port map ( d => d, q => q );
end architecture;
