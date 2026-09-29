-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A signal a package declares, handed to a procedure, which hands a slice of
-- its formal on to another that drives it: VUnit's test_runner_setup does
-- "notify(runner(runner_phase_idx to ...))", and notify activates the event
-- and waits for it. The drive goes to the driver the calling process owns
-- (4.2.2.3), or the event never becomes active and the process waits for
-- ever. VUnit's event encoding rests at "ZZ".

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_pkgsigslice_pkg is
  subtype any_event_t is std_logic_vector;
  constant p_active_event : std_logic := '1';
  constant p_inactive_event : std_logic_vector := "ZZ";
  impure function is_active(signal event : any_event_t) return boolean;
  procedure notify(signal event : inout any_event_t; n_delta_cycles : natural := 1);
  procedure setup(signal sync : inout std_logic_vector);
  signal runner : std_logic_vector(0 to 5) := "01ZZ10";
  signal net : std_logic_vector(0 to 3) := "ZZ01";
end package;
package body t_vhdl_pkgsigslice_pkg is
  impure function is_active(signal event : any_event_t) return boolean is
  begin
    return event(event'left to event'left + 1) /= p_inactive_event;
  end;
  procedure activate(signal event : inout any_event_t) is
  begin
    if event(event'left) = p_active_event then
      event(event'left + 1) <= p_active_event;
    else
      event(event'left) <= p_active_event;
    end if;
  end;
  procedure inactivate(signal event : inout any_event_t) is
  begin
    event(event'left to event'left + 1) <= p_inactive_event;
  end;
  procedure notify(signal event : inout any_event_t; n_delta_cycles : natural := 1) is
  begin
    activate(event);
    wait until is_active(event);
    inactivate(event);
    for iter in 1 to n_delta_cycles loop
      wait for 0 ns;
    end loop;
  end;
  procedure setup(signal sync : inout std_logic_vector) is
  begin
    notify(sync(2 to 3));
  end;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_pkgsigslice_pkg.all;
entity t_vhdl_pkgsigslice is
end entity;
architecture a of t_vhdl_pkgsigslice is
  signal seen : natural := 0;
begin
  main : process
  begin
    setup(runner);
    wait for 5 ns;
    setup(runner);
    wait for 1 ns;
    assert seen = 2 report "activations seen: " & integer'image(seen) severity failure;
    assert not is_active(runner(2 to 3)) report "event left active" severity failure;
    assert runner(0 to 1) = "01" and runner(4 to 5) = "10" report "runner left at " & to_string(runner)
      severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
  watch : process
  begin
    wait until is_active(runner(2 to 3));
    seen <= seen + 1;
  end process;
end architecture;
