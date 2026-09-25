-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A function returning an unconstrained array, where no return path folds
-- to a length, was given the length of its first parameter of the same
-- type -- right for "return a and b", wrong for an aggregate of arrays or a
-- join: "(x(5 downto 0), y(W - 1 downto 0))" came back as long as x, and
-- "bv(0 to n - 1) & '1'" failed its length check when it ran. Such a result
-- now carries its own bounds, and an aggregate of arrays is built as the
-- join it is (9.3.3.3). GHDL's issue2519 is the aggregate.
library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_retagg is
  generic (W : positive := 32; IDW : positive := 4);
end entity;

architecture a of t_vhdl_retagg is
  constant IDX : positive := 2;
  constant SID : positive := IDW + IDX;

  function to_slv(awid, awaddr : std_logic_vector) return std_logic_vector is
  begin
    return (awid(SID - 1 downto 0), awaddr(W - 1 downto 0));
  end function;

  function with_bit(bv : in bit_vector) return bit_vector is
    variable n : natural := bv'length;
  begin
    return bv(bv'left to bv'left + n - 1) & '1';
  end function;

  function joined(a, b : bit_vector) return bit_vector is
  begin
    return a & b;
  end function;

  function mixed(a : bit_vector) return bit_vector is
  begin
    return ('1', a, '0');
  end function;
begin
  process
    variable awid : std_logic_vector(7 downto 0) := x"ba";
    variable awaddr : std_logic_vector(31 downto 0) := x"f012_345e";
    constant c : std_logic_vector := to_slv(awid, awaddr);
    variable up : bit_vector(0 to 7) := "10110010";
  begin
    assert c'length = 38 severity failure;
    assert c = 38x"3a_f012_345e" severity failure;
    assert with_bit(up) = "101100101" severity failure;
    assert joined(up, "11") = "1011001011" severity failure;
    assert mixed("0110") = "101100" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
