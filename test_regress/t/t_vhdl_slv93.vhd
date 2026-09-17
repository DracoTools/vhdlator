-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- The 1993 edition of STD_LOGIC_1164 declares STD_LOGIC_VECTOR as a type of
-- its own; the 2008 edition, which is the one Verilator ships, makes it a
-- resolved subtype of STD_ULOGIC_VECTOR. So a pre-2008 design may declare a
-- subprogram over each, as VESTS' ashenden/compliant/util_pk_test.vhd does,
-- and against the shipped package the two have the same profile. They are
-- held apart before 2008, and a call is given the one its actual is.
--
-- nvc 1.22.1 at --std=1993 and Questa 2025.2 at -93 both accept this; nvc at
-- --std=2008 refuses it, which is the distinction being kept.

library ieee;
use ieee.std_logic_1164.all;

package slv93_pkg is
  function widen (v : std_ulogic_vector) return integer;
  function widen (v : std_logic_vector) return integer;
end package;

package body slv93_pkg is
  function widen (v : std_ulogic_vector) return integer is
  begin
    return v'length;
  end function;
  function widen (v : std_logic_vector) return integer is
  begin
    return 100 + v'length;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.slv93_pkg.all;

entity t_vhdl_slv93 is
end entity;

architecture tb of t_vhdl_slv93 is
  signal u : std_ulogic_vector(1 to 4);
  signal r : std_logic_vector(1 to 3);
begin
  process is
  begin
    -- The unresolved actual takes the unresolved overload
    assert widen(u) = 4 report "ulogic overload" severity failure;
    -- And the resolved one takes the other, which the 2008 package alone
    -- could not tell apart from it
    assert widen(r) = 103 report "logic overload" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
