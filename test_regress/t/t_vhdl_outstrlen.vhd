-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- An unconstrained STRING parameter of mode out has the bounds of its
-- actual (4.2.2.3), so "result'length" says how long the caller's variable
-- is: VUnit's codec_builder decode, "result := code(index + 9 to index + 9
-- + result'length - 1)". Held as text, the parameter started empty, and a
-- procedure given a string only running knows the length of copied
-- nothing into it.
entity t_vhdl_outstrlen is
end entity;

architecture a of t_vhdl_outstrlen is
  procedure take(constant code : string; variable index : inout positive;
                 variable result : out string) is
  begin
    result := code(index to index + result'length - 1);
    index := index + result'length;
  end procedure;

  type range_t is array (integer range <>) of bit;

  function span(n : natural) return range_t is
    constant c : range_t(1 to n) := (others => '0');
  begin
    return c;
  end function;

  -- How long the variable is is known only from an object while running
  function first(constant code : string; n : natural) return string is
    constant rr : range_t := span(n);
    variable v : string(rr'range) := (others => ' ');
    variable index : positive := code'left;
  begin
    take(code, index, v);
    return v & integer'image(index);
  end function;
begin
  process
  begin
    assert first("alphabet", 5) = "alpha6" report first("alphabet", 5) severity failure;
    assert first("alphabet", 2) = "al3" report first("alphabet", 2) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
