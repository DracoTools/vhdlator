-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package lg is
    procedure emit (msg : string; wide : boolean);
end package;

package body lg is
    procedure emit (msg : string; wide : boolean) is
        -- Reaches out for the enclosing procedure's own parameter, and has a
        -- parameter of its own that a call may leave out (4.2.2.1)
        procedure put (pad : string; twice : boolean := false) is
        begin
            report pad & msg;
            if twice then report pad & msg; end if;
        end procedure;
    begin
        if wide then
            put("  ", twice => true);
        else
            put(">");
        end if;
    end procedure;
end package body;

use work.lg.all;

entity t_vhdl_capname is
end entity;

architecture test of t_vhdl_capname is
begin
    check : process is
    begin
        emit("one", false);
        emit("two", true);
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
