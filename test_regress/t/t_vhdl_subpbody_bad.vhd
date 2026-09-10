-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Every subprogram declaration is completed by a body in the same region --
-- for a package declaration, in its package body -- and the body repeats
-- the declaration's specification element for element. Case, comments, how
-- a number is written and whether a name is expanded may differ; nothing
-- else may, not even to say the same thing another way.

package p is
  function f (i, j : integer) return integer;
  procedure q (x : in integer := 16#10#);
  function g (v : std.standard.integer) return boolean;
  procedure never (x : integer);
  procedure split (x, y : integer);
  function mode (x : integer) return integer;
end package;

package body p is
  -- Legal: case, a comment, a numeric literal of the same value and an
  -- expanded name are all allowed variations
  FUNCTION F (I, J : INTEGER) -- a comment
    RETURN INTEGER IS
  begin
    return i + j;
  end function;
  procedure q (x : in integer := 16) is
  begin
  end procedure;
  function g (v : integer) return boolean is
  begin
    return v > 0;
  end function;
  -- Illegal
  procedure split (x : integer; y : integer) is
  begin
  end procedure;
  function mode (x : in integer) return integer is
  begin
    return x;
  end function;
end package body;

entity t_vhdl_subpbody_bad is
end entity;

architecture a of t_vhdl_subpbody_bad is
  procedure local_never;
begin
end architecture;
