-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_metaval is
end entity;

architecture a of t_vhdl_metaval is
begin

  main : process is
    variable s : std_ulogic;
    variable v : std_logic_vector(3 downto 0);
    variable hit : integer;
  begin
    -- This runtime has no metavalues, so a comparison against one is never
    -- true however the value was reached
    s := '0';
    assert not is_x(s) report "zero reads as a metavalue" severity failure;
    assert s /= 'U' report "zero equals U" severity failure;
    assert s /= 'X' report "zero equals X" severity failure;
    assert s /= 'W' report "zero equals W" severity failure;
    assert s /= '-' report "zero equals dash" severity failure;
    s := '1';
    assert not is_x(s) report "one reads as a metavalue" severity failure;
    assert s /= 'X' report "one equals X" severity failure;

    -- Nor can a case choice naming one ever be taken
    s := '0';
    hit := 0;
    case s is
      when 'U' | 'X' | 'W' | '-' => hit := 1;
      when '0' => hit := 2;
      when others => hit := 3;
    end case;
    assert hit = 2 report "case took " & integer'image(hit) severity failure;

    -- A matching case is the exception: there a '-' means any value (10.9)
    v := "1010";
    hit := 0;
    case? v is
      when "10--" => hit := 1;
      when others => hit := 2;
    end case?;
    assert hit = 1 report "matching case took " & integer'image(hit) severity failure;

    v := "0010";
    hit := 0;
    case? v is
      when "10--" => hit := 1;
      when others => hit := 2;
    end case?;
    assert hit = 2 report "matching case matched too much" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;

end architecture;
