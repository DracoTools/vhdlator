-- DESCRIPTION: Verilator: VHDL array compared with a value written with a metavalue
--
-- This file is part of Verilator.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
--
-- A two-state runtime holds no 'U', 'X', 'W' or '-', so a comparison with
-- one never holds -- for an array as for a single value. VUnit's
-- resolve_tstrb asks "tstrb = (tstrb'range => 'U')" to tell an unconnected
-- port; with 'U' written as zero it took every all-zero tstrb for one.

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_cmpaggmeta is
end entity;
architecture a of t_vhdl_cmpaggmeta is
  function resolve(keep, strb : std_logic_vector) return std_logic_vector is
  begin
    if strb = (strb'range => 'U') then
      return keep;
    else
      return strb;
    end if;
  end;
  signal strb : std_logic_vector(3 downto 0) := "0000";
  signal keep : std_logic_vector(3 downto 0) := "1111";
begin
  process
  begin
    wait for 1 ns;
    report "resolved " & to_string(resolve(keep, strb)) & " str " & boolean'image(strb = "00U0")
      & " ne " & boolean'image(strb /= (strb'range => 'X')) & " plain " & boolean'image(strb = "0000");
    assert resolve(keep, strb) = "0000" report "resolved" severity failure;
    assert not (strb = "00U0") report "string" severity failure;
    assert strb /= (strb'range => 'X') report "aggregate" severity failure;
    assert strb = "0000" report "plain" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
