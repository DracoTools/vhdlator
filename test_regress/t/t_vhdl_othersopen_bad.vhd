-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An aggregate with 'others' takes its index range from its context
-- (9.3.3.3), and a target whose subtype is an unconstrained array type -- a
-- port, a parameter, or a record element of one -- has none to give it; a
-- slice of one does. References: nvc 1.22.1, GHDL 7.0.0-dev, Questa 2025.2.

package pkg is
  type rec is record
    elem : bit_vector;
  end record;
  procedure pr(x : out bit_vector; signal y : out bit_vector);
end package;

package body pkg is
  procedure pr(x : out bit_vector; signal y : out bit_vector) is
  begin
    x := (x'range => '0');
    x := (others => '0');  -- Bad
    y <= (others => '0');  -- Bad
  end procedure;
end package body;

use work.pkg.all;

entity sub is
  port (p : out rec; q : out bit_vector; r : out bit_vector(3 downto 0));
end entity;

architecture a of sub is
  signal s1 : rec(elem(3 downto 0));
begin
  r <= (others => '0');
  s1.elem <= (others => '0');
  p.elem <= (others => '0');  -- Bad
  q <= (others => '0') when true else (others => '1');  -- Bad
  process
  begin
    q(1 downto 0) <= (others => '0');
    q <= (others => '0');  -- Bad
    wait;
  end process;
end architecture;

use work.pkg.all;

entity t is
end entity;

architecture a of t is
  signal p : rec(elem(1 downto 0));
  signal q : bit_vector(1 downto 0);
  signal r : bit_vector(3 downto 0);
begin
  u : entity work.sub port map (p => p, q => q, r => r);
end architecture;
