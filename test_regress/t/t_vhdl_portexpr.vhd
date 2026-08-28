-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity watcher is
    port ( d : in bit_vector(15 downto 0) );
end entity;

architecture rtl of watcher is
begin
    check : process is
    begin
        -- The port holds its default until the association first delivers,
        -- rather than following the expression with no delay at all. How
        -- many delta cycles that takes is not observable here.
        assert d = X"0000" report "at zero: " & to_hstring(d) severity failure;
        wait for 1 ns;
        assert d = X"42ff" report "after: " & to_hstring(d) severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;

entity t_vhdl_portexpr is
end entity;

architecture test of t_vhdl_portexpr is
    signal hi : bit_vector(7 downto 0);
    signal lo : bit_vector(7 downto 0);
begin
    u : entity work.watcher port map ( d => hi & lo );

    hi <= X"42";
    lo <= X"ff";
end architecture;
