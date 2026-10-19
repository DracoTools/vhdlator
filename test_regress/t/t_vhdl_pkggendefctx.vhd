-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- The default of a generic of a generic package is written in that package,
-- and names what the package's own context clause made visible, which the
-- unit instantiating it need not use (6.5.6.2): UVVM's generic_sb_pkg,
-- whose SB_CONFIG_DEFAULT defaults to C_SB_CONFIG_DEFAULT of a package only
-- it uses. As a design unit of its own and inside another package.

package t_vhdl_pkggendefctx_support is
  type cfg_t is record
    depth : natural;
    name : string(1 to 3);
  end record;
  constant C_CFG_DEFAULT : cfg_t := (depth => 7, name => "abc");
end package;

use work.t_vhdl_pkggendefctx_support.all;
package t_vhdl_pkggendefctx_gen is
  generic (
    type t_element;
    constant CFG : cfg_t := C_CFG_DEFAULT;
    constant MAX : natural := 10
  );
  function depth return natural;
  function name return string;
end package;

package body t_vhdl_pkggendefctx_gen is
  function depth return natural is
  begin
    return CFG.depth + MAX;
  end function;
  function name return string is
  begin
    return CFG.name;
  end function;
end package body;

package t_vhdl_pkggendefctx_unit is new work.t_vhdl_pkggendefctx_gen
  generic map (t_element => bit);

package t_vhdl_pkggendefctx_outer is
  package inner is new work.t_vhdl_pkggendefctx_gen
    generic map (t_element => integer, MAX => 20);
end package;

entity t_vhdl_pkggendefctx is
end entity;

architecture a of t_vhdl_pkggendefctx is
begin
  process
  begin
    assert work.t_vhdl_pkggendefctx_unit.depth = 17 report "unit depth /= 17"
      severity failure;
    assert work.t_vhdl_pkggendefctx_unit.name = "abc" report "unit name /= abc"
      severity failure;
    assert work.t_vhdl_pkggendefctx_outer.inner.depth = 27 report "inner depth /= 27"
      severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
