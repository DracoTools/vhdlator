-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- The time given to 'DELAYED, 'STABLE and 'QUIET is static: each makes an
-- implicit signal when the design is elaborated, and the time is part of
-- what that signal is.

entity t_vhdl_sigattrtime_bad is
  generic (g : time := 5 ns);
end entity;

architecture a of t_vhdl_sigattrtime_bad is
  signal s : bit;
  procedure check (constant t : time) is
    constant c : time := t;
  begin
    -- Illegal
    assert s'stable(t);
    assert s'quiet(c);
  end procedure;
begin
  process
  begin
    -- Legal
    assert s'stable(10 ns) and s'stable(g);
    check(1 ns);
    wait;
  end process;
end architecture;
