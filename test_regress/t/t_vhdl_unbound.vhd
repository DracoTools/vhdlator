-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_unbound is
end entity;

architecture test of t_vhdl_unbound is
    signal a, b : integer;
    signal c : bit;

    -- No entity of this name, so default binding finds nothing and the
    -- instance is not elaborated (7.3.3, 12.4.3)
    component nowhere is
        port (x : in integer;
              y : out integer := 55;
              z : out bit := '1');
    end component;

begin

    c1 : component nowhere
        port map (a, b, c);

    process is
    begin
        wait for 1 ns;
        -- Nothing drove a, so it holds what an INTEGER starts at
        assert a = integer'left report "a" severity failure;
        -- What the formal would have driven takes the formal's default
        assert b = 55 report "b" severity failure;
        assert c = '1' report "c" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
