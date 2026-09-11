-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- What a port map may give a port: nothing may write a port of mode in, so
-- one cannot be given to a formal that writes it; and a port of mode in
-- with no default must be given something, or it has no value at all.

entity t_vhdl_portmode_sub is
  port (
    a : in bit;
    d : in bit := '0';
    b : out bit;
    c : inout bit
  );
end entity;

architecture a of t_vhdl_portmode_sub is
begin
  b <= a and d;
end architecture;

entity t_vhdl_portmode_bad is
  port (
    pi : in bit;
    po : out bit;
    pio : inout bit
  );
end entity;

architecture a of t_vhdl_portmode_bad is
  signal s1, s2, s3, s4, s5 : bit;
begin
  -- Legal: d has a default, so it may be left open
  ok1 : entity work.t_vhdl_portmode_sub port map (a => pi, d => open, b => po, c => pio);
  ok2 : entity work.t_vhdl_portmode_sub port map (pi, open, s1, pio);
  -- Illegal
  bad1 : entity work.t_vhdl_portmode_sub port map (a => s1, b => pi, c => pio);
  bad2 : entity work.t_vhdl_portmode_sub port map (a => s1, b => s2, c => pi);
  bad3 : entity work.t_vhdl_portmode_sub port map (a => open, b => s3, c => pio);
  bad4 : entity work.t_vhdl_portmode_sub port map (b => s4, c => pio);
end architecture;
