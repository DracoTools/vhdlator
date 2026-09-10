-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A qualified expression names the type its operand already has; it
-- converts nothing. Every element of an aggregate is of the type of what it
-- stands for, and elements given one value together are of one type, and
-- 'others' stands for at least one. A type conversion has one operand whose
-- type it does not supply. A slice names one range of a one-dimensional
-- array.

entity t_vhdl_exprtype_bad is
end entity;

architecture a of t_vhdl_exprtype_bad is
  type rec is record
    a : integer;
    b : boolean;
    c : integer;
  end record;
  type mat is array (1 to 3, 1 to 3) of bit;
  type ivec is array (1 to 3) of integer;
begin
  process
    variable i : integer;
    variable r : rec;
    variable m : mat;
    variable iv : ivec;
    variable bv : bit_vector(0 to 3);
  begin
    -- Legal
    i := integer'(5);
    r := (a | c => 1, b => true);
    r := (b => false, others => 7);
    bv := bit_vector'("0101");
    i := integer(2.7);
    -- Illegal
    i := integer'(true);
    r := (a => 1, others => 2);
    r := (a | b => 1, c => 2);
    r := (1, true, 3, others => 4);
    iv := (1, 2, 3.5);
    bv := bit_vector("0101");
    i := integer(1, 2);
    m(1 to 2, 1 to 2) := m(2 to 3, 2 to 3);
    wait;
  end process;
end architecture;
