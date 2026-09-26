-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A package whose arrays are sized by an element of a shared variable it
-- declares: UVVM's "t_requirement_entry_array(0 to
-- shared_spec_cov_config.max_requirement_lines)" (bitvis_vip_spec_cov/src/
-- spec_cov_pkg.vhd). While the package elaborates, the shared variable
-- holds its initial value (12.3.1.4 in VHDL-93), so the bound is that
-- value's element; it was lowered as a read of the variable, which is no
-- bound. A subprogram of the package reads the variable as it is when it
-- runs. VHDL-93, which has shared variables of any type.
package t_vhdl_sharedinit_pkg is
  type t_config is record
    max_lines : natural;
    max_cases : natural;
  end record;
  constant C_CONFIG_DEFAULT : t_config := (max_lines => 5, max_cases => 3);
  shared variable shared_config : t_config := C_CONFIG_DEFAULT;
  type t_line_vector is array (0 to shared_config.max_cases - 1) of integer;
  impure function current_lines return natural;
end package;

package body t_vhdl_sharedinit_pkg is
  type t_entry_array is array (natural range <>) of t_line_vector;
  shared variable priv_entries : t_entry_array(0 to shared_config.max_lines);

  impure function current_lines return natural is
  begin
    return shared_config.max_lines;
  end function;
end package body;

use work.t_vhdl_sharedinit_pkg.all;

entity t_vhdl_sharedinit is
end entity;

architecture a of t_vhdl_sharedinit is
begin
  process
    variable v : t_line_vector;
  begin
    assert v'length = 3 severity failure;
    assert current_lines = 5 severity failure;
    shared_config.max_lines := 9;
    assert current_lines = 9 severity failure;
    -- The subtype was fixed as the package elaborated
    assert v'length = 3 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
