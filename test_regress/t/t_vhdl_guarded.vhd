-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A guarded signal has a driver that can be turned off: by a null waveform,
-- or by the guard of the block a guarded assignment stands in going false
-- (10.5.2.2). With every driver of it turned off, the signal takes what its
-- resolution function makes of no drivers at all -- a null array (14.7.3.2),
-- which is a value only the function itself can give.

use std.env.all;

entity t_vhdl_guarded is
end entity;

architecture tb of t_vhdl_guarded is
  -- '1' wins over '0'; no drivers at all leave it at 'H'
  function res (inputs : bit_vector) return bit is
  begin
    for i in inputs'range loop
      if inputs(i) = '1' then return '1'; end if;
    end loop;
    return '0';
  end function;

  subtype rbit is res bit;
  signal g   : boolean := false;
  signal byg : rbit bus;              -- turned off by its guard
  signal byn : rbit bus;              -- turned off by a null waveform
  signal dly : rbit bus;              -- the same, through a delay
  signal byname : rbit bus;           -- turned off by a GUARD of its own
  signal pair : rbit bus;             -- one driver of two turned off
  signal guard : boolean := false;    -- what "guarded" reads where no block says
begin

  guard <= true after 10 ns, false after 20 ns;

  g <= true after 10 ns, false after 20 ns;

  b1: block (g)
  begin
    byg <= guarded '1';
  end block;

  b2: block (g)
  begin
    dly <= guarded '1' after 1 ns;
  end block;

  -- The equivalent process, written out: the same signal, driven the same way
  byn_driver: process is
  begin
    if g then
      byn <= '1';
    else
      byn <= null;
    end if;
    wait on g;
  end process;

  -- A guarded assignment reads the GUARD visible where it is written, and a
  -- block with no guard expression of its own leaves that to the design
  own: block
  begin
    byname <= guarded '1';
  end block;

  -- Two drivers, of which only one is ever turned off: the signal is then
  -- resolved over the one that is left, not over both
  two: block (g)
  begin
    pair <= guarded '1';
  end block;
  other: process is
  begin
    pair <= '0';
    wait;
  end process;

  main: process is
  begin
    -- Nothing drives any of them yet, so each is what no drivers give
    wait for 1 ns;
    assert byg = '0' report "byg started driven" severity failure;
    assert byn = '0' report "byn started driven" severity failure;

    -- The guard opens
    wait for 10 ns;                   -- 11 ns
    assert byg = '1' report "byg did not take its driver" severity failure;
    assert byn = '1' report "byn did not take its driver" severity failure;
    assert dly = '1' report "dly did not take its driver" severity failure;

    assert byname = '1' report "a GUARD of the design's own was not read"
      severity failure;
    -- Two drivers, one driving '1' and one '0': '1' wins
    assert pair = '1' report "the pair did not resolve to '1'" severity failure;

    -- ... and closes again, which turns every driver off
    wait for 10 ns;                   -- 21 ns
    assert byg = '0' report "byg stayed driven after its guard closed"
      severity failure;
    assert byn = '0' report "byn stayed driven after a null waveform"
      severity failure;
    assert dly = '0' report "dly stayed driven after its guard closed"
      severity failure;
    assert byname = '0' report "byname stayed driven after its GUARD closed"
      severity failure;
    -- One driver of the two is gone; the other still drives '0'
    assert pair = '0' report "the pair did not resolve over the driver left"
      severity failure;
    stop(0);
  end process;

end architecture;
