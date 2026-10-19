-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A declaration naming, in its own value, an enumeration literal of its name visible from outside (12.3; GHDL gna bug20767, as nvc, Questa and GHDL's relaxed rules take it)

package t_vhdl_ownhomograph_types is
  type state is (state1, state2, state3);
end package;

use work.t_vhdl_ownhomograph_types.all;
package t_vhdl_ownhomograph_pkg is
  constant state1 : state := state1;
end package;

entity t_vhdl_ownhomograph is
end entity;

architecture a of t_vhdl_ownhomograph is
begin
  process is
  begin
    assert work.t_vhdl_ownhomograph_types.state'pos(work.t_vhdl_ownhomograph_pkg.state1) = 0
      severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
