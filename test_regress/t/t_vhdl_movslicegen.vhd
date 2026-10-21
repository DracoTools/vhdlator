-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A slice by a generate parameter, "r.ce1((i + 1) * bab_c - 1 downto i *
-- bab_c)", of a record element whose length goes through a real-valued ceil
-- of the generics, handed to a function whose parameter is open: the element
-- is as long as the instance makes it, and the slice is cut while the design
-- runs. Shaped after open-logic's olo_base_cam.

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_movslicegen_pkg is
  function to01 (inp : in std_logic) return std_logic;
  function to01 (inp : in std_logic_vector) return std_logic_vector;
end package;
package body t_vhdl_movslicegen_pkg is
  function to01 (inp : in std_logic) return std_logic is
  begin
    case inp is
      when '0' | 'L' => return '0';
      when '1' | 'H' => return '1';
      when others => return '0';
    end case;
  end function;
  function to01 (inp : in std_logic_vector) return std_logic_vector is
    variable Result_v : std_logic_vector(inp'range);
  begin
    for i in inp'low to inp'high loop
      Result_v(i) := to01(inp(i));
    end loop;
    return Result_v;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use ieee.math_real.all;
use work.t_vhdl_movslicegen_pkg.all;
entity t_vhdl_movslicegen_m is
  generic (cw : positive := 10; depth : positive := 16);
  port (o : out std_logic_vector(11 downto 0));
end entity;
architecture a of t_vhdl_movslicegen_m is
  constant bab_c : positive := integer(ceil(log2(real(depth))));
  constant bp_c : positive := integer(ceil(real(cw) / real(bab_c)));
  constant tab_c : positive := bp_c * bab_c;
  type two_r is record
    ce1 : std_logic_vector(tab_c - 1 downto 0);
  end record;
  signal r : two_r := (ce1 => "LLLHHH001LH0");
begin
  g : for i in 0 to bp_c - 1 generate
    signal wa : std_logic_vector(bab_c - 1 downto 0);
  begin
    wa <= to01(r.ce1((i + 1) * bab_c - 1 downto i * bab_c));
    o((i + 1) * bab_c - 1 downto i * bab_c) <= wa;
  end generate;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_movslicegen is
end entity;
architecture a of t_vhdl_movslicegen is
  signal o : std_logic_vector(11 downto 0);
begin
  u : entity work.t_vhdl_movslicegen_m port map (o => o);
  process
  begin
    wait for 1 ns;
    assert o = "000111001010" report "o " & to_string(o) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
