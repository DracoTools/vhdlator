-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_indivfml is
end entity;

architecture a of t_vhdl_indivfml is

  -- A record whose element has no length of its own: the call is what says
  -- how long it is (5.3.3)
  type t_if is record
    d : std_logic_vector;
    v : std_logic;
  end record;

  signal sd : std_logic_vector(3 downto 0) := "0000";
  signal sv : std_logic := '0';
  signal seen : integer := -1;

  -- The innermost worker takes the whole record
  procedure put(signal iface : inout t_if; constant n : in integer) is
  begin
    iface.d <= std_logic_vector(to_unsigned(n, iface.d'length));
    iface.v <= '1';
    wait for 1 ns;
    iface.v <= '0';
  end procedure;

  -- and this one hands it on whole
  procedure drive(signal iface : inout t_if; constant n : in integer) is
  begin
    put(iface, n);
    wait for 1 ns;
  end procedure;

  -- The flattened overload names the elements of the record one at a time
  procedure drive(signal d : inout std_logic_vector;
                  signal v : inout std_logic;
                  constant n : in integer) is
  begin
    drive(iface.d => d, iface.v => v, n => n);
  end procedure;

begin

  watch : process(sv)
  begin
    if sv = '1' then seen <= to_integer(unsigned(sd)); end if;
  end process;

  main : process
  begin
    drive(sd, sv, 5);
    assert sd = "0101" report "sd is " & to_string(sd) severity failure;
    assert sv = '0' report "sv did not fall" severity failure;
    assert seen = 5 report "seen is " & integer'image(seen) severity failure;

    drive(sd, sv, 12);
    assert sd = "1100" report "sd is " & to_string(sd) severity failure;
    assert seen = 12 report "seen is " & integer'image(seen) severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;

end architecture;
