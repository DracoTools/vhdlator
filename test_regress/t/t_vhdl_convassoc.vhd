-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity sub is
    port (o : out integer;
          i : in real);
end entity;

architecture rtl of sub is
begin
    p : process is
    begin
        o <= 1;
        assert i = real(5) report "i" severity failure;
        wait for 1 ns;
        o <= 2;
        wait;
    end process;
end architecture;

entity t_vhdl_convassoc is
end entity;

architecture test of t_vhdl_convassoc is
    signal x : real;
    signal y : integer := 5;
begin

    -- A conversion written on the formal converts what the port drives on
    -- its way out; one written on the actual converts what goes in (6.5.7.1)
    u : entity work.sub port map (real(o) => x, i => real(y));

    process is
    begin
        assert x = real(integer'left) report "elaborated" severity failure;
        wait for 0 ns;
        assert x = 1.0 report "first" severity failure;
        wait for 2 ns;
        assert x = 2.0 report "second" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
