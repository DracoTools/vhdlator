-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- 'PATH_NAME and 'INSTANCE_NAME of a design unit's own name (16.2.7): an
-- entity or architecture names the instance being elaborated, whose path the
-- instances above it give, and a package's path starts at its library (NVC
-- elab9, attr12; nvc and GHDL agree, Questa writes a package's path otherwise)

package t_vhdl_pathname_unit_pkg is
  constant iname : string := t_vhdl_pathname_unit_pkg'instance_name;
end package;

entity t_vhdl_pathname_unit_sub is
end entity;

architecture test of t_vhdl_pathname_unit_sub is
begin
  process
  begin
    assert t_vhdl_pathname_unit_sub'path_name = ":t_vhdl_pathname_unit:sub_i:" severity failure;
    assert t_vhdl_pathname_unit_sub'instance_name
      = ":t_vhdl_pathname_unit(test):sub_i@t_vhdl_pathname_unit_sub(test):" severity failure;
    assert test'path_name = ":t_vhdl_pathname_unit:sub_i:" severity failure;
    wait;
  end process;
end architecture;

use work.t_vhdl_pathname_unit_pkg.all;

entity t_vhdl_pathname_unit is
end entity;

architecture test of t_vhdl_pathname_unit is
begin
  sub_i : entity work.t_vhdl_pathname_unit_sub;

  process
  begin
    wait for 1 ns;
    assert t_vhdl_pathname_unit'path_name = ":t_vhdl_pathname_unit:" severity failure;
    assert test'instance_name = ":t_vhdl_pathname_unit(test):" severity failure;
    assert iname = ":work:t_vhdl_pathname_unit_pkg:" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
