-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- Before VHDL-2008 two array types are closely related only where their
-- index types are too (LRM-93 7.3.5): an array indexed by BIT is no
-- conversion of one indexed by INTEGER. VESTS tc2549 must be refused; it
-- was refused only by accident until an array of reals could be converted
-- at all. nvc 1.22.1 and GHDL 7.0.0-dev refuse it in VHDL-93.
entity t_vhdl_convidx_bad is
end entity;

architecture a of t_vhdl_convidx_bad is
begin
  process
    type century is array (1 to 2) of real;
    type millenia is array (bit'('0') to bit'('1')) of real;
    variable hundreds : century;
    variable thousand : millenia;
  begin
    thousand := millenia(hundreds);
    wait;
  end process;
end architecture;
