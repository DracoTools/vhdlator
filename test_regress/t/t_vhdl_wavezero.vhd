-- DESCRIPTION: Verilator: VHDL zero-delay transaction preempted by a later one
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

-- "s <= 1; s <= 2 after 1 us": the inertial second assignment deletes the
-- first from the driver before the process suspends, so s goes from 0 to 2
-- at 2 us and never holds 1 (10.5.2.2, GHDL gna ticket11)

entity t_vhdl_wavezero is
end entity;

architecture a of t_vhdl_wavezero is
  signal s : integer := 0;
  signal t : integer := 0;
begin
  process is
  begin
    wait for 1 us;
    s <= 1;
    s <= 2 after 1 us;
    wait on s;
    assert s = 2 and now = 2 us severity failure;
    -- Transport keeps what is due before it
    t <= 1;
    t <= transport 2 after 1 us;
    wait on t;
    assert t = 1 and now = 2 us severity failure;
    wait on t;
    assert t = 2 and now = 3 us severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
