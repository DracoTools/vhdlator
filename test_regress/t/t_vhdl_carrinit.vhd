-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An object whose own length only running decides, given a value that a copy
-- made for one call site fixed the length of. The two are held differently --
-- the object carries its bounds, the value does not -- so the elements are
-- copied across one at a time, and how many there are is what the call gives
-- back rather than what its declaration says.
--
-- Two things were in the way. What the copy returns was never asked, only
-- what the subprogram was written with, which is unconstrained. And a call
-- gives its value once, so reading it once per element read it many times.

entity t_vhdl_carrinit is
end entity;

architecture a of t_vhdl_carrinit is

    type int_vec is array (natural range <>) of integer;

    -- The result's length is the argument's value, so only the copy made for
    -- a call site knows it
    function ramp (n : natural) return int_vec is
        variable v : int_vec (1 to n);
    begin
        for i in v'range loop
            v (i) := i * 10;
        end loop;
        return v;
    end function;

begin

    main: process is
        variable m  : natural := 6;
        variable k  : natural := 3;
        -- The target's bounds name a variable, so it carries them
        variable d  : int_vec (1 to m) := ramp (6);
        variable e  : int_vec (1 to k) := ramp (3);
        variable f  : int_vec (0 to m - 1);
    begin
        assert d'length = 6 report "d length " & integer'image (d'length) severity failure;
        assert d (1) = 10 and d (3) = 30 and d (6) = 60
            report "d elements" severity failure;

        assert e'length = 3 report "e length " & integer'image (e'length) severity failure;
        assert e (2) = 20 report "e elements" severity failure;

        -- And assigned rather than initialised, counting from zero
        f := ramp (6);
        assert f (0) = 10 and f (5) = 60 report "f elements" severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
        wait;
    end process;

end architecture;
