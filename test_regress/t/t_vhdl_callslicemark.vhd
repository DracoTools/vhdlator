-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A call of a function with no parameters, sliced by a subtype's name (8.5; GHDL gna bug08 paren9)

entity t_vhdl_callslicemark is
end entity;

architecture a of t_vhdl_callslicemark is
begin
  process is
    function f return string is
    begin
      return "hello";
    end function;
    subtype b is natural range 1 to 2;
    subtype c is natural range 3 to 5;
  begin
    assert f(b) = "he" severity failure;
    assert f(c) = "llo" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
