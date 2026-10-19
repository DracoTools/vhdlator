-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- STD_LOGIC_1164's matching orderings given a '-' say so, as the assertion in their body
-- does, once per operation: "?<=" is "?<" or "?=" and "?>=" is not "?<", so each asks "?<"
-- once (9.2.3; GHDL gna issue1872).

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_nine_matchord_dash is
end entity;

architecture a of t_vhdl_nine_matchord_dash is
begin
  process
    variable d : std_ulogic := '-';
    variable o : std_ulogic := '1';
    variable z : std_ulogic := '0';
    variable r : std_ulogic;
  begin
    r := d ?< o;
    assert r = 'X' report "?< gave " & std_ulogic'image(r) severity failure;
    r := o ?<= d;
    assert r = '1' report "?<= gave " & std_ulogic'image(r) severity failure;
    r := d ?> o;
    assert r = '0' report "?> gave " & std_ulogic'image(r) severity failure;
    r := o ?>= d;
    assert r = 'X' report "?>= gave " & std_ulogic'image(r) severity failure;
    r := z ?< o;
    assert r = '1' report "0 ?< 1 gave " & std_ulogic'image(r) severity failure;
    report "done";
    wait;
  end process;
end architecture;
