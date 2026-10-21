-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Two things PoC's io_Debounce asks of an instance (6.5.6.3, 14.4.2):
-- * an out port whose default is a function of the generics returning a
--   value of its own length, associated element by element, starts there;
-- * a constant worked out from the instance's generics by a function that
--   makes a report on the way, as PoC's TimingToCycles does, still has the
--   value the function returns

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_portdyndflt_pkg is
  function resize(vec : std_logic_vector; length : natural) return std_logic_vector;
  function cycles(t : time; period : time) return natural;
end package;

package body t_vhdl_portdyndflt_pkg is
  function resize(vec : std_logic_vector; length : natural) return std_logic_vector is
    variable res : std_logic_vector(length - 1 downto 0) := (others => '0');
    constant v : std_logic_vector(vec'length - 1 downto 0) := vec;
  begin
    for i in 0 to length - 1 loop
      if i < vec'length then
        res(i) := v(i);
      end if;
    end loop;
    return res;
  end function;
  function cycles(t : time; period : time) return natural is
    variable n : natural := t / period;
  begin
    if n * period < t then
      n := n + 1;
    end if;
    report "cycles: " & integer'image(n);
    return n;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_portdyndflt_pkg.all;
entity t_vhdl_portdyndflt_deb is
  generic (BITS : positive := 1; INIT : std_logic_vector := x"00000000";
           BOUNCE : time := 50 ns; PERIOD : time := 10 ns);
  port (
    count : out natural;
    o : out std_logic_vector(BITS - 1 downto 0) := resize(INIT, BITS)
  );
end entity;
architecture a of t_vhdl_portdyndflt_deb is
  constant LOCK_COUNT : integer := cycles(BOUNCE, PERIOD) - 1;
  signal counter : natural range 0 to LOCK_COUNT := LOCK_COUNT;
begin
  count <= counter;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_portdyndflt is
end entity;
architecture a of t_vhdl_portdyndflt is
  signal one : std_logic;
  signal two : std_logic_vector(1 downto 0);
  signal c1, c2 : natural;
begin
  u1 : entity work.t_vhdl_portdyndflt_deb
    generic map (BITS => 1, INIT => "1")
    port map (count => c1, o(0) => one);
  u2 : entity work.t_vhdl_portdyndflt_deb
    generic map (BITS => 2, INIT => "10", BOUNCE => 25 ns)
    port map (count => c2, o(1) => two(0), o(0) => two(1));
  process
  begin
    wait for 1 ns;
    assert one = '1' report "one " & std_logic'image(one) severity failure;
    assert to_string(two) = "01" report "two " & to_string(two) severity failure;
    assert c1 = 4 report "c1 " & integer'image(c1) severity failure;
    assert c2 = 2 report "c2 " & integer'image(c2) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
