-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Under --vhdl-metavalues, numeric_std's "+" and "-" of a vector whose
-- length a generic gives, or a generate's parameter, and an integer: the
-- integer is made that long however large it is, cut to the length as
-- TO_UNSIGNED and TO_SIGNED cut it, and is emitted natively at the length
-- the elaboration settles. It checks itself: the packages' "vector
-- truncated" warnings are not printed natively (bugs.md 388mf)

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_nine_genericarith_sub is
  generic (w : integer := 6);
  port (u : in unsigned(w - 1 downto 0);
        s : in signed(w - 1 downto 0);
        n : in integer;
        u1 : out unsigned(w - 1 downto 0);
        u2 : out unsigned(w - 1 downto 0);
        s1 : out signed(w - 1 downto 0);
        s2 : out signed(w - 1 downto 0));
end entity;

architecture a of t_vhdl_nine_genericarith_sub is
begin
  u1 <= u + 21;
  u2 <= u - n;
  s1 <= s + (-3);
  s2 <= 100 - s + n;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_nine_genericarith is
end entity;

architecture a of t_vhdl_nine_genericarith is
  type t_w is array (0 to 2) of integer;
  constant C_W : t_w := (3, 5, 8);
  signal u : unsigned(5 downto 0) := "001010";
  signal s : signed(5 downto 0) := "111100";
  signal n : integer := 7;
  signal u1, u2 : unsigned(5 downto 0);
  signal s1, s2 : signed(5 downto 0);
  type t_res is array (0 to 2) of unsigned(7 downto 0);
  signal res : t_res;
begin
  i : entity work.t_vhdl_nine_genericarith_sub port map (u, s, n, u1, u2, s1, s2);

  g : for k in 0 to 2 generate
    signal d : unsigned(C_W(k) - 1 downto 0) := to_unsigned(k + 1, C_W(k));
    signal e : unsigned(C_W(k) - 1 downto 0);
  begin
    e <= d + 16;
    res(k) <= resize(e, 8);
  end generate;

  process
  begin
    wait for 1 ns;
    report to_string(u1) & " " & to_string(u2) & " " & to_string(s1) & " " & to_string(s2);
    report to_string(res(0)) & " " & to_string(res(1)) & " " & to_string(res(2));
    assert u1 = "011111" and u2 = "000011" and s1 = "111001" severity failure;
    -- 100 is cut to six elements, -28, as TO_SIGNED cuts it
    assert to_integer(s2) = -17 severity failure;
    -- 16 is cut to three elements, 0
    assert res(0) = 1 and res(1) = 18 and res(2) = 19 severity failure;
    n <= 40;
    u <= "0X1010";
    wait for 1 ns;
    report to_string(u1) & " " & to_string(u2) & " " & to_string(s1) & " " & to_string(s2);
    assert to_string(u1) = "XXXXXX" and to_string(u2) = "XXXXXX" severity failure;
    -- 40 is cut to six elements, -24, as TO_SIGNED cuts it
    assert to_integer(s2) = 16 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
