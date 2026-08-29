-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package msgs is
  -- A record whose elements have no length of their own, and a function
  -- that builds one: only what the call says settles them (5.3.3)
  type t_if is record
    addr : std_logic_vector;
    data : std_logic_vector;
  end record;

  function make_if(constant aw : natural; constant dw : natural) return t_if;

  -- The message has no length any call site can measure
  function tag_of(constant n : integer) return string;

  -- Called with a vector whose length matters and a string whose does not
  procedure show(constant v : in std_logic_vector; constant msg : in string);
end package;

package body msgs is
  function make_if(constant aw : natural; constant dw : natural) return t_if is
    variable r : t_if(addr(aw - 1 downto 0), data(dw - 1 downto 0));
  begin
    r.addr := (others => '0');
    r.data := (others => '1');
    return r;
  end function;

  function tag_of(constant n : integer) return string is
  begin
    return "n=" & integer'image(n);
  end function;

  procedure show(constant v : in std_logic_vector; constant msg : in string) is
  begin
    -- The vector's length has to have come from the call
    assert v'length = 8 report "v'length is " & integer'image(v'length) severity failure;
    report msg & " v=" & to_string(v);
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.msgs.all;

entity t_vhdl_strarg is
end entity;

architecture a of t_vhdl_strarg is
begin
  main : process
    variable v : std_logic_vector(7 downto 0) := x"5a";
    variable r : t_if(addr(3 downto 0), data(7 downto 0));
  begin
    -- The string argument cannot be measured here; the vector still can
    show(v, tag_of(3));

    r := make_if(4, 8);
    assert r.addr'length = 4 report "addr len" severity failure;
    assert r.data'length = 8 report "data len" severity failure;
    assert r.data = x"ff" report "data value" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
