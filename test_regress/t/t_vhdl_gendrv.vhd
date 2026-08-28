-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity sub is
    generic (pick : boolean);
    port (o : out bit);
end entity;

architecture rtl of sub is
begin
    -- Two drivers of o as written, of which elaboration keeps one (11.8),
    -- so the signal needs no resolution function
    g1 : if pick generate
        o <= '1';
    end generate;
    g2 : if not pick generate
        o <= '0';
    end generate;
end architecture;

entity t_vhdl_gendrv is
end entity;

architecture test of t_vhdl_gendrv is
    signal a, b : bit;
begin

    u1 : entity work.sub generic map (pick => true) port map (o => a);
    u2 : entity work.sub generic map (pick => false) port map (o => b);

    process is
    begin
        wait for 1 ns;
        assert a = '1' report "a" severity failure;
        assert b = '0' report "b" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
