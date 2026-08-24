-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Loops whose bounds are not static, and exit and next naming a loop that is
-- not the innermost (10.10, 10.11, 10.12).

use std.env.all;

entity t_vhdl_loop is
end entity;

architecture tb of t_vhdl_loop is

  -- A bound that is a parameter, so it is not known when this is analyzed
  function total (n : integer) return integer is
    variable sum : integer := 0;
  begin
    for i in 1 to n loop
      sum := sum + i;
    end loop;
    return sum;
  end function;

begin

  process
    variable a, b, count, n : integer;
  begin
    assert total(4) = 10 report "total(4)" severity failure;
    assert total(1) = 1 report "total(1)" severity failure;
    -- A null range runs the body no times at all
    assert total(0) = 0 report "total(0)" severity failure;

    -- The range is evaluated once, when the loop is entered: assigning to
    -- what a bound names inside the body does not change the trip count
    n := 6;
    count := 0;
    for i in 1 to n loop
      n := 3;
      count := count + 1;
    end loop;
    assert count = 6 report "range evaluated again" severity failure;

    -- Counting down
    count := 0;
    n := 4;
    for i in n downto 1 loop
      count := count + i;
    end loop;
    assert count = 10 report "downto" severity failure;

    -- "exit" naming an outer loop leaves both
    a := 0;
    outer : loop
      for i in 1 to 10 loop
        a := a + 1;
        exit outer when a = 25;
      end loop;
    end loop;
    assert a = 25 report "labelled exit" severity failure;

    -- "next" naming an outer loop ends that loop's pass, not the inner one
    a := 0;
    b := 0;
    rows : for i in 1 to 3 loop
      a := a + 1;
      for j in 1 to 3 loop
        next rows when j = 2;
        b := b + 1;
      end loop;
    end loop;
    assert a = 3 report "labelled next outer count" severity failure;
    assert b = 3 report "labelled next inner count" severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
