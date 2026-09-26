-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A procedure with an unconstrained parameter is emitted once per length a
-- call gives it, so each copy has one call site -- and a copy with one call
-- site was always inlined, however many copies of the one procedure there
-- were. VUnit's logging (write_to_log in vunit/vhdl/logging/src/
-- file_pkg.vhd, reached from every check) became one process body of 64 MB
-- of C++ that the compiler could not finish. The copies are now costed
-- together, as calls of one procedure.
library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_inlinefamily is
end entity;

architecture a of t_vhdl_inlinefamily is
  procedure count_ones(v : std_logic_vector; variable n : inout natural) is
  begin
    for i in v'range loop
      if v(i) = '1' then
        n := n + 1;
      end if;
    end loop;
  end procedure;
begin
  process
    variable n : natural := 0;
  begin
    count_ones("1", n);
    count_ones("10", n);
    count_ones("110", n);
    count_ones("1101", n);
    count_ones("11011", n);
    count_ones("110110", n);
    count_ones("1101101", n);
    count_ones("11011011", n);
    assert n = 26 report "n = " & integer'image(n) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
