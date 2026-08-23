-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- FOREIGN subprograms that analyze but cannot be given to C.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_foreign_lower_bad is
end entity;

architecture rtl of t_vhdl_foreign_lower_bad is

  -- A body as well as a C implementation
  function f_body (v : integer) return integer is
  begin
    return v;
  end function;
  attribute foreign of f_body : function is "VHPIDIRECT c_f_body";

  -- C has no way to say how long the result is
  function f_open (v : std_logic_vector) return std_logic_vector;
  attribute foreign of f_open : function is "VHPIDIRECT c_f_open";

  -- Nor how long a parameter is
  function f_openarg (v : std_logic_vector) return integer;
  attribute foreign of f_openarg : function is "VHPIDIRECT c_f_openarg";

  -- A whole architecture cannot be written in C here
  attribute foreign of rtl : architecture is "VHPIDIRECT c_arch";

  signal a : std_logic_vector(7 downto 0) := (others => '0');
  signal b : std_logic_vector(7 downto 0);
  signal n : integer;
begin
  b <= f_open(a);
  n <= f_openarg(a) + f_body(1);
end architecture;
