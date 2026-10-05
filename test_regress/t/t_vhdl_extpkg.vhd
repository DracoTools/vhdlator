-- DESCRIPTION: Verilator: VHDL external name of an object a package declares
--
-- "<< signal @work.pkg.sig_in_package : natural >>" names the signal the
-- package declares (8.7): read through it, and assigned through an alias
-- of it, it is the signal every other reference to the package reaches
-- (nvc regress ename16).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_extpkg_pkg is
  signal sig_in_package : natural := 17;
  constant c_in_package : natural := 42;
end package;

use work.t_vhdl_extpkg_pkg.all;

entity t_vhdl_extpkg is
end entity;

architecture test of t_vhdl_extpkg is
begin
  process is
    alias al is << signal @work.t_vhdl_extpkg_pkg.sig_in_package : natural >>;
  begin
    wait for 1 ns;
    assert al = 17 severity failure;
    assert << constant @work.t_vhdl_extpkg_pkg.c_in_package : natural >> = 42
      severity failure;
    al <= 5;
    wait for 1 ns;
    assert al = 5 severity failure;
    -- The package's own name for it sees the same signal
    assert sig_in_package = 5 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
