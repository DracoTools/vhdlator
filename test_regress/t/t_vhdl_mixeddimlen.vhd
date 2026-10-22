-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Attributes of each dimension of a two-dimensional parameter whose actual
-- has one dimension the text fixes and one a generic sizes:
-- "vec'length(1)" of "SlVectorArray(17 downto 0, W_G - 1 downto 0)". Shaped
-- after surf's muxSlVectorArray.

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_mixeddimlen_pkg is
  type SlVectorArray is array (natural range <>, natural range <>) of std_logic;
  function muxSl (vec : SlVectorArray; addr : natural; allowOutOfRange : boolean := false)
    return std_logic_vector;
  function shape (vec : SlVectorArray) return integer_vector;
end package;

package body t_vhdl_mixeddimlen_pkg is
  function muxSl (vec : SlVectorArray; addr : natural; allowOutOfRange : boolean := false)
    return std_logic_vector is
    variable retVar : std_logic_vector(vec'range(2));
  begin
    if (addr < vec'length(1)) or (allowOutOfRange = false) then
      for i in vec'range(2) loop
        retVar(i) := vec(addr, i);
      end loop;
    else
      retVar := (others => '0');
    end if;
    return retVar;
  end function;
  function shape (vec : SlVectorArray) return integer_vector is
  begin
    return (vec'length(1), vec'left(1), vec'right(1), vec'high(1), vec'low(1),
            vec'length(2), vec'left(2), vec'right(2));
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_mixeddimlen_pkg.all;

entity t_vhdl_mixeddimlen is
  generic (W_G : positive := 4);
end entity;

architecture rtl of t_vhdl_mixeddimlen is
  signal a : SlVectorArray(17 downto 0, W_G - 1 downto 0) := (others => (others => '0'));
  signal b : SlVectorArray(2 to 4, W_G - 1 downto 0) := (others => (others => '0'));
  signal sel : natural := 1;
  signal o : std_logic_vector(W_G - 1 downto 0);
begin
  o <= muxSl(a, sel);
  process
    variable sa, sb : integer_vector(0 to 7);
  begin
    a(1, 2) <= '1';
    a(1, 0) <= '1';
    wait for 1 ns;
    sa := shape(a);
    sb := shape(b);
    report to_string(o);
    assert o = "0101" report "mux" severity failure;
    assert sa = (18, 17, 0, 17, 0, 4, 3, 0) report "shape a" severity failure;
    assert sb = (3, 2, 4, 4, 2, 4, 3, 0) report "shape b" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
