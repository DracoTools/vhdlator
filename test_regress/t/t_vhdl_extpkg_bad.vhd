-- DESCRIPTION: Verilator: VHDL external name of a package that does not
-- exist, or of an object a package does not declare
--
-- A package pathname names a package of the library and an object it
-- declares (8.7); either missing is an error.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_extpkg_bad_pkg is
  signal sig : natural := 1;
end package;

entity t_vhdl_extpkg_bad is
end entity;

architecture test of t_vhdl_extpkg_bad is
begin
  process is
  begin
    assert << signal @work.no_such_pkg.sig : natural >> = 1 severity failure;
    assert << signal @work.t_vhdl_extpkg_bad_pkg.nothing : natural >> = 1 severity failure;
    wait;
  end process;
end architecture;
