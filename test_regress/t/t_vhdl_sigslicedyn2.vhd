-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A slice of a record signal's element, at ends only running settles,
-- given a function's result held two levels deep: the words go into the
-- slice's places in order, each at the element's own width (10.5.2.2).
-- UVVM's HVVC-to-VVC bridge writes "bridge_to_hvvc.data_words(0 to
-- hvvc_to_bridge.num_data_words - 1) <= convert_slv_to_slv_array(...)",
-- where bridge_to_hvvc is a record port whose element has the length of the
-- signal the instance is given.

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_sigslicedyn2_pkg is
  type t_slv_array is array (natural range <>) of std_logic_vector;
  type t_b2h is record
    trigger    : boolean;
    data_words : t_slv_array;
  end record;
  function to_words(v : std_logic_vector; w : natural) return t_slv_array;
  procedure send(signal h2b : inout t_b2h; constant data_words : in t_slv_array);
end package;

package body t_vhdl_sigslicedyn2_pkg is
  function to_words(v : std_logic_vector; w : natural) return t_slv_array is
    constant n : natural := v'length / w;
    variable r : t_slv_array(0 to n - 1)(w - 1 downto 0);
    alias vv : std_logic_vector(v'length - 1 downto 0) is v;
  begin
    for i in 0 to n - 1 loop
      r(i) := vv(w * (n - i) - 1 downto w * (n - i - 1));
    end loop;
    return r;
  end function;

  procedure send(signal h2b : inout t_b2h; constant data_words : in t_slv_array) is
  begin
    h2b.data_words(0 to data_words'length - 1) <= data_words;
    h2b.trigger <= true;
    wait for 0 ns;
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_sigslicedyn2_pkg.all;

entity t_vhdl_sigslicedyn2_bridge is
  port (h2b : in t_b2h; b2h : out t_b2h);
end entity;

architecture a of t_vhdl_sigslicedyn2_bridge is
begin
  process
    variable v_data_slv : std_logic_vector(63 downto 0) := x"0102030405060708";
    variable n : natural := 3;
  begin
    b2h.trigger <= false;
    wait until h2b.trigger;
    b2h.data_words(0 to n - 1) <= to_words(v_data_slv(n * 8 - 1 downto 0), 8);
    b2h.trigger <= true;
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_sigslicedyn2_pkg.all;

entity t_vhdl_sigslicedyn2 is
end entity;

architecture a of t_vhdl_sigslicedyn2 is
  signal b2h : t_b2h(data_words(0 to 7)(7 downto 0));
  signal dn : t_slv_array(7 downto 0)(7 downto 0) := (others => x"ee");
  signal h2b : t_b2h(data_words(0 to 7)(7 downto 0));
  signal i2b : t_b2h(data_words(0 to 7)(7 downto 0));
begin
  i_bridge : entity work.t_vhdl_sigslicedyn2_bridge port map (h2b => h2b, b2h => i2b);

  process
    variable v_data_slv : std_logic_vector(63 downto 0) := x"0102030405060708";
    variable n : natural := 3;
  begin
    b2h.data_words <= (others => x"ee");
    wait for 1 ns;
    b2h.data_words(0 to n - 1) <= to_words(v_data_slv(n * 8 - 1 downto 0), 8);
    wait for 1 ns;
    assert b2h.data_words(0) = x"06" and b2h.data_words(1) = x"07"
           and b2h.data_words(2) = x"08" and b2h.data_words(3) = x"ee"
      report "words " & to_hstring(b2h.data_words(0)) & " " & to_hstring(b2h.data_words(1))
             & " " & to_hstring(b2h.data_words(2)) & " " & to_hstring(b2h.data_words(3))
      severity failure;
    -- A signal of its own, running the other way
    n := 2;
    dn(n - 1 downto 0) <= to_words(v_data_slv(n * 8 - 1 downto 0), 8);
    wait for 1 ns;
    assert dn(1) = x"07" and dn(0) = x"08" and dn(2) = x"ee"
      report "down " & to_hstring(dn(1)) & " " & to_hstring(dn(0)) & " " & to_hstring(dn(2))
      severity failure;
    -- Through the instance
    send(h2b, (x"aa", x"bb"));
    wait until i2b.trigger;
    wait for 0 ns;
    assert h2b.data_words(0) = x"aa" and h2b.data_words(1) = x"bb"
      report "sent " & to_hstring(h2b.data_words(0)) & " " & to_hstring(h2b.data_words(1))
      severity failure;
    assert i2b.data_words(0) = x"06" and i2b.data_words(2) = x"08"
      report "bridge " & to_hstring(i2b.data_words(0)) & " " & to_hstring(i2b.data_words(2))
      severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
