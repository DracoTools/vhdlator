-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package growing is
    type real_vector is array (natural range <>) of real;
    procedure walk (prefix : string; n : natural; total : inout natural);
    function series (n : natural) return real_vector;
    function table_length return natural;
    function table_sum return real;
end package;

package body growing is
    -- Calls itself with an argument four characters longer at every level.
    -- A copy per argument length is a series with no last member, since what
    -- ends the recursion is a value and no copy can settle a value.
    procedure walk (prefix : string; n : natural; total : inout natural) is
    begin
        total := total + prefix'length;
        if n /= 0 then walk(prefix & "....", n - 1, total); end if;
    end procedure;

    function series (n : natural) return real_vector is
        variable r : real_vector(0 to n);
    begin
        for i in 0 to n loop r(i) := real(i); end loop;
        return r;
    end function;

    -- How long this is, only the copy made for that call says
    constant table : real_vector := series(5);

    function table_length return natural is
    begin
        return table'length;
    end function;

    function table_sum return real is
        variable s : real := 0.0;
    begin
        for i in table'range loop s := s + table(i); end loop;
        return s;
    end function;
end package body;

use work.growing.all;

entity t_vhdl_recgrow is
end entity;

architecture test of t_vhdl_recgrow is
begin
    check : process is
        variable total : natural := 0;
    begin
        -- 1 + 5 + 9 + 13
        walk("*", 3, total);
        assert total = 28 report "total = " & integer'image(total) severity failure;
        assert table_length = 6
            report "length = " & integer'image(table_length) severity failure;
        assert table_sum = 15.0
            report "sum = " & real'image(table_sum) severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
