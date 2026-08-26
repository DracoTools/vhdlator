-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A library of its own: a package and a context declaration that names it.

package types_pkg is
  type t_colour is (red, green, blue);
  constant DEFAULT_COLOUR : t_colour := green;
  function next_colour (c : t_colour) return t_colour;
end package;

package body types_pkg is
  function next_colour (c : t_colour) return t_colour is
  begin
    if c = blue then return red; end if;
    return t_colour'succ(c);
  end function;
end package body;

context my_context is
  library mylib;
  use mylib.types_pkg.all;
  use std.textio.all;
end context;
