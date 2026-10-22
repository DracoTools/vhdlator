-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A slice of a record port's element, whose ends a function of a constant
-- settles once the instance is elaborated, handed to a parameter of an open
-- subtype: "conv_integer(m.tKeep(bitsFor(B_C) - 1 downto 0))". The constant
-- is an element of a generic record, and another subtype the architecture
-- declares names it, which makes its bounds the instance's to settle. The
-- slice is then copied into a value carrying its bounds from the whole
-- element. Shaped after surf's AxiStreamGearbox.

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_recsliceopen_pkg is
  type CfgType is record
    BYTES_C : positive;
  end record;
  type MType is record
    tKeep : std_logic_vector(15 downto 0);
  end record;
  function bitsFor (n : natural) return positive;
  function countOnes (v : std_logic_vector) return natural;
end package;

package body t_vhdl_recsliceopen_pkg is
  function bitsFor (n : natural) return positive is
    variable bits : positive := 1;
  begin
    while 2 ** bits <= n loop
      bits := bits + 1;
    end loop;
    return bits;
  end function;
  function countOnes (v : std_logic_vector) return natural is
    variable count : natural := 0;
  begin
    for i in v'range loop
      if v(i) = '1' then
        count := count + 1;
      end if;
    end loop;
    return count;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_unsigned.all;
use work.t_vhdl_recsliceopen_pkg.all;

entity t_vhdl_recsliceopen_child is
  generic (CFG_G : CfgType);
  port (m : in MType; value, ones : out natural);
end entity;

architecture rtl of t_vhdl_recsliceopen_child is
  constant B_C : positive := CFG_G.BYTES_C;
  constant SH_C : positive := B_C * 2 + 2;
  subtype ShiftType is std_logic_vector(SH_C - 1 downto 0);
  signal sh : ShiftType := (others => '0');
begin
  process (m)
  begin
    value <= conv_integer(m.tKeep(bitsFor(B_C) - 1 downto 0));
    ones <= countOnes(m.tKeep(bitsFor(B_C) - 1 downto 0)) + sh'length;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_recsliceopen_pkg.all;

entity t_vhdl_recsliceopen is
  generic (N_G : positive := 5; M_G : positive := 2);
end entity;

architecture rtl of t_vhdl_recsliceopen is
  constant C5 : CfgType := (BYTES_C => N_G);
  constant C2 : CfgType := (BYTES_C => M_G);
  signal m : MType := (tKeep => x"0000");
  signal v5, o5, v2, o2 : natural;
begin
  u5 : entity work.t_vhdl_recsliceopen_child generic map (CFG_G => C5)
    port map (m => m, value => v5, ones => o5);
  u2 : entity work.t_vhdl_recsliceopen_child generic map (CFG_G => C2)
    port map (m => m, value => v2, ones => o2);
  process
  begin
    m.tKeep <= x"00AD";
    wait for 1 ns;
    report "v5=" & integer'image(v5) & " o5=" & integer'image(o5) & " v2="
      & integer'image(v2) & " o2=" & integer'image(o2);
    -- Three bits of "10101101" for five, two for two; the shift registers are
    -- 12 and 6 long
    assert v5 = 5 and o5 = 2 + 12 report "u5" severity failure;
    assert v2 = 1 and o2 = 1 + 6 report "u2" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
