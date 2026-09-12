-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A package body continues the declarative region of the package it completes
-- (12.1), so what the package declares is visible inside it -- including the
-- generics of a package that carries its own generic map (4.7, 4.9).

entity t_vhdl_nestpkg is
end entity;

use std.env.all;

architecture tb of t_vhdl_nestpkg is

  package inner_pkg is
    constant k : natural := 7;
    type small_t is range 0 to 15;
    function twice_k return natural;
    function of_small (v : small_t) return natural;
  end package;

  package body inner_pkg is
    -- Everything the package declares is in reach here
    function twice_k return natural is
    begin
      return 2 * k;
    end function;
    function of_small (v : small_t) return natural is
    begin
      return natural(v) + k;
    end function;
  end package body;

  package sized_pkg is
    generic (width : natural);
    generic map (width => 4);
    function bits return natural;
  end package;

  package body sized_pkg is
    function bits return natural is
    begin
      return width;
    end function;
  end package body;

begin

  process is
  begin
    assert inner_pkg.twice_k = 14
      report "twice_k is " & integer'image(inner_pkg.twice_k) severity failure;
    assert inner_pkg.of_small(inner_pkg.small_t'(3)) = 10
      report "of_small" severity failure;
    assert sized_pkg.bits = 4
      report "bits is " & integer'image(sized_pkg.bits) severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
