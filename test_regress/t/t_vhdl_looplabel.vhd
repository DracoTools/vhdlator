-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A loop statement is a declarative region and its label names it, so an
-- inner loop that declares a parameter of the same name can still read the
-- outer one by its label (8.3, 10.10).

entity t_vhdl_looplabel is
end entity;

use std.env.all;

architecture tb of t_vhdl_looplabel is
  signal total : integer := 0;
begin

  process is
    variable sum : integer := 0;
    variable outers : integer := 0;
  begin
  l_outer:
    for i in 0 to 3 loop
      for i in 0 to 1 loop
        -- The inner "i" hides the outer one, which the label reaches past
        sum := sum + l_outer.i * 10 + i;
      end loop;
      outers := outers + l_outer.i;
    end loop;

    -- Each outer pass adds 2*i*10 + 0 + 1, so 20*(0+1+2+3) + 4*1 = 124
    assert sum = 124 report "sum is " & integer'image(sum) severity failure;
    assert outers = 6 report "outers is " & integer'image(outers) severity failure;

    total <= sum;
    wait for 1 ns;
    assert total = 124 report "total" severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
