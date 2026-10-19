-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A subtype declaration is elaborated where it is written, and so is the
-- impure function its constraint calls: once, whether or not anything is
-- declared of the subtype (6.3, 14.4.2; GHDL gna bug0117). In a process that
-- is once at the start; in a subprogram, once per call.

entity t_vhdl_subtypeimpure is
end entity;

architecture behav of t_vhdl_subtypeimpure is
  type bv_array is array (natural range <>) of bit_vector;
begin
  process
    variable count : natural := 0;
    variable calls : natural := 0;
    impure function seven return natural is
    begin
      count := count + 1;
      return 7;
    end function;
    impure function counted return natural is
    begin
      calls := calls + 1;
      return 7;
    end function;
    procedure declares is
      subtype rng is natural range counted downto 0;
    begin
    end procedure;
    subtype word_rng is natural range seven downto 0;
    subtype word_array is bv_array(open)(word_rng);
    subtype word_array4 is word_array(0 to 3);
    subtype other is bv_array(open)(seven downto 0);
  begin
    assert count = 2 report "seven called " & integer'image(count) & " times"
      severity failure;
    declares;
    declares;
    assert calls = 2 report "counted called " & integer'image(calls) & " times"
      severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
