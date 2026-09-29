-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- "d := f(n)(d'range)", where d is a formal given a variable only running
-- sizes and f gives back a value only running sizes: the slice takes d's own
-- range, which d carries, not the range of the index subtype of its
-- unconstrained type (5.3.2.2). VUnit's await_read_bus_reply does this, and
-- check_bus read back no bits.

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_dynrangeslice_pkg is
  type rec is record
    len : natural;
  end record;
  impure function get32(k : natural) return std_logic_vector;
  procedure inner2(signal s : inout std_logic; variable d : inout std_logic_vector);
  procedure inner1(signal s : inout std_logic; variable d : inout std_logic_vector);
  procedure outer(signal s : inout std_logic; constant h : rec);
  procedure outer_fixed(signal s : inout std_logic);
end package;
package body t_vhdl_dynrangeslice_pkg is
  impure function get32(k : natural) return std_logic_vector is
    variable r : std_logic_vector(k - 1 downto 0) := (others => '0');
  begin
    r(31 downto 0) := x"00112233";
    return r;
  end;
  procedure inner2(signal s : inout std_logic; variable d : inout std_logic_vector) is
    variable n : natural := 40;
  begin
    s <= '1';
    wait for 1 ns;
    d := get32(n)(d'range);
  end;
  procedure inner1(signal s : inout std_logic; variable d : inout std_logic_vector) is
  begin
    inner2(s, d);
  end;
  procedure outer(signal s : inout std_logic; constant h : rec) is
    variable data : std_logic_vector(h.len - 1 downto 0);
  begin
    inner1(s, data);
    assert data'length = 32 and data = x"00112233"
      report "outer got " & to_hstring(data) & " length " & integer'image(data'length)
      severity failure;
  end;
  procedure outer_fixed(signal s : inout std_logic) is
    variable data32 : std_logic_vector(31 downto 0);
  begin
    inner1(s, data32);
    assert data32 = x"00112233" report "fixed got " & to_hstring(data32) severity failure;
  end;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_dynrangeslice_pkg.all;
entity t_vhdl_dynrangeslice is
end entity;
architecture a of t_vhdl_dynrangeslice is
  signal s : std_logic := '0';
begin
  process
    variable n : natural := 32;
  begin
    outer_fixed(s);
    outer(s, (len => n));
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
