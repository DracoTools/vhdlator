-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A component instance no entity is bound to (7.3.3). Legal, and most often
-- a file left off the command line, so it is a warning rather than an error.

entity t_vhdl_unbound_bad is
end entity;

architecture test of t_vhdl_unbound_bad is
    signal b : integer;

    component nowhere is
        port (y : out integer := 55);
    end component;
begin

    c1 : component nowhere
        port map (b);

end architecture;
