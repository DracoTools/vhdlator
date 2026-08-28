-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity pass_thru is
    port ( i : in integer; o : out integer );
end entity;

architecture rtl of pass_thru is
begin
    o <= i;
end architecture;

configuration pass_cfg of pass_thru is
    for rtl
    end for;
end configuration;

entity add_ten is
    port ( i : in integer; o : out integer );
end entity;

architecture rtl of add_ten is
begin
    o <= i + 10;
end architecture;

configuration add_cfg of add_ten is
    for rtl
    end for;
end configuration;

entity t_vhdl_confblk is
end entity;

architecture test of t_vhdl_confblk is
    component step is
        port ( i : in integer; o : out integer );
    end component;
    signal src : integer := 1;
    signal blocked : integer := 0;
    signal genned : integer_vector(0 to 2) := (others => 0);
begin
    outer: block
    begin
        -- Configured here to add ten
        u_top : component step port map ( src, blocked );

        g : for k in 0 to 2 generate
            -- Configured inside the generate to pass through, which what is
            -- written for the block must not reach
            u_gen : component step port map ( src, genned(k) );
        end generate;
    end block;

    check : process is
    begin
        wait for 1 ns;
        assert blocked = 11 report "blocked = " & integer'image(blocked) severity failure;
        for k in 0 to 2 loop
            assert genned(k) = 1
                report "genned(" & integer'image(k) & ") = " & integer'image(genned(k))
                severity failure;
        end loop;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;

configuration t_vhdl_confblk_cfg of t_vhdl_confblk is
    for test
        for outer
            for all : step use configuration work.add_cfg;
            end for;
            for g(0 to 2)
                for u_gen : step use configuration work.pass_cfg;
                end for;
            end for;
        end for;
    end for;
end configuration;
