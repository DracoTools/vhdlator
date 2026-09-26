-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A function giving text that returns "" on one path: VUnit's
-- get_running_test_case (vunit/vhdl/run/src/runner_pkg.vhd), which is ""
-- until a test case runs. A null return is skipped as a guard when the
-- result's length is worked out from what the paths return, so the
-- result took the other path's length, and "" came back eight long.
entity t_vhdl_strnullret is
end entity;

architecture a of t_vhdl_strnullret is
  function len(s : string) return natural is
  begin
    return s'length;
  end function;

  function name(k : natural) return string is
  begin
    if k = 0 then
      return "";
    end if;
    return "Should a";
  end function;
begin
  process
    variable k : natural := 0;
  begin
    assert name(k) = "" and len(name(k)) = 0
      report "[" & name(k) & "]" severity failure;
    k := 1;
    assert name(k) = "Should a" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
