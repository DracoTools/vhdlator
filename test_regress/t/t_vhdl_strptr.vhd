-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_strptr is
end entity;

architecture a of t_vhdl_strptr is
    -- An access to STRING, which is what STD.TEXTIO's LINE is and what
    -- VUnit's string_ptr holds
    type str_ptr is access string;
begin
    process
        variable p : str_ptr;
        variable q : str_ptr;
    begin
        p := new string'("abcde");

        -- Read one character of what it points at
        assert p(1) = 'a' report "read first" severity failure;
        assert p(5) = 'e' report "read last" severity failure;

        -- And write one
        p(2) := 'X';
        p(5) := 'Z';
        assert p.all = "aXcdZ" report "wrote " & p.all severity failure;

        -- A slice of it, read and written
        assert p(2 to 4) = "Xcd" report "slice read " & p(2 to 4) severity failure;
        p(3 to 4) := "yz";
        assert p.all = "aXyzZ" report "slice wrote " & p.all severity failure;

        -- A second one, so nothing depends on there being only one
        q := new string'("wxyz");
        q(1) := 'W';
        q(4) := 'Z';
        assert q.all = "WxyZ" report "q wrote " & q.all severity failure;
        assert p.all = "aXyzZ" report "p disturbed " & p.all severity failure;

        deallocate(p);
        deallocate(q);
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
