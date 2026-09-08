-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A subprogram written against an unconstrained array is compiled once per
-- call site, and the copy's parameter takes its length from the actual. Where
-- the actual is the result of an operator the design declares itself, that
-- result has no length until the copy of the operator is made -- so the call
-- was given a length inferred beforehand, which is an operand's, and two call
-- sites of different widths shared one copy. The answer was wrong, not merely
-- the diagnostics: the second call ran the first call's body.
--
-- ieee.fixed_pkg is where this lands: "+" on sfixed declares its result
-- "sfixed (maximum(l'high, r'high) + 1 downto mine(l'low, r'low))", one
-- element longer than either operand.

entity t_vhdl_opwid is
end entity;

architecture a of t_vhdl_opwid is

    -- Written against an unconstrained array: one copy per call site
    function widthof (v : bit_vector) return integer is
    begin
        return v'length;
    end function;

    -- An operator whose result is one longer than its operands
    function "+" (l, r : bit_vector) return bit_vector is
        variable o : bit_vector (l'length downto 0) := (others => '0');
    begin
        o (l'length - 1 downto 0) := l;
        return o;
    end function;

    -- And one whose result is shorter, so the inference is wrong the other
    -- way round too
    function "-" (l, r : bit_vector) return bit_vector is
        variable o : bit_vector (l'length - 2 downto 0) := (others => '0');
    begin
        return o;
    end function;

    signal p : bit_vector (7 downto 0) := "00000001";
    signal q : bit_vector (7 downto 0) := "00000010";
    signal w : bit_vector (3 downto 0) := "0011";

begin

    main: process is
    begin
        -- The plain call, which was always right
        assert widthof (p) = 8 report "plain" severity failure;

        -- The operator result, one longer
        assert widthof (p + q) = 9
            report "longer: got " & integer'image (widthof (p + q)) severity failure;

        -- And one shorter
        assert widthof (p - q) = 7
            report "shorter: got " & integer'image (widthof (p - q)) severity failure;

        -- A second width of the same operator, so the copies really are two
        assert widthof (w + w) = 5
            report "narrow operand: got " & integer'image (widthof (w + w)) severity failure;

        -- Nested: the operator's result handed to the operator again
        assert widthof ((p + q) + (p + q)) = 10
            report "nested: got " & integer'image (widthof ((p + q) + (p + q)))
            severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
        wait;
    end process;

end architecture;
