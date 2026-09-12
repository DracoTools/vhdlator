-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A verification library hands out a handle: a record an impure function
-- builds, carrying the widths everything is sized from beside an actor no
-- one can work out. Elaboration works the widths out element by element
-- (9.4.3), and a bound written in terms of such a handle -- including one
-- inside a component, sized from the handle its instance is given -- folds
-- to the number that instance gives it.

package handle_pkg is
  type handle_t is record
    p_actor : integer;
    p_data_length : natural;
    p_address_length : natural;
  end record;
  shared variable counter : integer := 0;
  impure function new_actor return integer;
  impure function new_handle (data_length : natural; address_length : natural) return handle_t;
  function data_length (h : handle_t) return natural;
end package;

package body handle_pkg is
  impure function new_actor return integer is
  begin
    counter := counter + 1;
    return counter;
  end function;
  impure function new_handle (data_length : natural; address_length : natural) return handle_t is
    variable a : integer;
  begin
    a := new_actor;
    return (p_actor => a, p_data_length => data_length, p_address_length => address_length);
  end function;
  function data_length (h : handle_t) return natural is
  begin
    return h.p_data_length;
  end function;
end package body;


library ieee;
use ieee.std_logic_1164.all;
use work.handle_pkg.all;

entity handle_vc is
  generic ( handle : handle_t );
  port ( d : out std_logic_vector(data_length(handle) - 1 downto 0) := (others => '0') );
end entity;

architecture rtl of handle_vc is
begin
  d <= (others => '1');
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.handle_pkg.all;
use std.env.all;

entity t_vhdl_handle is
end entity;

architecture tb of t_vhdl_handle is
  constant handle : handle_t := new_handle(data_length => 32, address_length => 16);
  constant other : handle_t := new_handle(data_length => 8, address_length => 16);
  signal v : bit_vector(data_length(handle) - 1 downto 0) := (others => '0');
  signal dv : std_logic_vector(31 downto 0);
  signal ov : std_logic_vector(7 downto 0);
begin
  -- One instance per set of widths, as VHDL elaboration says
  u : entity work.handle_vc generic map (handle => handle) port map (d => dv);
  w : entity work.handle_vc generic map (handle => other) port map (d => ov);
  process is
  begin
    assert v'length = 32 report "len " & integer'image(v'length) severity failure;
    wait for 1 ns;
    assert dv = x"ffffffff" report "dv " & to_string(dv) severity failure;
    assert ov = x"ff" report "ov " & to_string(ov) severity failure;
    -- The actor no one could work out is still worked out while running,
    -- once per handle: the value elaboration guessed at never replaced it
    assert handle.p_actor /= other.p_actor
      report "actors " & integer'image(handle.p_actor) & " and "
             & integer'image(other.p_actor) severity failure;
    assert handle.p_actor > 0 and other.p_actor > 0 report "actor ids" severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;
end architecture;
