-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A design file is analyzed in order, so a unit cannot need one written
-- after it. A use clause in a declarative part needs its package as much as
-- one in a context clause, and a configuration needs the architecture it
-- configures. References: nvc 1.22.1, GHDL 7.0.0-dev, Questa 2025.2.

entity t_vhdl_unitorder_bad is
end entity;

architecture a of t_vhdl_unitorder_bad is
  use work.nowhere.all;
begin
end architecture;

configuration t_vhdl_unitorder_cfg of t_vhdl_unitorder_bad is
  for missing
  end for;
end configuration;

architecture early of t_vhdl_unitorder_late is
begin
end architecture;

entity t_vhdl_unitorder_late is
end entity;
