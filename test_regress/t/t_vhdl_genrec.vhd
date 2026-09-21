-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A generic of a record type whose value is only partly static still fixes
-- the bounds of what the architecture declares. A verification library hands
-- a component a handle an impure function built: one element of it is a
-- registration no elaboration can work out, and the rest are the function's
-- own arguments, which are literals. It is those the design is sized from.
--
-- The bound is written on a variable of a process, which is the shape that
-- matters: the same expression on a port already worked.

library ieee;
use ieee.std_logic_1164.all;

package genrecpkg is
  type counter_t is protected
    impure function next_id return natural;
  end protected;

  type handle_t is record
    p_id   : natural;      -- not static: the counter says
    p_idle : std_logic;    -- static: the call's own argument
    p_len  : positive;     -- static: likewise
  end record;

  impure function new_handle(idle_state : std_logic := '1';
                             len : positive := 8) return handle_t;
end package;

package body genrecpkg is
  type counter_t is protected body
    variable n : natural := 0;
    impure function next_id return natural is
    begin
      n := n + 1;
      return n;
    end function;
  end protected body;

  shared variable counter : counter_t;

  impure function new_handle(idle_state : std_logic := '1';
                             len : positive := 8) return handle_t is
  begin
    return (p_id => counter.next_id, p_idle => idle_state, p_len => len);
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.genrecpkg.all;

entity t_vhdl_genrec_sub is
  generic (h : handle_t);
  port (tx : out std_logic;
        idle_out : out std_logic);
end entity;

architecture rtl of t_vhdl_genrec_sub is
  -- A declaration of the architecture, and one written after another
  -- statement, so that a walk which stops at the first of a list is caught
  signal seen : std_logic_vector(h.p_len - 1 downto 0) := (others => '0');
begin
  idle : process
  begin
    idle_out <= h.p_idle;
    wait for 5 ns;
    wait;
  end process;

  drive : process
    variable data : std_logic_vector(h.p_len - 1 downto 0);
  begin
    wait for 10 ns;
    assert data'length = 12 report "bad width from the generic record" severity error;
    data := (others => '0');
    data(11) := '1';
    tx <= data(11);
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.genrecpkg.all;

entity t_vhdl_genrec is
end entity;

architecture rtl of t_vhdl_genrec is
  constant handle : handle_t := new_handle(len => 12);
  signal chan : std_logic;
  signal idle : std_logic;
begin
  inst : entity work.t_vhdl_genrec_sub generic map (h => handle) port map (tx => chan, idle_out => idle);

  check : process
  begin
    wait for 2 ns;
    assert idle = '1' report "bad idle state from the generic record" severity error;
    wait for 20 ns;
    assert chan = '1' report "bad driven value" severity error;
    report "*-* All Finished *-*";
    std.env.stop;
    wait;
  end process;
end architecture;
