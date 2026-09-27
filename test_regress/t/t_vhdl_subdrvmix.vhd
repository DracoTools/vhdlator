-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A process has one driver of a signal however its assignments are written
-- (14.7.2): what a procedure it calls assigns to a signal parameter goes to
-- that driver, and so does what the process then assigns itself. Here the
-- port is an inout of a resolved type, so the two must not become two
-- drivers resolved against each other ("011" and "110" read "111").

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_subdrvmix_pkg is
  type t_bus is record
    cs   : std_logic;
    addr : std_logic_vector(2 downto 0);
  end record;
  procedure put(signal b : inout t_bus; constant a : in std_logic_vector);
end package;

package body t_vhdl_subdrvmix_pkg is
  procedure put(signal b : inout t_bus; constant a : in std_logic_vector) is
  begin
    b.cs <= '1';
    b.addr <= a;
    wait for 1 ns;
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_subdrvmix_pkg.all;

entity t_vhdl_subdrvmix_sub is
  port (m : inout t_bus := ('0', "000"));
end entity;

architecture a of t_vhdl_subdrvmix_sub is
begin
  process
  begin
    put(m, "011");
    assert m.addr = "011" report "after the call addr=" & to_string(m.addr) severity failure;
    m.addr <= "110";
    m.cs <= '0';
    wait for 1 ns;
    assert m.addr = "110" report "after the process addr=" & to_string(m.addr)
      severity failure;
    assert m.cs = '0' report "after the process cs=" & to_string(m.cs) severity failure;
    put(m, "100");
    assert m.addr = "100" report "after the second call addr=" & to_string(m.addr)
      severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_subdrvmix_pkg.all;

entity t_vhdl_subdrvmix is
end entity;

architecture a of t_vhdl_subdrvmix is
  signal b : t_bus;
begin
  i_sub : entity work.t_vhdl_subdrvmix_sub port map (m => b);
end architecture;
