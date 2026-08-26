-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- On BOOLEAN and BIT, "and", "or", "nand" and "nor" are short-circuit
-- operations: the right operand is evaluated only if the left has not
-- already decided the answer (9.2.2). It is not an optimisation -- a guard
-- like "i <= v'high and v(i) = '1'" depends on it, and so does any impure
-- function written on the right. On a nine-value logic type the rule does
-- not apply, and both operands are evaluated.

use std.env.all;

entity t_vhdl_shortcircuit is
end entity;

architecture tb of t_vhdl_shortcircuit is

  signal v : bit_vector(3 downto 0) := "1010";

begin

  main : process is
    -- The counter is a variable of the process, and the functions that bump
    -- it are declared where they can see it, so nothing needs a shared
    -- variable (which VHDL-2002 onwards allows only of a protected type)
    variable calls : integer := 0;

    impure function counted (result : boolean) return boolean is
    begin
      calls := calls + 1;
      return result;
    end function;

    impure function counted_bit (result : bit) return bit is
    begin
      calls := calls + 1;
      return result;
    end function;

    variable b : boolean;
    variable bit_r : bit;
    variable i : integer;
  begin
    -- "false and X" is false whatever X is, so X is not evaluated
    calls := 0;
    b := false and counted(true);
    assert not b report "and result" severity failure;
    assert calls = 0 report "and evaluated its right operand" severity failure;

    -- "true or X" is true whatever X is
    calls := 0;
    b := true or counted(true);
    assert b report "or result" severity failure;
    assert calls = 0 report "or evaluated its right operand" severity failure;

    -- nand and nor are short-circuit for the same reason
    calls := 0;
    b := false nand counted(true);
    assert b report "nand result" severity failure;
    assert calls = 0 report "nand evaluated its right operand" severity failure;

    calls := 0;
    b := true nor counted(true);
    assert not b report "nor result" severity failure;
    assert calls = 0 report "nor evaluated its right operand" severity failure;

    -- Where the left operand does not decide it, the right one does run
    calls := 0;
    b := true and counted(false);
    assert not b report "and taking the right operand" severity failure;
    assert calls = 1 report "right operand not evaluated" severity failure;

    -- The classic guard: the index is only formed when it is in range
    for k in 0 to 4 loop
      if k <= v'high and v(k) = '1' then
        i := k;
      end if;
    end loop;
    assert i = 3 report "guard picked " & integer'image(i) severity failure;

    -- On BIT as well as BOOLEAN
    calls := 0;
    bit_r := '0' and counted_bit('1');
    assert bit_r = '0' report "bit and result" severity failure;
    assert calls = 0 report "bit and evaluated its right operand" severity failure;

    calls := 0;
    bit_r := '1' or counted_bit('0');
    assert bit_r = '1' report "bit or result" severity failure;
    assert calls = 0 report "bit or evaluated its right operand" severity failure;

    -- xor is never short-circuit
    calls := 0;
    b := true xor counted(true);
    assert calls = 1 report "xor must evaluate both" severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
