-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package widths is
    -- Two parameters with no length of their own, and two with defaults, so
    -- a call that leaves the last ones out has fewer actuals than formals
    function widen (constant value : in bit_vector;
                    constant target : in bit_vector;
                    constant name : in string := "value";
                    constant fill : in bit := '0') return bit_vector;
end package;

package body widths is
    function widen (constant value : in bit_vector;
                    constant target : in bit_vector;
                    constant name : in string := "value";
                    constant fill : in bit := '0') return bit_vector is
        variable r : bit_vector(target'range) := (others => fill);
    begin
        assert value'length <= target'length report name severity failure;
        r(value'length - 1 downto 0) := value;
        return r;
    end function;
end package body;

use work.widths.all;

entity t_vhdl_specdflt is
end entity;

architecture test of t_vhdl_specdflt is
begin
    check : process is
        variable short : bit_vector(3 downto 0) := "1010";
        variable long  : bit_vector(7 downto 0);
    begin
        -- Positional, both defaults left out
        long := widen(short, long);
        assert long = "00001010" report to_string(long) severity failure;
        -- Named, and out of the order the declaration gives
        long := widen(fill => '1', target => long, value => short);
        assert long = "11111010" report to_string(long) severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
