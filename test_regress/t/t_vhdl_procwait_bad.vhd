-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A process with a sensitivity list resumes when one of those signals
-- changes and nowhere else, so nothing in it waits, itself or through a
-- procedure it calls; nor does a function, which computes its value at
-- once. What a sensitivity list or "wait on" names is a signal, by a static
-- name, and a timeout is a TIME. Outside a process or subprogram a variable
-- is shared, and of a protected type, and inside one it is not shared.

entity t_vhdl_procwait_bad is
end entity;

architecture a of t_vhdl_procwait_bad is
  signal s, t : bit;
  signal v : bit_vector(0 to 3);
  type counter is protected
    procedure incr;
  end protected;
  type counter is protected body
    variable n : integer := 0;
    procedure incr is
    begin
      n := n + 1;
    end procedure;
  end protected body;
  shared variable ok_shared : counter;
  shared variable bad_shared : integer;
  variable bad_var : integer;
  procedure pause is
  begin
    wait for 1 ns;
  end procedure;
  procedure no_pause is
  begin
    null;
  end procedure;
  function f return integer is
  begin
    pause;
    return 1;
  end function;
begin
  -- Legal
  ok1 : process (s)
  begin
    no_pause;
  end process;
  ok2 : process
  begin
    wait on s, v(1);
    pause;
    wait for 2 ns;
  end process;
  -- Illegal
  bad1 : process (s)
  begin
    wait for 1 ns;
  end process;
  bad2 : process (s)
  begin
    pause;
  end process;
  bad3 : process
    variable x : bit;
    variable i : integer := 0;
    shared variable sv : integer;
  begin
    wait on x;
    wait on v(i);
    wait for 5;
    wait until s = '1' for true;
  end process;
end architecture;
