-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- One subprogram written against unconstrained operands, called with several
-- lengths. Each call must get its own fixed-width copy, so that the emitted
-- code carries no runtime array descriptor.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_generic is
  port (a  : in  std_logic_vector(7 downto 0);
        b  : in  std_logic_vector(15 downto 0);
        ra : out std_logic_vector(7 downto 0);
        rb : out std_logic_vector(15 downto 0);
        pa : out std_logic;
        pb : out std_logic;
        sa : out std_logic_vector(7 downto 0));
end entity;

architecture rtl of t_vhdl_generic is

  -- Unconstrained parameter and unconstrained result
  function reverse (v : std_logic_vector) return std_logic_vector is
    variable r : std_logic_vector(v'range);
  begin
    for i in v'range loop
      r(v'high - i + v'low) := v(i);
    end loop;
    return r;
  end function;

  -- Unconstrained parameter, scalar result
  function parity (v : std_logic_vector) return std_logic is
    variable p : std_logic := '0';
  begin
    for i in v'range loop
      p := p xor v(i);
    end loop;
    return p;
  end function;

  -- Two unconstrained parameters at once
  function mask (v : std_logic_vector; m : std_logic_vector)
    return std_logic_vector is
  begin
    return v and m;
  end function;

begin
  ra <= reverse(a);
  rb <= reverse(b);
  pa <= parity(a);
  pb <= parity(b);
  sa <= mask(a, a);
end architecture;
