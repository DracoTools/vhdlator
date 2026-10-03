-- DESCRIPTION: Verilator: VHDL generic function instantiated where its body's context is not visible
--
-- The body of a generic subprogram names what the region it was declared
-- in makes visible -- here "round" of math_real, used only by the package
-- body, and a function local to that body -- and an instance made in
-- another package, which uses neither, still means those (4.4).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_gensubscope_convert is
    function from_real generic (scale : real := 65536.0)
        parameter (x : in real) return integer;
end package;

library ieee;
use ieee.math_real.all;

package body t_vhdl_gensubscope_convert is
    function twice (n : integer) return integer is
    begin
        return 2 * n;
    end function;

    function from_real generic (scale : real := 65536.0)
        parameter (x : in real) return integer is
    begin
        return twice(integer(round(scale * x)));
    end function;
end package body;

use work.t_vhdl_gensubscope_convert.from_real;

package t_vhdl_gensubscope_q is
    function from_real_q12 is new from_real generic map (scale => 4096.0);
end package;

use work.t_vhdl_gensubscope_q.all;

entity t_vhdl_gensubscope is
end entity;

architecture test of t_vhdl_gensubscope is
begin
    process is
    begin
        assert from_real_q12(0.5) = 4096 report integer'image(from_real_q12(0.5))
            severity failure;
        assert from_real_q12(0.25) = 2048 severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
