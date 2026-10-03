-- DESCRIPTION: Verilator: VHDL signal of a package with 'Z' elements compared with another value
--
-- Against another such signal the two are equal where the same elements
-- are undriven and the rest agree; against a variable or a constant, which
-- hold no 'Z' here, only where every element is driven (9.2.3).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_pkgzcmp2_pkg is
    signal p, q : std_logic_vector(3 downto 0);
end package;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_pkgzcmp2_pkg.all;

entity t_vhdl_pkgzcmp2 is
end entity;

architecture test of t_vhdl_pkgzcmp2 is
begin
    process is
        variable v : std_logic_vector(3 downto 0) := "0100";
        constant c : std_logic_vector(3 downto 0) := "0101";
    begin
        p <= "01ZZ";
        q <= "01ZZ";
        wait for 1 ns;
        assert p = q severity failure;
        assert not (p /= q) severity failure;
        assert p /= v severity failure;   -- "0100" has no 'Z'
        assert v /= p severity failure;
        q <= "01Z0";
        wait for 1 ns;
        assert p /= q severity failure;
        assert p(3 downto 2) = q(3 downto 2) severity failure;
        assert p(1) = q(1) severity failure;
        assert p(0) /= q(0) severity failure;
        p <= "0101";
        wait for 1 ns;
        assert p = c severity failure;
        assert c = p severity failure;
        assert p /= v severity failure;
        v := "0101";
        assert p = v severity failure;
        assert p(1 downto 0) = v(1 downto 0) severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
