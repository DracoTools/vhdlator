-- DESCRIPTION: Verilator: VHDL protected method driving a signal parameter
--
-- A method of a protected type with an out signal parameter, called on a shared object: the formal is copied out, and the caller's signal is driven from it in the next delta where the method assigned it (NVC issue675).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_protsigout_pkg is
  type t is protected
    procedure set(v : std_logic_vector);
    procedure g(signal a : out std_logic_vector);
    procedure maybe(signal a : out std_logic_vector; doit : boolean);
    impure function count return natural;
  end protected;
end package;

package body t_vhdl_protsigout_pkg is
  type t is protected body
    variable held : std_logic_vector(7 downto 0) := x"00";
    variable n : natural := 0;
    procedure set(v : std_logic_vector) is
    begin
      held := v;
    end procedure;
    procedure g(signal a : out std_logic_vector) is
    begin
      a <= held;
      n := n + 1;
    end procedure;
    procedure maybe(signal a : out std_logic_vector; doit : boolean) is
    begin
      if doit then
        a <= not held;
      end if;
      n := n + 1;
    end procedure;
    impure function count return natural is
    begin
      return n;
    end function;
  end protected body;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_protsigout_pkg.all;
entity t_vhdl_protsigout is end entity;
architecture a of t_vhdl_protsigout is
  shared variable obj : t;
  signal c, d : std_logic_vector(7 downto 0) := x"11";
begin
  process
    procedure via(x : std_logic_vector) is
    begin
      obj.set(x);
      obj.g(c);
    end procedure;
  begin
    obj.set(x"5a");
    obj.g(c);
    -- A signal assignment takes effect in the next delta
    assert c = x"11" report "c changed at once" severity failure;
    wait for 0 ns;
    assert c = x"5a" report "c: " & to_hstring(c) severity failure;
    via(x"c3");
    wait for 0 ns;
    assert c = x"c3" report "via: " & to_hstring(c) severity failure;
    -- Not assigned: the signal keeps its value
    obj.maybe(d, false);
    wait for 0 ns;
    assert d = x"11" report "d: " & to_hstring(d) severity failure;
    obj.maybe(d, true);
    wait for 0 ns;
    assert d = x"3c" report "d2: " & to_hstring(d) severity failure;
    assert obj.count = 4 report "count" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
