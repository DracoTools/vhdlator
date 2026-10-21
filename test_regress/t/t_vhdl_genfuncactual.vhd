-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An open generic given what a function returns, where which return runs --
-- and so how long the result is -- depends on the value of the argument, a
-- literal or a generic of the root (6.5.6.2, 14.4.2). open-logic's CRC
-- testbenches pass "CrcPolynomial_g => getPolynomial(CrcWidth_g)", the
-- function a case on the width returning x"D5" or x"0589".

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_genfuncactual_crc is
  generic (Poly_g : std_logic_vector);
  port (o : out std_logic_vector(Poly_g'length - 1 downto 0));
end entity;
architecture a of t_vhdl_genfuncactual_crc is
begin
  o <= Poly_g;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_genfuncactual is
  generic (W_g : positive := 16);
end entity;
architecture a of t_vhdl_genfuncactual is
  function by_case(w : natural) return std_logic_vector is
  begin
    case w is
      when 8 => return x"D5";
      when 16 => return x"0589";
      when others => report "unsupported width" severity error;
    end case;
    return "X";
  end function;
  function by_if(w : natural) return std_logic_vector is
  begin
    if w = 8 then
      return x"D5";
    end if;
    return x"0589";
  end function;
  signal o1, o2, o3 : std_logic_vector(W_g - 1 downto 0);
  signal o4 : std_logic_vector(7 downto 0);
begin
  u1 : entity work.t_vhdl_genfuncactual_crc generic map (Poly_g => by_case(W_g)) port map (o1);
  u2 : entity work.t_vhdl_genfuncactual_crc generic map (Poly_g => by_if(W_g)) port map (o2);
  u3 : entity work.t_vhdl_genfuncactual_crc generic map (Poly_g => by_case(16)) port map (o3);
  u4 : entity work.t_vhdl_genfuncactual_crc generic map (Poly_g => by_case(8)) port map (o4);
  process
  begin
    wait for 1 ns;
    assert to_string(o1) = "0000010110001001" report "o1 " & to_string(o1) severity failure;
    assert to_string(o2) = "0000010110001001" report "o2 " & to_string(o2) severity failure;
    assert to_string(o3) = "0000010110001001" report "o3 " & to_string(o3) severity failure;
    assert to_string(o4) = "11010101" report "o4 " & to_string(o4) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
