-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity stage is
    generic (COUNT : natural;
             H : natural;
             D : positive);
    -- A range using "**", which has to have a width of its own to stand here
    port (o : out bit_vector(0 to D**H - 1);
          count_o : out bit_vector(0 to 3));
end entity;

architecture bhv of stage is
    -- One element per iteration, so COUNT of zero leaves it all zero
    signal seen : bit_vector(0 to 3) := (others => '0');
begin
    -- A range that names a generic, and which COUNT of zero makes null
    g : for c in 0 to COUNT - 1 generate
        seen(c) <= '1';
    end generate;

    o <= (others => '1');
    count_o <= seen;
end architecture;

entity t_vhdl_genrange is
end entity;

architecture test of t_vhdl_genrange is
    signal a : bit_vector(0 to 0);
    signal b : bit_vector(0 to 3);
    signal ca, cb : bit_vector(0 to 3);
begin

    ia : entity work.stage generic map (COUNT => 2, H => 0, D => 4) port map (a, ca);
    ib : entity work.stage generic map (COUNT => 0, H => 1, D => 4) port map (b, cb);

    process is
    begin
        wait for 1 ns;
        assert a = "1" report "a" severity failure;
        assert b = "1111" report "b" severity failure;
        assert ca = "1100" report "ca" severity failure;
        assert cb = "0000" report "cb" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
