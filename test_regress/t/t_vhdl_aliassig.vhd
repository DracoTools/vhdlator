-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An alias of a subprogram names it by its signature, which is how one of
-- several overloads is picked out (6.6.3), and the alias is called like the
-- subprogram it denotes. Called by a selected name, whether "p (a, b)" is a
-- call or an indexed name was decided from the alias's own kind rather than
-- from what it denotes -- so an alias of a function returning an array was
-- taken for an array, and its arguments for indices. An alias of a function
-- returning a scalar was unaffected, which is why only half of a pair of
-- them failed.
--
-- VUnit's run package declares four functions of one name telling themselves
-- apart only by what they return, and an alias of each:
-- "alias get_string_seed is get_seed[string, string return string]".

package t_vhdl_aliassig_pkg is
    impure function pick (s : string; salt : string := "") return string;
    alias pick_string is pick [string, string return string];
    impure function pick (s : string; salt : string := "") return integer;
    alias pick_int is pick [string, string return integer];
end package;

package body t_vhdl_aliassig_pkg is
    impure function pick (s : string; salt : string := "") return string is
    begin
        return "S" & s & salt;
    end function;
    impure function pick (s : string; salt : string := "") return integer is
    begin
        return s'length + salt'length;
    end function;
end package body;

entity t_vhdl_aliassig is
end entity;

architecture a of t_vhdl_aliassig is
begin

    main: process is
    begin
        -- By a selected name, which is where the two answers parted
        assert work.t_vhdl_aliassig_pkg.pick_string ("ab", "cd") = "Sabcd"
            report "selected alias returning an array" severity failure;
        assert work.t_vhdl_aliassig_pkg.pick_int ("ab", "cd") = 4
            report "selected alias returning a scalar" severity failure;

        -- And with the second parameter left to its default
        assert work.t_vhdl_aliassig_pkg.pick_string ("ab") = "Sab"
            report "selected alias with a defaulted parameter" severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
        wait;
    end process;

end architecture;
