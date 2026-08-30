-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A signal parameter whose subtype leaves the length open is bound to the
-- caller's own signal, and that signal's bounds are the ones its attributes
-- answer with -- through as many calls as it is passed on (4.2.2.3).

library ieee;
use ieee.std_logic_1164.all;

package sigunc_pkg is
  subtype any_ev_t is std_logic_vector;
  subtype ev_t is any_ev_t(0 to 3);
  signal net : ev_t := "0000";

  impure function is_set(signal e : any_ev_t) return boolean;
  procedure raise_it(signal e : inout any_ev_t);
  procedure notify_net(signal e : inout any_ev_t);
end package;

package body sigunc_pkg is
  impure function is_set(signal e : any_ev_t) return boolean is
  begin
    return e(e'left) = '1' and e'length = 4;
  end;

  procedure raise_it(signal e : inout any_ev_t) is
  begin
    e(e'left) <= '1';
    e(e'right) <= '0';
  end;

  -- One copy calls another: the innermost still names the design's signal
  procedure notify_net(signal e : inout any_ev_t) is
  begin
    if not is_set(e) then
      raise_it(e);
    end if;
  end;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.sigunc_pkg.all;

entity t_vhdl_sigunc is
end entity;

architecture a of t_vhdl_sigunc is
  signal other : std_logic_vector(7 downto 4) := "0000";
begin
  process
  begin
    assert not is_set(net) report "net set too early" severity failure;
    notify_net(net);
    wait for 1 ns;
    assert net(0) = '1' report "net left not driven" severity failure;
    assert net(3) = '0' report "net right not driven" severity failure;
    assert is_set(net) report "net not set" severity failure;

    -- The same subprograms, a signal with the other direction and bounds
    notify_net(other);
    wait for 1 ns;
    assert other(7) = '1' report "other left not driven" severity failure;
    assert other(4) = '0' report "other right not driven" severity failure;
    assert is_set(other) report "other not set" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
