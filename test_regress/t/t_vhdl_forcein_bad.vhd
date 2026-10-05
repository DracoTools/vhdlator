-- DESCRIPTION: Verilator: VHDL force mode out of a target of mode in
--
-- A port or signal parameter of mode in has no driving value to force:
-- force mode out of one is an error, as is any ordinary assignment
-- (10.5.2.1, 6.5.2).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_forcein_bad is
  port (i : in bit);
end entity;

architecture a of t_vhdl_forcein_bad is
  procedure p(signal s : in bit) is
  begin
    s <= force out '1';
  end procedure;
begin
  process is
  begin
    i <= force out '0';
    i <= release out;
    wait;
  end process;
end architecture;
