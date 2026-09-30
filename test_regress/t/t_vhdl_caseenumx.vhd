-- DESCRIPTION: Verilator: VHDL case choice 'X' of a design's own enumeration
--
-- This file is part of Verilator.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
--
-- 'X' is a metavalue of std_logic, which a two-state runtime cannot hold,
-- so a case choice naming it there can never be taken. Of an enumeration a
-- design declares it is an ordinary literal: VUnit's inactive_bus_policy_t
-- is ('0', '1', 'X', hold, rand01), and its axi_stream_master selects on it.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_caseenumx is
end entity;

architecture a of t_vhdl_caseenumx is
  type policy_t is ('0', '1', 'X', hold, rand01);

  function name_of(p : policy_t) return string is
  begin
    case p is
      when 'X' => return "x";
      when '0' => return "zero";
      when '1' => return "one";
      when hold => return "hold";
      when rand01 => return "rand";
    end case;
  end;

  function logic_of(s : std_logic) return string is
  begin
    case s is
      when 'X' => return "x";
      when '0' => return "zero";
      when '1' => return "one";
      when others => return "other";
    end case;
  end;
begin
  process
    variable v : policy_t := 'X';
  begin
    report name_of(v) & " " & name_of('0') & " " & name_of('1') & " " & name_of(hold);
    assert name_of(v) = "x" report "'X'" severity failure;
    assert name_of('0') = "zero" and name_of('1') = "one" report "'0', '1'" severity failure;
    assert name_of(hold) = "hold" and name_of(rand01) = "rand" report "others" severity failure;
    -- A logic type's choices are unchanged
    assert logic_of('0') = "zero" and logic_of('1') = "one" report "logic" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
