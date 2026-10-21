-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A slice target whose two ends both name "to_integer(sh)", sh a vector
-- sized by a function of a generic: the two writings cancel, so the slice is
-- as wide as the generic makes it wherever sh puts it. Shaped after
-- open-logic's olo_axi_master_full, "v.WrAlignReg((to_integer(r.WrAlignShift)
-- + AxiBytes_c) * 8 - 1 downto to_integer(r.WrAlignShift) * 8) := ...".

package t_vhdl_slicetermtemp_pkg is
  function log2(arg : in natural) return natural;
end package;
package body t_vhdl_slicetermtemp_pkg is
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
end package body;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.t_vhdl_slicetermtemp_pkg.all;
entity t_vhdl_slicetermtemp_m is
  generic (aw : positive);
  port (d : in std_logic_vector(aw - 1 downto 0);
        sh : in unsigned(log2(aw / 8) - 1 downto 0);
        o : out std_logic_vector(2 * aw - 1 downto 0));
end entity;
architecture a of t_vhdl_slicetermtemp_m is
  constant ab_c : natural := aw / 8;
  type two_r is record
    reg : std_logic_vector(aw * 2 - 1 downto 0);
    shift : unsigned(log2(ab_c) - 1 downto 0);
  end record;
begin
  process (all)
    variable v : two_r;
  begin
    v.reg := (others => '0');
    v.shift := sh;
    v.reg((to_integer(v.shift) + ab_c) * 8 - 1 downto to_integer(v.shift) * 8) := d;
    o <= v.reg;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
entity t_vhdl_slicetermtemp is
end entity;
architecture a of t_vhdl_slicetermtemp is
  signal o : std_logic_vector(63 downto 0);
  signal sh : unsigned(1 downto 0) := "00";
begin
  u : entity work.t_vhdl_slicetermtemp_m generic map (aw => 32)
    port map (d => x"DEADBEEF", sh => sh, o => o);
  process
  begin
    wait for 1 ns;
    assert o = x"00000000DEADBEEF" report "0: " & to_hstring(o) severity failure;
    sh <= "01";
    wait for 1 ns;
    assert o = x"000000DEADBEEF00" report "1: " & to_hstring(o) severity failure;
    sh <= "11";
    wait for 1 ns;
    assert o = x"00DEADBEEF000000" report "3: " & to_hstring(o) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
