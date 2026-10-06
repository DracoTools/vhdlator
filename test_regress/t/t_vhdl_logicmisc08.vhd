-- DESCRIPTION: Verilator: VHDL ieee.std_logic_misc in a VHDL-2008 design
--
-- The IEEE library is the 2008 edition, where STD_LOGIC_VECTOR is a subtype
-- of STD_ULOGIC_VECTOR, and std_logic_misc's overloads for both are then
-- homographs. Its sources mark them "--!V08", which were not left out
-- (GHDL synth issue2850).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_misc.all;

entity t_vhdl_logicmisc08 is
end entity;

architecture a of t_vhdl_logicmisc08 is
  signal v : std_logic_vector(3 downto 0) := "1011";
begin
  process
  begin
    wait for 1 ns;
    assert and_reduce(v) = '0' severity failure;
    assert or_reduce(v) = '1' severity failure;
    assert xor_reduce(v) = '1' severity failure;
    report "logicmisc08 done";
    std.env.finish;
  end process;
end architecture;
