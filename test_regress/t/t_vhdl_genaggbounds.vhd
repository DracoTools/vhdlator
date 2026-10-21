-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A generic of an open array of integers whose default is an aggregate, PoC's
-- arith_scaler: "DIVS : T_POSVEC := (0 => 1)". The bounds an instance gives
-- it are generics of their own, whose defaults are the default's bounds, and
-- "(0 => 1)'left" has no type to be read in: refused ("attribute 'left").
-- Its range constrains a record's element and a variable.

entity t_vhdl_genaggbounds_sub is
  generic (DIVS : integer_vector := (0 => 1); MULS : integer_vector := (0 => 1));
end;
architecture a of t_vhdl_genaggbounds_sub is
  type tRes is record
    steps : integer_vector(DIVS'range);
  end record;
  function compute return tRes is
    variable res : tRes;
  begin
    for i in DIVS'range loop
      res.steps(i) := DIVS(i) * 2;
    end loop;
    return res;
  end function;
  constant R : tRes := compute;
begin
  process
    variable v : integer_vector(MULS'range);
  begin
    for i in MULS'range loop v(i) := MULS(i) + 1; end loop;
    report "l=" & integer'image(DIVS'left) & " r=" & integer'image(DIVS'right)
      & " s=" & integer'image(R.steps(DIVS'right)) & " m=" & integer'image(MULS'length)
      & " v=" & integer'image(v(MULS'left));
    assert DIVS'left = 0 and DIVS'right = 3 and R.steps(3) = 8 severity failure;
    assert MULS'length = 1 and v(0) = 2 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end;

entity t_vhdl_genaggbounds is end;
architecture t of t_vhdl_genaggbounds is
  constant D : integer_vector := (1, 2, 3, 4);
begin
  u : entity work.t_vhdl_genaggbounds_sub generic map (DIVS => D);
end;
