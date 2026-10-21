-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A constant whose length comes from a function of the generics is
-- elaborated before the declarations after it (14.4.2), so a signal of a
-- generate given one of its elements as its initial value starts from that
-- element (PoC's sync_Bits: "constant INIT_I : std_logic_vector :=
-- resize(descend(INIT), BITS)" and "signal Data_meta : std_logic :=
-- INIT_I(i)"). A generic map naming the constant, in a generate the
-- instance leaves out, does not make it any less an object given its value

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_dynconstinit_pkg is
  function imin(a, b : integer) return integer;
  function resize(vec : std_logic_vector; length : natural; fill : std_logic := '0')
    return std_logic_vector;
  function descend(vec : std_logic_vector) return std_logic_vector;
end package;

package body t_vhdl_dynconstinit_pkg is
  function imin(a, b : integer) return integer is
  begin
    if a < b then
      return a;
    end if;
    return b;
  end function;
  function resize(vec : std_logic_vector; length : natural; fill : std_logic := '0')
    return std_logic_vector is
    constant high2b : natural := vec'low + length - 1;
    constant highcp : natural := imin(vec'high, high2b);
    variable res_up : std_logic_vector(vec'low to high2b);
    variable res_dn : std_logic_vector(high2b downto vec'low);
  begin
    if vec'ascending then
      res_up := (others => fill);
      res_up(vec'low to highcp) := vec(vec'low to highcp);
      return res_up;
    else
      res_dn := (others => fill);
      res_dn(highcp downto vec'low) := vec(highcp downto vec'low);
      return res_dn;
    end if;
  end function;
  function descend(vec : std_logic_vector) return std_logic_vector is
    variable res : std_logic_vector(vec'high downto vec'low);
  begin
    res := vec;
    return res;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_dynconstinit_other is
  generic (INIT : std_logic_vector);
  port (o : out std_logic_vector(INIT'range));
end entity;
architecture a of t_vhdl_dynconstinit_other is
begin
  o <= INIT;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_dynconstinit_pkg.all;
entity t_vhdl_dynconstinit_sync is
  generic (BITS : positive := 3; INIT : std_logic_vector := x"00000005");
  port (o : out std_logic_vector(BITS - 1 downto 0));
end entity;
architecture a of t_vhdl_dynconstinit_sync is
  constant INIT_I : std_logic_vector := resize(descend(INIT), BITS);
  signal whole : std_logic_vector(BITS - 1 downto 0) := INIT_I;
begin
  gen : for i in 0 to BITS - 1 generate
    signal m : std_logic := INIT_I(i);
  begin
    o(i) <= m;
  end generate;
  gx : if BITS > 100 generate
    other : entity work.t_vhdl_dynconstinit_other generic map (INIT => INIT_I)
      port map (o => open);
  end generate;
  process
  begin
    assert to_string(whole) = to_string(INIT_I)
      report "whole " & to_string(whole) severity failure;
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_dynconstinit is
end entity;
architecture a of t_vhdl_dynconstinit is
  signal o3 : std_logic_vector(2 downto 0);
  signal o5 : std_logic_vector(4 downto 0);
begin
  u3 : entity work.t_vhdl_dynconstinit_sync generic map (BITS => 3) port map (o => o3);
  u5 : entity work.t_vhdl_dynconstinit_sync generic map (BITS => 5, INIT => "10110")
    port map (o => o5);
  process
  begin
    wait for 1 ns;
    assert to_string(o3) = "101" report "o3 " & to_string(o3) severity failure;
    assert to_string(o5) = "10110" report "o5 " & to_string(o5) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
