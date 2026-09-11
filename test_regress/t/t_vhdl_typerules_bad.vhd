-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A protected type declares only its methods. No access type designates a
-- file, nor before VHDL-2019 a protected object, and an incomplete type is
-- completed, and not as one if an access type designates it. A pure function
-- declares no file. A port's default
-- is of its subtype. An aggregate target takes its type from the value
-- assigned, so that value has one of its own. A universal real scaled by a
-- universal integer becomes no named type, and REAL has no operator taking
-- a universal integer.
-- References: nvc 1.22.1, GHDL 7.0.0-dev, Questa 2025.2.

entity t_vhdl_typerules_sub is
  port (i : in bit_vector(2 downto 0) := "000");
end entity;

architecture a of t_vhdl_typerules_sub is
begin
end architecture;

entity t_vhdl_typerules_bad is
end entity;

architecture a of t_vhdl_typerules_bad is
  type ft is file of integer;
  type pt is protected
    procedure p;
  end protected;
  type pt is protected body
    procedure p is
    begin
    end procedure;
  end protected body;
  -- Legal
  impure function fi return integer is file fh : ft; begin return 1; end function;
  constant r1 : real := real(1000.0 / 100);
  constant r2 : real := 1000.0 / 100.0;
  -- Illegal
  type pt2 is protected type t is range 0 to 2; procedure q; end protected;
  type fa is access ft;
  type pa is access pt;
  type inc; type inca is access inc; type inc is file of character;
  type never;
  function fp return integer is file fh : ft; begin return 1; end function;
  constant r3 : real := 1000.0 / 100;
  constant r4 : real := 2.0 * 3;
begin
  process
    variable x, y : bit;
    variable r : real := 1.0;
  begin
    -- Legal
    (x, y) := bit_vector'("10");
    -- Illegal
    (x, y) := "10";
    r := r * 2;
    wait;
  end process;
end architecture;

entity t_vhdl_typerules_sub2 is
  -- Illegal
  port (i : in bit_vector(2 downto 0) := ('1', '0'));
end entity;
