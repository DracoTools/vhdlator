-- DESCRIPTION: Verilator: VHDL open generic given bounds by a generate
-- parameter
--
-- "vec : bit_vector" is left unconstrained, so it takes its bounds from its
-- actual. In a for generate the actual is "(0 to ix => '1')", or a constant
-- declared "(0 to ix)" there, whose bounds name the generate parameter:
-- every iteration's instance has its own (GHDL gna issue626).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_genopenix_sub is
    generic (v : natural; asc : boolean; vec : bit_vector);
end entity;

architecture a of t_vhdl_genopenix_sub is
begin
    process is
    begin
        assert vec'length = v + 1 report "length" severity failure;
        assert vec'ascending = asc report "ascending" severity failure;
        if asc then
            assert vec'left = 0 and vec'right = v report "to" severity failure;
        else
            assert vec'left = v and vec'right = 0 report "downto" severity failure;
        end if;
        wait;
    end process;
end architecture;

entity t_vhdl_genopenix is
end entity;

architecture test of t_vhdl_genopenix is
begin
    g : for ix in 0 to 4 generate
        constant c : bit_vector(0 to ix) := (others => '1');
    begin
        up : entity work.t_vhdl_genopenix_sub
            generic map (v => ix, asc => true, vec => (0 to ix => '1'));
        named : entity work.t_vhdl_genopenix_sub
            generic map (v => ix, asc => true, vec => c);
    end generate;

    process is
    begin
        wait for 1 ns;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
