-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package walker is
    type real_vector is array (natural range <>) of real;
    function series (n : natural) return real_vector;
    function trail (id : natural; pad : string) return string;
    function total return real;
    function table_length return natural;
end package;

package body walker is
    function parent_of (id : natural) return natural is
    begin
        if id = 0 then return 0; end if;
        return id - 1;
    end function;

    -- Calls itself with the argument it was given. Nothing about the call
    -- changes from one level to the next, so one copy answers all of them.
    function trail (id : natural; pad : string) return string is
    begin
        if id = 0 then return pad; end if;
        return trail(parent_of(id), pad) & "-";
    end function;

    function series (n : natural) return real_vector is
        variable r : real_vector(0 to n);
    begin
        for i in 0 to n loop r(i) := real(i); end loop;
        return r;
    end function;

    -- How long this is, only the copy made for that call says. A design that
    -- has spent every copy it is allowed never finds out.
    constant table : real_vector := series(5);

    function total return real is
        variable s : real := 0.0;
    begin
        for i in table'range loop s := s + table(i); end loop;
        return s;
    end function;

    function table_length return natural is
    begin
        return table'length;
    end function;
end package body;

use work.walker.all;

entity t_vhdl_recself is
end entity;

architecture test of t_vhdl_recself is
begin
    check : process is
    begin
        assert trail(3, "*") = "*---"
            report "trail = " & trail(3, "*") severity failure;
        assert table_length = 6
            report "length = " & integer'image(table_length) severity failure;
        assert total = 15.0 report "total = " & real'image(total) severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
