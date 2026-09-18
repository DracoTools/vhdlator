-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An association element written "open" leaves the formal to its own default
-- (6.5.7.1). Standing the default where the actual is written keeps the
-- positions of the arguments that follow, so an "open" in the middle of a
-- positional list is no different from one at the end.

use std.env.all;

entity t_vhdl_openarg is
end entity;

architecture tb of t_vhdl_openarg is

  function f (a, b : integer := 1) return integer is
  begin
    return 10 * a + b;
  end function;

  procedure p (delay : time := 0 ns; a, b : integer := 1;
               variable got : out integer) is
  begin
    got := 10 * a + b;
  end procedure;

  signal seen : integer := 0;

begin

  main : process is
    variable v : integer;
  begin
    -- A function, every way round
    assert f = 11 report "all open" severity failure;
    assert f(2) = 21 report "one given" severity failure;
    assert f(2, open) = 21 report "trailing open" severity failure;
    assert f(open, 2) = 12 report "leading open" severity failure;
    assert f(a => 2, b => open) = 21 report "named open" severity failure;
    assert f(open, b => 2) = 12 report "positional open then named" severity failure;

    -- A procedure, where the open is in the middle of the list
    p(0 ns, open, b => 2, got => v);
    assert v = 12 report "procedure open" severity failure;
    p(0 ns, 3, open, got => v);
    assert v = 31 report "procedure trailing open" severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
