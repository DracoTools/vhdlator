-- DESCRIPTION: Verilator: VHDL process of a for generate waiting on the
-- element its index names
--
-- "wait on arcs(index)" and "process (bits(index))" in a for generate's
-- block: each copy of the process wakes on its own element and on no other
-- (10.2, 11.3). The signal that follows the element reads the generate's
-- index, so it is declared in the generate's block, beside the process --
-- VESTS ashenden ch_19's queueing models, "wait on fork_arc(index)".
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_genwaitidx is
end entity;

architecture test of t_vhdl_genwaitidx is
    type rec_t is record
        v : integer;
        t : time;
    end record;
    type rec_arr is array (1 to 3) of rec_t;
    type int_arr is array (1 to 3) of integer;
    signal arcs : rec_arr := (others => (v => 0, t => 0 ns));
    signal bits : bit_vector(1 to 3);
    signal seen, wakes, flips : int_arr := (others => 0);
begin
    gen : for index in 1 to 3 generate
        mon : process
        begin
            wait on arcs(index);
            seen(index) <= arcs(index).v;
            wakes(index) <= wakes(index) + 1;
        end process;

        flip : process (bits(index))
        begin
            flips(index) <= flips(index) + 1;
        end process;
    end generate;

    process is
    begin
        wait for 1 ns;
        arcs(2) <= (v => 7, t => 1 ns);
        bits(3) <= '1';
        wait for 1 ns;
        arcs(3).v <= 9;
        wait for 1 ns;
        assert seen = (0, 7, 9) report "seen" severity failure;
        assert wakes = (0, 1, 1) report "wakes" severity failure;
        -- A process with a sensitivity list runs once as the design starts
        assert flips = (1, 1, 2) report "flips" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
