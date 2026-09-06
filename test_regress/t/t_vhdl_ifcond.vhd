-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A condition that is not a BOOLEAN is read through the condition operator
-- (9.2.9), so what decides the branch is whether the value is '1'.  A
-- folded STD_LOGIC is a position in its type -- '0' sits at position two
-- of U X 0 1 Z W L H - -- and reading that as a truth value takes the
-- branch and throws the else away, leaving a false test with nothing under
-- it.  "if G_ENABLE then ... else ... end if" over a STD_LOGIC generic is
-- ordinary RTL.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_ifcond is
  generic (g_off : std_logic := '0';
           g_on  : std_logic := '1');
end entity;

architecture a of t_vhdl_ifcond is
  constant c_off : std_logic := '0';
  constant c_on  : std_logic := '1';
  constant b_off : bit := '0';
  signal   s_off : std_logic := '0';
begin
  process
    variable hits : integer := 0;
    variable v : std_logic;
  begin
    -- A constant that is not '1' takes the else, and the else must be there
    if c_off then hits := hits + 100; else hits := hits + 1; end if;
    assert hits = 1 report "a constant '0' lost its else" severity failure;

    if c_on then hits := hits + 10; else hits := hits + 100; end if;
    assert hits = 11 report "a constant '1' took the wrong branch" severity failure;

    -- The same over a generic, which an instance settles
    hits := 0;
    if g_off then hits := hits + 100; else hits := hits + 1; end if;
    assert hits = 1 report "a generic '0' lost its else" severity failure;
    if g_on then hits := hits + 10; else hits := hits + 100; end if;
    assert hits = 11 report "a generic '1' took the wrong branch" severity failure;

    -- An elsif chain, where a false first branch must not swallow the rest
    hits := 0;
    if c_off then hits := 100; elsif c_on then hits := 2; else hits := 100; end if;
    assert hits = 2 report "elsif after a false condition" severity failure;

    -- BIT beside it, whose '0' is position zero and was right all along
    hits := 0;
    if b_off then hits := 100; else hits := 3; end if;
    assert hits = 3 report "a bit constant lost its else" severity failure;

    -- and one nothing settles before it runs
    wait for 1 ns;
    v := s_off;
    hits := 0;
    if v then hits := 100; else hits := 4; end if;
    assert hits = 4 report "a running value lost its else" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
