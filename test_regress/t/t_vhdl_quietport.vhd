-- DESCRIPTION: Verilator: VHDL implicit S'QUIET(T) and S'STABLE(T) signals waking what reads them
--
-- S'QUIET(T) is a signal: FALSE in a cycle with a transaction on S, TRUE
-- again T after the last one (16.2.4). A block port mapped to it, and a
-- process sensitive to S'STABLE(T), are woken at that instant; the value
-- only read the time of the last event, so nothing woke them, and the port
-- started at its type's default where the implicit signal is TRUE from the
-- start (GHDL gna bug086).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_quietport is
end entity;

architecture behav of t_vhdl_quietport is
  signal s : natural;
  signal lastq, lasts : time;
  signal cnt_q, cnt_s : natural := 0;
begin
  s <= 1 after 20 ns;

  b : block
    port (q : boolean);
    port map (q => s'quiet(10 ns));
  begin
    process (q)
    begin
      report "q is " & boolean'image(q);
      lastq <= now;
      cnt_q <= cnt_q + 1;
    end process;
  end block;

  process (s'stable(5 ns))
  begin
    report "stable is " & boolean'image(s'stable(5 ns));
    lasts <= now;
    cnt_s <= cnt_s + 1;
  end process;

  process
  begin
    wait for 100 ns;
    assert lastq = 30 ns report "q last" severity failure;
    assert lasts = 25 ns report "stable last" severity failure;
    assert cnt_q = 3 and cnt_s = 3 report "counts" severity failure;
    report "quietport done";
    std.env.finish;
  end process;
end architecture;
