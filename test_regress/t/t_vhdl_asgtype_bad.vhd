-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- What is assigned must be of the target's type. A literal or an aggregate is
-- analyzed against the target and so already has it; what this catches is a
-- value with a type of its own and a different one. The universal types are
-- the exception: they convert to any type of their own class.

entity t is
end entity;

architecture rtl of t is
  type r is record
    f : integer;
  end record;
  signal s : integer := 0;
begin
  process
    variable x : r;
    variable i : integer;
    variable b : boolean;
    variable q : real;
  begin
    -- These are fine: a literal takes the target's type, and the universal
    -- types convert to any integer or floating type
    i := 25;
    q := 1.5;
    x := (f => 3);

    -- These are not
    x := 25;
    i := true;
    s <= b;
    wait;
  end process;
end architecture;
