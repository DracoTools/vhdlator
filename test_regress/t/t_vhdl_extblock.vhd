-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An external name reaches what a block declares, and a block is no module
-- of its own (8.7, 11.2): its signals, constants, generics and ports are
-- named through its label wherever the process naming them is written.

entity t_vhdl_extblock is
end entity;

architecture test of t_vhdl_extblock is
  type t_three is array (1 to 3) of integer;
  signal feed : bit := '1';
begin

  check : process is
  begin
    assert << constant b1.c1 : string(1 to 5) >> = "hello"
      report "constant" severity failure;
    assert << constant b2.g : integer >> = 42 report "generic" severity failure;
    assert << signal b2.p : bit >> = '1' report "port" severity failure;
    assert << signal b1.inner.deep : integer >> = 7 report "nested" severity failure;
    wait for 2 ns;
    assert << signal b1.s1 : t_three >> = (1, 2, 3) report "signal" severity failure;
    assert << signal .t_vhdl_extblock.b1.s1 : t_three >> = (1, 2, 3)
      report "absolute" severity failure;
    << signal b1.s1 : t_three >> <= (4, 5, 6);
    wait for 1 ns;
    assert << signal b1.s1 : t_three >> = (4, 5, 6) report "written" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
    wait;
  end process;

  b1 : block is
    signal s1 : t_three := (0, 0, 0);
    constant c1 : string := "hello";
  begin
    s1 <= (1, 2, 3) after 1 ns;
    inner : block is
      signal deep : integer := 7;
    begin
    end block;
  end block;

  b2 : block is
    generic (g : integer);
    generic map (g => 42);
    port (p : in bit);
    port map (feed);
  begin
  end block;

end architecture;
