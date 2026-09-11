-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Before VHDL-2008 a port of mode inout is given only an inout port, and a
-- port of mode buffer only a buffer port; VHDL-2008 relaxed both.

entity t_vhdl_portmode93_sub is
  port (c1 : inout integer; c2 : buffer integer);
end entity;

architecture a of t_vhdl_portmode93_sub is
begin
end architecture;

entity t_vhdl_portmode93_bad is
  port (pio : inout integer; pbuf : buffer integer; pout : out integer);
end entity;

architecture a of t_vhdl_portmode93_bad is
begin
  ok : entity work.t_vhdl_portmode93_sub port map (c1 => pio, c2 => pbuf);
  bad1 : entity work.t_vhdl_portmode93_sub port map (c1 => pout, c2 => pbuf);
  bad2 : entity work.t_vhdl_portmode93_sub port map (c1 => pio, c2 => pio);
end architecture;
