-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- One character of a STRING given for a CHARACTER parameter the procedure
-- writes (4.2.2.1), where the procedure also takes a signal and so is copied
-- for the call. A STRING is held as text, whose characters have no place of
-- their own to pass, and the build stopped with an internal error.

entity t_vhdl_strcharinout is
end entity;

architecture a of t_vhdl_strcharinout is
  signal s : bit := '0';
  procedure mark(variable c : inout character) is
  begin
    c := 'x';
  end procedure;
  procedure mark2(signal b : in bit; variable c : inout character) is
  begin
    if b = '1' then
      c := 'y';
    end if;
  end procedure;
  procedure put(signal b : in bit; variable c : out character) is
  begin
    c := character'val(character'pos('a') + bit'pos(b));
  end procedure;
begin
  process
    variable v : string(1 to 5) := ".....";
  begin
    mark(v(2));
    s <= '1';
    wait for 1 ns;
    mark2(s, v(3));
    put(s, c => v(5));
    assert v = ".xy.b" report "v=" & v severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
