-- DESCRIPTION: Verilator: VHDL generic subprogram default naming nothing visible
--
-- The default of a generic subprogram is a name resolved where the generic
-- is declared (6.5.6.2); here no subprogram of that name is visible, as in
-- GHDL gna issue2736's mwe4, which GHDL refuses at analysis.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_gensubnamed_bad is
  generic (procedure drive_sig (signal s : out bit) is drive_sig_default);
end entity;

architecture bhv of t_vhdl_gensubnamed_bad is
begin
end architecture;
