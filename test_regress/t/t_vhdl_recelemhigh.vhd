-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A slice of a record element, bounded by an attribute of that element,
-- "r.vld(r.vld'high downto db_c)": the element is sized by constants of the
-- generics, so the slice is as long as each instance makes it, and it is
-- handed to a function whose parameter is open. open-logic's
-- olo_axi_master_full ("r.RdAlignByteVld'high downto DataBytes_c").

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_recelemhigh_pkg is
  function or_reduce(vec : in std_logic_vector) return std_logic;
  function shape(vec : in std_logic_vector) return string;
end package;
package body t_vhdl_recelemhigh_pkg is
  function or_reduce(vec : in std_logic_vector) return std_logic is
    variable tmp : std_logic := '0';
  begin
    for i in vec'low to vec'high loop
      tmp := tmp or vec(i);
    end loop;
    return tmp;
  end function;
  function shape(vec : in std_logic_vector) return string is
  begin
    return to_string(vec) & "@" & integer'image(vec'left) & ":" & integer'image(vec'right);
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_recelemhigh_pkg.all;
entity t_vhdl_recelemhigh_m is
  generic (aw : positive; uw : positive; init : std_logic_vector);
  port (last : out std_logic);
end entity;
architecture a of t_vhdl_recelemhigh_m is
  constant ab_c : natural := aw / 8;
  constant db_c : natural := uw / 8;
  type two_t is record
    vld : std_logic_vector(ab_c * 2 - 1 downto 0);
  end record;
  signal r : two_t := (vld => init);
begin
  last <= not (or_reduce(r.vld(r.vld'high downto db_c)));
  process
  begin
    wait for 1 ns;
    report "upper " & shape(r.vld(r.vld'high downto db_c));
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_recelemhigh is
end entity;
architecture a of t_vhdl_recelemhigh is
  signal last1, last2 : std_logic;
begin
  -- Eight bits, the upper six read: 000100 has a bit set
  u1 : entity work.t_vhdl_recelemhigh_m generic map (aw => 32, uw => 16, init => "00010011")
    port map (last => last1);
  -- Sixteen bits, the upper twelve read: none set
  u2 : entity work.t_vhdl_recelemhigh_m
    generic map (aw => 64, uw => 32, init => "0000000000001111")
    port map (last => last2);
  process
  begin
    wait for 2 ns;
    assert last1 = '0' report "last1 " & std_logic'image(last1) severity failure;
    assert last2 = '1' report "last2 " & std_logic'image(last2) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
