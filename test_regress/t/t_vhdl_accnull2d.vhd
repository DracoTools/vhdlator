-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- An access to an array of more than one dimension whose bounds only an
-- allocation could give it, used only as a value: null, compared, assigned
-- and handed to a subprogram. Declaring one was refused (VESTS tc519).
entity t_vhdl_accnull2d is
end entity;

architecture test of t_vhdl_accnull2d is
    type grid is array (natural range <>, natural range <>) of bit;
    type grid_ptr is access grid;

    procedure check_null(variable p : in grid_ptr; variable r : out boolean) is
    begin
        r := p = null;
    end procedure;
begin
    process
        variable a : grid_ptr;
        variable b : grid_ptr := null;
        variable r : boolean := false;
    begin
        assert a = null report "starts null" severity failure;
        b := a;
        assert b = null and a = b report "assigned" severity failure;
        check_null(a, r);
        assert r report "handed on" severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
