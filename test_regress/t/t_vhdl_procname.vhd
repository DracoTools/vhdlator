-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A procedure and a function are never homographs, so both may be declared
-- with one name and one profile.  A procedure call statement can only name
-- a procedure and an expression can only name a function (4.3, 10.7), so
-- the two never compete.  A subprogram that takes no arguments at all is
-- written as a bare name (4.2.2.1), which is the only place the statement
-- position has to decide.  VUnit's logger_pkg declares "final_log_check"
-- both ways and calls it both ways.

package t_vhdl_procname_pkg is
    procedure act (flag : boolean := false);
    impure function act (flag : boolean := false) return boolean;
    -- and a pair taking a required argument, so the bare-name case is not
    -- the only one covered
    procedure tally (n : integer);
    impure function tally (n : integer) return integer;
end package;

package body t_vhdl_procname_pkg is
    procedure act (flag : boolean := false) is
    begin
        report "procedure act ran";
    end procedure;

    impure function act (flag : boolean := false) return boolean is
    begin
        return true;
    end function;

    procedure tally (n : integer) is
    begin
        report "procedure tally ran with " & integer'image(n);
    end procedure;

    impure function tally (n : integer) return integer is
    begin
        return n + 1;
    end function;
end package body;

use work.t_vhdl_procname_pkg.all;

entity t_vhdl_procname is
end entity;

architecture a of t_vhdl_procname is
begin

    p: process is
        variable b : boolean;
        variable i : integer;
    begin
        -- a statement, so the procedure, however it is written
        act;
        act(true);
        act(flag => false);
        tally(3);

        -- an expression, so the function
        b := act;
        assert b report "function act" severity failure;
        b := act(true);
        assert b report "function act with an argument" severity failure;
        i := tally(3);
        assert i = 4 report "function tally" severity failure;

        report "*-* All Finished *-*";
        wait;
    end process;

end architecture;
