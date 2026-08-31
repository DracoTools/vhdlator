-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_dynunary is
end entity;

architecture test of t_vhdl_dynunary is

    -- Returns of differing lengths, so what comes back carries its bounds
    function pick (n : natural; v : std_logic_vector) return std_logic_vector is
    begin
        if n = 0 then
            return v(3 downto 0);
        end if;
        return v;
    end function;

    -- A package of its own may declare RESIZE, which is not numeric_std's
    function resize (v : std_logic_vector; n : natural) return std_logic_vector is
        variable r : std_logic_vector(n - 1 downto 0) := (others => '0');
    begin
        for i in 0 to n - 1 loop
            if i <= v'length - 1 then
                r(i) := v(v'low + i);
            end if;
        end loop;
        return r;
    end function;

    constant NUL : string(2 to 1) := (others => ' ');

    signal d : natural := 1;

begin

    main : process is
        variable v : std_logic_vector(7 downto 0) := "10110010";
    begin
        assert not pick(d, v) = "01001101" report "not" severity failure;
        assert not pick(0, v) = "1101" report "not short" severity failure;

        assert (or pick(d, v)) = '1' report "or" severity failure;
        assert (and pick(d, v)) = '0' report "and" severity failure;
        assert (xor pick(d, v)) = '0' report "xor" severity failure;
        assert (nand pick(d, v)) = '1' report "nand" severity failure;
        assert (nor pick(d, v)) = '0' report "nor" severity failure;
        assert (xnor pick(d, v)) = '1' report "xnor" severity failure;

        assert NUL'length = 0 report "null string" severity failure;
        assert resize(v, 4) = "0010" report "own resize" severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
