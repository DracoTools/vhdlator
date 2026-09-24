-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- TO_STRING of a TIME in a unit is the number of that unit the value is, as
-- an integer or real literal (5.2.6): "1.5 ns" for 1500 ps, not "1 ns".  A
-- unit that is a power of ten of the femtosecond gives an exact decimal.  Each
-- expected string is what nvc 1.22.1 and GHDL 7.0.0-dev both print, and the
-- unit may be one only running knows.

entity t_vhdl_timestr is
end entity;

architecture a of t_vhdl_timestr is
   procedure check (got, want : string) is
   begin
      assert got = want report "got [" & got & "], want [" & want & "]" severity failure;
   end procedure;
begin
   p : process
      variable u : time := ns;
   begin
      check(to_string(50 ns, unit => us), "0.05 us");
      check(to_string(1310408750 ps, unit => ns), "1310408.75 ns");
      check(to_string(1500 ps, unit => ns), "1.5 ns");
      check(to_string(-1500 ps, unit => ns), "-1.5 ns");
      check(to_string(2 ns, unit => ns), "2 ns");
      check(to_string(3 us, unit => ns), "3000 ns");
      check(to_string(250 ms, unit => sec), "0.25 sec");
      -- The unit is a value like any other
      check(to_string(2500 ps, u), "2.5 ns");
      u := us;
      check(to_string(2500 ns, u), "2.5 us");
      report "*-* All Finished *-*";
      wait;
   end process;
end architecture;
