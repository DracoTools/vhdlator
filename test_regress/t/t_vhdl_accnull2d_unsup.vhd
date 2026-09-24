-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- The object such an access points at has nowhere here to keep its bounds,
-- so an allocator and a dereference are refused where they are written
entity t_vhdl_accnull2d_unsup is
end entity;

architecture test of t_vhdl_accnull2d_unsup is
    type grid is array (natural range <>, natural range <>) of bit;
    type grid_ptr is access grid;
begin
    process
        variable a : grid_ptr;
    begin
        a := new grid(0 to 1, 0 to 2);
        a(0, 1) := '1';
        wait;
    end process;
end architecture;
