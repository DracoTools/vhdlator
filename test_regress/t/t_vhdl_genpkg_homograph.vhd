-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Instantiating a generic package may make two of its subprograms alike,
-- "f(v : et)" and "f(i : natural)" with et bound to INTEGER. A call written in
-- the package means what it meant in the template (4.9): "f(v)" with v of the
-- generic type is the first, "f(3)" the second (GHDL gna bug060).

package t_vhdl_genpkg_homograph_gp is
  generic (type et);
  function f (v : et) return integer;
  function f (i : natural) return integer;
  function by_value (v : et) return integer;
  function by_index return integer;
end package;

package body t_vhdl_genpkg_homograph_gp is
  function f (v : et) return integer is
  begin
    return 1;
  end function;
  function f (i : natural) return integer is
  begin
    return 2;
  end function;
  function by_value (v : et) return integer is
  begin
    return f(v);
  end function;
  function by_index return integer is
  begin
    return f(3);
  end function;
end package body;

package t_vhdl_genpkg_homograph_ip is new work.t_vhdl_genpkg_homograph_gp
  generic map (et => integer);

use work.t_vhdl_genpkg_homograph_ip.all;

entity t_vhdl_genpkg_homograph is
end entity;

architecture behav of t_vhdl_genpkg_homograph is
begin
  process begin
    assert by_value(5) = 1 report "by_value " & integer'image(by_value(5)) severity failure;
    assert by_index = 2 report "by_index " & integer'image(by_index) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
