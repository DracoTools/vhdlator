-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- 6.5.6.1: a generic clause declares constants, and since 2008 types,
-- subprograms and packages. A signal, a variable or a file is not a generic.
entity t_vhdl_genericclass_bad is
    generic (signal   s : in integer;
             variable v : in integer;
             file     f : integer;
             constant c : integer := 1;
             type     t);
end entity;

architecture test of t_vhdl_genericclass_bad is
begin
end architecture;
