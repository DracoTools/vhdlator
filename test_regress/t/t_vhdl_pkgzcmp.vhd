-- DESCRIPTION: Verilator: VHDL signal of a package compared with a value holding 'Z'
--
-- An element no driver drives is 'Z', and "=" tells it from one that is
-- driven (9.2.3): the signal equals a literal only where exactly the
-- literal's 'Z' elements are undriven.  Two units drive the signal here.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_pkgzcmp_pkg is
    signal s : std_logic_vector(3 downto 0);
    signal b : std_logic;
    signal go : boolean := false;
end package;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_pkgzcmp_pkg.all;

entity t_vhdl_pkgzcmp_other is
end entity;

architecture rtl of t_vhdl_pkgzcmp_other is
begin
    process is
    begin
        s <= "ZZZZ";
        wait until go;
        s <= "ZZZ1";
        wait;
    end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_pkgzcmp_pkg.all;

entity t_vhdl_pkgzcmp is
end entity;

architecture test of t_vhdl_pkgzcmp is
begin
    u : entity work.t_vhdl_pkgzcmp_other;

    process is
    begin
        s <= "01ZZ";
        b <= 'Z';
        wait for 1 ns;
        assert s = "01ZZ" severity failure;
        assert not (s /= "01ZZ") severity failure;
        assert s /= "0100" severity failure;
        assert s /= "ZZZZ" severity failure;
        assert b = 'Z' severity failure;
        assert b /= '0' severity failure;
        -- An element and a slice are asked the same way
        assert s(0) = 'Z' and s(1) = 'Z' severity failure;
        assert s(0) /= '0' and s(3) = '0' and s(2) = '1' severity failure;
        assert s(3) /= 'Z' severity failure;
        assert s(1 downto 0) = "ZZ" severity failure;
        assert s(1 downto 0) /= "00" severity failure;
        assert s(3 downto 2) = "01" severity failure;
        assert s(2 downto 1) = "1Z" severity failure;
        -- The other unit's driver takes an element this one left alone
        go <= true;
        wait for 1 ns;
        assert s = "01Z1" severity failure;
        assert s /= "01ZZ" severity failure;
        s <= "1100";
        b <= '1';
        wait for 1 ns;
        -- Element 0 has a '0' against a '1'; the others are as given
        assert s(3 downto 1) = "110" severity failure;
        assert s /= "01ZZ" severity failure;
        assert b = '1' severity failure;
        assert b /= 'Z' severity failure;
        s <= "ZZZZ";
        wait for 1 ns;
        assert s = "ZZZ1" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
