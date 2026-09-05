-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package padding is
    function widen (s : string; extra : natural) return string;
end package;

package body padding is
    function widen (s : string; extra : natural) return string is
        -- Reads the enclosing subprogram's own parameters, so what it gives
        -- back is settled by the call and by nothing before it
        function grown (n : natural) return natural is
        begin
            return s'length + n * extra;
        end function;
        -- A variable of a subprogram is elaborated afresh on every call
        -- (4.3), and this one is as long as that call says
        variable r : string(1 to grown(2));
    begin
        for i in r'range loop
            if i <= s'length then r(i) := s(i); else r(i) := '-'; end if;
        end loop;
        return r;
    end function;
end package body;

use work.padding.all;

entity t_vhdl_strcall is
end entity;

architecture test of t_vhdl_strcall is
begin
    check : process is
    begin
        assert widen("ab", 3) = "ab------"
            report "widen = " & widen("ab", 3) severity failure;
        assert widen("xyz", 1) = "xyz--"
            report "widen = " & widen("xyz", 1) severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
