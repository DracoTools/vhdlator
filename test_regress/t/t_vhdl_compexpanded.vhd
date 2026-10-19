-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An instance of a component a package declares, named through the package with no maps (11.7.2; GHDL gna bug21052)

package t_vhdl_compexpanded_pkg is
  component t_vhdl_compexpanded_sub
  end component;
end package;

entity t_vhdl_compexpanded_sub is
end entity;

architecture a of t_vhdl_compexpanded_sub is
begin
  process is
  begin
    report "sub elaborated";
    wait;
  end process;
end architecture;

entity t_vhdl_compexpanded is
end entity;

architecture a of t_vhdl_compexpanded is
begin
  inst : work.t_vhdl_compexpanded_pkg.t_vhdl_compexpanded_sub;
  process is
  begin
    wait for 1 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
