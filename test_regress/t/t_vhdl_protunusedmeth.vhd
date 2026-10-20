-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A method of a protected type that nothing calls is never run, so one this
-- cannot lower -- a result of two dimensions whose bounds only running
-- decides -- is left out rather than refused, as a subprogram is (OSVVM
-- 2026.09's ScoreboardPType has a matrix NewID that no test calls).

package t_vhdl_protunusedmeth_pkg is
  type mat_t is array (integer range <>, integer range <>) of integer;
  type pt is protected
    impure function count return integer;
    impure function matrix(x, y : integer_vector) return mat_t;
  end protected;
end package;

package body t_vhdl_protunusedmeth_pkg is
  type pt is protected body
    variable n : integer := 0;
    impure function count return integer is
    begin
      n := n + 1;
      return n;
    end function;
    impure function local_matrix(x, y : integer_vector) return mat_t is
      variable r : mat_t(x(x'left) to x(x'right), y(y'left) to y(y'right));
    begin
      for i in r'range(1) loop
        for j in r'range(2) loop
          r(i, j) := i * j;
        end loop;
      end loop;
      return r;
    end function;
    impure function matrix(x, y : integer_vector) return mat_t is
    begin
      return local_matrix(x, y);
    end function;
  end protected body;
end package body;

use work.t_vhdl_protunusedmeth_pkg.all;
entity t_vhdl_protunusedmeth is
end entity;

architecture a of t_vhdl_protunusedmeth is
begin
  process
    variable v : pt;
  begin
    assert v.count = 1 report "count" severity failure;
    assert v.count = 2 report "count again" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
