-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An aggregate with "others" whose target is a parameter of an unconstrained
-- array type has no index range to take (9.3.3.3). Strict VHDL-93 refuses it
-- as later revisions do (GHDL --std=93, nvc --std=1993; GHDL gna bug044);
-- GHDL's default "93c" takes it (t_vhdl_others_open93c).

entity t_vhdl_others_open93_bad is
end entity;

architecture behav of t_vhdl_others_open93_bad is
  procedure proc (b, c : out bit_vector) is
  begin
    b := (others => '0');
    c := ('1', others => '0');
  end proc;
begin
end behav;
