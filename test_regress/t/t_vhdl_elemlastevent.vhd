-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- 'LAST_EVENT and 'LAST_VALUE of an element of a record signal are that
-- element's (16.2.4): when it last changed and what it was before. The
-- history was kept of the record as a whole, so a change of any element was
-- an event of every one. UVVM's gmii_bfm_tb checks that a GMII interface's
-- txen stayed high while txd carried a byte every cycle, with check_stable
-- on the element given as a signal parameter, and saw "switched from 1 to
-- 1 2 ns ago".
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package t_vhdl_elemlastevent_pkg is
  type t_inner is record
    a : std_logic;
    b : std_logic_vector(3 downto 0);
  end record;
  type t_if is record
    txd : std_logic_vector(7 downto 0);
    txen : std_logic;
    inner : t_inner;
  end record;
  procedure check_since(signal target : in std_logic; constant since : in time;
                        constant was : in std_logic; constant msg : in string);
end package;

package body t_vhdl_elemlastevent_pkg is
  procedure check_since(signal target : in std_logic; constant since : in time;
                        constant was : in std_logic; constant msg : in string) is
  begin
    assert target'last_event = since and target'last_value = was
      report msg & " last_event=" & time'image(target'last_event)
             & " last_value=" & std_logic'image(target'last_value) severity failure;
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.t_vhdl_elemlastevent_pkg.all;

entity t_vhdl_elemlastevent is
end entity;

architecture a of t_vhdl_elemlastevent is
  signal gif : t_if := (x"00", '0', ('0', x"0"));
begin
  process
  begin
    wait for 10 ns;
    gif.txen <= '1';
    gif.inner.a <= '1';
    for i in 1 to 5 loop
      gif.txd <= std_logic_vector(to_unsigned(i, 8));
      gif.txen <= '1';
      gif.inner.b <= std_logic_vector(to_unsigned(i, 4));
      wait for 8 ns;
    end loop;
    wait;
  end process;

  process
  begin
    wait for 49 ns;
    -- txen changed at 10 ns, txd last at 42 ns
    assert gif.txen'last_event = 39 ns and gif.txen'last_value = '0' severity failure;
    assert gif.txen'stable(30 ns) and not gif.txd'stable(30 ns) severity failure;
    assert gif.txd'last_event = 7 ns and gif.txd'last_value = x"04" severity failure;
    assert gif.inner.a'last_event = 39 ns and gif.inner.b'last_event = 7 ns severity failure;
    check_since(gif.txen, 39 ns, '0', "txen through a parameter");
    check_since(gif.inner.a, 39 ns, '0', "inner.a through a parameter");
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
