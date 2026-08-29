-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_concproc is
end entity;

architecture a of t_vhdl_concproc is

  -- One signal parameter it reads, one it drives, and a constant parameter
  -- that is nonetheless given a signal
  procedure pick(signal x : in bit_vector(1 downto 0);
                 sel      : in integer;
                 signal y : out bit) is
  begin
    y <= x(sel);
  end procedure;

  -- and one that waits inside itself, so the process is in it until it ends
  procedure once(signal g : in bit; signal done : out bit) is
  begin
    wait for 3 ns;
    done <= g;
  end procedure;

  signal sel : integer range 0 to 1 := 0;
  signal x   : bit_vector(1 downto 0) := "00";
  signal y   : bit := '0';
  signal g   : bit := '1';
  signal done : bit := '0';

begin

  -- 11.4: a process that calls it and then waits on every signal named in
  -- the actual of a parameter of mode in or inout
  pick(x, sel, y);
  once(g, done);

  main : process is
  begin
    x <= "10";
    sel <= 1;
    wait for 1 ns;
    assert y = '1' report "y is " & bit'image(y) severity failure;

    sel <= 0;
    wait for 1 ns;
    assert y = '0' report "y again is " & bit'image(y) severity failure;

    -- the waiting one has not finished yet
    assert done = '0' report "done too early" severity failure;
    wait for 2 ns;
    assert done = '1' report "done never arrived" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;

end architecture;
