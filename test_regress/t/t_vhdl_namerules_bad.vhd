-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A type mark names a type or subtype, which 'image and 'range do not. A
-- record aggregate names its elements. A signal attribute's prefix is a
-- static name. A subprogram has one body, in a package's body rather than
-- its declaration, and objects as parameters. A configuration aspect names
-- a visible configuration.
-- References: nvc 1.22.1, GHDL 7.0.0-dev, Questa 2025.2.

package t_vhdl_namerules_pkg is
  -- Legal
  function f (a : integer) return integer;
  -- Illegal
  function g (a : integer) return integer is begin return a; end function;
end package;

package body t_vhdl_namerules_pkg is
  -- Legal
  function f (a : integer) return integer is begin return a; end function;
  -- Illegal
  function f (a : integer) return integer is begin return a + 1; end function;
end package body;

entity t_vhdl_namerules_bad is
end entity;

architecture a of t_vhdl_namerules_bad is
  type rec is record
    x, y : integer;
  end record;
  type arr is array (0 to 3) of bit;
  component comp
  end component;
  signal s : bit_vector(0 to 3);
  -- Legal
  constant r1 : rec := (x => 1, y => 2);
  subtype sub_t is integer range 0 to 3;
  -- Illegal
  constant r2 : rec := (x => 1, z => 2);
  subtype arridx is arr'range;
  constant ti : time'image(now) := "0 fs";
  procedure pr (procedure q) is begin end procedure;
  for u : comp use configuration nocfg;
begin
  u : comp;
  process (s)
  begin
    -- Legal
    if s(1)'event then end if;
    -- Illegal
    for i in s'range loop if s(i)'event then end if; end loop;
  end process;
end architecture;
