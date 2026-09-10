-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- What an assertion or a report statement reports is a STRING, and its
-- severity a SEVERITY_LEVEL, both the ones STD.STANDARD declares. A
-- CHARACTER is not a one-character string, and a type of the design's own
-- that happens to be named SEVERITY_LEVEL is not the one a simulator acts on.

entity t_vhdl_reporttype_bad is
end entity;

architecture a of t_vhdl_reporttype_bad is
begin
  process
    type severity_level is (one, two);
    constant c : character := 'R';
    variable k : integer := 0;
  begin
    report "fine" & integer'image(k) severity std.standard.note;
    assert false report c severity std.standard.note;
    report "text" severity k;
    report "text" severity one;
    wait;
  end process;
end architecture;
