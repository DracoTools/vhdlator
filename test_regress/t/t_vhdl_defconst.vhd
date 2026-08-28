-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package deferred is
    -- Declared with no value at all: the value comes with the package body
    constant mask : bit_vector;
    constant width : natural;
end package;

package body deferred is
    pure function low_ones (v : bit_vector; n : natural) return bit_vector is
        variable r : bit_vector(v'length - 1 downto 0);
    begin
        r := v;
        return r(n - 1 downto 0);
    end function;

    constant mask : bit_vector := low_ones(X"1F", 5);
    constant width : natural := mask'length;
end package body;

use work.deferred.all;

entity t_vhdl_defconst is
end entity;

architecture test of t_vhdl_defconst is
    signal held : bit_vector(width - 1 downto 0) := mask;
begin
    check : process is
    begin
        assert mask = "11111" report "mask = " & to_string(mask) severity failure;
        assert width = 5 report "width = " & integer'image(width) severity failure;
        assert held = "11111" report "held = " & to_string(held) severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
