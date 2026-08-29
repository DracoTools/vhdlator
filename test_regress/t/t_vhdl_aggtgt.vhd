-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_aggtgt is
end entity;

architecture a of t_vhdl_aggtgt is
  signal sel : integer := 6;
  signal g1, g2 : std_logic := 'U';
  signal wide : std_logic_vector(8 downto 0) := (others => '0');
begin

  -- A selected signal assignment whose target is an aggregate
  with sel select (g1, g2) <= std_logic_vector'("01") when 2,
                              "10" when others;

  main : process
    variable b1, b2 : std_logic;
  begin
    -- and a selected variable assignment, likewise. The type of the target
    -- is the type of the value, so the bare string literal in the second
    -- alternative is a vector and not a STRING.
    with sel select (b1, b2) := std_logic_vector'("01") when 2,
                                "10" when others;
    assert b1 = '1' report "b1 is " & to_string(b1) severity failure;
    assert b2 = '0' report "b2 is " & to_string(b2) severity failure;

    wait for 1 ns;
    assert g1 = '1' report "g1 is " & to_string(g1) severity failure;
    assert g2 = '0' report "g2 is " & to_string(g2) severity failure;

    sel <= 2;
    wait for 1 ns;
    assert g1 = '0' report "g1 again is " & to_string(g1) severity failure;
    assert g2 = '1' report "g2 again is " & to_string(g2) severity failure;

    -- An element of an aggregate may be of the aggregate's own type rather
    -- than of its element type, positionally as well as by choice (9.3.3.3)
    wide <= ('1', x"5a");
    wait for 1 ns;
    assert wide = "1" & x"5a" report "wide is " & to_string(wide) severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;

end architecture;
