-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- 'INSTANCE_NAME of a generic package written in its own body, in an instance: the generic
-- package's name, from the library the instance is in. ieee.fixed_pkg's truncation warning
-- reads ":ieee:fixed_generic_pkg:TO_UFIXED(NATURAL): vector truncated" (GHDL gna issue2946,
-- where nvc, GHDL and Questa agree).

library ieee;
use ieee.fixed_pkg.all;

entity t_vhdl_genpkgname is
end entity;

architecture a of t_vhdl_genpkgname is
begin
  process is
    variable u : ufixed(3 downto 0);
  begin
    u := to_ufixed(100, 3, 0);
    report "u = " & to_string(u);
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
