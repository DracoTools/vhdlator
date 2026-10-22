-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- std_logic_unsigned's "=" and "/=" of two vectors, and std_logic_arith's of
-- two UNSIGNED or two SIGNED, over the nine values. Each operand is made a
-- number first (CONV_UNSIGNED, CONV_SIGNED): 'L' and 'H' read as '0' and
-- '1', the shorter widened, and one with a metavalue becomes all 'X', so two
-- such operands compare equal. A generic picks a constant through one of
-- them at elaboration, as surf's Synchronizer does with "INIT_G = "0"".

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;

entity t_vhdl_syneqmeta is
  generic (INIT_G : std_logic_vector := "0000");
end entity;

architecture rtl of t_vhdl_syneqmeta is
  function pick (c : boolean) return integer is
  begin
    if c then
      return 7;
    end if;
    return 9;
  end function;
  constant PICK_C : integer := pick(INIT_G = "0");
  signal a, b : std_logic_vector(3 downto 0) := "0000";
  signal c : std_logic_vector(1 downto 0) := "00";
  signal ua, ub : unsigned(3 downto 0) := "0000";
  signal sa : signed(3 downto 0) := "0000";
  signal sb : signed(1 downto 0) := "00";
begin
  process
  begin
    assert PICK_C = 7 report "PICK_C" severity failure;
    a <= "0101"; b <= "0101"; c <= "01";
    wait for 1 ns;
    assert a = b and not (a /= b) report "equal" severity failure;
    assert a /= c report "lengths, other value" severity failure;
    b <= "0001";
    wait for 1 ns;
    assert b = c and not (b /= c) report "lengths, same value" severity failure;
    b <= "LHHL";
    wait for 1 ns;
    assert a /= b report "weak, other value" severity failure;
    b <= "L1LH";
    wait for 1 ns;
    assert a = b report "weak, same value" severity failure;
    a <= "01X1";
    wait for 1 ns;
    assert not (a = b) and a /= b report "one metavalue" severity failure;
    b <= "U000";
    wait for 1 ns;
    assert a = b and not (a /= b) report "two metavalues" severity failure;
    c <= "Z0";
    wait for 1 ns;
    assert a = c report "two metavalues, lengths" severity failure;
    ua <= "1-00"; ub <= "W111";
    wait for 1 ns;
    assert ua = ub report "unsigned metavalues" severity failure;
    ub <= "0111";
    wait for 1 ns;
    assert ua /= ub report "unsigned one metavalue" severity failure;
    sa <= "1111"; sb <= "11";
    wait for 1 ns;
    assert sa = sb report "signed widened" severity failure;
    sb <= "01";
    wait for 1 ns;
    assert sa /= sb report "signed other" severity failure;
    sb <= "X1";
    wait for 1 ns;
    assert sa /= sb and not (sa = sb) report "signed one metavalue" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
