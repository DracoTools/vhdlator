-- DESCRIPTION: Verilator: VHDL concatenation with a slice null in some instances
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_catnullgen_sub is
  generic (n : positive := 4);
end entity;

architecture a of t_vhdl_catnullgen_sub is
  subtype word is std_logic_vector(7 downto 0);
  type reg_t is array (integer range <>) of word;
  signal r : reg_t(0 to n - 1) := (others => (others => '0'));
  signal q : reg_t(n - 1 downto 0) := (others => (others => '0'));

  function w(i : natural) return word is
  begin
    return std_logic_vector(to_unsigned(i, 8));
  end function;
begin
  process
    variable d : word := x"55";
    variable v : reg_t(0 to n - 1);
    variable t : reg_t(1 to 2 * n);
  begin
    for i in 0 to n - 1 loop
      r(i) <= w(i + 1);
      q(i) <= w(i + 1);
    end loop;
    wait for 1 ns;
    -- Element first: a shift register, null slice when n = 1
    r <= d & r(0 to r'high - 1);
    -- Element last, downto
    q <= q(q'high - 1 downto 0) & d;
    wait for 1 ns;
    assert r(0) = x"55" severity failure;
    for i in 1 to n - 1 loop
      assert r(i) = w(i) severity failure;
    end loop;
    assert q(0) = x"55" severity failure;
    for i in 1 to n - 1 loop
      assert q(i) = w(i) severity failure;
    end loop;
    -- Slice and slice, each null when n = 1
    v := r(1 to n - 1) & r(0 to 0);
    for i in 0 to n - 2 loop
      assert v(i) = w(i + 1) severity failure;
    end loop;
    assert v(n - 1) = x"55" severity failure;
    -- Whole and whole
    t := r & v;
    assert t(1) = x"55" severity failure;
    assert t(n + 1) = v(0) severity failure;
    assert t(2 * n) = x"55" severity failure;
    report "n=" & integer'image(n) & " ok";
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_catnullgen is
end entity;

architecture a of t_vhdl_catnullgen is
begin
  u1 : entity work.t_vhdl_catnullgen_sub generic map (n => 1);
  u3 : entity work.t_vhdl_catnullgen_sub generic map (n => 3);

  process
  begin
    wait for 5 ns;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
