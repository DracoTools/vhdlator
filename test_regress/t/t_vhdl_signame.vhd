-- DESCRIPTION: Verilator: VHDL name attribute of an overloaded subprogram chosen by a signature
--
-- "foo [bit return string]'instance_name" names the declaration of foo
-- the signature matches, not the first one (8.6, 16.2.7).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_signame is
end entity;

architecture test of t_vhdl_signame is
    function foo (x : integer) return boolean is
    begin
        return true;
    end function;

    function foo (y : bit) return string is
    begin
        return "hello";
    end function;

    procedure foo (z : character) is
    begin
    end procedure;
begin

    p : process is
    begin
        assert foo [integer return boolean]'instance_name
            = ":t_vhdl_signame(test):foo[integer return boolean]:" severity failure;
        assert foo [bit return string]'instance_name
            = ":t_vhdl_signame(test):foo[bit return string]:"
            report foo [bit return string]'instance_name severity failure;
        assert foo [character]'instance_name = ":t_vhdl_signame(test):foo[character]:"
            report foo [character]'instance_name severity failure;
        assert foo [bit return string]'simple_name = "foo" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
