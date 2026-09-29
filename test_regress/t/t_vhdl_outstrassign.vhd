-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A STRING parameter of mode out, written whole with a value as long as
-- its actual: VUnit's codec does "result := code(index + 9 to index + 9 +
-- result'length - 1)". That is right, and the length check an assignment to
-- a STRING makes while running says nothing about it.

package t_vhdl_outstrassign_pkg is
  procedure decode(constant code : string; variable index : inout positive; variable result : out string);
  impure function get(code : string) return string;
end package;
package body t_vhdl_outstrassign_pkg is
  procedure decode(constant code : string; variable index : inout positive; variable result : out string) is
  begin
    result := code(index + 9 to index + 9 + result'length - 1);
    index := index + 9 + result'length;
  end;
  impure function get(code : string) return string is
    variable n : positive := 1;
    variable r : string(1 to 3);
  begin
    decode(code, n, r);
    return r;
  end;
end package body;
use work.t_vhdl_outstrassign_pkg.all;
entity t_vhdl_outstrassign is
end entity;
architecture a of t_vhdl_outstrassign is
begin
  process
    variable code : string(1 to 20) := "123456789abcdefghijk";
    variable s : string(1 to 5);
    variable n : positive := 1;
    variable len : natural := 4;
  begin
    decode(code, n, s);
    assert s = "abcde" and n = 15 report "s=" & s & " n=" & integer'image(n) severity failure;
    assert get(code(1 to 10 + len)) = "abc" report "get gave " & get(code(1 to 10 + len))
      severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
