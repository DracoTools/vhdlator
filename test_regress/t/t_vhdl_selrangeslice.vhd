-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A slice by the range of a record element, "addr(v.shift'range)", is as
-- long as the subtype the element was declared with, here one the generics
-- settle through a function: it was the whole of addr, which an arithmetic
-- operator then measured (open-logic's olo_axi_master_full,
-- "to_unsigned(AxiBytes_c, v.RdLowIdx'length) - unsigned(CmdRd_Addr(
-- v.RdShift'range))").

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
package t_vhdl_selrangeslice_pkg is
  function log2(arg : in natural) return natural;
  function shape(x : unsigned) return string;
end package;
package body t_vhdl_selrangeslice_pkg is
  function log2(arg : in natural) return natural is
    variable v : natural := arg;
    variable r : natural := 0;
  begin
    while v > 1 loop
      v := v / 2;
      r := r + 1;
    end loop;
    return r;
  end function;
  function shape(x : unsigned) return string is
  begin
    return to_string(x) & "@" & integer'image(x'left) & ":" & integer'image(x'right);
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.t_vhdl_selrangeslice_pkg.all;
entity t_vhdl_selrangeslice_m is
  generic (aw : positive);
  port (addr : in std_logic_vector(31 downto 0));
end entity;
architecture a of t_vhdl_selrangeslice_m is
  constant ab_c : natural := aw / 8;
  type two_r is record
    shift : unsigned(log2(ab_c) - 1 downto 0);
    lowidx : unsigned(log2(ab_c) downto 0);
    mid : unsigned(log2(ab_c) + 3 downto log2(ab_c));
  end record;
begin
  process
    variable v : two_r;
  begin
    wait for 1 ns;
    assert shape(unsigned(addr(v.shift'range))) = "11@1:0"
      report "shift " & shape(unsigned(addr(v.shift'range))) severity failure;
    assert shape(unsigned(addr(v.mid'range))) = "1010@5:2"
      report "mid " & shape(unsigned(addr(v.mid'range))) severity failure;
    v.shift := unsigned(addr(v.shift'range));
    v.lowidx := to_unsigned(ab_c, v.lowidx'length) - unsigned(addr(v.shift'range));
    assert v.lowidx = "001" report "lowidx " & to_string(v.lowidx) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_selrangeslice is
end entity;
architecture a of t_vhdl_selrangeslice is
begin
  u : entity work.t_vhdl_selrangeslice_m generic map (aw => 32) port map (addr => x"0000002B");
end architecture;
