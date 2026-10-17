-- DESCRIPTION: Verilator: VHDL external name whose subtype leaves the bounds open, used directly
--
-- "<< signal i_sub.bid : std_logic_vector >>" takes its bounds from the
-- object it denotes, as an alias of it does: an operand of to_hstring, a
-- comparison and 'LENGTH.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_extname_open_sub is
end entity;

architecture a of t_vhdl_extname_open_sub is
  signal bid : std_logic_vector(7 downto 0) := x"a5";
begin
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_extname_open is
end entity;

architecture a of t_vhdl_extname_open is
begin
  i_sub : entity work.t_vhdl_extname_open_sub;

  process
    alias bid is << signal i_sub.bid : std_logic_vector >>;
  begin
    wait for 1 ns;
    assert to_hstring(bid) = "A5" severity failure;
    assert to_hstring(<< signal i_sub.bid : std_logic_vector >>) = "A5" severity failure;
    assert << signal i_sub.bid : std_logic_vector >> = x"a5" severity failure;
    assert << signal i_sub.bid : std_logic_vector >>'length = 8 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
