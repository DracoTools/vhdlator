-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A function written around a signal in a port map is a conversion
-- function, whose one parameter is a constant (6.5.7.1). References: GHDL
-- 7.0.0-dev, Questa 2025.2 (nvc 1.22.1 takes it).

entity sub is
  port (x : in bit_vector(4 downto 0));
end entity;

architecture a of sub is
begin
end architecture;

entity t is
end entity;

architecture a of t is
  function conv_c(d : bit_vector(4 downto 0)) return bit_vector is
  begin
    return d;
  end function;
  function conv_s(signal d : bit_vector(4 downto 0)) return bit_vector is
  begin
    return d;
  end function;
  signal s : bit_vector(4 downto 0);
begin
  u1 : entity work.sub port map (x => conv_c(s));
  u2 : entity work.sub port map (x => conv_s(s));  -- Bad
end architecture;
