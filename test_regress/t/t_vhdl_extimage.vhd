-- DESCRIPTION: Verilator: VHDL image of an extended identifier literal
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

-- 'IMAGE keeps an extended identifier's backslashes, its own doubled;
-- TO_STRING gives the name alone (GHDL gna ticket61)

entity t_vhdl_extimage is
end entity;

architecture a of t_vhdl_extimage is
begin
  process is
    type enum is (a, short, \Extended\, \Weird\\Name\);
    variable e : enum;
  begin
    assert enum'image(short) = "short" severity failure;
    assert enum'image(\Extended\) = "\Extended\" severity failure;
    assert enum'image(\Weird\\Name\) = "\Weird\\Name\" severity failure;
    e := \Weird\\Name\;
    assert enum'image(e) = "\Weird\\Name\" severity failure;
    assert to_string(e) = "Weird\Name" severity failure;
    assert to_string(\Extended\) = "Extended" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
