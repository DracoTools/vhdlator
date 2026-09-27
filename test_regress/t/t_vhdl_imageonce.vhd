-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- The image of an enumeration value an impure function gives: the value was
-- tested against each literal in turn, and the call made once per test, so a
-- function that counts or reports as it goes did so twice.
entity t_vhdl_imageonce is
end entity;

architecture a of t_vhdl_imageonce is
  type t_state is (idle, busy, done);
begin
  process
    variable n : natural := 0;

    impure function bump return boolean is
    begin
      n := n + 1;
      return true;
    end function;

    impure function next_state return t_state is
    begin
      n := n + 1;
      return done;
    end function;
  begin
    report "img " & boolean'image(bump);
    assert n = 1 report "boolean'image called " & integer'image(n) severity failure;
    report "state " & t_state'image(next_state);
    assert n = 2 report "t_state'image called " & integer'image(n - 1) severity failure;
    assert t_state'image(next_state) = "done" severity failure;
    assert n = 3 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
