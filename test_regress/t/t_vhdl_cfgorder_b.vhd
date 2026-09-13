-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- The entity the binding above names, with two architectures to choose from.

entity worker is
    port ( d : in bit; q : out bit );
end entity;

architecture fast of worker is
begin
    q <= d;
end architecture;

architecture slow of worker is
begin
    q <= not d;
end architecture;
