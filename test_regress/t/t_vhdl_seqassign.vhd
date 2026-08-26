-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Inside a process, "s <= a when c else b" and "with e select s <= ..." are
-- the if and case statements they stand for (10.5.3, 10.5.4), and the same
-- two forms exist for variables (10.6.3, 10.6.4). Where nothing is chosen
-- and no else was written, no assignment happens at all -- so the signal
-- keeps its value rather than taking a new transaction.

use std.env.all;

entity t_vhdl_seqassign is
end entity;

architecture tb of t_vhdl_seqassign is

  signal code  : integer := 2;
  signal cond  : boolean := true;
  signal s1    : bit_vector(3 downto 0) := "0000";
  signal s2    : bit_vector(3 downto 0) := "0000";
  signal s3    : bit_vector(3 downto 0) := "1001";
  signal s4    : integer := 0;
  signal held  : bit_vector(3 downto 0) := "1010";
  signal after1, after2 : bit_vector(3 downto 0) := "0000";

begin

  main : process is
    variable v1, v2, v3 : bit_vector(3 downto 0);
  begin
    -- Conditional signal assignment, taking the first branch
    s1 <= "0011" when cond else "1100";
    -- Selected signal assignment, taking a middle choice
    with code select
      s2 <= "0001" when 1,
            "0010" when 2,
            "1111" when others;
    -- Neither branch holds and there is no else, so s3 is left alone
    s3 <= "0110" when code = 9;
    -- The alternatives may name a range or a list
    with code select
      s4 <= 10 when 0 | 1,
            20 when 2 to 4,
            30 when others;
    -- Each branch carries its own delay
    after1 <= "1111" after 3 ns when cond else "0000" after 1 ns;
    with code select
      after2 <= "0101" after 2 ns when 2,
                "0000" after 9 ns when others;

    -- The same two forms for a variable
    v1 := "1010" when cond else "0101";
    with code select
      v2 := "0001" when 1,
            "0010" when 2,
            "1111" when others;
    v3 := "1110";
    v3 := "0001" when not cond;

    wait for 1 ns;
    assert s1 = "0011" report "s1" severity failure;
    assert s2 = "0010" report "s2" severity failure;
    assert s3 = "1001" report "s3 was written" severity failure;
    assert s4 = 20 report "s4" severity failure;
    assert v1 = "1010" report "v1" severity failure;
    assert v2 = "0010" report "v2" severity failure;
    assert v3 = "1110" report "v3 was written" severity failure;
    assert after1 = "0000" report "after1 early" severity failure;
    wait for 2 ns;
    assert after2 = "0101" report "after2" severity failure;
    assert after1 = "1111" report "after1" severity failure;

    -- Now with the other branches chosen
    cond <= false;
    code <= 1;
    wait for 1 ns;
    s1 <= "0011" when cond else "1100";
    with code select
      s2 <= "0001" when 1,
            "0010" when 2,
            "1111" when others;
    wait for 1 ns;
    assert s1 = "1100" report "s1 else" severity failure;
    assert s2 = "0001" report "s2 first" severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
