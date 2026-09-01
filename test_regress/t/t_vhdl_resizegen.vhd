-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_resizegen_sub is
    generic (GC_WIDE : natural := 12;
             GC_NARROW : natural := 2);
    port (wide   : out unsigned(GC_WIDE - 1 downto 0);
          narrow : out unsigned(GC_NARROW - 1 downto 0);
          swide  : out signed(GC_WIDE - 1 downto 0));
end entity;

architecture test of t_vhdl_resizegen_sub is
    constant c_v  : unsigned(3 downto 0) := "1011";
    constant c_sv : signed(3 downto 0) := "1011";
begin

    main : process is
        -- A length a generic settles is not a number here, but it is one by
        -- the time widths are worked out, so RESIZE says how long its
        -- result is rather than leaving that to what reads it (9.3.6)
        variable v_wide : unsigned(GC_WIDE - 1 downto 0);
    begin
        v_wide := resize(c_v, GC_WIDE);
        assert v_wide = 11 report "variable" severity failure;
        wide   <= resize(c_v, GC_WIDE);
        narrow <= resize(c_v, GC_NARROW);
        swide  <= resize(c_sv, GC_WIDE);
        wait;
    end process;

end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_resizegen is
end entity;

architecture test of t_vhdl_resizegen is
    signal wide   : unsigned(11 downto 0);
    signal narrow : unsigned(1 downto 0);
    signal swide  : signed(11 downto 0);
begin

    sub : entity work.t_vhdl_resizegen_sub
        generic map (GC_WIDE => 12, GC_NARROW => 2)
        port map (wide => wide, narrow => narrow, swide => swide);

    check : process is
    begin
        wait for 1 ns;
        -- Widening keeps the value, narrowing keeps the rightmost bits, and
        -- a SIGNED value keeps its sign (9.3.6)
        assert wide = 11 report "wide " & to_string(wide) severity failure;
        assert narrow = 3 report "narrow " & to_string(narrow) severity failure;
        assert swide = -5 report "swide " & to_string(swide) severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
