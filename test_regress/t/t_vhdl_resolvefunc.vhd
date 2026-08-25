-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A signal whose subtype names a resolution function of its own: the drivers
-- are gathered into an array and the function says what they add up to
-- (6.5.2). It is run however many drivers there are, since with one it may
-- still change the value it is given.

use std.env.all;

entity t_vhdl_resolvefunc is
end entity;

architecture tb of t_vhdl_resolvefunc is

  type level_t is (idle, low, high, error_l);
  type level_vec is array (natural range <>) of level_t;

  -- The strongest driver wins, and two that disagree are an error
  function strongest (v : level_vec) return level_t is
    variable acc : level_t := idle;
  begin
    for i in v'range loop
      if v(i) /= idle then
        if acc = idle then
          acc := v(i);
        elsif acc /= v(i) then
          acc := error_l;
        end if;
      end if;
    end loop;
    return acc;
  end function;

  subtype res_t is strongest level_t;

  -- The sum of the drivers, which is not a value any one of them has
  type int_vec is array (natural range <>) of integer;
  function total (v : int_vec) return integer is
    variable acc : integer := 0;
  begin
    for i in v'range loop
      acc := acc + v(i);
    end loop;
    return acc;
  end function;

  subtype sum_t is total integer;

  signal s : res_t := idle;
  signal n : sum_t := 0;

  -- One driver, and the function still decides what the signal holds
  signal one : sum_t := 0;

begin

  a : process is
  begin
    wait for 1 ns;
    s <= low;
    n <= 10;
    wait for 2 ns;
    s <= high;
    wait;
  end process;

  b : process is
  begin
    wait for 2 ns;
    n <= 7;
    wait for 1 ns;
    s <= low;
    wait;
  end process;

  c : process is
  begin
    one <= 4;
    wait;
  end process;

  check : process is
  begin
    -- Every driver starts where the declaration says, so the function sees
    -- them from the beginning
    assert s = idle report "before anything drives" severity failure;
    assert n = 0 report "sum before anything drives" severity failure;

    wait for 1500 ps;
    assert s = low report "one driver away from idle" severity failure;
    assert n = 10 report "one contributing driver" severity failure;

    wait for 1 ns;
    assert n = 17 report "two drivers added" severity failure;

    wait for 1 ns;
    -- Both drivers of s are away from idle and disagree
    assert s = error_l report "two drivers disagreeing" severity failure;

    -- A single driver, whose value the function still passes through
    assert one = 4 report "one driver through the function" severity failure;

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;

end architecture;
