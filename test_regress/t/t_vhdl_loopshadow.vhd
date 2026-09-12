-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A loop parameter may have the name of one of the subprogram's own
-- parameters, which it hides for the length of the loop (10.10, 12.3).

entity t_vhdl_loopshadow is
end entity;

architecture test of t_vhdl_loopshadow is

  procedure copy(signal b : in bit_vector; i : in integer; c : out bit_vector) is
  begin
    assert i = 9 report "parameter" severity failure;
    for i in b'range loop
      c(i) := b(i);
    end loop;
  end procedure;

  signal s : bit_vector(1 to 3) := ('1', '0', '1');

begin

  main : process is
    variable got : bit_vector(1 to 3);
  begin
    copy(s, 9, got);
    assert got = "101" report "copied" severity failure;
    copy(s, 9, got);
    assert got = "101" report "copied twice" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
    wait;
  end process;

end architecture;
