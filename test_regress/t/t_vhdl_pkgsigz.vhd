-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A signal a package declares, initialised with 'Z' elements and written
-- through a procedure's signal parameter: VUnit's "runner" (vunit/vhdl/run/
-- src/run_pkg.vhd, run.vhd), which every VUnit testbench drives. Once
-- Verilator's tristate pass looked inside tasks, the procedure's drive of a
-- signal the design's unit owns was taken into a resolution that pass
-- cannot build across modules, and every VUnit testbench was refused.
library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_pkgsigz_pkg is
  signal runner : std_logic_vector(0 to 3) := (0 => '0', others => 'Z');
  procedure set_idx(signal r : inout std_logic_vector; constant idx : natural;
                    constant v : std_logic);
end package;

package body t_vhdl_pkgsigz_pkg is
  procedure set_idx(signal r : inout std_logic_vector; constant idx : natural;
                    constant v : std_logic) is
  begin
    r(idx) <= v;
    wait for 0 ns;
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_pkgsigz_pkg.all;

entity t_vhdl_pkgsigz is
end entity;

architecture a of t_vhdl_pkgsigz is
begin
  process
  begin
    set_idx(runner, 1, '1');
    assert runner(1) = '1' severity failure;
    set_idx(runner, 0, '1');
    assert runner(0) = '1' and runner(1) = '1' severity failure;
    -- Released again: high impedance written by the procedure
    set_idx(runner, 1, 'Z');
    assert runner(0) = '1' severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
