-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A signal a package declares is one signal for the whole design, which is
-- what VUnit's message passing is built on
package pkgsig_pkg is
  signal net : integer := 0;
  signal flag : bit := '0';
  procedure bump (signal s : inout integer);
end package;

package body pkgsig_pkg is
  procedure bump (signal s : inout integer) is
  begin
    s <= s + 1;
  end procedure;
end package body;

use work.pkgsig_pkg.all;

-- A module of its own drives it ...
entity pkgsig_sub is
end entity;

architecture a of pkgsig_sub is
begin
  -- Reads the signal the top drives, and drives one the top reads: one
  -- object each, seen from two modules
  p : process is
  begin
    wait until net = 1;
    assert net = 1 report "sub sees " & integer'image(net) severity failure;
    flag <= '1';
    wait;
  end process;
end architecture;

use work.pkgsig_pkg.all;

entity t_vhdl_pkgsig is
end entity;

architecture a of t_vhdl_pkgsig is
begin

  u : entity work.pkgsig_sub;

  -- ... and the top reads the same one
  main : process is
  begin
    bump(net);
    wait for 1 ns;
    assert net = 1 report "net is " & integer'image(net) severity failure;
    -- the other module saw the same object, and its own drive arrives here
    wait until flag = '1';
    assert net = 1 report "net is " & integer'image(net) severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;

end architecture;
