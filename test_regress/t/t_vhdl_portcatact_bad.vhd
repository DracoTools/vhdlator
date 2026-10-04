-- DESCRIPTION: Verilator: VHDL unconstrained port given a concatenation of
-- signals
--
-- What is associated with an unconstrained port must have a globally
-- static subtype (6.5.6.3), and a concatenation of signals has none --
-- GHDL gna issue3229, refused by nvc 1.22.1 and GHDL 7.0.0-dev alike.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_portcatact_bad_inner is
  port (data : in std_logic_vector);
end entity;

architecture rtl of t_vhdl_portcatact_bad_inner is
begin
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_portcatact_bad is
end entity;

architecture rtl of t_vhdl_portcatact_bad is
  signal a : std_logic_vector(1 downto 0) := "00";
  signal b : std_logic_vector(1 downto 0) := "00";
begin
  u : entity work.t_vhdl_portcatact_bad_inner
    port map (data => a & b);
end architecture;
