-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_dynactual is
end entity;

architecture test of t_vhdl_dynactual is

    -- Returns of differing lengths: what comes back has the length of
    -- whichever path ran, and no call can be made for one of them
    function pick (n : natural; v : signed) return signed is
    begin
        if n = 0 then
            return v(3 downto 0);
        end if;
        return v;
    end function;

    function widths (l, r : signed) return integer is
        constant len : integer := maximum(l'length, r'length);
    begin
        return len;
    end function;

    function total (v : signed) return integer is
    begin
        return v'length + to_integer(v);
    end function;

    signal d : natural := 1;

begin

    main : process is
        variable x : signed(7 downto 0) := x"05";
    begin
        assert widths(pick(d, x), pick(d, x)) = 8 report "widths" severity failure;
        assert widths(pick(0, x), pick(0, x)) = 4 report "widths short" severity failure;
        assert total(pick(d, x)) = 13 report "total" severity failure;
        assert total(pick(0, x)) = 9 report "total short" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
