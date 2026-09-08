-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A register per bit, written concurrently rather than in a process, inside a
-- generate. This crashed the compiler outright.
--
-- What keeps a signal's history -- the shadow of its previous value that
-- rising_edge reads -- belongs to the module, wherever the expression that
-- first asked for it happens to be. A generate body is collected rather than
-- added to the module and is then unrolled once per iteration, so a variable
-- left inside one is cloned away from the block that maintains it, which is
-- emitted for the module once. The block was then left pointing at a variable
-- that no longer existed.
--
-- Found in cocotb's own array_module test design, and by no VHDL corpus: the
-- shape is ordinary in real RTL and rare in a language test suite.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_genclk is
end entity;

architecture a of t_vhdl_genclk is
    signal clk  : std_logic := '0';
    signal d    : std_logic_vector (3 downto 0) := "0000";
    signal q    : std_logic_vector (3 downto 0);
    signal up   : std_logic_vector (0 to 2) := "000";
    signal upq  : std_logic_vector (0 to 2);
    signal done : boolean := false;
begin

    -- Descending, one signal declared per iteration
    g : for i in 3 downto 0 generate
        signal s : std_logic;
    begin
        s <= d (i) when rising_edge (clk);
        q (i) <= s;
    end generate;

    -- Ascending, and asking the same signal's history a second time from a
    -- second generate, which is what shares one shadow between them
    h : for i in 0 to 2 generate
        signal s : std_logic;
    begin
        s <= up (i) when rising_edge (clk);
        upq (i) <= s;
    end generate;

    clocking: process is
    begin
        while not done loop
            wait for 5 ns;
            clk <= not clk;
        end loop;
        wait;
    end process;

    main: process is
    begin
        d <= "1010";
        up <= "110";
        wait for 12 ns;
        assert q = "1010" report "descending: got " & to_string (q) severity failure;
        assert upq = "110" report "ascending: got " & to_string (upq) severity failure;

        d <= "0110";
        up <= "011";
        wait for 10 ns;
        assert q = "0110" report "descending again: got " & to_string (q) severity failure;
        assert upq = "011" report "ascending again: got " & to_string (upq) severity failure;

        done <= true;
        report "*-* All Finished *-*";
        std.env.stop;
        wait;
    end process;

end architecture;
