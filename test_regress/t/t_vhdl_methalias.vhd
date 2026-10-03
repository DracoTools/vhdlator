-- DESCRIPTION: Verilator: VHDL alias of a protected type's method
--
-- "alias inc is pt.increment []" names a method of the object pt, and a
-- call through the alias runs on that object (6.6.3, 5.6.2): a procedure
-- and a function, neither with parameters.  The arbiters differ: nvc
-- 1.22.1 accepts this (its regression test alias15), Questa 2025.2 refuses
-- it as a method "visible only by selection", and GHDL 7.0.0-dev has an
-- internal error; with parameters nvc fails too, so those are not tested.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_methalias is
end entity;

architecture test of t_vhdl_methalias is

    type t_counter is protected
        procedure increment;
        procedure add (n : natural);
        impure function get return natural;
        impure function plus (n : natural) return natural;
    end protected;

    type t_counter is protected body
        variable count : natural := 0;

        procedure increment is
        begin
            count := count + 1;
        end procedure;

        procedure add (n : natural) is
        begin
            count := count + n;
        end procedure;

        impure function get return natural is
        begin
            return count;
        end function;

        impure function plus (n : natural) return natural is
        begin
            return count + n;
        end function;
    end protected body;

    shared variable pt, other : t_counter;

    alias inc is pt.increment [];
    alias get is pt.get [return natural];
    alias other_get is other.get [return natural];
begin

    tb : process is
    begin
        inc;
        assert get = 1 severity failure;
        inc;
        assert get = 2 severity failure;
        pt.add(5);
        assert get = 7 severity failure;
        -- The alias names one object, and the other is left alone
        assert other_get = 0 severity failure;
        other.increment;
        assert other_get = 1 and get = 7 severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
