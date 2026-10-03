-- DESCRIPTION: Verilator: VHDL generic constant of a generic type (6.5.6.2)
--
-- A generic constant whose subtype is a generic type of the same package
-- has, in an instance, the type that instance gave the generic type, so an
-- operator generic left as "is <>" applies to it.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_genpkgtyped_pt is
    generic ( type t;
              one, zero : t;
              function "+"(x, y : t) return t is <>;
              function "-"(x, y : t) return t is <> );

    function step(x : t) return t;

    type counter is protected
        procedure increment;
        procedure decrement;
        impure function value return t;
    end protected;
end package;

package body t_vhdl_genpkgtyped_pt is
    function step(x : t) return t is
    begin
        return x + one;
    end function;

    type counter is protected body
        variable val : t := zero;

        procedure increment is
        begin
            val := val + one;
        end procedure;

        procedure decrement is
        begin
            val := val - one;
        end procedure;

        impure function value return t is
        begin
            return val;
        end function;
    end protected body;
end package body;

package t_vhdl_genpkgtyped_int is new work.t_vhdl_genpkgtyped_pt
    generic map ( t => integer, one => 1, zero => 0 );

package t_vhdl_genpkgtyped_sv is
    shared variable var : work.t_vhdl_genpkgtyped_int.counter;
end package;

entity t_vhdl_genpkgtyped is
end entity;

architecture test of t_vhdl_genpkgtyped is
    package real_counter is new work.t_vhdl_genpkgtyped_pt
        generic map ( t => real, one => 1.5, zero => 0.0 );
    shared variable rvar : real_counter.counter;
begin

    process is
        alias ivar is work.t_vhdl_genpkgtyped_sv.var;
    begin
        assert work.t_vhdl_genpkgtyped_int.step(41) = 42 severity failure;
        assert real_counter.step(1.0) = 2.5 severity failure;

        assert ivar.value = 0 severity failure;
        ivar.increment;
        ivar.increment;
        ivar.decrement;
        assert ivar.value = 1 severity failure;

        assert rvar.value = 0.0 severity failure;
        rvar.increment;
        rvar.increment;
        rvar.decrement;
        assert rvar.value = 1.5 severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
