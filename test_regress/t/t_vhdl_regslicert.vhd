-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A procedure walks a register 32 elements at a time and hands each slice,
-- whose ends only running settles, to an inout formal of another:
-- "inner(reg(highbit downto (32 * i) + reg'low))". Called with a variable
-- "std_logic_vector(reg'length - 1 downto 0)" of a register a generic sizes,
-- the register is a value carrying its bounds, and the slice is made one for
-- the call and written back after (4.2.2.3). Shaped after surf's AxiLitePkg,
-- axiSlaveRegisterR through axiSlaveRegister to axiSlaveRegisterLegacy.

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_regslicert_pkg is
  procedure inner (reg : inout std_logic_vector);
  procedure outer (reg : inout std_logic_vector);
  procedure outerR (reg : in std_logic_vector; o : out std_logic_vector);
end package;

package body t_vhdl_regslicert_pkg is
  procedure inner (reg : inout std_logic_vector) is
  begin
    reg := not reg;
  end procedure;
  procedure outer (reg : inout std_logic_vector) is
    variable highbit : integer;
  begin
    for i in ((reg'length - 1) / 32) downto 0 loop
      if i = ((reg'length - 1) / 32) then
        highbit := ((reg'length - 1) mod 32) + (32 * i) + reg'low;
      else
        highbit := 31 + (32 * i) + reg'low;
      end if;
      inner(reg(highbit downto (32 * i) + reg'low));
    end loop;
  end procedure;
  procedure outerR (reg : in std_logic_vector; o : out std_logic_vector) is
    variable regTmp : std_logic_vector(reg'length - 1 downto 0);
  begin
    regTmp := reg;
    outer(regTmp);
    o := regTmp;
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_regslicert_pkg.all;

entity t_vhdl_regslicert is
  generic (N_G : positive := 40);
end entity;

architecture rtl of t_vhdl_regslicert is
  signal g : std_logic_vector(N_G - 1 downto 0) := x"00000000FF";
begin
  process
    variable x : std_logic_vector(39 downto 0) := x"00000000FF";
    variable y : std_logic_vector(7 downto 0) := x"0F";
  begin
    outerR(g, x);
    outer(y);
    report to_hstring(x) & " " & to_hstring(y);
    assert x = x"FFFFFFFF00" severity failure;
    assert y = x"F0" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
