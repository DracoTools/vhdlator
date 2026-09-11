-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A choice is a value of what it chooses among, so a string choice is as
-- long as the expression; a selected assignment names each value once, as a
-- case statement does; an element given a value gets it whole; and a port
-- associated in parts is associated once in every element.

entity t_vhdl_choices_sub is
  port (a : in bit_vector(7 downto 0));
end entity;

architecture a of t_vhdl_choices_sub is
begin
end architecture;

entity t_vhdl_choices_bad is
end entity;

architecture a of t_vhdl_choices_bad is
  type rec is record
    f : bit_vector(3 downto 0);
    g : bit_vector(2 downto 0);
  end record;
  constant n : string(1 to 2) := "ab";
  signal b1, b2 : bit;
  signal v1, v2 : bit_vector(7 downto 0);
  -- Legal
  constant r_ok : rec := (f => "0000", g => "000");
  -- Illegal
  constant r_bad : rec := (others => "000");
begin
  -- Legal
  ok : entity work.t_vhdl_choices_sub port map (a(7 downto 4) => v1(3 downto 0),
                                                a(3 downto 0) => v2(3 downto 0));
  -- Illegal
  bad1 : entity work.t_vhdl_choices_sub port map (a(7 downto 4) => v1(3 downto 0),
                                                  a(4 downto 0) => v2(4 downto 0));
  bad2 : entity work.t_vhdl_choices_sub port map (a(7 downto 4) => v1(3 downto 0),
                                                  a(2 downto 0) => v2(2 downto 0));
  with true select b1 <=
    '0' when false | true,
    '1' when true;
  with 2 select b2 <=
    '0' when 0 to 2,
    '1' when 2 to 3;
  process
  begin
    case n is
      when "aa" => null;
      when "bbb" => null;
      when others => null;
    end case;
    wait;
  end process;
end architecture;
