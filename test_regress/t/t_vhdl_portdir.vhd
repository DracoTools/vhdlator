-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An unconstrained port takes its bounds from the actual, which two
-- synthesized generics carry so that they fold per instance. Which way round
-- the range runs is equally the instance's to say, and nothing carried it --
-- so the helper that fills a value carrying its own bounds, shared by every
-- instance, had to guess. It guessed "downto", which is right for the great
-- majority of vectors and wrong for an ascending one, where the length came
-- out negative.
--
-- A third generic now carries the direction, bound to the actual's
-- 'ASCENDING. It folds per instance like the other two, so it costs nothing
-- to run. Where nothing binds it -- a shape this does not reach -- it keeps
-- the answer it had before, which is what its default says.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_portdir_probe is
    port (v : in std_logic_vector);
end entity;

architecture a of t_vhdl_portdir_probe is
    -- A protected type's method is one thing that needs the value carried
    type holder is protected
        procedure put (x : std_logic_vector);
        impure function len return integer;
        impure function bit0 return std_logic;
    end protected;
    type holder is protected body
        variable n : integer := -1;
        variable b : std_logic := 'U';
        procedure put (x : std_logic_vector) is
        begin
            n := x'length;
            b := x (x'left);
        end procedure;
        impure function len return integer is
        begin
            return n;
        end function;
        impure function bit0 return std_logic is
        begin
            return b;
        end function;
    end protected body;
    shared variable h : holder;
begin
    main: process is
    begin
        h.put (v);
        assert h.len = 5
            report "length " & integer'image (h.len) severity failure;
        -- The leftmost element, which says the walk ran the right way too
        assert h.bit0 = v (v'left)
            report "leftmost element" severity failure;
        wait;
    end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_portdir is
end entity;

architecture a of t_vhdl_portdir is
    signal up   : std_logic_vector (0 to 4) := "10110";
    signal down : std_logic_vector (4 downto 0) := "10110";
begin
    -- The same entity, once each way round
    u_up   : entity work.t_vhdl_portdir_probe port map (v => up);
    u_down : entity work.t_vhdl_portdir_probe port map (v => down);

    main: process is
    begin
        wait for 1 ns;
        assert up'ascending report "up ascending" severity failure;
        assert not down'ascending report "down descending" severity failure;
        report "*-* All Finished *-*";
        std.env.stop;
        wait;
    end process;
end architecture;
