-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A subprogram declared inside another sees that one's objects (12.1),
-- and reaches them as extra parameters here.  Where the object carries
-- its own bounds, so does the parameter: its length is no more decided
-- inside than out.  Answer from nvc 1.22.1.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_dyncap is
end entity;

architecture rtl of t_vhdl_dyncap is

  signal k : integer := 12;

  function mkv(constant kk : integer) return std_logic_vector is
    variable z : std_logic_vector(kk - 1 downto 0) := (others => '0');
  begin
    z(2) := '1';
    return z;
  end function;

  procedure outer(constant expr : in std_logic_vector;
                  variable ref : inout std_logic_vector;
                  variable o : out integer) is
    -- Reaches out for both "expr" and "ref", each of a length only the
    -- call to "outer" decided
    procedure open_window(variable ok : out boolean) is
    begin
      ref := expr;
      ok := true;
    end procedure;
    variable ok : boolean;
  begin
    open_window(ok);
    o := to_integer(unsigned(ref)) + 1000 * ref'length;
  end procedure;

begin
  process
    variable r : integer;
    variable ref : std_logic_vector(11 downto 0);
  begin
    outer(mkv(k), ref, r);
    assert r = 12004 report "captured a carried object" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
