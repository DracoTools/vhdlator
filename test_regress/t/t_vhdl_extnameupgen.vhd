-- DESCRIPTION: Verilator: VHDL external names climbing out of an instance
-- of a generate (8.7)
--
-- From inside the instance g(N).uut, "^.^" climbs to the architecture that
-- instantiates it -- through the generate it sits in -- and "g(N).uut" comes
-- back down, with the index the instance's own generic gives, or to a
-- sibling by a literal index.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_extnameupgen_bot is
    generic ( N : integer );
end entity;

architecture test of t_vhdl_extnameupgen_bot is
    constant k : integer := N * 10;
    signal x, y : natural;
begin

    p1: process (y) is
    begin
        x <= y + 5;
    end process;

    p2: process is
    begin
        assert << constant ^.^.g(N).uut.k : integer >> = N * 10 severity failure;
        wait for 1 ns;
        assert << signal ^.^.g(N).uut.x : natural >> = 5 severity failure;
        -- A sibling, by a literal index
        assert << signal ^.^.g(1).uut.x : natural >> = 5 severity failure;
        assert << constant ^.^.g(3).uut.k : integer >> = 30 severity failure;
        << signal ^.^.g(N).uut.y : natural >> <= force N;
        wait for 1 ns;
        assert x = N + 5 severity failure;
        wait;
    end process;

end architecture;

entity t_vhdl_extnameupgen is
end entity;

architecture test of t_vhdl_extnameupgen is
begin

    g: for i in 1 to 3 generate
        uut: entity work.t_vhdl_extnameupgen_bot generic map ( N => i );
    end generate;

    process is
    begin
        wait for 3 ns;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
