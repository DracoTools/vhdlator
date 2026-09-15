-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- 13.1: a package declaration is a design unit of its own and stands
-- without its body, so a design that names a subprogram declared here
-- analyzes. Elaborating the call is what needs the body (12.3).
package t_vhdl_nobody_pkg is
    function to_int (v : bit_vector) return integer;
    procedure note (n : integer);
end package;

use work.t_vhdl_nobody_pkg.all;

entity t_vhdl_nobody is
end entity;

architecture test of t_vhdl_nobody is
begin
    main : process is
        variable n : integer;
    begin
        n := to_int("0101");
        note(n);
        wait;
    end process;
end architecture;
