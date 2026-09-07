-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- The conversions between the logic families keep the same bits, so the
-- frontend carries them out itself rather than compiling their bodies. Which
-- calls those are is decided by the name and by what the call operates on --
-- and a name is not enough: a design may declare a function of that name over
-- a type of its own, and that one has to be called. Otherwise the body a
-- design wrote is silently replaced by the identity, and the answer is wrong
-- with nothing reported.
--
-- ieee.fixed_pkg declares TO_X01 over its own fixed-point types, so this is
-- not a hypothetical shape.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_ownconv is
end entity;

architecture a of t_vhdl_ownconv is

    -- Named for a built-in, over the type the built-in takes, and doing
    -- something else entirely
    function to_x01 (v : std_ulogic_vector) return std_ulogic_vector is
        variable r : std_ulogic_vector (v'range);
    begin
        for i in v'range loop
            r (i) := not v (i);
        end loop;
        return r;
    end function;

    -- The same for one of the conversions between the vector families.
    -- Not TO_BITVECTOR: that one is declared with a defaulted second
    -- parameter, so a design's own of one parameter really is ambiguous
    -- against it, and nvc 1.22.1 says so.
    function to_stdulogicvector (v : bit_vector) return std_ulogic_vector is
        variable r : std_ulogic_vector (v'range);
    begin
        for i in v'range loop
            r (i) := '1';
        end loop;
        return r;
    end function;

    signal s    : std_ulogic_vector (3 downto 0) := "1010";
    signal bits : bit_vector (3 downto 0) := "1100";

begin

    main: process is
        variable w : std_ulogic_vector (3 downto 0);
    begin
        w := to_x01 (s);
        assert w = "0101" report "own to_x01 not called" severity failure;

        w := to_stdulogicvector (bits);
        assert w = "1111" report "own to_stdulogicvector not called" severity failure;

        -- The built-in of the same name is still the built-in: this one is
        -- declared over BIT_VECTOR, which the design's own does not take
        w := ieee.std_logic_1164.to_x01 (s);
        assert w = "1010" report "built-in to_x01 not called" severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
        wait;
    end process;

end architecture;
