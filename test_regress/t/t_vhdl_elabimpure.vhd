-- DESCRIPTION: Verilator: VHDL constants an impure function gives at
-- elaboration (14.4.2)
--
-- An architecture's declarations elaborate in order, and an impure function
-- they call reads and writes the architecture's own shared variables as it
-- goes: each call gives the next value, every instance elaborates its own,
-- and the run starts with what elaboration left in them. VHDL-93, which
-- has shared variables of any type.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_elabimpure_sub is
    generic ( id : natural );
    port ( o : out integer; own : out integer );
end entity;

architecture test of t_vhdl_elabimpure_sub is
    shared variable mine : natural := 10;

    impure function next_mine return natural is
    begin
        mine := mine + 1;
        return mine;
    end function;

    constant first : natural := next_mine;
begin
    g1: if id = 1 generate
        o <= 1;
    end generate;
    g2: if id = 2 generate
        o <= 2;
    end generate;
    own <= first;
end architecture;

entity t_vhdl_elabimpure is
end entity;

architecture test of t_vhdl_elabimpure is
    shared variable counter : natural := 0;

    impure function get_id return natural is
    begin
        counter := counter + 1;
        return counter;
    end function;

    constant id1 : natural := get_id;
    constant id2 : natural := get_id;
    signal s1, s2, own1, own2 : integer;
begin

    u1: entity work.t_vhdl_elabimpure_sub generic map ( id1 ) port map ( s1, own1 );
    u2: entity work.t_vhdl_elabimpure_sub generic map ( id2 ) port map ( s2, own2 );

    process is
    begin
        wait for 1 ns;
        assert id1 = 1 and id2 = 2 severity failure;
        assert s1 = 1 and s2 = 2 severity failure;
        -- Each instance elaborates its own shared variable from the start
        assert own1 = 11 and own2 = 11 severity failure;
        -- The run starts where elaboration left the counter
        assert counter = 2 severity failure;
        assert get_id = 3 severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;

end architecture;
