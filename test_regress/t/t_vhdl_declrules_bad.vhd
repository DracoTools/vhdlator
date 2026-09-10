-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Four rules about what may be written where, each of which the frontend
-- used to let through: what a declarative part admits, what a composite type
-- may be made of, and where an "others" choice may stand.

entity t is
end entity;

architecture rtl of t is

  -- A file is a handle on something outside the design, so it cannot be an
  -- element of a composite: the composite would be copied and it cannot be
  type ft is file of integer;
  type bad_array is array (1 to 10) of ft;

  -- A subprogram is for code; a signal outlives one call of it
  function f return character is
    signal s : bit;
  begin
    return 'a';
  end function;

begin

  -- A process may not declare a component either: a binding belongs to a
  -- structure, and a process is not one
  p1 : process
    component c1 port (b : bit); end component;
  begin
    wait;
  end process;

  -- "others" stands for whatever is left, so nothing may follow it
  p2 : process
    type six is array (1 to 6) of integer;
    variable v : six := (1 => 10, 3 => 30, others => 20, 2 => 25);
    variable w : six := (others => 20, 25);
  begin
    wait;
  end process;

end architecture;
