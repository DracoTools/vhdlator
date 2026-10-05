-- DESCRIPTION: Verilator: VHDL alias of a type declared in a package that
-- is not made visible, used with its literals, units and operations
--
-- "alias t_phys is work.pkg.t_phys" declares aliases of the type's units as
-- well as of its literals and predefined operations (6.6.3): "5 kilothing",
-- "const = one" and "to_string(const)" all work through the alias where
-- nothing else makes the package visible (nvc parse issue776).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_aliastypeops_pkg is
  type t_type is (one, two, three);
  type t_phys is range 1 to 1000000
    units
      thing;
      kilothing = 1000 thing;
    end units;
end package;

entity t_vhdl_aliastypeops is
end entity;

architecture beh of t_vhdl_aliastypeops is
  alias t_type is work.t_vhdl_aliastypeops_pkg.t_type;
  alias t_phys is work.t_vhdl_aliastypeops_pkg.t_phys;
begin
  process is
    constant const : t_type := one;
    variable v : t_type := three;
    constant const2 : t_phys := 5 kilothing;
  begin
    assert to_string(const) = "one" severity failure;
    assert const = one severity failure;
    assert v > const and v /= two severity failure;
    assert to_string(const2) = "5000 thing" severity failure;
    assert const2 = 5000 thing severity failure;
    assert const2 * 2 = 10 kilothing severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
