-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An index a generate's parameter gives, in the arm of an "if" that the iteration never
-- takes, is no error: "c(i - 1)" where i = 0 takes the other arm (NVC issue112). An index
-- only generics give is checked as the model starts only outside a statement.

entity t_vhdl_genindex_unreached is
end entity;

architecture a of t_vhdl_genindex_unreached is
  signal c : bit_vector(7 downto 0) := "10110100";
  signal d : bit_vector(7 downto 0);
begin
  g : for i in 0 to 7 generate
    process (c) is
    begin
      if i = 0 then
        d(i) <= '1';
      else
        d(i) <= c(i - 1);
      end if;
    end process;
  end generate;
  process is
  begin
    wait for 1 ns;
    assert d = "01101001" report "d = " & to_string(d) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
