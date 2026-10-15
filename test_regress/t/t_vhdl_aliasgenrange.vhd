-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An alias that renumbers a parameter "1 to l'length", in a copy made for
-- a slice whose bound a generic gives: an index through the alias is put
-- onto the parameter's own range "7 downto lsb", as std_logic_1164's
-- operators renumber their operands

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_aliasgenrange_sub is
  generic (lsb : natural := 4);
  port (addr : in std_ulogic_vector(7 downto 0);
        o : out std_ulogic_vector(1 to 8 - lsb));
end entity;

architecture a of t_vhdl_aliasgenrange_sub is
  function rev(l : std_ulogic_vector) return std_ulogic_vector is
    alias lv : std_ulogic_vector(1 to l'length) is l;
    variable r : std_ulogic_vector(1 to l'length);
  begin
    for i in r'range loop
      r(i) := lv(l'length + 1 - i);
    end loop;
    return r;
  end function;
begin
  o <= rev(addr(7 downto lsb));
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_aliasgenrange is
end entity;

architecture a of t_vhdl_aliasgenrange is
  signal addr : std_ulogic_vector(7 downto 0) := "11010001";
  signal o : std_ulogic_vector(1 to 4);
begin
  s : entity work.t_vhdl_aliasgenrange_sub port map (addr, o);

  process
  begin
    wait for 1 ns;
    assert o = "1011" report to_string(o) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
