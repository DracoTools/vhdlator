-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- TO_STRING of a REAL with a FORMAT: one C fprintf conversion specification
-- (5.2.6).  The DIGITS form takes a number and this one a string, and it is
-- the argument's type that tells them apart.  Each expected string is what
-- nvc 1.22.1 and GHDL 7.0.0-dev both print.

entity t_vhdl_tostrfmt is
end entity;

architecture a of t_vhdl_tostrfmt is
   procedure check (got, want : string) is
   begin
      assert got = want report "got [" & got & "], want [" & want & "]" severity failure;
   end procedure;
begin
   p : process
      variable r : real := 3.14159265;
      variable n : real := -0.000123456;
   begin
      check(to_string(r, "%f"), "3.141593");
      check(to_string(r, "%.3f"), "3.142");
      check(to_string(r, "%10.2f"), "      3.14");
      check(to_string(r, "%-10.2f"), "3.14      ");
      check(to_string(r, "%08.3f"), "0003.142");
      check(to_string(r, "%e"), "3.141593e+00");
      check(to_string(n, "%.2e"), "-1.23e-04");
      check(to_string(n, "%.2E"), "-1.23E-04");
      check(to_string(r, "%g"), "3.14159");
      check(to_string(n, "%G"), "-0.000123456");
      -- The DIGITS form is still the DIGITS form
      check(to_string(r, 2), "3.14");
      report "*-* All Finished *-*";
      wait;
   end process;
end architecture;
