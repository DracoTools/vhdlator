-- DESCRIPTION: Verilator: VHDL generics of the root entity given on the command line
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

-- Each value is written as GHDL's and nvc's -g take it: a scalar as its
-- image, as 'VALUE reads it; a string or a vector as its characters

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_gengiven is
  generic (
    num : integer range -200 to 100 := 0;
    str : string := "default";
    ch : character := ' ';
    flag : boolean := false;
    lg : std_logic := '0';
    MixedCase : natural := 0;
    vec : std_logic_vector;
    kind : severity_level := note);
end entity;

architecture a of t_vhdl_gengiven is
begin
  process is
  begin
    assert num = -159 severity failure;
    assert str = "Hello" severity failure;
    assert ch = 'A' severity failure;
    assert flag severity failure;
    assert lg = '1' severity failure;
    assert MixedCase = 7 severity failure;
    assert vec'length = 4 and vec = x"9" severity failure;
    assert kind = warning severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
