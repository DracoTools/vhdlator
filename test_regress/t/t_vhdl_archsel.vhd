-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity sub is
    port (x : out integer);
end entity;

architecture one of sub is
begin
    x <= 1;
end architecture;

architecture two of sub is
begin
    x <= 2;
end architecture;

architecture three of sub is
begin
    x <= 3;
end architecture;

entity t_vhdl_archsel is
end entity;

architecture test of t_vhdl_archsel is
    signal x1, x2, x3 : integer;
begin

    s1 : entity work.sub(one) port map (x1);
    s2 : entity work.sub(two) port map (x2);
    s3 : entity work.sub port map (x3);  -- The one analyzed last, "three"

    process is
    begin
        wait for 1 ns;
        assert x1 = 1 report "bad x1" severity failure;
        assert x2 = 2 report "bad x2" severity failure;
        assert x3 = 3 report "bad x3" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
