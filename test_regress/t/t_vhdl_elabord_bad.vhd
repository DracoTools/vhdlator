-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- The declarations of a region are elaborated in order, so none may call a
-- function whose body comes later, make an object of a protected type whose
-- body does, or read a deferred constant; a package body comes after all of
-- its package. The deferred constant nvc 1.22.1, GHDL 7.0.0-dev and Questa
-- 2025.2 all refuse. The rest nvc refuses, Questa with a suppressible error,
-- and GHDL warns about; Verilator has every body before anything runs, and
-- warns.

package t_vhdl_elabord_pkg is
  function f return natural;
  type pt is protected
    procedure p;
  end protected;
  constant d : natural;
  -- Illegal
  constant c1 : natural := f;
  shared variable v1 : pt;
  constant c4 : natural := d + 1;
end package;

package body t_vhdl_elabord_pkg is
  constant d : natural := 4;
  function f return natural is
  begin
    return 1;
  end function;
  type pt is protected body
    procedure p is
    begin
    end procedure;
  end protected body;
end package body;

entity t_vhdl_elabord_bad is
end entity;

architecture a of t_vhdl_elabord_bad is
  function g return natural;
  function h return natural is
  begin
    return 3;
  end function;
  -- Legal
  constant c2 : natural := h;
  -- Illegal
  constant c3 : natural := g;
  -- Legal
  function g return natural is begin return 2; end function;
begin
  process
    -- Legal
    variable v2 : natural := g;
  begin
    wait;
  end process;
end architecture;
