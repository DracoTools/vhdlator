-- DESCRIPTION: Verilator: VHDL array type indexed by a subtype constrained by a range attribute
--
-- "type counter_array is array (positive range out_arc'range) of natural"
-- with out_arc an open port: the range is the port's, which elaboration
-- settles per instance, and the type mark only says what it is a range of
-- (5.3.2.1). It was refused as an unconstrained array; the plain
-- "out_arc'range" was not (VESTS ashenden ch_19_fork-b).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_typerangeattr_sub is
  port (o : out bit_vector);
end entity;

architecture a of t_vhdl_typerangeattr_sub is
begin
  process
    type counter_array is array (positive range o'range) of natural;
    variable c : counter_array := (others => 0);
    constant v : bit_vector(3 downto 1) := "101";
    type small_array is array (natural range v'range) of integer;
    variable s : small_array := (others => 7);
  begin
    c(c'left) := 3;
    c(c'right) := 4;
    assert c'length = o'length severity failure;
    assert c'left = o'left and c'right = o'right severity failure;
    assert c(o'left) = 3 and c(o'right) = 4 severity failure;
    assert s'length = 3 and s'left = 3 and s(1) = 7 severity failure;
    report "typerangeattr " & integer'image(c'left) & " to " & integer'image(c'right);
    wait;
  end process;
end architecture;

entity t_vhdl_typerangeattr is
end entity;

architecture a of t_vhdl_typerangeattr is
  signal s1 : bit_vector(2 to 5);
  signal s2 : bit_vector(1 to 3);
begin
  u1 : entity work.t_vhdl_typerangeattr_sub port map (o => s1);
  u2 : entity work.t_vhdl_typerangeattr_sub port map (o => s2);
end architecture;
